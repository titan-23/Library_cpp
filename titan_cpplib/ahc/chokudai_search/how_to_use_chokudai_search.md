# Chokudai Search の使い方

各世代に未展開候補を残し、浅い世代から順に上位候補を少しずつ展開する探索。
同じ世代を何度も巡回するため、最初の巡回で選ばれなかった候補も後から探索できる。
スコアは小さいほど良いものとして扱う。

問題側の `Action` / `State`、設定の `BeamParam`、戻り値の `BeamResult` は既存ビームサーチと共通。
`State` は一つだけ持ち、候補間の移動を `apply_op` / `rollback` で行う。

## 実装の選択

| | 毎世代型 | 世代を飛ばす型 |
|---|---|---|
| include | `titan_cpplib/ahc/chokudai_search/chokudai_search.cpp` | `titan_cpplib/ahc/chokudai_search/chokudai_search_turn.cpp` |
| クラス | `flying_squirrel::ChokudaiSearch` | `flying_squirrel::ChokudaiSearchTurn` |
| 別名 | `ChokudaiSearchWithTree` | `ChokudaiSearchWithTreeTurn` |
| 次の世代 | 現在の世代 + 1 | `Action::target_turn` |
| 対応する既存 State | `beam_search.cpp` 用 | `beam_search_turn.cpp` 用 |

二つの実装は同時に include できる。どちらも C++20 で利用する。
クラスと別名のテンプレート引数は共通である。

```cpp
template<typename ScoreType, typename HashType, class Action, class State,
         ScoreType INF, bool record_history = false>
```

問題ごとに埋める雛形は `chokudai_search_state.cpp` と `chokudai_search_state_turn.cpp`。
設定の追加部分は `chokudai_param.cpp`、二つの型に共通する探索本体は `chokudai_search_base.cpp` にある。
通常は選んだ探索本体だけを include すれば、`BeamParam`、`ChokudaiParam`、`BeamResult` も利用できる。

## 最小の呼び出し例

既存の毎世代型ビームサーチ用に `ScoreType`、`HashType`、`Action`、`State`、`INF` を定義してある場合、
探索クラスを変更するだけで利用できる。

```cpp
#include "titan_cpplib/ahc/chokudai_search/chokudai_search.cpp"

using Search = flying_squirrel::ChokudaiSearch<
    ScoreType, HashType, Action, State, INF>;

// 最大 100 世代。1 巡回につき各世代から最大 1 状態を展開する。
// 探索開始から 1,900 ms を過ぎたら打ち切る。
flying_squirrel::BeamParam param(100, 1, 1900.0);
Search search;
auto result = search.search(param);

for (const Action &action : result.actions) {
    // 根から順に操作を出力する。
}
if (result.final_state) {
    // result.actions を適用した最終 State を参照できる。
}
```

世代を飛ばす型では include とクラスを次のように置き換える。

```cpp
#include "titan_cpplib/ahc/chokudai_search/chokudai_search_turn.cpp"

using Search = flying_squirrel::ChokudaiSearchTurn<
    ScoreType, HashType, Action, State, INF>;
```

この場合の `State` は、既存の `beam_search_turn.cpp` と同じインターフェースを持たせる。
`target_turn` が `0 → 3 → 7` と進む経路では、操作数は 2、到達世代は 7 になる。

## 設定と巡回

`BeamParam(max_turn, beam_width, time_limit_ms)` をそのまま渡せる。

| 設定 | Chokudai Search での意味 |
|---|---|
| `max_turn` | 到達を許す世代の最大値。毎世代型では最大手数。正の値が必要 |
| `beam_width` | 1 巡回で各世代から展開する状態数の上限。正の値が必要 |
| `time_limit` | 探索開始からの時間上限 [ms]。`0` 以上で有効、負なら時間無制限 |
| `is_adjusting` | 使用しない。展開数は `beam_width` で固定 |
| `clear_hash_every_turn` | 使用しない。世代ごとに探索全体を通して重複除去する |
| `hash_window_turns` | 使用しない |
| `seen_hash_capacity_hint` | 使用しない |

各巡回では世代の小さい順に候補を展開する。同じ巡回で生成した未来世代の候補も、その世代に進んだときに展開できる。
初期状態の展開も最初の巡回に含む。`max_turn` に達した状態と `finished` の状態は、それ以上展開しない。
`beam_width` は保存する候補数の上限ではない。まだ展開していない候補は次の巡回に残す。

