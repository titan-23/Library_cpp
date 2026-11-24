## 焼きなましの実装ガイド

### 0. ライブラリの概要

提供する関数は提出用の `sa_run` とローカル専用の `replica_run` です。シグネチャと用途は Section 0.5 を参照してください。
ライブラリはスコアを最小化します。最大化問題の場合はスコアの符号を反転してください。

`sa_run` の処理手順

```
state.init(seed)                          // sarnd のシードを設定し、状態を初期化する
while (時間内):
    reset_is_valid()                      // is_valid = true にリセット
    modify(iter, threshold, progress)     // 近傍操作とスコアの差分計算
    new_score = get_score()
    if is_valid && new_score <= threshold:
        advance()                         // 遷移を確定する / 遅延評価の場合はここで盤面を更新する
        score = new_score
    else:
        rollback()
        state.score = score               // ライブラリがスコアを元の値に戻す
```

`replica_run` が State のメソッドを呼び出す順序は `sa_run` と同じです。threshold の計算、レプリカ間のスワップ、各レプリカへの seed の割り当てはすべてライブラリの内部で行うため、ユーザが実装する必要はありません。

重要な前提

- 関数のシグネチャは絶対に変更しないでください。
- seed 版と `replica_run` を使用する場合は、`State` および `Changed` のデフォルトコンストラクタが必要です。初期化済み State 版だけを使用する場合は、`State` のデフォルトコンストラクタは不要です。
- 探索ループ内で `State` 自体のコピーは発生しません。単一のインスタンスを差分更新しながら探索します。
- グローバル変数と関数内の `static` 変数の新規定義を禁止します。必要な状態はすべて `State` または `Changed` のメンバとして保持してください。`replica_run` は OpenMP によりスレッド並列で動作します。各スレッドは異なる `State` インスタンスを担当するため、`State` のメンバでは競合が発生しません。一方、グローバル変数や `static` 変数には複数のスレッドが同時にアクセスするため、データ競合が発生します。

### 0.5 セットアップ

初めてライブラリを組み込む場合は、この節の内容だけで作業が完了します。

#### include するファイル

| ファイル | 役割 |
|---|---|
| `titan_cpplib/ahc/sa/sa.cpp` | `sa_run`, `replica_run` を提供するエンジン本体。`<omp.h>`, `timer.cpp`, `random.cpp` を include している |
| `titan_cpplib/ahc/sa/sa_state.cpp` | **コピーして使用するテンプレート** (`namespace sa { class State {...} }`)。冒頭で `sa.cpp` を include している |

ユーザが行う作業は、`sa_state.cpp` 相当の内容を自分のファイルに用意し、`State` の `TODO` の箇所を埋めることだけです。エンジンを個別に include する必要はありません。

#### 各記号の提供元

| 記号 | 提供元 | 備考 |
|---|---|---|
| `ScoreType` | **ユーザ** (`State` 内の `using ScoreType = double;`) | 既定は double。整数スコアなら `long long` などに変更する。スコアは**最小化**される |
| `sarnd` | seed 版のテンプレート (`State` のパブリックメンバ `titan23::Random`) | seed 版では `init` の中で `sarnd.set_seed(seed)` を呼び出す必要がある。初期化済み State 版では任意の乱数生成器を使用できる |
| `is_valid` / `score` / `reset_is_valid` / `get_score` / `get_true_score` / `get_result` | テンプレート | シグネチャの変更は禁止。中身は Section 2 の仕様に従う |
| `Param` (`inline static`) / `Changed` / `Result` | 枠はテンプレート、**中身はユーザ** | `start_temp` と `end_temp`、`TYPE_CNT` と `type`、`Result::print` を埋める |

#### 最小構成のコード (テンプレートの要点と main)

