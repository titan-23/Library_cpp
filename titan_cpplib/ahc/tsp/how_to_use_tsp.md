# AHC 向け TSP ライブラリ

対称 TSP と非対称 TSP の巡回路を短時間で改善する探索と、焼きなましに組み込むための巡回路の状態と近傍操作を提供します。巡回順の末尾に開始点を再び含めず、末尾の点から先頭の点への辺は自動的にコストに加えます。

## 巡回順を求める最短の例

`solve_tsp` は、点数と辺のコストを返す関数から巡回順を求めます。

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
    // 距離行列を先に作成し、辺のコストは表の参照だけにする
    vector<vector<int>> dist(n, vector<int>(n));
    for (int i = 0; i < n; ++i) for (int j = 0; j < n; ++j) {
        dist[i][j] = (int)round(hypot(x[i] - x[j], y[i] - y[j]));
    }
    auto edge_cost = [&](int u, int v) -> long long { return dist[u][v]; };
    // 100 ms で巡回順を求める / order[0] は 0
    vector<int> order = solve_tsp(n, edge_cost, 100.0);
    for (int v : order) cout << v << '\n';
}
```

```cpp
vector<int> solve_tsp(int node_count, EdgeCost edge_cost, double time_limit_ms, int start = 0, uint32_t seed = 23);
```

- `edge_cost(u, v)` は対称で 0 以上の値を返す関数です。戻り値の型がコストの型になり、符号付き整数と浮動小数点数を使用できます。
- 返す巡回順の先頭は `start` です。
- `time_limit_ms` には候補表と初期解の作成時間を含みます。この 2 つは `O(n²)` で、途中で中断しません。
- `edge_cost` は探索中に繰り返し呼び出されます。座標から毎回計算するより、距離行列を参照する方が高速です。

## 非対称コストと、戻らない経路

`edge_cost(u, v)` と `edge_cost(v, u)` が異なる問題と、始点から始まり戻らない経路には、`atsp_iterated_local_search.cpp` の関数を使用します。

```cpp
#include "titan_cpplib/ahc/tsp/atsp_iterated_local_search.cpp"

// u の作業の終了位置から v の作業の開始位置までの距離のように、向きによって異なるコスト
auto edge_cost = [&](int u, int v) -> long long { return cost[u][v]; };

// 全点を巡って戻る巡回順 / order[i] から order[i + 1] へ進み、末尾から先頭へ戻る / order[0] は 0
vector<int> order = solve_atsp(n, edge_cost, 100.0);

// 点 s から始まり、全点を 1 回ずつ通って戻らない経路 / path[0] は s
vector<int> path = solve_atsp_path(n, edge_cost, 100.0, s);

// 終点も固定する場合 / path.back() は g
vector<int> fixed = solve_atsp_path(n, edge_cost, 100.0, s, g);
```

```cpp
vector<int> solve_atsp(int node_count, EdgeCost edge_cost, double time_limit_ms, int start = 0, uint32_t seed = 23);
vector<int> solve_atsp_path(int node_count, EdgeCost edge_cost, double time_limit_ms, int start, int goal = -1, uint32_t seed = 23);
```

- 辺のコストが対称な問題にも使用できます。戻らない経路は、辺のコストが対称な場合も `solve_atsp_path` で求めます。
- `time_limit_ms` には候補表と初期解の作成時間を含みます。
- `solve_atsp_path` では、`Cost` が `(辺のコストの最大値 + 1) * (node_count + 2) * 2` を表現できる必要があります。コストが大きい場合は `long long` を返す関数にしてください。

部品を個別に呼び出す場合は、巡回順を `vector<int>` で渡します。

```cpp
TspProblem problem(n, edge_cost);
AtspCandidates candidates = make_atsp_candidates(problem, 10);
vector<int> order = problem.make_nearest_neighbor_state(0).order();   // 初期解