時間に依存しない打ち切りには、`BeamParam` を継承した `ChokudaiParam` の `max_sweeps` を使う。

```cpp
flying_squirrel::ChokudaiParam param(100, 1, -1);
param.max_sweeps = 20;  // 各世代を最大 20 巡回する。
auto result = search.search(param);
```

`max_sweeps` の既定値は `-1` で、巡回数を制限しない。`0` では初期状態も展開せず終了し、`-1` 未満は不正な設定となる。
時間と巡回数を両方指定した場合は、先に達した制限で停止する。
どちらも無制限の場合は展開可能な候補がなくなるまで探索するため、大きな探索空間では時間やメモリが大きくなる。

`ChokudaiParam` はそのまま `search` に渡す。`BeamParam` へコピーしたり、`BeamParam&` として渡したりすると、
追加設定の `max_sweeps` は探索に渡らない。

## State のインターフェース

共通して次の関数を実装する。

```cpp
void init();
void apply_op(const Action &action);
void rollback(const Action &action);
```

`init` は探索ごとに一度呼ばれる。`try_op` は State を変更せず、Action にロールバック情報などを記録する。
`apply_op` と `rollback` は対称な差分更新にする。探索中に State のコピーは行わない。

### 毎世代型

```cpp
template<class Submit>
void enumerate_actions(int turn, const Action &last_action,
                       Submit &&submit) const {
    // turn == 0 では last_action はダミー。
    Action action;
    // 合法手ごとに action を設定して submit(action) を呼ぶ。
    // submit.threshold() を既存コードと同様に参照できる。
}

tuple<ScoreType, HashType, bool>
try_op(Action &action, ScoreType threshold) const;
```

古い形式の `enumerate_actions(vector<Action>& actions, int turn, const Action& last_action, ScoreType threshold)`
にも対応する。callback 形式があればそちらを使う。

### 世代を飛ばす型

```cpp
struct Action {
    int target_turn = -1;
    // 操作とロールバックに必要な情報。
};

template<class Submit>
void enumerate_actions(const Action &last_action, Submit &&submit) const {
    // last_action.target_turn == -1 なら初期状態。
    // 合法手ごとに submit(action) を呼ぶ。
    // submit.threshold(target_turn) を参照できる。
}

tuple<ScoreType, HashType, bool>
try_op(Action &action, const vector<ScoreType> &thresholds) const;
```

`try_op` で `action.target_turn` を確定させる。現在世代より真に大きく、`max_turn` 以下である必要がある。
条件を満たさない候補は採用しない。同じ列挙中に異なる到達世代を混在させてよい。
`thresholds` の長さは `max_turn + 1`。問題側で `thresholds[action.target_turn]` を参照する場合は、
添字の範囲確認を先に行う。

### 評価、閾値、重複除去

`try_op` の戻り値は `{次状態のスコア, 次状態のハッシュ, finished}`。
スコアが `INF` 以上の候補は採用しない。

`submit.threshold()`、`submit.threshold(target_turn)`、`try_op` に渡す閾値は常に `INF`。
候補を幅で捨てない探索なので、ビームサーチの「保存候補中の最悪スコア」に相当する枝刈り閾値は使わない。
途中スコアと完成解スコアは大小関係が保証されないため、完成解を得ても途中候補のスコアによる枝刈りは行わない。

未完成で最大世代に達していない候補は、同じ世代・同じハッシュについて、これまでの最良スコアより真に改善するものだけを採用する。
すでに展開した状態についても最良スコアを記録し、改善した経路が後から現れた場合は再び展開対象になる。
異なる世代の同一ハッシュは別の候補として扱う。
ハッシュには、その世代における今後の遷移・評価に必要な状態を含める。

`finished=true` は最終解として採用できる状態を表し、それ以降は展開しない。
完成解を見つけた後も残りの候補を探索し、より小さいスコアの完成解に更新する。
完成解と最大世代に達した候補はそれぞれ最良スコアを直接比較し、未完成候補との hash 重複で捨てることはない。
行き止まりの状態は、`finished` にせず合法手を一つも submit しないことで表す。

## 戻り値と停止理由

返す型は既存と同じ `BeamResult<ScoreType, Action, State>`。
返却経路を次の優先順位で選ぶ。

1. `finished=true` の候補のうち最小スコア。
2. 完成解がなければ、`max_turn` に到達した候補のうち最小スコア。
3. どちらもなければ、受理した候補の最大到達世代にある最小スコアの途中経路。

