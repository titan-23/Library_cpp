## 焼きなまし実装ガイド

### 0. ライブラリ概要

提供される関数は以下の2つです。

| 関数 | 概要 |
|------|------|
| `sa_run(TIME_LIMIT, seed, verbose)` | 通常の焼きなまし。単一 State。 |
| `replica_run(TIME_LIMIT, NUM_REPLICAS, ...)` | レプリカ交換法。OpenMP 並列で複数温度の State を同時に動かします。 |

スコアは 最小化 で扱われます。最大化問題の場合はスコアを負にしてください。

ライフサイクル — `sa_run`

```
state.init(seed)                          // sarnd のシード設定 + 初期化
while (時間内):
    reset_is_valid()                      // is_valid = true にリセット
    modify(iter, threshold, progress)     // 近傍操作・差分スコア計算
    new_score = get_score()
    if is_valid && new_score <= threshold:
        advance()                         // 遷移を確定。遅延評価の場合はここで盤面更新
        score = new_score
    else:
        rollback()
        state.score = score               // ライブラリがスコアを上書き復元
```

ライフサイクル — `replica_run`

```
// 各レプリカを初期化
for i in 0..NUM_REPLICAS:
    states[i].init(1000 + i)              // ライブラリ側でレプリカごとに異なる seed を渡す

// OpenMP で並列に SWAP_ITER_INTERVAL 回更新 → レプリカ間でスワップ
while (時間内):
    cur_score = get_score()
    threshold = cur_score - temp * log(rnd)
    reset_is_valid()
    modify(iter, threshold, progress)
    new_score = get_score()
    if is_valid && new_score <= threshold:
        advance()
    else:
        rollback()
        states[r].score = cur_score      // ライブラリがスコアを上書き復元
```

重要な前提

- 関数のシグネチャは 絶対に変更しないでください。
- `State` および `Changed` のデフォルトコンストラクタは削除しないでください。
- 探索ループ内で `State` 自体のコピーは発生しません。単一インスタンスを差分更新で使い回します。
- グローバル変数・関数内 `static` 変数の新規定義は禁止します。必要な状態はすべて `State` または `Changed` のメンバとして持たせてください。`replica_run` は OpenMP でスレッド並列動作します。各スレッドは異なる `State` インスタンスを担当するため `State` メンバへの競合はありませんが、グローバル変数や `static` 変数は複数スレッドから同時アクセスされデータ競合が発生します。

### 1. 実装上の制約と高速化の指針

状態の差分更新
- 盤面全体の再計算や配列全体のコピーは避け、変更箇所のみを更新してください。

ホットパスでのメモリアロケーション回避
- `modify` および `rollback` は毎ループ呼ばれるホットパスです。これらの関数内で `vector` / `set` / `map` などの動的メモリを伴うコンテナを毎回宣言・破棄しないでください。ヒープアロケーションが繰り返し発生して大幅に遅くなります。
- 作業用バッファが必要な場合は `Changed` または `State` のメンバとして事前確保し、毎ループ上書きして使い回してください。`Changed` はこの用途のために用意された作業領域です。
- 既存メンバを使い回す場合、毎ループの先頭で必要な部分を必ず上書きしてから参照してください。

`changed.type` の代入
- `changed.type` は毎 `modify` で必ず `[0, TYPE_CNT)` の値を代入してください。ライブラリ側で統計配列の添字に使われるため、未代入だと未定義動作になります。

スコア計算の早期打ち切り
- `modify` に渡される `threshold` は採択条件 `new_score <= threshold` の上限値です。等号成立時は採択されます。
- 差分計算の過程で `new_score > threshold` が確定したら `is_valid = false` にして即リターンしてください。
- 盤面を書き換える前に遅延評価で判定する実装を推奨します。この場合 `advance()` で書き込みを確定させ、棄却時の `rollback()` は盤面を変えていないため自明になります。
- `is_valid = false` で抜ける場合、ライブラリは `score` の値を参照しません。途中まで計算した値が `score` に残っていても問題ありません。

スコアの最小化と二重反転の禁止
- 最大化問題の場合は計算したスコアにマイナスをつけて `score` に保持してください。
- `get_score()` は `return score;` のままにしてください。内部で再度マイナスをつける二重反転は行わないでください。
- `get_true_score()` には本来の生のスコアを返してください。最大化問題なら `-score` を返すことになります。

### 2. 各構造体および関数の仕様

#### `State::Param` 構造体

- 目的: 焼きなましの温度パラメータを保持します。`inline static` メンバです。
- 要件: 対象問題のスコアスケールに合わせて `start_temp` および `end_temp` の初期値を設定してください。

