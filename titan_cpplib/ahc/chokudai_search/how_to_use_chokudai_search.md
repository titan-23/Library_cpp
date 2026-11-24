# Chokudai Search の使用方法

Chokudai Search は、各世代に未展開の候補を残し、浅い世代から順に上位の候補を少しずつ展開する探索です。
同じ世代を何度も巡回するため、最初の巡回で選ばれなかった候補も後から探索できます。
スコアは小さいほど良いものとして扱います。

問題側の `Action` と `State`、設定の `BeamParam`、戻り値の `BeamResult` は既存のビームサーチと共通です。
探索中は `State` を一つだけ保持し、候補間の移動を `apply_op` と `rollback` で行います。

## 実装の選択

| | 毎世代型 | 世代をスキップする型 |
|---|---|---|
| include | `titan_cpplib/ahc/chokudai_search/chokudai_search.cpp` | `titan_cpplib/ahc/chokudai_search/chokudai_search_turn.cpp` |
| クラス | `flying_squirrel::ChokudaiSearch` | `flying_squirrel::ChokudaiSearchTurn` |
| 別名 | `ChokudaiSearchWithTree` | `ChokudaiSearchWithTreeTurn` |
| 次の世代 | 現在の世代 + 1 | `Action::target_turn` |
| 対応する既存 State | `beam_search.cpp` 用 | `beam_search_turn.cpp` 用 |

二つの実装は同時に include できます。どちらも C++20 で利用します。
クラスと別名のテンプレート引数は共通です。

```cpp
template<typename ScoreType, typename HashType, class Action, class State,
         ScoreType INF, bool record_history = false>
```

問題ごとに内容を記述する雛形は `chokudai_search_state.cpp` と `chokudai_search_state_turn.cpp` です。
追加の設定は `chokudai_param.cpp`、二つの型に共通する探索本体は `chokudai_search_base.cpp` にあります。
通常は選択した探索本体だけを include すれば、`BeamParam`、`ChokudaiParam`、`BeamResult` も利用できます。

## 最小の呼び出し例

既存の毎世代型ビームサーチ用に `ScoreType`、`HashType`、`Action`、`State`、`INF` を定義してある場合は、
探索クラスを変更するだけで利用できます。

```cpp
#include "titan_cpplib/ahc/chokudai_search/chokudai_search.cpp"

using Search = flying_squirrel::ChokudaiSearch<
    ScoreType, HashType, Action, State, INF>;

// 最大 100 世代
// 1 巡回につき各世代から最大 1 状態を展開する
// 探索開始から 1,900 ms を過ぎたら打ち切る
flying_squirrel::BeamParam param(100, 1, 1900.0);
Search search;
auto result = search.search(param);

for (const Action &action : result.actions) {
    // 根から順に操作を出力する
}
if (result.final_state) {
    // result.actions を適用した最終 State を参照できる
}
```

世代をスキップする型では include とクラスを次のように置き換えます。

```cpp
#include "titan_cpplib/ahc/chokudai_search/chokudai_search_turn.cpp"

using Search = flying_squirrel::ChokudaiSearchTurn<
    ScoreType, HashType, Action, State, INF>;
```

この場合の `State` には、既存の `beam_search_turn.cpp` と同じインターフェースを実装します。
`target_turn` が `0 → 3 → 7` と進む経路では、操作数は 2、到達世代は 7 になります。

## 設定と巡回

探索には `BeamParam(max_turn, beam_width, time_limit_ms)` をそのまま渡すことができます。

| 設定 | Chokudai Search での意味 |
|---|---|
| `max_turn` | 到達を許す世代の最大値。毎世代型では操作数の最大値と等しい。正の値が必要 |
| `beam_width` | 1 巡回で各世代から展開する状態数の上限。正の値が必要 |
| `time_limit` | 探索開始からの時間上限 [ms]。`0` 以上の場合に有効で、負の場合は時間無制限 |
| `is_adjusting` | 使用しない。展開数は `beam_width` で固定 |
| `clear_hash_every_turn` | 使用しない。世代ごとに探索全体を通して重複除去する |
| `hash_window_turns` | 使用しない |
| `seen_hash_capacity_hint` | 使用しない |

各巡回では世代の小さい順に候補を展開します。巡回の途中で生成したより大きい世代の候補も、同じ巡回でその世代に進んだときに展開できます。
初期状態の展開も最初の巡回に含めます。`max_turn` に達した状態と `finished` の状態はそれ以上展開しません。
`beam_width` は保存する候補数を制限しません。まだ展開していない候補は次の巡回に残します。

時間に依存しない打ち切りには、`BeamParam` を継承した `ChokudaiParam` の `max_sweeps` を使用します。

```cpp
flying_squirrel::ChokudaiParam param(100, 1, -1);
param.max_sweeps = 20;  // 各世代を最大 20 回巡回する
auto result = search.search(param);
```

