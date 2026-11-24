## ビームサーチ実装ガイド

### 0. ライブラリ概要

`flying_squirrel::BeamSearchWithTree` は探索木の上で状態を差分更新するビームサーチです。

各ターンでは次のように探索木をオイラーツアーの順に DFS で辿ります。

```
state->init()                                  // 初回に 1 回だけ

for each turn:
    for each node in beam tree (DFS 順):
        PRE_ORDER  : apply_op(action)          // 子へ進む
        LEAF       : enumerate_actions(turn, last, submit)   // 合法な操作を 1 つずつ生成して
                     // submit(action) を呼ぶと、内部で try_op、判定、候補への push を実行する
        POST_ORDER : rollback(action)          // 親へ戻る
```

`apply_op` と `rollback` は完全に対称にします。同一の `action` に対して `apply_op → rollback` を呼んだ後は、状態が完全に元に戻っている必要があります。

関数の間では `Action` を介して情報を受け渡します。`enumerate_actions` が操作を生成して `submit(action)` を呼ぶと、ライブラリが `try_op` を呼びます。`try_op` は `State` を変更せずにスコアとハッシュを計算し、`action` に `pre_*` などを格納します。ビームに残った候補では `apply_op` がその値を参照して差分更新し、`rollback` が `pre_*` で差分を巻き戻します。`try_op` の `const` は `State` を変更しないことを表し、値を格納する先は `action` です。各関数の詳細は Section 2 を参照してください。

前提

- `State` オブジェクトは探索を通して 1 つだけ生成され、ディープコピーは発生しません。
- スコアは最小化します。最大化問題ではスコアを負にして返します。
- `try_op(action)` が返す score は、ビームに入れる候補を選ぶための評価値です。累積スコアそのものでも、累積しない一時的な評価を加えた値でもかまいません。
- `State` が保持する累積スコアは、`try_op` の評価値を計算するための状態です。`apply_op` で正しく差分更新し、`rollback` で正しく復元します。
- `try_op(action)` が返す hash は、同じ action を `apply_op` した直後の状態を表す値にします。内部では現在値からの XOR で計算してもかまいませんが、返す hash は差分ではなく次の状態の値です。
- 関数のシグネチャは変更しません。

---

### 0.5 セットアップ

#### 使用ファイル

| include するファイル | 役割 |
|---|---|
| `titan_cpplib/ahc/beam_search/beam_search.cpp` | 探索木の上で状態をインプレースに差分更新する版。`State` は 1 個で、コピーは発生しない。`search()` を提供する |
| `beam_param.cpp` | `BeamParam` を定義する。`beam_search.cpp` から自動的に include されるため、個別の include は不要 |

#### 自分で定義すべき記号

次の記号はライブラリでは定義しないため、`State` を定義する名前空間内で宣言します。`ScoreType`, `HashType`, `INF` はテンプレート引数として渡すため、定義しないとコンパイルできません。

| 記号 | 例 | 意味 |
|---|---|---|
| `ScoreType` | `using ScoreType = int;` (`long long`/`double`) | スコアの型。最小化する。最大化問題では符号を反転する |
| `HashType` | `using HashType = uint64_t;` | Zobrist ハッシュの型 |
| `INF` | `const ScoreType INF = 1e9;` | `try_op` で打ち切るときに返す番兵。到達し得るどの実際のスコアよりも大きく、`ScoreType` でオーバーフローしない値にする。実際のスコアが `INF` 以上になる設計は不可。`-INF` は内部で番兵に使用するため、`ScoreType` で表現できる必要がある |
| `brnd` | `titan23::Random brnd;` | 利用者が Zobrist ハッシュ用の乱数を得るために使用する乱数生成器。乱数は `brnd.rand_u64()` で取得する。`random.cpp` の include が必要 |

#### 最小テンプレート

次のテンプレートをコピーし、State の中身だけを記述します。