```cpp
#include <bits/stdc++.h>
#include "titan_cpplib/ahc/timer.cpp"
#include "titan_cpplib/alg/random.cpp"
#include "titan_cpplib/ahc/sa/sa.cpp"          // エンジン (sa_run など)
using namespace std;

namespace sa {
class State {
public:
    titan23::Random sarnd;                      // テンプレートにあるメンバ / 削除しない
    using ScoreType = double;                   // スコアは最小化される / 型は問題に合わせて変更できる

    struct Param { double start_temp, end_temp; // スコアのスケールに合わせて設定する
        Param() : start_temp(1e3), end_temp(1e0) {} };
    inline static Param param;

    struct Changed { int TYPE_CNT = /*近傍操作の種類数*/1; int type; Changed() {} } changed;

    struct Result { ScoreType score, true_score;
        Result() {} Result(ScoreType s, ScoreType ts): score(s), true_score(ts) {}
        void print(ostream &os = cout) const { /* 解答の出力 */ } };

    bool is_valid; ScoreType score;
    State() {}
    void init(uint32_t seed) { sarnd.set_seed(seed); /* 盤面の初期化 */ score = 0; }
    void reset_is_valid() { is_valid = true; }
    ScoreType get_score() const { return score; }       // 二重反転は禁止
    ScoreType get_true_score() const { return score; }  // 最大化なら -score
    void modify(const int64_t iter, const ScoreType threshold, const double progress) {
        // changed.type に [0,TYPE_CNT) の値を必ず代入する / score を差分で更新する /
        // threshold を超えることが確定したら is_valid=false にして直ちに return する
    }
    void rollback() { /* changed を使用して modify の前の状態に完全に戻す / score の復元は不要 */ }
    void advance() { /* 遅延評価ならここで盤面を確定する */ }
    Result get_result() const { return {get_score(), get_true_score()}; }
};
}

int main() {
    sa::State::param.start_temp = 1e3;          // スコアのスケールに合わせる
    sa::State::param.end_temp   = 1e0;
    auto r = sa::sa_run<sa::State>(/*TIME_LIMIT_ms=*/1900.0, /*seed=*/23, /*verbose=*/true);
    r.print();
}
```

#### 実行関数のシグネチャ

関数はすべて `namespace sa` で定義されています。`TIME_LIMIT` の単位は ms、戻り値は `State::Result` です。

```cpp
template<class State> State::Result
sa_run(double TIME_LIMIT, uint32_t seed=23, bool verbose=true);

template<class State> State::Result
sa_run(double TIME_LIMIT, State& state, bool verbose=true);

template<class State> State::Result
replica_run(double TIME_LIMIT, int NUM_REPLICAS=32, int SWAP_ITER_INTERVAL=100,
            bool verbose=true, bool record=false, uint32_t seed=23); // レプリカ交換 / OpenMP で並列に実行する
```

| 関数 | 主な引数 | 用途 |
|---|---|---|
| `sa_run` | TIME_LIMIT[ms], seed, verbose | **提出にはこれを使用する**。単一の State で通常の焼きなましを行う。本番 (AtCoder) の解はこちらで実装し、チューニングする |
| `sa_run` (初期化済み State 版) | TIME_LIMIT[ms], state, verbose | 引数付きコンストラクタや外部で作成した初期状態から探索する。`TIME_LIMIT` はこの関数を呼び出してからの時間 |
| `replica_run` | + NUM_REPLICAS, SWAP_ITER_INTERVAL, record, seed | **ローカル専用のツールで、提出には使用しない**。スレッド並列で長時間実行し、良い解、良いパラメータ、構造についての知見をオフラインで得るための探索 |

seed 版は `State::init(seed)` の時間も `TIME_LIMIT` に含めます。初期化後の残り時間で探索し、探索の開始時を `progress=0` として初期温度から冷却します。初期化済み State 版で状態の構築時間も全体の時間に含める場合は、呼び出し側で構築時間を引いた残り時間を渡してください。

`replica_run` は提出するコードではなく、ローカルで良い解を探索するためのツールです。`-fopenmp` を付けてビルドし、`TIME_LIMIT` を長めに、`NUM_REPLICAS` をローカルのコア数程度にして長時間実行します。
得られた知見 (到達できるスコアの範囲、有効な近傍と初期解、パラメータ) は、提出用の `sa_run` に反映します。
`seed` から各 State 用、各温度の受理判定用、レプリカ交換用の独立した乱数列を生成します。
`record=true` での記録時と `verbose=true` での終了時には、異なる State の `get_true_score()` を並列に呼び出します。そのため、`get_true_score()` では共有している可変データを変更しないようにしてください。

