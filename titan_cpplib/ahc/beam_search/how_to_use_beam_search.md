## ビームサーチ実装ガイド

### 0. ライブラリ概要

`flying_squirrel::BeamSearchWithTree` は 木上の差分更新型ビームサーチです。

ライフサイクルは DFS オイラーツアー型で、各ターンで以下のように木を辿ります。

```
state->init()                                  // 初回1回だけ

for each turn:
    for each node in beam tree (DFS順):
        PRE_ORDER  : apply_op(action)          // 子へ進む
        LEAF       : get_actions(...)          // 合法手列挙
                     for each action:
                         try_op(action, ...)   // 次状態のスコア・ハッシュ計算
        POST_ORDER : rollback(action)          // 親へ戻る
```

`apply_op` と `rollback` は完全に対称でなければなりません。すなわち、同一の `action` に対して `apply_op → rollback` を呼ぶと状態が完全に元へ復元される必要があります。

情報は `Action` を介して流れます: `get_actions`(操作を書込) → `try_op`(const でスコア/ハッシュ計算・pre_* 等を書込) → `apply_op`(参照して差分更新) → `rollback`(pre_* で差分巻戻し)。各関数の詳細は Section 2。

重要な前提

- `State` オブジェクトは `search()` 経由では 1 つだけ生成され、ディープコピーは発生しません。`naive_search()` を使うときに限り State コピーが発生します。
- スコアは最小化で扱われます。最大化問題の場合はスコアを負にしてください。`get_score` 内で二重否定にならないよう注意してください。
- 関数のシグネチャは絶対に変更しないでください。

---

### 0.5 セットアップ(最小テンプレート / ここだけで初回統合が完結する)

#### 使うファイル(include 索引)

| include するファイル | 役割 |
|---|---|
| `titan_cpplib/ahc/beam_search/beam_fast.cpp` | **通常これ**。木・差分・インプレース更新版(`State` は 1 個、コピー無し)。`search()` を提供 |
| `titan_cpplib/ahc/beam_search/beam_search.cpp` | 同系の別実装。必要時のみ |
| `beam_param.cpp` | `BeamParam`。上記から自動 include されるので個別 include 不要 |

`search()` が木・高速版、`naive_search()` が `State` ディープコピー版(挙動検証・デバッグ用)。本番は `search()`。

#### 自分で定義すべき記号(これらが無いとコンパイル不可)

ライブラリは供給しません。`State` を置く名前空間内で必ず宣言:

| 記号 | 例 | 意味 |
|---|---|---|
| `ScoreType` | `using ScoreType = int;`(または `long long`) | スコア型。**最小化**。最大化問題は符号反転 |
| `HashType` | `using HashType = unsigned long long;` | Zobrist ハッシュ型 |
| `INF` | `const ScoreType INF = 1e9;` | `try_op` 打ち切り時の返り値。`ScoreType` に収まる十分大きな値 |
| `brnd` | `titan23::Random brnd;` | 乱数器。`brnd.rand_u64()` で Zobrist 乱数。`random.cpp` を include |

#### 最小テンプレート(これをコピーして State 中身だけ書く)

```cpp
#include <bits/stdc++.h>
#include "titan_cpplib/others/print.cpp"
#include "titan_cpplib/ahc/state_pool.cpp"
#include "titan_cpplib/ahc/timer.cpp"
#include "titan_cpplib/algorithm/random.cpp"
#include "titan_cpplib/ahc/beam_search/beam_fast.cpp"   // search() 本体
using namespace std;

namespace beam_search {
    using ScoreType = int;                 // 最小化
    using HashType  = unsigned long long;
    const ScoreType INF = 1e9;
    titan23::Random brnd;                  // brnd.rand_u64()

    struct Action { /* 操作パラメータ + pre_*/nxt_* (Section 2) */
        friend ostream& operator<<(ostream& os, const Action&){ return os; }
        string to_string() const { return ""; }
    };
    class State { /* init/try_op/apply_op/rollback/get_actions/print/get_state_info */ };

    vector<Action> search(flying_squirrel::BeamParam &param, const bool verbose=false, const string& history_file = "") {
        flying_squirrel::BeamSearchWithTree<ScoreType, HashType, Action, State, INF> bs;
        return bs.search(param, verbose, history_file);
    }
    vector<Action> naive_search(flying_squirrel::BeamParam &param, const bool verbose=false, const string& history_file = "") {
        flying_squirrel::NaiveBeamSearch<ScoreType, HashType, Action, State, INF> bs;
        return bs.search(param, verbose, history_file);
    }
}

int main() {
    flying_squirrel::BeamParam param(/*max_turn=*/2000, /*beam_width=*/100,
                                     /*time_limit_ms=*/-1, /*is_adjusting=*/false);
    auto res = beam_search::search(param);
    for (auto &a : res) { /* a を根→葉の順に適用して解を再生・出力 */ }
}
```