```cpp
#include <bits/stdc++.h>
#include "titan_cpplib/others/print.cpp"
#include "titan_cpplib/ahc/timer.cpp"
#include "titan_cpplib/alg/random.cpp"
#include "titan_cpplib/ahc/beam_search/beam_search.cpp"            // search() 本体
using namespace std;

namespace beam_search {
    using ScoreType = int;                 // 最小化
    using HashType  = unsigned long long;
    const ScoreType INF = 1e9;
    titan23::Random brnd;                  // brnd.rand_u64()

    struct Action {
        // 操作パラメータ + pre_/nxt_* (Section 2)
        friend ostream& operator<<(ostream& os, const Action&){ return os; }
        string to_string() const { return ""; }
    };
    class State { /* init/try_op/apply_op/rollback/enumerate_actions(submit)/print/get_state_info */ };

    using Result = flying_squirrel::BeamResult<ScoreType, Action, State>;

    template<bool materialize_final_state=true>
    Result search(flying_squirrel::BeamParam &param, const bool verbose=false, const string& history_file = "") {
        flying_squirrel::BeamSearchWithTree<ScoreType, HashType, Action, State, INF> bs;
        return bs.search<materialize_final_state>(param, verbose, history_file);
    }
}

int main() {
    flying_squirrel::BeamParam param(/*max_turn=*/2000, /*beam_width=*/100,
                                     /*time_limit_ms=*/-1, /*is_adjusting=*/false);
    auto result = beam_search::search(param); // search<true>() と同じで、最終 State を構築する
    for (auto &a : result.actions) { /* a を根から葉の順に適用し、解を再生して出力する */ }
    // result.final_state は result.actions 適用後の State
}
```

#### `BeamParam` 仕様

コンストラクタ:
`flying_squirrel::BeamParam param(int max_turn, int beam_width, double time_limit_ms, bool is_adjusting=false, bool clear_hash_every_turn=true, int hash_window_turns=0);`

| 引数 | 型/単位 | 既定 | 意味 |
|---|---|---|---|
| `max_turn` | int。ターン数は木の深さに相当する | — | 探索木の最大深さ。これを超えると探索を打ち切る。1 操作で 1 ターン進む場合は最大の操作数に等しい |
| `beam_width` | int | — | ビーム幅。`is_adjusting=true` のときは実効幅の基準値になる。base 版では実効幅がこの値を超えて増えることがあり、turn 版ではこの値が上限になる |
| `time_limit_ms` | double, ms | — | `is_adjusting=true` のときに実効幅の見積もりに使用する持ち時間。単位は `Timer::elapsed()` と同じで、1 秒が 1000。この時間で探索を強制的に打ち切るわけではない。探索ループは時間では停止せず、`max_turn` に達するか `finished` になるまで実行される。`is_adjusting=false` のときは使用されない |
| `is_adjusting` | bool | `false` | `true` のときは残りの持ち時間から実効幅を動的に増減する。実行時間そのものを制御するわけではない。通常は `false` を推奨する |
| `clear_hash_every_turn` | bool | `true` | 重複排除用の hash を毎ターン clear する。安全側の既定値。`false` はターンをまたいで hash を保持する最適化用。`State` の hash がターンの情報を含まない場合は、候補が前のターンの entry と重複したものとして扱われ、破棄される。この挙動を理解した上でのみ使用する |
| `hash_window_turns` | int | `0` | `clear_hash_every_turn=false` のときだけ有効。`>0` なら K ターンごとに hash dict をすべて clear し、蓄積を抑える。`0` では際限なく蓄積する。推奨値は 0 か 1。base 版でのみ有効で、turn 版では使用されない |

`is_adjusting=false` で使用する場合は実行時間が `time_limit_ms` で制御されないため、実行時間は `max_turn` と `beam_width` で調整します。時間内に収める場合は `is_adjusting=true` にして `time_limit_ms` に持ち時間を渡しますが、この設定も幅を調整するだけで、ループ自体は `max_turn` まで実行されます。

#### 戻り値の意味

`search()` は `BeamResult<ScoreType, Action, State>` を返します。

| フィールド | 意味 |
|---|---|
| `actions` | 根から最良の葉までの `Action` の列。`apply_op` で適用する順序と一致する |
| `score` | `actions` の末尾の候補を選択したときの `try_op` の評価値。経路がない場合は `INF` |
| `turns_done` | 探索が処理した論理ターン数 |
| `elapsed_ms` | `State::init()` から `actions` が確定するまでの時間。最終 State の構築時間は含まない |
| `status` | `Finished` / `MaxTurnReached` / `NoCandidates` / `InvalidParameter` |
| `final_state` | `actions` を適用した後の `State`。構築しない場合や経路がない場合は `nullptr` |