AtspIteratedLocalSearchOptions options;
options.time_limit_ms = 100;
auto result = atsp_iterated_local_search(problem, candidates, order, options);
long long cost = atsp_tour_cost(problem, order);
```

- `order` は重複しない点の列です。一部の点だけを渡すと、その点だけを巡る巡回路を改善します。`order[0]` は変化しません。
- `options` の項目は対称版と同じ意味です。`max_reverse_length` は、区間を反転する操作で反転する区間の長さの上限で、既定値は 200 です。0 にすると区間を反転する操作を使用しません。
- `make_atsp_assignment_order(problem, start)` は、割当問題の最適解から初期解を作成します。作業の段取り替えのコストのように、辺のコストに座標のような構造がない問題では、最近傍法より良い初期解になります。`O(n³)` 時間と `O(n²)` メモリを使用します。`solve_atsp` と `solve_atsp_path` は、2000 点以下で時間に余裕がある場合にこの初期解も試し、コストの小さい方から探索します。
- `TspState` の `make_two_opt` などの近傍操作は、辺のコストが対称であることを前提にしています。非対称コストでは使用しないでください。

次の表は、`solve_atsp` が返した巡回路のコストの、その入力で得られた最良のコストに対する超過の割合です。5 個のシードの平均で、時間には候補表と初期解の作成を含みます。

| 入力の種類 | 点数 | 100 ms | 1000 ms |
|---|---:|---:|---:|
| 対称 (平面上の距離) | 1000 | 0.57% | 0.08% |
| 対称 (平面上の距離) | 2000 | 1.07% | 0.36% |
| 進む向きで距離が最大 1.3 倍になる | 1000 | 0.56% | 0.08% |
| 進む向きで距離が最大 1.3 倍になる | 2000 | 1.04% | 0.14% |
| 上りに追加のコストがかかる | 1000 | 0.34% | 0.04% |
| 上りに追加のコストがかかる | 2000 | 0.97% | 0.02% |
| 各点に開始位置と終了位置がある | 1000 | 0.44% | 0.17% |
| 各点に開始位置と終了位置がある | 2000 | 1.95% | 0.00% |
| 乱数の行列 | 1000 | 1.91% | 0.86% |
| 乱数の行列 | 2000 | 70.83% | 0.24% |

- 200 点の入力は、乱数の行列を除いて 100 ms で超過が 0.05% 未満でした。
- 最良のコストは、割当問題の最適値 (巡回路のコストの下界) に対して、開始位置と終了位置がある 1000 点の入力で +1.2%、乱数の行列の 1000 点で +2.4%、2000 点で +2.5% でした。
- 対称な 1000 点の入力では、対称版の `tsp_iterated_local_search` の 1000 ms での超過は 0.13% で、同程度です。
- 乱数の行列の 2000 点では、100 ms では割当問題による初期解を作成する時間が足りず、最近傍法の初期解から探索するため結果が悪くなります。

## 部品を組み合わせる例

同じ問題を繰り返し解く場合や、部分巡回路を改善する場合は、部品を個別に呼び出します。

```cpp
#include "titan_cpplib/ahc/tsp/tsp_iterated_local_search.cpp"

TspProblem problem(n, edge_cost);                      // 問題 / コピーと移動はできない
TspCandidates candidates = make_tsp_candidates(problem, 10);   // 各点の近い順の 10 点
auto state = problem.make_nearest_neighbor_state(0);   // 最近傍法の初期解 / order()[0] は 0

TspIteratedLocalSearchOptions options;
options.time_limit_ms = 100;
auto result = tsp_iterated_local_search(problem, candidates, state, options);