`max_sweeps` の既定値は `-1` で、この場合は巡回数を制限しません。`0` では初期状態も展開せずに終了し、`-1` 未満は不正な設定です。
時間と巡回数を両方指定した場合は、先に達した上限で停止します。
どちらも無制限の場合は展開可能な候補がなくなるまで探索するため、探索空間が大きいと実行時間とメモリ使用量が増加します。

`ChokudaiParam` はそのまま `search` に渡します。`BeamParam` へコピーしたり `BeamParam&` として渡したりすると、
追加の設定である `max_sweeps` は探索に反映されません。

## State のインターフェース

どちらの型でも次の関数を実装します。

```cpp
void init();
void apply_op(const Action &action);
void rollback(const Action &action);
```

`init` は探索ごとに一度呼び出されます。`try_op` は State を変更せず、Action にロールバック用の情報などを記録します。
`apply_op` と `rollback` は差分更新として実装し、ロールバックでは適用時の変更を取り消します。探索中に State はコピーしません。

### 毎世代型

```cpp
template<class Submit>
void enumerate_actions(int turn, const Action &last_action,
                       Submit &&submit) const {
    // turn == 0 では last_action はダミー
    Action action;
    // 合法な操作ごとに action を設定して submit(action) を呼び出す
    // submit.threshold() を既存コードと同様に参照できる
}

tuple<ScoreType, HashType, bool>
try_op(Action &action, ScoreType threshold) const;
```

古い形式の `enumerate_actions(vector<Action>& actions, int turn, const Action& last_action, ScoreType threshold)`
にも対応しています。両方を実装している場合はコールバック形式を使用します。

### 世代をスキップする型

```cpp
struct Action {
    int target_turn = -1;
    // 操作とロールバックに必要な情報
};

template<class Submit>
void enumerate_actions(const Action &last_action, Submit &&submit) const {
    // last_action.target_turn == -1 なら初期状態
    // 合法な操作ごとに submit(action) を呼び出す
    // submit.threshold(target_turn) を参照できる
}

tuple<ScoreType, HashType, bool>
try_op(Action &action, const vector<ScoreType> &thresholds) const;
```

`try_op` で `action.target_turn` を確定させます。この値は現在の世代より真に大きく、`max_turn` 以下である必要があります。
条件を満たさない候補は採用しません。同じ列挙の中で到達世代の異なる候補を混在させることができます。
`thresholds` の長さは `max_turn + 1` です。問題側で `thresholds[action.target_turn]` を参照する場合は、
先に添字の範囲を確認します。

### 評価、閾値、重複除去

`try_op` の戻り値は `{次状態のスコア, 次状態のハッシュ, finished}` です。
スコアが `INF` 以上の候補は採用しません。

`submit.threshold()` と `submit.threshold(target_turn)` が返す値と `try_op` に渡す閾値は、常に `INF` です。
この探索はビーム幅で候補を除外しないため、ビームサーチの「保存した候補の中の最悪スコア」に相当する枝刈りの閾値は使用しません。
途中の候補と完成解ではスコアの大小関係が保証されないため、完成解を得た後も途中の候補をスコアで枝刈りしません。

未完成で最大世代に達していない候補は、世代とハッシュが同じ候補のこれまでの最良スコアを真に改善する場合だけ採用します。
展開済みの状態についても最良スコアを記録し、スコアを改善する経路が後から見つかった場合は、その状態が再び展開の対象になります。
ハッシュが同じでも世代が異なる候補は別の候補として扱います。
ハッシュには、その世代以降の遷移と評価に必要な状態を含めます。

`finished=true` は最終解として採用できる状態を表し、その状態はそれ以上展開しません。
完成解を見つけた後も残りの候補を探索し、よりスコアの小さい完成解が見つかった場合は更新します。
完成解と最大世代に達した候補は、それぞれの種類のこれまでの最良スコアと直接比較します。未完成の候補とハッシュが重複していても破棄しません。
遷移先のない状態は、`finished` にせずに合法な操作を一つも submit しないことで表します。

## 戻り値と停止理由

戻り値の型は既存のビームサーチと同じ `BeamResult<ScoreType, Action, State>` です。
返す経路は次の優先順位で選択します。

1. `finished=true` の候補のうちスコアが最小のもの
2. 完成解がない場合は、`max_turn` に到達した候補のうちスコアが最小のもの
3. どちらもない場合は、受理した候補が到達した最大の世代にある途中経路のうちスコアが最小のもの

| フィールド | 意味 |
|---|---|
| `actions` | 根から選択した候補までの操作列 |
| `score` | 選択した候補の評価値。経路がない場合は `INF` |
| `turns_done` | 探索中に生成した有効な候補が到達した最大の世代。巡回数や返す経路の操作数とは異なる |
| `elapsed_ms` | 探索にかかった時間 [ms]。最終 State の復元時間は含まない |
| `status` | 下表の終了状態 |
| `final_state` | `actions` を適用した最終状態。生成しない場合と経路がない場合は `nullptr` |