`search()` のテンプレート引数 `materialize_final_state` の既定値は `true` です。最終 State が不要な場合は
`search<false>(param, ...)` とします。この場合、最終 State の復元処理は `if constexpr` により
コンパイル時に除去されます。

解を再生するには `result.actions` を先頭から順に適用します。最終状態を直接使用する場合は
`result.final_state` が保持する `State` を参照します。

既定では `max_turn` ターンまで進むため、`finished` に到達しなければ
`actions.size()` は `max_turn` になります。
`finished` で打ち切られた場合に限り、その時点までの短い列が返ります。

---

### 1. 実装上の制約と高速化の指針

Action 構造体のサイズ最小化
- `Action` は毎ターン多数生成され、コピーされます。ロールバックに必要な情報だけを保持し、型は必要十分な最小のサイズを選びます。`int` の代わりに `short` や `int8_t` で足りる場合は、小さい型に変更します。

`score` / `hash` を `Action` に保持するかの判断
- 保持する場合: `try_op` で計算した値を `action.nxt_*` に保存し、`apply_op` では `score = action.nxt_score` とするだけです。実装は単純ですが、`sizeof(Action)` が増えます。
- 保持しない場合: `apply_op` 内で操作パラメータから再計算します。`Action` は小さくなりますが、`try_op` と `apply_op` で同じ計算を二度行います。
- 判断基準: 差分計算のコストが小さい場合は「保持しない」、それ以外は「保持する」を選ぶのが無難です。

スコア計算の早期打ち切り
- `try_op` に渡される `threshold` は、現在のビーム内の最悪スコアです。ビームの候補数が `beam_width` 個に達するまでは `threshold == INF` です。
- 差分計算の途中で、次のスコアが `threshold` 以上になると確定した時点で `{INF, 0, false}` を返します。
- この場合、その `Action` は `apply_op` されないため、ロールバック情報の格納も省略します。
- スコアを大きく変化させる要因から先に計算し、`threshold` を超えることを早く検出します。

`enumerate_actions` 側でスコアまで計算する選択肢
- `try_op` の差分計算に、`enumerate_actions` が合法な操作の生成中に参照する情報をそのまま使用できる場合は、`enumerate_actions` 内でスコアまで計算して `Action` に保存する方が速いことがあります。`try_op` は保存した値を返すだけになり、データ構造への二重のアクセスを避けることができます。
- この場合でも、`submit.threshold()` による枝刈りは submit の前に行います。

ホットパス `try_op` / `enumerate_actions` でのコピー回避
- ビームサーチの中で最も多く呼ばれる箇所です。
- `State` のメンバの一時的なコピーは禁止です。`auto tmp_board = board;` のような盤面のコピーが該当します。
- スコアとハッシュの計算は、スカラーのローカル変数だけで完結させます。
- 内部で `vector` の生成やコピーをしません。ヒープアロケーション全般を禁止します。

Zobrist Hash の設計
- XOR による差分更新を実装します。
- ライブラリは同一のハッシュ値の候補を同一の状態とみなし、スコアの良い方だけを残します。
- ハッシュ化する対象は、その後の状態遷移やスコア計算に影響する要素に限定します。無関係な情報を含めると、同一視すべき状態が別の状態として扱われ、ビームの多様性が低下します。
- 乱数テーブルは `brnd.rand_u64()` で事前に初期化します。

状態の差分更新
- `State` 内の盤面配列などのディープコピーは禁止です。変更箇所だけを計算し、更新します。

`enumerate_actions` での枝刈り
- スコアを悪化させる操作や逆操作は submit しません。有望な操作を先に submit して、`submit.threshold()` を早く下げます。詳細は Section 2 の `enumerate_actions` を参照してください。

---

### 2. 各関数の仕様と実装要件

#### Action 構造体