#### ビルド時の注意

- 提出用の `sa_run` には OpenMP は不要です。`replica_run` は `<omp.h>` を使用してスレッド並列で動作するため、ローカルで実行するときだけ **`-fopenmp` が必要**です。提出用のビルドとは別に、ローカルでの探索用のビルドを用意すると便利です。
- `replica_run` の並列実行時は、グローバル変数と関数内の `static` 変数でデータ競合が発生します (Section 0 の「重要な前提」を参照)。状態は必ず `State` または `Changed` のメンバとして保持してください。
- 解は `get_result()` が返す `Result` の `print()` だけで出力してください。`State::print()` は標準エラー出力に表示するデバッグ用の関数です。

---

### 1. 実装上の制約と高速化の指針

状態の差分更新
- 盤面全体の再計算や配列全体のコピーは避け、変更箇所のみを更新してください。

ホットパスでのメモリアロケーションの回避
- `modify` および `rollback` はループごとに呼び出されるホットパスです。これらの関数内で `vector`, `set`, `map` などの動的にメモリを確保するコンテナの宣言と破棄を毎回行わないでください。ヒープアロケーションが繰り返し発生し、実行速度が大幅に低下します。
- 作業用バッファが必要な場合は `Changed` または `State` のメンバとして事前に確保し、ループごとに内容を更新して再利用してください。`Changed` はこの用途のために用意された作業領域です。
- 既存のメンバを再利用する場合は、各ループの先頭で必要な部分に必ず値を設定してから参照してください。

`changed.type` の代入
- `modify` を呼び出すたびに、`changed.type` に必ず `[0, TYPE_CNT)` の値を代入してください。ライブラリが統計用の配列の添字に使用するため、代入しないと未定義動作になります。

スコア計算の早期打ち切り
- `modify` に渡される `threshold` は採択条件 `new_score <= threshold` の上限値です。等号が成立する場合も採択されます。
- 差分計算の過程で `new_score > threshold` が確定したら、`is_valid = false` にして直ちにリターンしてください。
- 盤面を変更する前に遅延評価で判定する実装を推奨します。この場合は `advance()` で盤面の変更を確定させます。棄却時は盤面を変更していないため、`rollback()` の処理は自明になります。
- `is_valid = false` にしてリターンする場合、ライブラリは `score` の値を参照しません。途中まで計算した値が `score` に残っていても問題ありません。

スコアの最小化と二重反転の禁止
- 最大化問題の場合は計算したスコアの符号を反転して `score` に格納してください。
- `get_score()` は `return score;` のままにしてください。内部で再度符号を反転する二重反転は行わないでください。
- `get_true_score()` は本来の生のスコアを返すようにしてください。最大化問題の場合は `-score` を返します。

### 2. 各構造体および関数の仕様

#### `State::Param` 構造体

- 目的: 焼きなましの温度パラメータを保持します。`inline static` メンバです。
- 要件: 問題のスコアのスケールに合わせて `start_temp` および `end_temp` の初期値を設定してください。

#### `State::Changed` 構造体

- 目的: 1 回の遷移の差分の情報と、ロールバックに必要な情報を保持する作業領域です。
- 要件:
  - `TYPE_CNT` に実装する近傍操作の種類の総数を設定してください。統計ログで参照されます。
  - `type` に実行した操作の種類を 0 始まりのインデックスで代入してください。
  - ロールバックに必要な情報 (変更前の値、変更箇所のインデックスなど) をメンバとして追加してください。
  - サイズが可変の作業バッファが必要な場合は、ここに `vector` などのメンバを用意して再利用してください。`modify` の中で毎回新たに確保することは避けてください。

#### `State::Result` 構造体

