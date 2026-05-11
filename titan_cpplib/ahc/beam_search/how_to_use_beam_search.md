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

情報の流れは `Action` を介して以下のように繋がります。

```
get_actions : 操作パラメータを action に書き込む
    ↓
try_op      : const でスコア・ハッシュを計算し、ロールバック情報を action に書き込む
    ↓
apply_op    : action を参照して State を差分更新する
    ↓
rollback    : action に保存された pre_* 値を使って State を差分巻き戻す
```

重要な前提

- `State` オブジェクトは `search()` 経由では 1 つだけ生成され、ディープコピーは発生しません。`naive_search()` を使うときに限り State コピーが発生します。
- スコアは最小化で扱われます。最大化問題の場合はスコアを負にしてください。`get_score` 内で二重否定にならないよう注意してください。
- 関数のシグネチャは絶対に変更しないでください。

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

---

#### `State::init`

- シグネチャ: `void init()`
- 目的: 探索開始時の初期状態を構築します。`search()` から 1 回だけ呼ばれます。
- 要件:
  - 盤面・スコア・ハッシュ値を初期化してください。
  - 標準入力からの問題データ読み込みはここで行ってください。
  - **Zobrist Hash 用の乱数テーブルは `static` 変数として保持し**、`init` 冒頭で初期化フラグを使って一度だけ初期化してください。`naive_search` 利用時に State コピーが発生してもテーブルがコピー対象にならないようにするためです。

---

#### `State::try_op`

- シグネチャ: `tuple<ScoreType, HashType, bool> try_op(Action &action, const ScoreType threshold) const`
- 目的: 現在の状態に操作を適用した場合の次状態スコア・ハッシュ値・終了フラグを計算します。
- 要件:
  - `const` 関数であるため `State` のメンバは変更しないでください。
  - スコアとハッシュ値は差分計算で求めてください。
  - 差分計算の途中で `nxt_score >= threshold` が確定したら即座に `{INF, 0, false}` を返してください。`Action` へのロールバック情報の書き込みもスキップしてかまいません。
  - 計算が最後まで完了した場合のみ、ロールバックに必要な情報を `action` に記録してください。
  - 3 つ目の戻り値 `finished` には、この操作によって探索が終了状態（最終手）に達したかどうかを返してください。終了条件のない問題では常に `false` を返して構いません。
  - `finished == true` の `Action` はそれ以上展開されません。複数の終了候補があった場合、最良スコアのものが最終解として採用されます。
- ⚠️ ホットパスです。詳細は Section 1 を参照。

---

#### `State::apply_op`

- シグネチャ: `void apply_op(const Action &action)`
- 目的: `State` をインプレース更新して次の状態へ進めます。
- 要件:
  - `try_op` を通過した合法な `Action` に対してのみ呼ばれるため、再バリデーションは不要です。
  - `action` に `nxt_*` を保存しているならそれを使い、保存していないなら操作パラメータから差分計算してください。いずれにせよ変更箇所のみ更新してください。

---

#### `State::rollback`

- シグネチャ: `void rollback(const Action &action)`
- 目的: `apply_op` 実行前の状態に完全に復元します。
- 要件:
  - `action` に保存された `pre_*` 情報を用いて `apply_op` 前の状態に完全に戻してください。
  - `apply_op` と完全に逆の操作を行うこと。対称性を保ってください。
  - ここでも盤面全体ではなく差分のみを巻き戻してください。

---

#### `State::get_actions`

- シグネチャ: `void get_actions(vector<Action> &actions, const int turn, const Action &last_action, const ScoreType threshold) const`
- 目的: 現在の状態で実行可能な合法手を列挙します。
- 要件:
  - `actions` はライブラリ側で `clear()` 済みの状態で渡されます。追加のみ行ってください。capacity は使い回されるので毎ターン `reserve` する必要はありません。
  - ⚠️ `last_action` はターン 0 の呼び出し時にダミー値が入ります。`turn == 0` のときに `last_action` のメンバを参照すると不正アクセスになるため、参照しないでください。
  - 明らかにスコアが悪化する手・直前操作の打ち消しに相当する逆操作は生成しないでください。
  - `threshold` を使った枝刈りをここで行うと、`try_op` の呼び出し回数そのものを削減できます。
- ⚠️ ホットパスです。詳細は Section 1 を参照。

---

#### `State::print`

- シグネチャ: `void print() const`
- 目的: デバッグ用に現在の状態を標準エラー出力に表示します。計算量は多少大きくても構わず、人間可読を優先してください。`print` のためだけのメンバを `State` に追加しないこと。

---

#### `State::get_state_info`

- `string get_state_info() const` は `record_history=true` 時の JSON ログ出力用の互換関数です。`return "{}";` のままにしてください。

---

#### `search` / `naive_search` 関数

以下のコードをそのまま書いてください。変更不要です。

```cpp
vector<Action> search(flying_squirrel::BeamParam &param, const bool verbose=false, const string& history_file = "") {
    flying_squirrel::BeamSearchWithTree<ScoreType, HashType, Action, State, INF> bs;
    return bs.search(param, verbose, history_file);
}

vector<Action> naive_search(flying_squirrel::BeamParam &param, const bool verbose=false, const string& history_file = "") {
    flying_squirrel::NaiveBeamSearch<ScoreType, HashType, Action, State, INF> bs;
    return bs.search(param, verbose, history_file);
}
```