- 状態遷移を表す「操作パラメータ」と `rollback` に必要な「巻き戻し情報」をまとめて保持します。
- スケルトン内のメンバ変数は一例です。問題に応じて自由に変更、追加できます。
- `try_op` で置き換えられる前の値 `pre_*` と `apply_op` 後の値 `nxt_*` を保持するのが基本パターンです。詳細は Section 1 の「保持するかの判断」を参照してください。
- `string to_string() const` はデバッグ出力用です。テンプレートの第 6 引数 `record_history` (既定値 `false`) を `true` にすると、探索履歴を `search()` の `history_file` へ JSON で出力します。中身は空でもかまいませんが、削除はしないでください。

#### `State::init`

- シグネチャ: `void init()`
- 探索開始時の初期状態を構築します。`search()` から 1 回だけ呼ばれます。
- 盤面、スコア、ハッシュ値を初期化します。標準入力からの問題データの読み込みもここで行います。
- Zobrist Hash 用の乱数テーブルは一度だけ初期化します。`State` は探索を通して 1 個であるため、テーブルはメンバでも `static` でもかまいません。ただし、`init` が複数回呼ばれてもテーブルを再作成しないように、初期化フラグを使用してテーブルの初期化が一度だけ行われるようにします。盤面サイズに依存しない巨大なテーブルは、`static` にすると再生成を避けることができます。

#### `State::try_op`

- シグネチャ: `tuple<ScoreType, HashType, bool> try_op(Action &action, const ScoreType threshold) const`
- 現在の状態に操作を適用したと仮定して、次の状態のスコア、ハッシュ値、終了フラグを計算します。
- `const` 関数なので `State` のメンバは変更しません。値を格納できるのは引数 `action` だけです。score として候補の評価値を、hash として次の状態を表す値を返します。どちらも差分値ではありません。
- 早期打ち切りは Section 1 に従います。次のスコアが `threshold` 以上だと確定した時点で `{INF, 0, false}` を返します。INF を返した操作は `apply_op` されないため、ロールバック情報を `action` に格納する必要はありません。格納するのは、打ち切らなかった操作の分だけです。
- 戻り値 `finished` は、この操作で「最終解として確定してよいゴール状態」に達したかを表します。終了条件がない問題では常に `false` です。`finished` となる操作は候補に追加されず、展開もされません。ライブラリは `finished` が初めて現れたターンで探索を打ち切り、そのターン内に複数ある場合はスコアが最小のものを最終解にします。それより深いターンは探索しないため、浅い `finished` が最良とは限らない問題では、終了フラグを立てる条件に注意してください。
- ホットパスです。詳細は Section 1 を参照してください。

#### `State::apply_op`

- シグネチャ: `void apply_op(const Action &action)`
- `State` をインプレースで更新し、次の状態に進めます。
- `try_op` を通過した合法な `Action` に対してのみ呼ばれるため、合法性を再度確認する必要はありません。
- `action` に `nxt_*` を保存している場合はその値を使用し、保存していない場合は操作パラメータから差分を計算します。いずれの場合も変更箇所だけを更新します。

#### `State::rollback`

- シグネチャ: `void rollback(const Action &action)`
- `action` の `pre_*` を使用して、`apply_op` 前の状態に完全に戻します。
- `apply_op` と完全に対称な逆操作にします。
- 盤面全体ではなく差分だけを巻き戻します。

#### `State::enumerate_actions`