| フィールド | 意味 |
|---|---|
| `actions` | 根から選択した候補までの操作列 |
| `score` | 選択した候補の評価値。経路がなければ `INF` |
| `turns_done` | 探索中に生成した有効候補の最大到達世代。巡回数や返却経路の操作数ではない |
| `elapsed_ms` | 探索にかかった時間 [ms]。最終 State の復元時間は含まない |
| `status` | 下表の終了状態 |
| `final_state` | `actions` を適用した最終状態。生成しない場合や経路がない場合は `nullptr` |

| `BeamStatus` | 条件 |
|---|---|
| `Finished` | 完成解を返す |
| `MaxTurnReached` | 完成解がなく、最大世代の経路を返す |
| `TimeLimitReached` | 上のどちらもなく、時間上限で停止 |
| `SweepLimitReached` | 上のどちらもなく、巡回数上限で停止 |
| `NoCandidates` | 上のどちらもなく、展開候補が尽きた |
| `InvalidParameter` | `max_turn <= 0`、`beam_width <= 0`、`max_sweeps < -1`、または `time_limit` が NaN / 無限大 |

たとえば時間上限で探索を止めても、完成解があれば `status == Finished` になる。
`TimeLimitReached`、`SweepLimitReached`、`NoCandidates` でも途中経路を返すことがある。
`result.finished()` は `status == Finished`、`result.has_path()` は操作列が空でないことを表す。

最終 State が不要なら `search.search<false>(param)` とする。
この場合、State のコピー・ムーブが禁止されていても利用できる。
既定の `search<true>` では最終 State を結果に渡すため、State をムーブまたはコピーできる必要がある。

## 既存ビームサーチからの移行

`Action`、`State`、`BeamParam`、結果の受け取り方は対応する型の既存コードを流用できる。
include と探索クラスを置き換えた上で、次の設定と終了条件を見直す。

- `beam_width` は毎世代に残す件数から、巡回ごとに展開する件数に変わる。まずは `1` など小さな値から調整する。
- `time_limit` は `is_adjusting` に関係なく時間打ち切りとして働く。負なら時間無制限になる。
- `finished` の発見時には停止せず、時間・巡回数・候補の枯渇まで探索を続ける。
- hash に関する `BeamParam` の設定は使わず、常に世代ごとに探索中の最良スコアを記録する。
- 閾値は常に `INF`。既存の threshold を使う早期打ち切りコードはそのまま呼べるが、ビーム幅による枝刈りは起こらない。
- 候補が尽きた場合も途中経路が返ることがあるため、`status` と `actions` をそれぞれ確認する。

## 時間、メモリ、履歴

時間上限は状態の展開前と `submit` 時に確認する。
`enumerate_actions` 内で submit せずに長時間計算する処理や、一回の `try_op` などの実行途中には割り込めない。
解の操作列・最終 State の復元にも時間が必要なので、提出時は出力も含めた余裕を `time_limit` に持たせる。

受理した候補数を `N`、最大操作数を `D` とすると、候補の優先度キューへの追加・取り出しは `O(log N)`、
一つの候補へ移動する差分操作数は最悪 `O(D)`。
このほかに合法手の生成・評価と巡回管理の時間がかかる。

候補の Action と親情報、未展開キュー、世代別 hash の最良記録を探索中に保持する。
主な保存量は受理候補数と世代数に比例し、State を候補ごとにコピーする必要はないが、
`beam_width` を小さくしてもメモリ使用量は制限されない。
親経路や最良解からも参照されなくなったノードの領域は再利用する。
一方、展開済み hash の最良スコアは探索全体を通して残り、未展開候補やその親も保持するため、探索時間に応じてメモリが増える。

`verbose=true` で標準エラーへ探索ログを出力する。
テンプレート引数 `record_history=true` と `history_file` を指定すると、既存ビームサーチと同じ形式の JSON 履歴を出力する。
履歴出力時には `Action::to_string() const` と `State::get_state_info() const` も実装する。
履歴は生成候補の情報を保存するため、通常の探索より時間とメモリを使う。
`BeamParam` の集計用 `int` フィールドは上限で飽和させる。集計値は探索順や展開数の決定には使わない。

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

独立した参照実装とのランダム比較に加え、巡回による解の改善、重複候補の再展開、
世代のスキップ、差分更新、経路復元、時間・巡回制限、履歴出力を確認する。