const vector<int>& order = state.order();              // 改善後の巡回順
long long cost = state.total_cost();                   // 改善後のコスト
```

- 候補表は問題ごとに 1 回だけ作成し、探索と焼きなましで共有します。候補数は 8 から 12 を基本にします。
- 探索関数は渡された `state` を直接更新します。`order()[0]` は変化しません。巡回路の向きは変化する場合があります。
- 一部の点だけを巡る場合は `problem.make_state(order)` または `problem.make_nearest_neighbor_state(start, nodes_to_visit)` で部分巡回路を作成します。探索は巡回路に含まれる点だけを並べ替えます。
- `TspProblem` は `edge_cost` をその型のまま保持します。ラムダが参照する距離行列は、`TspProblem` より後に破棄してください。

## 探索の選択

| 関数 | 内容 | 用途 |
|---|---|---|
| `tsp_iterated_local_search` | 2-opt、Or-opt、3-opt の局所探索と Double Bridge を繰り返す | 巡回順を求める場合の基本 |
| `tsp_two_opt_local_search` | 2-opt だけの局所探索 | 1 回で終了する短い改善 |
| `tsp_guided_local_search` | 辺にペナルティを加える誘導局所探索 | 反復局所探索との比較 |
| `tsp_edge_penalty_search` | `tsp2.py` と同じ処理順の探索 | `tsp2.py` との比較 |

次の表は、最適解のコストに対する超過の割合です。TSPLIB の入力を距離行列で与え、候補数 10、最近傍法の初期解から探索しました。反復局所探索は 5 個のシードの平均です。時間には候補表と初期解の作成を含みません。

| 入力 | 点数 | 反復局所探索 10 ms | 100 ms | 1000 ms | 誘導局所探索 1000 ms |
|---|---:|---:|---:|---:|---:|
| eil51 | 51 | 0.00% | 0.00% | 0.00% | 0.00% |
| a280 | 280 | 0.00% | 0.00% | 0.00% | 0.00% |
| rd400 | 400 | 0.38% | 0.03% | 0.00% | 0.00% |
| pcb442 | 442 | 0.29% | 0.02% | 0.00% | 0.00% |
| fnl4461 | 4461 | 3.24% | 1.00% | 0.37% | 1.98% |

候補表と初期解の作成時間は、442 点で約 0.7 ms、1000 点で約 3 ms、2000 点で約 9 ms、4461 点で約 70 ms でした。計測は `g++ -O2 -march=native` で行いました。

## 反復局所探索

```cpp
TspIteratedLocalSearchOptions options;
options.time_limit_ms = 100;      // 実行時間の上限 [ms] / -1 で無効
options.max_iterations = -1;      // 反復の回数の上限 / -1 で無効 / 0 なら最初の局所探索だけ
options.max_kick_length = 50;     // Double Bridge で並べ替える各区間の長さの上限
options.temperature_ratio = 0.5;  // 悪化を採択する温度の初期値 / 0 なら悪化を採択しない
options.seed = 23;
options.use_three_opt = true;     // false なら局所探索を 2-opt と Or-opt だけにする
auto result = tsp_iterated_local_search(problem, candidates, state, options);
```

- 時間と反復の回数の少なくとも一方に上限を設定してください。両方が無効な場合は例外を投げます。
- 返却時の `state` は、探索中に見つけたコストが最小の巡回路です。
- `temperature_ratio` は、巡回路の辺の平均コストに対する比です。温度は時間または反復の回数の進行度に対して線形に 0 まで下げます。
- 時間の上限は、局所探索で 128 点を処理するたびと、各反復の先頭で確認します。
- `use_three_opt = false` にすると、反復 1 回あたりの時間は約 0.4 倍になります。計測では、1000 点を超える入力とクラスタ状の入力で `true` の方が良い結果になり、300 点以下で制限時間が 3 ms 以下の場合は明確な差がありませんでした。
- `result.iterations` は反復の回数、`result.accepted_iterations` は採択した反復の回数、`result.stop_reason` は停止した理由です。

## 2-opt 局所探索

```cpp
TspLocalSearchOptions options;
options.max_evaluated_moves = 200000;   // 評価する操作の数の上限 / -1 で無効
options.first_improvement = true;
auto result = tsp_two_opt_local_search(problem, candidates, state, options);
```

候補表を渡すオーバーロードは、点と候補点を隣接させる 2-opt だけを評価します。候補表を渡さないオーバーロードは全ての 2-opt を評価し、一巡に `O(n²)` 時間かかります。

`first_improvement = false` の場合は、一巡のうちコストの差分が最小の操作を適用します。評価回数の上限に達して終了した場合は `locally_optimal == false` になります。

## 誘導局所探索

```cpp
TspGuidedLocalSearchOptions options;
options.max_penalty_rounds = -1;
options.max_evaluated_moves = -1;
options.time_limit_ms = 100;
options.penalty_ratio = 0.3L;
auto result = tsp_guided_local_search(problem, candidates, state, options);
```

まず、ペナルティを含まないコストで 2-opt の局所探索を行います。その後、現在の巡回路の辺のうち次の値が最大の辺すべてにペナルティを加え、その辺の端点の周囲だけを、ペナルティを加えたコストで再び探索することを繰り返します。

```text
edge_cost(u,v) / (1 + penalty(u,v))
```

返却時の `state` は、局所探索が収束した時点の巡回路のうち、ペナルティを含まないコストが最小のものです。時間とペナルティの更新回数の少なくとも一方に上限を設定してください。

## tsp2.py との比較用の探索

```cpp
TspEdgePenaltySearchOptions options;
options.max_penalty_rounds = 10000;
options.max_evaluated_moves = 2000000;
options.time_limit_ms = 100;
auto result = tsp_edge_penalty_search(problem, candidates, state, options);
```

一周ごとに評価値が最大の辺を 1 本だけ選んでペナルティを加え、その辺を外す候補のうち改善となる最初の 2-opt を最大 1 回適用します。辺の選択、候補の順序、採択条件は `tsp2.py` と同じです。

## 焼きなましへの組み込み

巡回順とほかの決定を同時に探索する場合は、`TspState` と近傍操作を焼きなましの `State` に組み込みます。

操作を作成しても状態は変更されません。採択した場合だけ `apply` を呼び出します。

```cpp
#include "titan_cpplib/ahc/tsp/tsp_symmetric_moves.cpp"