- シグネチャ: `template<class Submit> void enumerate_actions(const int turn, const Action &last_action, Submit &&submit) const`
- 現在の状態で実行可能な合法な操作を 1 つ生成するたびに、`submit(action)` を呼びます。中間の `vector<Action>` を作成しないため、バッファの確保とコピーは発生しません。
- `submit(a)` を呼ぶだけで、`try_op`、`INF`/`finished` の判定、候補への push までが内部で実行されます。`try_op` を自分で呼ぶ必要はありません。
- 1 個の `a` を再利用してかまいません。push に必要な分は submit 側がコピーし、`try_op` が `a` に格納するロールバック情報も submit 側が扱います。
- `submit.threshold()` は現在のビーム内の最悪スコアを返します。push のたびに小さくなる最新の値であるため、列挙ループ内の枝刈りに使用できます。ビームの候補数が `beam_width` 個に達するまでは `INF` を返します。枝刈りを `try_op` 側の早期打ち切りに任せてもかまいません。
- ターン 0 の呼び出しでは `last_action` はダミー値です。`turn == 0` のときは参照しないでください。
- 明らかにスコアが悪化する操作や、直前の操作を打ち消すだけの逆操作は submit しません。
- 有望な操作を先に submit すると `submit.threshold()` が早く下がり、後続の操作で枝刈りや `try_op` の早期打ち切りが起こりやすくなります。
- あるターンで生き残ったすべてのノードが、スコアが有限の操作を 1 つも submit しない場合、探索は
  `BeamStatus::NoCandidates` で終了します。行き止まりのノードは、何も submit しなければ探索木から削除されます。
  `finished=true` はゴールへの到達を表すために使用し、行き止まりには使用しません。
- ホットパスです。詳細は Section 1 を参照してください。

```cpp
template<class Submit>
void enumerate_actions(const int turn, const Action &last_action, Submit &&submit) const {
    Action a;
    for (char c : "UDLR") {
        if (turn != 0 && is_reverse(c, last_action)) continue; // 逆操作は submit しない
        if (!in_bounds(c)) continue;
        a.d = c;
        submit(a);            // try_op + 判定 + push を内部で実行
    }
}
```

#### `State::print`

- シグネチャ: `void print() const`
- デバッグ用に、現在の状態を標準エラー出力に表示します。人間が確認できる内容にします。`print` のためだけのメンバを `State` に追加しないでください。

#### `State::get_state_info`

- `record_history=true` のときに JSON ログを出力するために使用します。不要な場合は `return "{}";` だけでかまいません。

#### `search` 関数

0.5 の最小テンプレート内のラッパをそのまま使用します。変更は不要です。

---

### 3. 可変深さ版 `beam_search_turn`

操作で進む論理ターン数が操作ごとに異なる問題で使用します。各 `Action` は到達する論理ターン番号 `target_turn` を保持し、その値ごとに別のプールへ振り分けられます。同じ呼び出しの中で `target_turn` が異なる操作を混ぜて submit できます。深さが固定の問題では `beam_search.cpp` を使用します。

#### base 版との差分

| 項目 | base (`beam_search.cpp`) | turn 版 (`beam_search_turn.cpp`) |
|---|---|---|
| include | `beam_search.cpp` | `beam_search_turn.cpp` |
| `Action` の必須メンバ | なし | `int target_turn;` (番兵は `-1`) |
| `try_op` の第 2 引数 | `ScoreType threshold` | `const vector<ScoreType>& thresholds` |
| `enumerate_actions` の第 1 引数 | `const int turn` | なし。`last_action` で代用する |
| `submit.threshold` の呼び方 | `submit.threshold()` | `submit.threshold(int target_turn)` |
| `BeamParam::max_turn` | 木の深さの上限 | `target_turn` の最大値 |
| ビームの重複排除の単位 | ターンごと | `target_turn` ごと |
| `is_adjusting=true` の実効幅 | `beam_width` を超え得る | `beam_width` が上限 |
| `hash_window_turns` | 有効 | 使用されない |
| 候補が空になったとき | `NoCandidates`。経路は空 | `NoCandidates`。確定済みの接頭辞を返す場合がある |

上表の項目以外は、次の項目を含めて base 版と完全に同一です。

- クラス `BeamSearchWithTree`
- 関数 `init`, `apply_op`, `rollback`, `print`, `get_state_info`
- 上表以外の `BeamParam` のフィールド
- 戻り値の意味
- Section 1 の高速化の指針
- Section 2 の対称性、score と hash、finished の扱い

#### `Action::target_turn` の決定