#### `BeamParam` 仕様

コンストラクタ:
`flying_squirrel::BeamParam param(int max_turn, int beam_width, double time_limit_ms, bool is_adjusting=false, bool clear_hash_every_turn=true);`

| 引数 | 型/単位 | 既定 | 意味 |
|---|---|---|---|
| `max_turn` | int(ターン=木の深さ) | — | 探索木の最大深さ。これを超えると打ち切り。1手=1ターンなら最大手数 |
| `beam_width` | int | — | ビーム幅。`is_adjusting=true` のときは**上限**として機能 |
| `time_limit_ms` | double, ms | — | 制限時間(`Timer::elapsed()` と同単位、1秒=1000) |
| `is_adjusting` | bool | `false` | `true` で残り時間から実効幅を動的調整(`beam_width` を上限に増減)。通常 `false` 推奨 |
| `clear_hash_every_turn` | bool | `true` | 毎ターン重複排除 hash を clear(安全既定)。`false` はターン跨ぎ最適化用で、`State` の hash がターン情報を含まないと候補が黙って drop。理解した上でのみ |

#### 戻り値の意味

`search()` は **根 → 最良葉(`finished` 優先・最小スコア)までの `Action` 列**を `apply_op` すべき順序で返す。
解の再生は `for (auto &a : res) apply 相当の処理` で先頭から順に適用するだけ。途中状態の再構築は不要。

---

### 1. 実装上の制約と高速化の指針

Action 構造体のサイズ最小化
- `Action` は毎ターン多数コピー・生成されます。ロールバックに必要な情報のみを持ち、変数の型は必要十分な最小サイズを選んでください。例えば `int` のかわりに `short` や `int8_t` で済むなら積極的に縮めてください。

`score` / `hash` を `Action` に持たせるかの判断
- **持たせる**: `try_op` で計算した値を `action.nxt_*` に保存し、`apply_op` では `score = action.nxt_score` するだけ。実装が単純だが `sizeof(Action)` が増える。
- **持たせない**: `apply_op` 内で操作パラメータから再計算する。`Action` は小さくなるが、`try_op` と `apply_op` で計算が二度走る。
- **判断基準**: 差分計算コストが極めて軽く数命令で済むなら「持たせない」が有利。それ以外は「持たせる」が無難。

スコア計算の早期打ち切り — 最重要
- `try_op` に渡される `threshold` は現在のビーム内の最悪スコアです。
- 差分計算の途中で次のスコアが `threshold` 以上になることが確定した時点で、即座に `{INF, 0, false}` を返してください。
- この場合その `Action` が `apply_op` されることはないため、`Action` へのロールバック情報の書き込みもスキップしてください。
- スコアを大きく変化させる要因から先に計算し、`threshold` 超を早めに検出してください。

`get_actions` 側でスコアまで計算する選択肢
- `try_op` の差分計算が `get_actions` の合法手生成中に参照する情報をそのまま使えるなら、`get_actions` 内でスコアまで計算して `Action` に保存してしまう方が速いことがあります。`try_op` は保存値を返すだけになり、データ構造への二重アクセスを避けられます。
- この場合でも `threshold` を使った枝刈りは `get_actions` の中で行ってください。

ホットパス `try_op` / `get_actions` でのコピー回避
- ビームサーチ中に最も多く呼ばれる箇所です。
- `State` メンバの一時コピーは禁止です。例えば `auto tmp_board = board;` のような盤面のコピーが該当します。
- スコア・ハッシュの計算はスカラーのローカル変数だけで完結させてください。
- 内部で `vector` を新たに生成・コピーすることも避けてください。ヒープアロケーション全般を禁止します。

Zobrist Hash の設計
- `XOR` 演算による差分更新を実装してください。
- ライブラリは同一ハッシュ値の候補を **「同一状態」とみなし、スコアの良い方のみ残します**。これにより重複した部分木の展開が防がれ、ビーム幅を実効的に増やせます。
- ハッシュ化する対象は「その後の状態遷移やスコア計算に影響を与える要素」のみに限定してください。無関係な情報をハッシュに含めると本来同一視すべき状態が別物として扱われ、ビーム多様性が損なわれます。
- 乱数テーブルは `uint64_t` の xorshift 系乱数で事前に初期化してください。`brnd.rand_u64()` が利用できます。

状態の差分更新
- `State` 内の盤面配列などのディープコピーは禁止します。変更があった箇所のみを計算・更新してください。