#### `State::Changed` 構造体

- 目的: 1手分の遷移の差分情報とロールバックに必要な情報を保持する作業領域です。
- 要件:
  - `TYPE_CNT` に実装する近傍操作の種類の総数を設定してください。統計ログで参照されます。
  - `type` に実行した操作の種類を 0 始まりのインデックスで代入してください。
  - ロールバックに必要な情報をメンバとして追加してください。例えば変更前の値や変更箇所のインデックスなどです。
  - 動的サイズの作業バッファが必要なら、ここに `vector` 等のメンバを置いて使い回してください。`modify` 内で毎回新規確保するのは避けてください。

#### `State::Result` 構造体

- 目的: 探索中に発見した最良解を保持し、最終的な解答を出力します。
- 要件:
  - 最良解更新のたびにコピーが発生します。盤面全体のディープコピーは避け、解答出力に必要な最小限の情報のみを持たせてください。
  - 内部スコアの `score` と生のスコアの `true_score` をメンバとして持たせてください。
  - `void print(ostream &os = cout) const` に解答の出力処理を実装してください。

#### `State::init`

- シグネチャ: `void init(uint32_t seed)`
- 目的: 探索開始時の初期状態を構築します。
- 要件:
  - 冒頭で `sarnd.set_seed(seed)` を呼んで乱数シードを設定してください。`sa_run` はユーザ指定の seed を、`replica_run` はレプリカごとに異なる seed を渡します。
  - 盤面の初期化と初期スコアの計算を行ってください。

#### `State::modify`

- シグネチャ: `void modify(const int64_t iter, const ScoreType threshold, const double progress)`
- 目的: 近傍操作を選択し、スコアを差分更新します。
- 要件:
  - `sarnd` を用いて近傍操作を選択してください。
  - 操作の種類を `changed.type` に代入し、ロールバックに必要な情報を `changed` に保存してください。
  - 次状態のスコアを差分計算し `score` に代入してください。直後にライブラリが `get_score()` を呼びます。
  - ルール違反・早期打ち切りの場合は `is_valid = false` にして即リターンしてください。`is_valid = false` でも `rollback()` は必ず呼ばれます。`modify` 内で盤面を部分的に変えた場合は、その変更も `rollback` で戻す必要があります。
  - `progress` は焼きなましの進行度で、0.0 〜 1.0 の値を取ります。近傍の切り替えに使えます。

#### `State::rollback`

- シグネチャ: `void rollback()`
- 目的: 遷移が棄却された場合に状態を元に戻します。
- 要件:
  - `changed` に保存した情報を用いて、`modify` 実行前の盤面に完全に復元してください。
  - `score` の復元は不要です。`sa_run` / `replica_run` ともにライブラリ側でスコアを上書き復元します。

#### `State::advance`

- シグネチャ: `void advance()`
- 目的: 遷移が採択された場合に状態を確定させます。
- 要件:
  - `modify` で遅延評価を採用した場合は、ここで盤面への書き込みを確定させてください。
  - `modify` ですでに盤面を更新済みの場合は空で構いません。

#### `State::get_score` / `State::get_true_score`

- シグネチャ: `ScoreType get_score() const` / `ScoreType get_true_score() const`
- 要件:
  - `get_score()` は `return score;` のままにしてください。
  - `get_true_score()` には生のスコアを返してください。最大化問題なら `-score` を返すことになります。verbose ログで使用されます。

#### `State::get_result`

- シグネチャ: `Result get_result() const`
- 目的: 現在の状態を `Result` 構造体として返します。
- 要件: 解答の出力に必要な最小限の情報を抽出して `Result` を生成してください。

#### `State::print`

- シグネチャ: `void print() const`
- 要件: 盤面の状態を標準エラー出力に表示してください。

### 3. 外部ライブラリ

#### 乱数 `sarnd`

`sarnd` は `State` のパブリックメンバとして宣言された `titan23::Random` 型のオブジェクトです。高速な XorShift 乱数生成器です。

| メソッド | 返り値 |
|----------|--------|
| `sarnd.random()` | `[0.0, 1.0]` の実数 |
| `sarnd.randint(end)` | `[0, end]` の整数（両端含む） |
| `sarnd.randint(begin, end)` | `[begin, end]` の整数（両端含む） |
| `sarnd.randrange(end)` | `[0, end)` の整数（end を含まない） |
| `sarnd.randrange(begin, end)` | `[begin, end)` の整数（end を含まない） |
| `sarnd.rand_u64()` | `[0, 2^64)` の整数 |
| `sarnd.shuffle(vec)` | vector をインプレースにシャッフル |