- どのプールに候補を入れるかを表すキーです。重複排除も同じプール内で行われます。
- 通常は `try_op` 内で操作パラメータから計算し、`action.target_turn` に設定します。`enumerate_actions` で先に決定してもかまいません。
- submit 時点の論理ターンである親の `target_turn` より真に大きく、`max_turn` 以下の値にします。親以下の値を設定した候補は展開されないまま破棄され、不正に短い解が返る原因になります。`-1` は根の番兵専用です。
- `target_turn > max_turn` の候補は、submit してもエラーにならずに破棄されます。ただし、`try_op` 内で `thresholds[target_turn]` を参照する前に `target_turn < (int)thresholds.size()` を確認してください。`max_turn` は到達し得る上限以上に設定します。プールは実際に使用する `target_turn` の分しか確保されないため、大きめに設定してもメモリはほとんど増えません。

#### `try_op` の差分

- `thresholds[action.target_turn]` はそのプールの現在の最悪スコアです。
- 早期打ち切りでは `target_turn` を確定してから `thresholds[t]` と比較します。順序が逆だと不要な差分計算が発生します。
- reject が確定したときに `{INF, 0, false}` を返し、ロールバック情報を格納しない点は base と同じです。

#### `enumerate_actions` の差分

- `turn` 引数はありません。現在の論理ターンが必要な場合は、`State` のメンバとして自分で保持し、`apply_op` と `rollback` で更新します。
- 根での呼び出しかどうかは、`last_action.target_turn == -1` で判定するのが慣例です。これは base 版の `turn == 0` に相当します。`DUMMY_ACTION` には -1 が格納されています。
- 1 回の呼び出しで `target_turn` が異なる操作を混ぜて submit できます。submit した操作はそれぞれ対応するプールに振り分けられます。
- `submit.threshold(t)` で `target_turn==t` のプールの最悪スコアを取得できます。`target_turn` が submit 前に確定している場合は、この値で先に枝刈りできます。

#### 最小スケルトンの差分

0.5 の最小テンプレートに対する変更箇所だけを示します。

```cpp
#include "titan_cpplib/ahc/beam_search/beam_search_turn.cpp"  // ← turn 版

struct Action {
    int target_turn;                              // 必須メンバ
    // 操作パラメータ + pre_/nxt_* (Section 2)
    Action() : target_turn(-1) {}                 // -1 = root sentinel
    friend ostream& operator<<(ostream& os, const Action&){ return os; }
    string to_string() const { return ""; }
};

class State {
    /* init / apply_op / rollback / print / get_state_info は base と同じ */

    tuple<ScoreType, HashType, bool>
    try_op(Action &action, const vector<ScoreType>& thresholds) const {
        // 1. action.target_turn を計算して設定する
        // 2. action.target_turn が thresholds の範囲外なら INF で reject
        // 3. thresholds[action.target_turn] と比較して早期 reject
        // 4. reject しなかった操作だけ、ロールバック情報を action に格納する
    }

    template<class Submit>
    void enumerate_actions(const Action &last_action, Submit &&submit) const {
        // last_action.target_turn == -1 なら根での呼び出し
        // 必要なら submit.threshold(t) で先に枝刈りしてから submit(a)
    }
};
```

#### 注意点

- `target_turn` のステップ幅は一定である必要はなく、同じ呼び出しの中で遠い target と近い target が混在してもかまいません。ただし、各候補の `target_turn` は親の `target_turn` より真に大きくします。
- `clear_hash_every_turn=false` のときは、すべての `target_turn` をまたいで hash による重複排除を行います。
  - 新しい候補が重複排除を通過するのは、次のいずれかの場合だけです。
    - hash が未登録の場合
    - 登録済みの候補より `target_turn` が小さい場合
    - 同じ `target_turn` で、登録済みの候補よりスコアが低い場合
  - 既に大きい `target_turn` のプールにある同じ hash の entry は除去されません。
  - hash 自体は base と同じく状態を表すだけでよく、`target_turn` を含める必要はありません。
- `max_turn` が小さすぎると、それを超える操作が破棄され、探索の幅が狭くなります。最初は大きめに設定し、ログで `target_turn` の分布を確認してから小さくするのが安全です。
- `result.actions` は base と同様に、根から最良の葉までの `Action` の列です。最良の葉には `finished` のものが優先され、そのうえでスコアが最小のものが選ばれます。
  列の長さは、finished に到達した場合はその直前の target_turn までの長さになります。探索木の中の最小の `target_turn` が
  `max_turn` に達するまで進めた場合は、その時点の最良パスの長さです。
