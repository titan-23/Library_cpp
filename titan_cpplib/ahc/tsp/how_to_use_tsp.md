# AHC 向け TSP ライブラリ

対称 TSP と非対称 TSP の巡回順を反復局所探索で求めます。返される巡回順には各点が 1 回ずつ現れ、巡回路のコストには末尾から先頭に戻る辺も含まれます。対称 TSP の状態と近傍操作は焼きなましにも使用できます。

## 対称 TSP の巡回順

`solve_tsp` は点数と辺のコスト関数を受け取り、指定した点を先頭とする巡回順を返します。次の例では、平面上の 2 点間の距離を最も近い整数に丸めて辺のコストとします。

```cpp
#include <bits/stdc++.h>
#include "titan_cpplib/ahc/tsp/tsp_iterated_local_search.cpp"
using namespace std;
using namespace titan23;

int main() {
    int n;
    cin >> n;
    vector<long long> x(n), y(n);
    for (int i = 0; i < n; ++i) cin >> x[i] >> y[i];
    // 各点間の距離を事前に計算する
    vector<vector<int>> dist(n, vector<int>(n));
    for (int i = 0; i < n; ++i) for (int j = 0; j < n; ++j) {
        dist[i][j] = (int)round(hypot(x[i] - x[j], y[i] - y[j]));
    }
    auto edge_cost = [&](int u, int v) -> long long { return dist[u][v]; };
    // 制限時間は 100 ms、開始点は 0
    vector<int> order = solve_tsp(n, edge_cost, 100.0);
    for (int v : order) cout << v << '\n';
}
```

```cpp
vector<int> solve_tsp(int node_count, EdgeCost edge_cost, double time_limit_ms, int start = 0, uint32_t seed = 23);
```

- `edge_cost(u, v)` は 0 以上の値を返し、`edge_cost(u, v) == edge_cost(v, u)` を満たす必要があります。戻り値の型には符号付き整数型または浮動小数点型を使用します。
- `start` の既定値は `0` です。
- `time_limit_ms` は候補表と初期解の作成を含む制限時間です。これらの作成には `O(n²)` 時間かかり、作成中に制限時間を超えた場合も最後まで実行します。
- 探索中は `edge_cost` を繰り返し呼び出します。座標から毎回計算するより、距離行列を参照する方が高速です。

## 非対称 TSP と始点に戻らない経路

`solve_atsp` は辺の向きによってコストが異なる巡回路を扱います。`solve_atsp_path` は指定した始点から全点を 1 回ずつ訪問する経路を求めます。終点は任意の点とすることも、引数 `goal` で指定することもできます。

```cpp
#include "titan_cpplib/ahc/tsp/atsp_iterated_local_search.cpp"

// コストは作業 u の終了位置から作業 v の開始位置までの距離
auto edge_cost = [&](int u, int v) -> long long { return cost[u][v]; };

// 点 0 を始点とする巡回路
vector<int> order = solve_atsp(n, edge_cost, 100.0);

// 始点は s、終点は任意
vector<int> path = solve_atsp_path(n, edge_cost, 100.0, s);

// 始点は s、終点は g
vector<int> fixed = solve_atsp_path(n, edge_cost, 100.0, s, g);
```

```cpp
vector<int> solve_atsp(int node_count, EdgeCost edge_cost, double time_limit_ms, int start = 0, uint32_t seed = 23);
vector<int> solve_atsp_path(int node_count, EdgeCost edge_cost, double time_limit_ms, int start, int goal = -1, uint32_t seed = 23);
```

- `solve_atsp_path` は対称コストにも対応します。点数は 2 以上とし、終点を指定する場合は始点と異なる点を指定します。
- `time_limit_ms` には候補表と初期解の作成時間も含まれます。
- `solve_atsp_path` は経路を巡回路に変換する際に、禁止する辺のコストとして `(辺のコストの最大値 + 1) * (node_count + 2) * 2` を使用します。辺のコスト関数の戻り値には、この値を表現できる型を指定してください。

### 非対称 TSP の初期解と探索設定

`atsp_iterated_local_search` に候補表と初期巡回順を渡すと、指定した点だけを巡る部分巡回路も改善できます。

```cpp
TspProblem problem(n, edge_cost);
AtspCandidates candidates = make_atsp_candidates(problem, 10);
vector<int> order = problem.make_nearest_neighbor_state(0).order();   // 初期解

AtspIteratedLocalSearchOptions options;
options.time_limit_ms = 100;
auto result = atsp_iterated_local_search(problem, candidates, order, options);
long long cost = atsp_tour_cost(problem, order);
```