- 目的: 探索中に発見した最良解を保持し、最終的な解答を出力します。
- 要件:
  - 最良解を更新するたびにコピーが発生します。盤面全体のディープコピーは避け、解答の出力に必要な最小限の情報だけを保持するようにしてください。
  - 内部で使用するスコア `score` と生のスコア `true_score` をメンバに含めてください。
  - `void print(ostream &os = cout) const` に解答の出力処理を実装してください。

#### `State::init`

- シグネチャ: `void init(uint32_t seed)`
- 目的: seed 版の探索開始時に初期状態を構築します。初期化済み State 版だけを使用する場合は不要です。
- 要件:
  - 冒頭で `sarnd.set_seed(seed)` を呼び出して乱数のシードを設定してください。`sa_run` はユーザが指定した seed を、`replica_run` はレプリカごとに異なる seed を渡します。
  - 盤面の初期化と初期スコアの計算を行ってください。

#### `State::modify`

- シグネチャ: `void modify(const int64_t iter, const ScoreType threshold, const double progress)`
- 目的: 近傍操作を選択し、スコアを差分更新します。
- 要件:
  - テンプレートでは `sarnd` を用いて近傍操作を選択してください。初期化済み State 版では、事前に seed を設定した任意の乱数生成器を State 内に保持することができます。
  - 操作の種類を `changed.type` に代入し、ロールバックに必要な情報を `changed` に保存してください。
  - 次状態のスコアを差分計算し、`score` に代入してください。直後にライブラリが `get_score()` を呼び出します。
  - ルール違反の場合と早期に打ち切る場合は、Section 1 の「スコア計算の早期打ち切り」に従い、`is_valid=false` にして直ちにリターンしてください。`is_valid=false` の場合も `rollback()` は必ず呼び出されるため、`modify` の中で盤面を部分的に変更した場合は、その変更も `rollback` で元に戻してください。
  - `progress` は焼きなましの進行度で、0.0 〜 1.0 の値を取ります。近傍の切り替えに使用できます。
    - 使用は任意です。

#### `State::rollback`

- シグネチャ: `void rollback()`
- 目的: 遷移が棄却された場合に状態を元に戻します。
- 要件:
  - `changed` に保存した情報を用いて、`modify` 実行前の盤面に完全に復元してください。
  - `score` の復元は不要です。`sa_run`, `replica_run` のどちらでも、ライブラリがスコアを元の値に戻します。

#### `State::advance`

- シグネチャ: `void advance()`
- 目的: 遷移が採択された場合に状態を確定させます。
- 要件:
  - `modify` で遅延評価を採用した場合は、ここで盤面の変更を確定させてください。
  - `modify` で盤面を更新済みの場合は、空の関数でかまいません。

#### `State::get_score` / `State::get_true_score`

- シグネチャ: `ScoreType get_score() const` / `ScoreType get_true_score() const`
- 要件:
  - `get_score()` は `return score;` のままにしてください。
  - `get_true_score()` は生のスコアを返すようにしてください。最大化問題の場合は `-score` を返します。この値は verbose のログで使用されます。

#### `State::get_result`

- シグネチャ: `Result get_result() const`
- 目的: 現在の状態を `Result` 構造体として返します。
- 要件: 解答の出力に必要な最小限の情報を抽出して `Result` を生成してください。

#### `State::print`

- シグネチャ: `void print() const`
- 要件: 盤面の状態を標準エラー出力に表示してください。

### 3. 外部ライブラリ

#### 乱数 `sarnd`

`sarnd` は `State` のパブリックメンバとして宣言された `titan23::Random` 型のオブジェクトで、高速な XorShift 乱数生成器です。

| メソッド | 返り値 |
|----------|--------|
| `sarnd.random()` | `[0.0, 1.0]` の実数 |
| `sarnd.randint(end)` | `[0, end]` の整数 (両端を含む) |
| `sarnd.randint(begin, end)` | `[begin, end]` の整数 (両端を含む) |
| `sarnd.randrange(end)` | `[0, end)` の整数 (end を含まない) |
| `sarnd.randrange(begin, end)` | `[begin, end)` の整数 (end を含まない) |
| `sarnd.rand_u64()` | `[0, 2^64)` の整数 |
| `sarnd.shuffle(vec)` | vector をインプレースでシャッフルする |