auto move = state.make_two_opt(problem, left, right);
if (move.valid()) {
    auto next_cost = state.total_cost() + move.delta();
    if (accept(next_cost)) state.apply(problem, move);
}
```

| 操作 | 作成 | 内容 |
|---|---|---|
| 2-opt | `make_two_opt(problem, left, right)` | 区間 `[left, right]` を反転する (`left >= 1`) |
| Or-opt | `make_or_opt(problem, first, length, after, false)` | 区間 `[first, first+length)` を変更前の位置 `after` の直後へ移動する |
| 反転 Or-opt | `make_or_opt(problem, first, length, after, true)` | 区間を反転して移動する |
| Double Bridge | `make_double_bridge(problem, cut1, cut2, cut3, cut4)` | 位置 `cut1 < cut2 < cut3 < cut4` を始点とする 4 辺を交換する |

- 作成できない引数の場合は、`valid()` が false の操作を返します。操作の型はデフォルト構築することができ、その値も無効な操作です。
- `order()[0]` は操作を適用しても変化しません。
- 操作は作成元の状態にだけ適用できます。状態を変更、代入、`reset` すると、それ以前に作成した操作は適用できなくなり、適用すると例外を投げます。
- 状態をコピーすると、コピー元から独立した状態になります。コピー元で作成した操作をコピー先に適用することはできません。

焼きなましの `State` は、確定済みの `TspState` と未適用の操作を保持します。次の例の `pending` は `TspTwoOptMove<Cost>` 型のメンバです。

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

初期状態は、焼きなましの前に反復局所探索などで改善することができます。

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

`initial_options.search` には `none`, `two_opt`, `guided_local_search`, `edge_penalty_search`, `iterated_local_search` を指定できます。既定値は `none` で、初期状態を改善しません。

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

各巡回路の 0 番目は固定デポです。全巡回路を通じて、すべての点を一度ずつ含める必要があります。デポだけからなる巡回路も使用できます。

```cpp
auto intra = state.make_two_opt(problem, route, left, right);
auto shift = state.make_block_shift(problem, source, first, length, target, after);
auto swap = state.make_block_swap(problem, route1, first1, length1, route2, first2, length2);
```

区間の移動と交換は、影響を受ける各巡回路のコストの差分を個別に返します。総距離、距離の二乗和、最大距離などの目的関数は利用側で選択します。

## ファイル

| ファイル | 内容 |
|---|---|
| `tsp.cpp` | `TspProblem`、`TspCandidates`、`TspState`、最近傍法による初期解 |
| `tsp_symmetric_moves.cpp` | 2-opt、Or-opt、反転 Or-opt、Double Bridge |
| `tsp_local_search.cpp` | 2-opt 局所探索、探索関数が共有する内部クラス |
| `tsp_iterated_local_search.cpp` | 反復局所探索、`solve_tsp` |
| `atsp_iterated_local_search.cpp` | 非対称コストの反復局所探索、`solve_atsp`、`solve_atsp_path` |
| `tsp_guided_local_search.cpp` | 誘導局所探索 |
| `tsp_edge_penalty_search.cpp` | `tsp2.py` と同じ処理順で動作する比較用の探索 |
| `tsp_initial_state.cpp` | SA の初期状態の改善方法を選択する補助関数 |
| `multiple_tours.cpp` | 固定デポのある複数の巡回路 |
| `test/tsp_test.cpp` | 操作によるコストの差分、状態の不変条件、探索結果の自動テスト |
| `test/tsp_sa_test.cpp` | `TspState` を焼きなましに組み込んだ場合の動作確認 |
| `test/atsp_test.cpp` | 非対称コストの探索と戻らない経路の自動テスト |

必要な機能のファイルだけを include してください。各ファイルは、依存するファイルを内部で include します。`tsp_initial_state.cpp` を include すると、`multiple_tours.cpp` と `atsp_iterated_local_search.cpp` を除く全ての機能を使用できます。`atsp_iterated_local_search.cpp` は `titan_cpplib/graph/hungarian.cpp` も include します。

## 計算量

点数を `n`、候補数を `c` とします。

| 処理 | 時間 | 追加メモリ |
|---|---:|---:|
| 状態の作成とコストの計算 | `O(n)` | `O(n)` |
| 最近傍法による初期解 | `O(n²)` | `O(n)` |
| 候補表 | `O(n²)` 回の `edge_cost` の呼び出し | `O(nc)` |
| 2-opt、Or-opt、Double Bridge の評価 | `O(1)` | `O(1)` |
| 2-opt の適用 | `O(right - left)` | `O(1)` |
| Or-opt の適用 | `O(移動する区間と移動先の間の長さ)` | `O(1)` |
| Double Bridge の適用 | `O(cut4 - cut1)` | State 内の `O(n)` の作業領域 |
| 探索関数の準備 | `O(nc²)` | `O(nc)` |
| 探索関数での 1 点の再評価 | `O(c)` | `O(1)` |

探索関数は、操作で辺が変化した点だけを再評価します。探索関数の内部では、区間の反転に、反転する区間とその補集合のうち短い方の長さに比例する時間がかかります。

複数巡回路で長さ `L` の区間を移動または交換する操作は、評価が `O(L)` 時間で、適用にかかる時間は関係する巡回路の長さに比例します。

## 利用条件

- C++20
- 点番号は `[0, node_count)` の範囲
- 辺のコストは有限で 0 以上
- `atsp_` で始まる関数以外では、辺のコストは対称
- `Cost` は符号付き整数または浮動小数点数
- 総コストと差分計算の中間値が `Cost` の範囲内
- 問題の寿命は候補表と状態より長くする
- 反復局所探索、誘導局所探索、比較用の探索には、時間または回数の上限を少なくとも 1 つ設定する

辺のコストが三角不等式を満たす必要はありません。辺のコストが非対称な場合は、`atsp_` で始まる関数だけを使用してください。