- `order` には重複のない点列を指定します。探索は `order` を直接更新し、先頭の点を維持します。
- `max_reverse_length` は 2-opt で反転する区間の長さの上限です。既定値は `200` です。`0` にすると 2-opt と反転 Or-opt の両方が無効になります。
- `make_atsp_assignment_order(problem, start)` は割当問題を解き、得られた閉路を併合して初期巡回順を作成します。計算量は時間 `O(n³)`、メモリ `O(n²)` です。作業の段取り替えなど、コストが平面上の距離に対応しない問題では、最近傍法より低コストの初期解が得られる場合があります。
- `solve_atsp` と `solve_atsp_path` は最近傍法で初期解を作成します。8 点以上 2000 点以下では、残り時間や相互に最近傍となる点の割合に応じて割当問題による初期解も求め、コストの小さい方を採用します。`solve_atsp_path` ではダミー点を加えた点数で判定します。[割当問題による初期解の採用条件](tsp_design.md#割当問題による初期解)

### 非対称 TSP の計測結果

各入力で得られた最小コストを基準に、`solve_atsp` が返した巡回路のコストの超過率を求めました。表の値は 5 種類のシードでの平均です。制限時間には候補表と初期解の作成時間を含みます。

| 入力の種類 | 点数 | 100 ms | 1000 ms |
|---|---:|---:|---:|
| 対称 (平面上の距離) | 1000 | 0.57% | 0.08% |
| 対称 (平面上の距離) | 2000 | 1.07% | 0.36% |
| 進行方向によって距離が最大 1.3 倍 | 1000 | 0.56% | 0.08% |
| 進行方向によって距離が最大 1.3 倍 | 2000 | 1.04% | 0.14% |
| 上り方向に追加コスト | 1000 | 0.34% | 0.04% |
| 上り方向に追加コスト | 2000 | 0.97% | 0.02% |
| 各点に開始位置と終了位置を指定 | 1000 | 0.44% | 0.17% |
| 各点に開始位置と終了位置を指定 | 2000 | 1.95% | 0.00% |
| 乱数で生成したコスト行列 | 1000 | 1.91% | 0.86% |
| 乱数で生成したコスト行列 | 2000 | 70.83% | 0.24% |

- 200 点の入力では、乱数で生成したコスト行列を除き、100 ms での超過率は 0.05% 未満でした。
- 割当問題の最適値は巡回路のコストの下界です。この下界に対する最良解の超過率は、各点に開始位置と終了位置を指定した 1000 点の入力で 1.2%、乱数で生成したコスト行列では 1000 点で 2.4%、2000 点で 2.5% でした。
- 平面上の対称な 1000 点の入力では、1000 ms での超過率は `solve_atsp` で 0.08%、対称版の `tsp_iterated_local_search` で 0.13% でした。
- 乱数で生成した 2000 点のコスト行列では、100 ms の制限で割当問題による初期解の作成を省略し、最近傍法の初期解から探索しました。

## 対称 TSP の候補表と初期状態

同じ問題を複数回探索する場合は `TspProblem` と `TspCandidates` を共有し、初期巡回路ごとに `TspState` を作成します。

```cpp
#include "titan_cpplib/ahc/tsp/tsp_iterated_local_search.cpp"

TspProblem problem(n, edge_cost);
TspCandidates candidates = make_tsp_candidates(problem, 10);   // 各点につき近い順に最大 10 点
auto state = problem.make_nearest_neighbor_state(0);           // 点 0 を開始点とする初期解

TspIteratedLocalSearchOptions options;
options.time_limit_ms = 100;
auto result = tsp_iterated_local_search(problem, candidates, state, options);

const vector<int>& order = state.order();              // 改善後の巡回順
long long cost = state.total_cost();                   // 改善後のコスト
```

- 候補表は問題ごとに 1 回だけ作成し、探索と焼きなましで共有します。候補数は 8 から 12 を基本にします。
- 探索関数は `state` を直接更新します。巡回路の向きが変化する場合も、`order()[0]` は元の開始点です。
- 一部の点だけを巡る場合は `problem.make_state(order)` または `problem.make_nearest_neighbor_state(start, nodes_to_visit)` で部分巡回路を作成します。探索は巡回路に含まれる点だけを並べ替えます。

## 対称 TSP の探索手法

対称 TSP の巡回順を求める場合は、最近傍法で初期解を作成して反復局所探索を行う `solve_tsp` を推奨します。初期巡回路を指定する場合は `tsp_iterated_local_search` を使用します。

| 関数 | 内容 | 用途 |
|---|---|---|
| `tsp_iterated_local_search` | 2-opt、Or-opt、3-opt の局所探索と Double Bridge を繰り返す | 巡回順を求める場合の基本 |
| `tsp_two_opt_local_search` | 2-opt でコストが減少する間、操作を繰り返す | 初期解の局所的な改善 |
| `tsp_guided_local_search` | 巡回路の辺にペナルティを加えて 2-opt を再開する | 元のコストでは 2-opt で改善できない巡回路から探索を続ける場合 |

TSPLIB の各入力について、最適解のコストに対する超過率を示します。辺のコストは距離行列で与え、候補数を 10 として最近傍法の初期解から探索しました。表の時間は探索時間のみで、反復局所探索の値は 5 種類のシードでの平均です。

| 入力 | 点数 | 反復局所探索 10 ms | 100 ms | 1000 ms | 誘導局所探索 1000 ms |
|---|---:|---:|---:|---:|---:|
| eil51 | 51 | 0.00% | 0.00% | 0.00% | 0.00% |
| a280 | 280 | 0.00% | 0.00% | 0.00% | 0.00% |
| rd400 | 400 | 0.38% | 0.03% | 0.00% | 0.00% |
| pcb442 | 442 | 0.29% | 0.02% | 0.00% | 0.00% |
| fnl4461 | 4461 | 3.24% | 1.00% | 0.37% | 1.98% |

候補表と初期解の作成には、442 点で約 0.7 ms、1000 点で約 3 ms、2000 点で約 9 ms、4461 点で約 70 ms かかりました。コンパイルには `g++ -O2 -march=native` を使用しました。

## 反復局所探索

```cpp
TspIteratedLocalSearchOptions options;
options.time_limit_ms = 100;      // 実行時間の上限 [ms] / -1 で無効
options.max_iterations = -1;      // 反復回数の上限 / -1 で無効、0 で初回の局所探索のみ
options.max_kick_length = 50;     // Double Bridge で並べ替える各区間の長さの上限
options.temperature_ratio = 0.5;  // 初期温度を決める係数 / 0 ならコストが増加する反復を棄却
options.seed = 23;
options.use_three_opt = true;     // false なら局所探索を 2-opt と Or-opt だけにする
auto result = tsp_iterated_local_search(problem, candidates, state, options);
```

- 実行時間と反復回数の少なくとも一方に上限を設定してください。両方が無効な場合は `invalid_argument` が発生します。
- 探索終了時の `state` には、探索で得た最良の巡回路が格納されています。
- 初期温度は `temperature_ratio` と初期巡回路の辺の平均コストの積です。温度は探索の進行に応じて線形に低下し、上限で 0 になります。時間と反復回数の両方を指定した場合は、それぞれの上限に対する割合の大きい方を使用します。
- 各反復の開始時と、局所探索でキューの点を 128 個処理するごとに経過時間を確認します。
- 計測時には `use_three_opt = false` で反復 1 回あたりの時間が約 0.4 倍になりました。同じ制限時間で比較すると、1000 点を超える入力とクラスタ状の入力では 3-opt を有効にした方が低コストでした。300 点以下かつ 3 ms 以下では明確な差はありませんでした。
- `result.iterations` は完了した反復回数、`result.accepted_iterations` は採択した反復回数、`result.stop_reason` は停止理由です。

## 2-opt 局所探索

```cpp
TspLocalSearchOptions options;
options.max_evaluated_moves = 200000;   // 評価する操作の数の上限 / -1 で無効
options.first_improvement = true;
auto result = tsp_two_opt_local_search(problem, candidates, state, options);
```

候補表を渡すと、各点とその候補点を隣接させる 2-opt を評価します。候補表を省略すると全ての 2-opt が対象になり、一巡の評価に `O(n²)` 時間かかります。

`first_improvement = true` では最初にコストが減少した操作を適用します。`false` では一巡のうちコストの減少量が最大の操作を選択します。評価回数の上限で探索を打ち切ると `result.locally_optimal` は `false` になります。

## 誘導局所探索

```cpp
TspGuidedLocalSearchOptions options;
options.max_penalty_rounds = -1;
options.max_evaluated_moves = -1;
options.time_limit_ms = 100;
options.penalty_ratio = 0.3L;
auto result = tsp_guided_local_search(problem, candidates, state, options);
```

最初に元の辺のコストで 2-opt の局所探索を行います。以降の各反復では、巡回路上で次の値が最大となる全ての辺のペナルティを 1 増やし、それらの端点からペナルティ付きのコストで 2-opt を再開します。

```text
edge_cost(u, v) / (1 + penalty(u, v))
```

各回の局所探索を終えるたびに元のコストを比較し、最良の巡回路を `state` に返します。実行時間とペナルティの更新回数の少なくとも一方に上限を設定してください。

## 焼きなましへの組み込み

巡回順と問題固有の変数を同時に探索する場合は、焼きなましの `State` に `TspState` を保持します。`make_two_opt` などのメソッドでコストの差分を求め、操作を採択した時点で `apply` を呼び出します。

```cpp
#include "titan_cpplib/ahc/tsp/tsp_symmetric_moves.cpp"

auto move = state.make_two_opt(problem, left, right);
if (move.valid()) {
    auto next_cost = state.total_cost() + move.delta();
    if (accept(next_cost)) state.apply(problem, move);
}
```

| 操作 | メソッド | 巡回順の変更 |
|---|---|---|
| 2-opt | `make_two_opt(problem, left, right)` | 区間 `[left, right]` を反転する (`left >= 1`) |
| Or-opt | `make_or_opt(problem, first, length, after, false)` | 区間 `[first, first + length)` を変更前の位置 `after` の直後へ移動する |
| 反転 Or-opt | `make_or_opt(problem, first, length, after, true)` | 区間を反転して移動する |
| Double Bridge | `make_double_bridge(problem, cut1, cut2, cut3, cut4)` | 位置 `cut1 < cut2 < cut3 < cut4` を始点とする 4 辺を交換する |

- 添字や区間長が条件を満たさない場合と、操作をデフォルト構築した場合は `valid()` が `false` を返します。
- 各操作は巡回順の先頭にある開始点を維持します。
- `apply` は操作の作成元と状態の更新回数を確認します。別の状態や、代入、`apply`、`reset` によって更新した状態に古い操作を適用すると例外が発生します。状態をコピーした場合も、操作はコピー先の状態から作成してください。

次の例では焼きなましの `State` に `TspTwoOptMove<Cost>` 型のメンバ `pending` を用意し、採択が決まるまで操作を保持します。

```cpp
void modify(int64_t, ScoreType, double) {
    changed.type = 0;
    auto [left, right] = random.rand_pair(1, tsp.size());
    pending = tsp.make_two_opt(*problem, left, right);
    if (!pending.valid()) { is_valid = false; return; }
    score = tsp.total_cost() + pending.delta();      // score だけを更新する
}
void rollback() {}                                   // 巡回路は変更していない
void advance() { tsp.apply(*problem, pending); }
```

焼きなましの前に `improve_tsp_initial_state` で初期巡回路を改善できます。初期化にかかった時間を全体の制限時間から引き、残り時間を `sa_run` に渡します。

```cpp
#include "titan_cpplib/ahc/sa/sa.cpp"
#include "titan_cpplib/ahc/tsp/tsp_initial_state.cpp"

Timer timer;
auto tsp = problem.make_nearest_neighbor_state(0);
TspInitialStateOptions initial_options;
initial_options.search = TspInitialSearch::iterated_local_search;
initial_options.iterated_local_search.time_limit_ms = 50;
improve_tsp_initial_state(problem, candidates, tsp, initial_options);
State state(problem, candidates, move(tsp), 23);
double remaining = max(0.0, 1900.0 - timer.elapsed());
auto result = sa::sa_run<State>(remaining, state, true);
```

`initial_options.search` の既定値は `none` で、作成した巡回路をそのまま使用します。初期解を改善する場合は `two_opt`, `guided_local_search`, `iterated_local_search` のいずれかを指定します。

## 複数巡回路

```cpp
#include "titan_cpplib/ahc/tsp/multiple_tours.cpp"

vector<int> depots = {0, 1};
vector<vector<int>> routes = {
    {0, 2, 3},
    {1, 4, 5},
};
auto state = make_multiple_tsp_state(problem, depots, routes);
```

各巡回路の先頭は固定デポです。デポを含む全ての点を、いずれかの巡回路に 1 回ずつ含めます。デポだけの巡回路も指定できます。

```cpp
auto intra = state.make_two_opt(problem, route, left, right);
auto shift = state.make_block_shift(problem, source, first, length, target, after);
auto swap = state.make_block_swap(problem, route1, first1, length1, route2, first2, length2);
```

区間を移動または交換するときは、操作から各巡回路のコストの差分を個別に取得できます。全巡回路の総距離、各巡回路の距離の二乗和、最長の巡回路の距離など、目的関数に応じて差分を使用します。

## ファイル

| ファイル | 内容 |
|---|---|
| `tsp.cpp` | `TspProblem`、`TspCandidates`、`TspState`、最近傍法による初期解 |
| `tsp_symmetric_moves.cpp` | 2-opt、Or-opt、反転 Or-opt、Double Bridge |
| `tsp_local_search.cpp` | 2-opt 局所探索、探索関数が共有する内部クラス |
| `tsp_iterated_local_search.cpp` | 反復局所探索、`solve_tsp` |
| `atsp_iterated_local_search.cpp` | 非対称コストの反復局所探索、`solve_atsp`、`solve_atsp_path` |
| `tsp_guided_local_search.cpp` | 誘導局所探索 |
| `tsp_initial_state.cpp` | SA の初期状態の改善方法を選択する補助関数 |
| `multiple_tours.cpp` | 固定デポのある複数の巡回路 |
| `test/tsp_test.cpp` | 操作によるコストの差分、状態の不変条件、探索結果の自動テスト |
| `test/tsp_sa_test.cpp` | `TspState` を焼きなましに組み込んだ場合の動作確認 |
| `test/atsp_test.cpp` | 非対称 TSP の巡回路と始点に戻らない経路の確認 |

使用する機能の `.cpp` ファイルを `#include` してください。`tsp_initial_state.cpp` からは対称 TSP の単一巡回路に関する全機能を使用できます。`atsp_iterated_local_search.cpp` は割当問題を解くために `titan_cpplib/graph/hungarian.cpp` も読み込みます。

## 計算量

対称 TSP の全点を含む巡回路について、点数を `n`、1 点あたりの候補数を `c` とします。

| 処理 | 時間 | 追加メモリ |
|---|---:|---:|
| 状態の作成とコストの計算 | `O(n)` | `O(n)` |
| 最近傍法による初期解 | `O(n²)` | `O(n)` |
| 候補表の作成 | `edge_cost` の呼び出しが `O(n²)` 回、候補の挿入を含めると最悪 `O(n²c)` | `O(nc)` |
| 2-opt、Or-opt、Double Bridge の評価 | `O(1)` | `O(1)` |
| 2-opt の適用 | `O(right - left)` | `O(1)` |
| Or-opt の適用 | 位置が変化する範囲の長さに比例 | `O(1)` |
| Double Bridge の適用 | `O(cut4 - cut1)` | `TspState` 内の `O(n)` の作業領域 |
| 候補表を使用する 2-opt の準備 | `O(nc²)` | `O(nc)` |
| 候補表を使用する 2-opt での 1 点の候補評価 | `O(c)` | `O(1)` |

候補表を使用する対称 TSP の反復局所探索では、操作によって隣接関係が変化した点をキューに追加します。区間の反転にかかる時間は、その区間と残りの区間のうち短い方の長さに比例します。3-opt を有効にした場合は、1 点につき最大 `O(c²)` 組の候補を評価します。

複数巡回路では、長さ `L` の区間移動の評価に `O(L)`、長さ `L1` と `L2` の区間交換の評価に `O(L1 + L2)` 時間かかります。いずれも操作の適用時間は対象となる 2 つの巡回路の長さの和に比例します。

## 利用条件

- 点数 `node_count` は 1 以上、点番号の範囲は `[0, node_count)` です。`solve_atsp_path` では 2 点以上が必要です。
- 辺のコストは有限かつ 0 以上とします。対称 TSP の探索と `TspState` の近傍操作では、往路と復路のコストが等しいことが条件です。三角不等式は不要です。
- コストの型 `Cost` には、巡回路の総コストと差分計算の中間値を表現できる符号付き整数型または浮動小数点型を使用します。
- `TspProblem` は、その問題から作成した候補表と状態より後に破棄してください。
- 反復局所探索には実行時間または反復回数の上限を設定します。誘導局所探索には実行時間またはペナルティの更新回数の上限が必要です。