| `BeamStatus` | 条件 |
|---|---|
| `Finished` | 完成解を返す |
| `MaxTurnReached` | 完成解がなく、最大世代の経路を返す |
| `TimeLimitReached` | 完成解も最大世代の経路もなく、時間上限で停止した |
| `SweepLimitReached` | 完成解も最大世代の経路もなく、巡回数の上限で停止した |
| `NoCandidates` | 完成解も最大世代の経路もなく、展開できる候補がなくなった |
| `InvalidParameter` | `max_turn <= 0`、`beam_width <= 0`、`max_sweeps < -1`、または `time_limit` が NaN か無限大 |

たとえば時間上限で探索を停止した場合も、完成解があれば `status == Finished` になります。
`TimeLimitReached`、`SweepLimitReached`、`NoCandidates` の場合も途中経路を返すことがあります。
`result.finished()` は `status == Finished` であること、`result.has_path()` は操作列が空でないことを表します。

最終 State が不要な場合は `search.search<false>(param)` を呼び出します。
この場合は State のコピーとムーブが禁止されていても利用できます。
既定の `search<true>` では最終 State を結果に格納するため、State がムーブまたはコピー可能である必要があります。

## 既存ビームサーチからの移行

`Action`、`State`、`BeamParam`、結果の受け取り方は、対応する型の既存コードを流用できます。
include と探索クラスを置き換えた上で、次の設定と終了条件を見直します。

- `beam_width` は、各世代に残す件数ではなく巡回ごとに展開する件数を表します。`1` などの小さな値から調整するようにします。
- `time_limit` は `is_adjusting` に関係なく探索を時間で打ち切る上限になります。負の場合は時間無制限です。
- `finished` の状態を見つけても停止せず、時間か巡回数の上限に達するか、候補がなくなるまで探索を続けます。
- ハッシュに関する `BeamParam` の設定は使用せず、常に世代ごとに探索中の最良スコアを記録します。
- 閾値は常に `INF` です。threshold を使用する既存の早期打ち切りのコードはそのまま呼び出すことができますが、ビーム幅による枝刈りは発生しません。
- 候補が尽きた場合も途中経路を返すことがあるため、`status` と `actions` をそれぞれ確認します。

## 時間、メモリ、履歴

時間上限は、状態を展開する前と `submit` の呼び出し時に確認します。
`enumerate_actions` の中で submit せずに長時間計算する処理や、一回の `try_op` などの実行中は、時間上限に達しても中断できません。
解の操作列と最終 State の復元にも時間が必要なため、提出時は出力にかかる時間も考慮して `time_limit` に余裕を持たせるようにします。

受理した候補数を `N`、最大操作数を `D` とすると、候補の優先度付きキューへの追加と取り出しは `O(log N)`、
一つの候補へ移動するための差分操作数は最悪 `O(D)` です。
このほかに、合法な操作の生成と評価、巡回の管理に時間がかかります。

探索中は、候補ごとの Action と親の情報、未展開の候補のキュー、世代とハッシュごとの最良スコアを保持します。
主なメモリ使用量は受理した候補数と世代数に比例します。State を候補ごとにコピーする必要はありませんが、
`beam_width` を小さくしてもメモリ使用量は制限されません。
親の経路からも最良解からも参照されなくなったノードの領域は再利用します。
一方で、展開済みのハッシュの最良スコアは探索の終了まで残り、未展開の候補とその親も保持するため、探索時間に応じてメモリ使用量が増加します。

`verbose=true` を指定すると、標準エラーへ探索のログを出力します。
テンプレート引数 `record_history=true` と `history_file` を指定すると、既存のビームサーチと同じ形式の JSON の履歴を出力します。
履歴を出力する場合は `Action::to_string() const` と `State::get_state_info() const` も実装します。
履歴には生成した候補の情報を保存するため、通常の探索より多くの時間とメモリを使用します。
`BeamParam` の集計用の `int` フィールドは上限で飽和させます。集計値は探索順や展開数の決定には使用しません。

```cpp
flying_squirrel::ChokudaiSearch<
    ScoreType, HashType, Action, State, INF, true> logged_search;
auto result = logged_search.search(param, true, "chokudai_history.json");
```

## 検証

```bash
bash test/ahc/chokudai_search.sh
CHOKUDAI_SANITIZE=1 bash test/ahc/chokudai_search.sh
```

ランダムに生成したケースで独立した参照実装と結果を比較します。このほかに次の項目を確認します。

- 巡回による解の改善
- 重複した候補の再展開
- 世代のスキップ
- 差分更新
- 経路の復元
- 時間と巡回数の上限
- 履歴の出力