`get_actions` での枝刈り
- 明らかにスコアが悪化する手や、直前の操作を単純に打ち消すだけの逆操作は生成しないでください。
- `threshold` を使った枝刈りをここで行うと、`try_op` の呼び出し回数そのものを削減できます。

---

### 2. 各関数の仕様と実装要件

#### Action 構造体

- 目的: 状態遷移を表す「操作パラメータ」と、`rollback` に必要な「巻き戻し情報」を一体で保持します。
- スケルトン内のメンバ変数は一例です。問題に応じて自由に変更・追加してください。
- `try_op` で上書きされる前の値 `pre_*` と、`apply_op` 後の値 `nxt_*` を持つのが基本パターンです。詳細は Section 1「持たせるか判断」を参照。
- `string to_string() const` は `record_history=true` 時のデバッグ出力用です。中身は空で構いませんが、削除はしないでください。

#### `State::init`

- シグネチャ: `void init()`
- 目的: 探索開始時の初期状態を構築します。`search()` から 1 回だけ呼ばれます。
- 要件:
  - 盤面・スコア・ハッシュ値を初期化してください。
  - 標準入力からの問題データ読み込みはここで行ってください。
  - **Zobrist Hash 用の乱数テーブルは `static` 変数として保持し**、`init` 冒頭で初期化フラグを使って一度だけ初期化してください。`naive_search` 利用時に State コピーが発生してもテーブルがコピー対象にならないようにするためです。

#### `State::try_op`

- シグネチャ: `tuple<ScoreType, HashType, bool> try_op(Action &action, const ScoreType threshold) const`
- 目的: 現在の状態に操作を適用した場合の次状態スコア・ハッシュ値・終了フラグを計算します。
- 要件:
  - `const` 関数であるため `State` のメンバは変更しないでください。スコア・ハッシュは差分計算。
  - 早期打ち切り(`nxt_score >= threshold` で `{INF,0,false}` 返し・ロールバック情報スキップ、完了時のみ `action` に記録)は Section 1「スコア計算の早期打ち切り」に従う。
  - 戻り値 `finished`: この操作で終了状態（最終手）に達したか。終了条件が無ければ常に `false`。`finished == true` は非展開、複数あれば最良スコアを最終解に採用。
- ⚠️ ホットパスです。詳細は Section 1 を参照。

#### `State::apply_op`

- シグネチャ: `void apply_op(const Action &action)`
- 目的: `State` をインプレース更新して次の状態へ進めます。
- 要件:
  - `try_op` を通過した合法な `Action` に対してのみ呼ばれるため、再バリデーションは不要です。
  - `action` に `nxt_*` を保存しているならそれを使い、保存していないなら操作パラメータから差分計算してください。いずれにせよ変更箇所のみ更新してください。

#### `State::rollback`

- シグネチャ: `void rollback(const Action &action)`
- 目的: `apply_op` 実行前の状態に完全に復元します。
- 要件:
  - `action` に保存された `pre_*` 情報を用いて `apply_op` 前の状態に完全に戻してください。
  - `apply_op` と完全に逆の操作を行うこと。対称性を保ってください。
  - ここでも盤面全体ではなく差分のみを巻き戻してください。

#### `State::get_actions`

- シグネチャ: `void get_actions(vector<Action> &actions, const int turn, const Action &last_action, const ScoreType threshold) const`
- 目的: 現在の状態で実行可能な合法手を列挙します。
- 要件:
  - `actions` はライブラリ側で `clear()` 済みの状態で渡されます。追加のみ行ってください。capacity は使い回されるので毎ターン `reserve` する必要はありません。
  - ⚠️ `last_action` はターン 0 の呼び出し時にダミー値が入ります。`turn == 0` のときに `last_action` のメンバを参照すると不正アクセスになるため、参照しないでください。
  - 明らかにスコアが悪化する手・直前操作の打ち消しに相当する逆操作は生成しないでください。
  - `threshold` を使った枝刈りをここで行うと、`try_op` の呼び出し回数そのものを削減できます。
- ⚠️ ホットパスです。詳細は Section 1 を参照。

#### `State::print`

- シグネチャ: `void print() const`
- 目的: デバッグ用に現在の状態を標準エラー出力に表示します。計算量は多少大きくても構わず、人間可読を優先してください。`print` のためだけのメンバを `State` に追加しないこと。

#### `State::get_state_info`

- `string get_state_info() const` は `record_history=true` 時の JSON ログ出力用の互換関数です。`return "{}";` のままにしてください。

#### `search` / `naive_search` 関数

0.5 の最小テンプレート内のラッパをそのまま使う(変更不要)。`search()`=本番、`naive_search()`=State コピー版(検証用)。
