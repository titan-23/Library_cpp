# AHC 向け TSP ライブラリ

対称 TSP の巡回路の状態、近傍操作、局所探索、誘導局所探索を提供します。巡回順の末尾に開始点を再び含めず、末尾の点から先頭の点への辺は自動的にコストに加えます。

## ファイル

| ファイル | 内容 |
|---|---|
| `tsp.cpp` | `TspProblem`、`TspCandidates`、`TspState`、最近傍法による初期解 |
| `tsp_symmetric_moves.cpp` | 2-opt、Or-opt、反転 Or-opt、Double Bridge |
| `tsp_local_search.cpp` | 2-opt 局所探索 |
| `tsp_guided_local_search.cpp` | 標準的な誘導局所探索 |
| `tsp_edge_penalty_search.cpp` | 旧 `tsp2.py` と同じ処理順で動作する比較用の探索 |
| `tsp_initial_state.cpp` | SA の初期状態の改善方法を選択する補助関数 |
| `multiple_tours.cpp` | 固定デポのある複数の巡回路 |
| `test/tsp_test.cpp` | 操作によるコストの差分、状態の不変条件、探索結果の自動テスト |
| `test/tsp_sa_test.cpp` | `TspState` を焼きなましに組み込んだ場合の動作確認 |

必要な機能のファイルだけを include してください。表で下にあるファイルは、それより上にある必要なファイルを内部で include します。

## 基本例

```cpp
#include <bits/stdc++.h>
#include "titan_cpplib/ahc/tsp/tsp_initial_state.cpp"
using namespace std;
using namespace titan23;

int main() {
    vector<pair<int, int>> points = {{0, 0}, {4, 0}, {5, 3}, {0, 4}};
    auto edge_cost = [&](int u, int v) -> long long {
        long long dx = points[u].first - points[v].first;
        long long dy = points[u].second - points[v].second;
        return dx * dx + dy * dy;
    };
    TspProblem problem((int)points.size(), edge_cost);
    auto candidates = make_tsp_candidates(problem, 20);
    auto state = problem.make_nearest_neighbor_state(0);
    TspLocalSearchOptions options;
    options.max_evaluated_moves = 100000;
    auto result = tsp_two_opt_local_search(
        problem, candidates, state, options);
    for (int node : state.order()) cout << node << ' ';
    cout << '\n' << state.total_cost() << '\n';
}
```

`TspProblem` は辺のコストを返す関数を、その型のまま保持します。距離行列を参照するラムダも使用できますが、参照先の寿命は `TspProblem` より長くする必要があります。

## TspState

```cpp
auto state = problem.make_state(order);
auto all_nodes = problem.make_nearest_neighbor_state(start);
auto subset = problem.make_nearest_neighbor_state(
    start, nodes_to_visit);
```

- `order()`: 現在の巡回順
- `position(node)`: 巡回路内の位置で、部分巡回路に含まれない点では `-1`
- `total_cost()`: 現在の巡回路のコスト
- `reset(problem, order)`: 巡回順全体を置き換える

巡回順、各点の位置、コストは常に同時に更新されます。`order()[0]` は近傍操作を適用しても変化しません。

点が 1 個だけの巡回路のコストは 0 で、`edge_cost(v, v)` は呼び出しません。点が 2 個の場合は往路と復路の 2 辺をコストに加えます。

## 近傍操作

操作を作成しても状態は変更されません。採択した場合だけ `apply` を呼び出します。

```cpp
auto move = state.make_two_opt(problem, left, right);
if (move) {
    auto next_cost = state.total_cost() + move->delta();
    if (accept(next_cost)) state.apply(problem, *move);
}
```

- 2-opt: 区間 `[left, right]` を反転する操作 (`left >= 1`)
- Or-opt: 区間 `[first, first+length)` を変更前の位置 `after` の直後へ移動する操作
- 反転 Or-opt: `make_or_opt(..., true)`
- Double Bridge: `cut1 < cut2 < cut3 < cut4` の 4 辺を切る操作で、各 `cut` は削除する辺の始点の位置

操作は作成元の問題、状態、リビジョン番号を保持します。別の状態から作成した操作と、状態を変更する前に作成した古い操作は適用できません。

状態をコピーすると、コピー元から独立した確定済みの状態が作成されます。コピー元で作成した操作をコピー先に適用することはできません。状態を代入した場合も、代入前に作成した操作は無効になります。

## 2-opt 局所探索

```cpp
TspLocalSearchOptions options;
options.max_evaluated_moves = 200000;
options.first_improvement = true;
auto result = tsp_two_opt_local_search(
    problem, candidates, state, options);
```

候補表を渡さないオーバーロードはすべての 2-opt を調べ、候補表を渡すオーバーロードは各候補点の前後の辺を調べます。候補数を `n-1` にすると、調べる 2-opt の集合は全探索と同じになります。

`first_improvement = false` の場合は、一巡のうちコストの差分が最小の操作を適用します。評価回数の上限に達して探索を終了した場合は `locally_optimal == false` になります。

## 標準的な誘導局所探索

```cpp
TspGuidedLocalSearchOptions options;
options.max_penalty_rounds = 1000;
options.max_evaluated_moves = 2000000;
options.time_limit_ms = 100;
options.penalty_ratio = 0.3L;
auto result = tsp_guided_local_search(
    problem, candidates, state, options);
```

まず、ペナルティを含まないコストで、候補表で制限した近傍の局所最適解になるまで探索します。その後、現在の巡回路の辺のうち次の評価値が最大の辺にペナルティを加えます。

```text
edge_cost(u,v) / (1 + penalty(u,v))
```

ペナルティを加えたコストで、再び候補表で制限した近傍の局所最適解になるまで探索します。返却時の `state` は、探索中に見つけた巡回路のうちペナルティを含まないコストが最小のものです。

時間とペナルティの更新回数の少なくとも一方に上限を設定してください。候補の評価回数の上限は、一周の途中で計算を打ち切る補助的な上限として併用できます。ペナルティは探索関数の内部の疎な表にだけ保存し、`TspState` には残しません。

## tsp2.py との比較用の探索

```cpp
TspEdgePenaltySearchOptions options;
options.max_penalty_rounds = 10000;
options.max_evaluated_moves = 2000000;
options.time_limit_ms = 100;
auto result = tsp_edge_penalty_search(
    problem, candidates, state, options);
```

一周ごとに評価値が最大の辺を 1 本だけ選んでペナルティを加え、その辺を外す候補のうち改善となる最初の 2-opt を最大 1 回適用します。ペナルティの係数は最初に 0.3 で計算し、最良解を更新した後は 0.1 で再計算します。標準的な誘導局所探索とは異なる探索です。

密な `n*n` のペナルティ行列は使用しませんが、辺の選択、候補の順序、採択条件は元の処理と同じです。

## SA の初期状態

```cpp
TspInitialStateOptions options;
options.search = TspInitialSearch::guided_local_search;
options.guided_local_search.time_limit_ms = 50;
options.guided_local_search.max_evaluated_moves = 500000;
auto result = improve_tsp_initial_state(
    problem, candidates, state, options);
```

選択できる方法は次の 4 種類です。

- `none`: 改善しない
- `two_opt`: 通常の 2-opt 局所探索
- `guided_local_search`: 標準的な誘導局所探索
- `edge_penalty_search`: `tsp2.py` との比較用の探索

既定値は `none` です。誘導局所探索は初期解のコストを下げる一方で、SA 本体に使用できる時間を減らします。そのため、上限は小さい値から試して比較するようにします。候補表は初期状態の改善と SA の近傍の選択で共有します。

焼きなましに渡す場合は、初期巡回路を作成し、必要であれば改善してから、初期化済みの状態を渡します。

```cpp
#include "titan_cpplib/ahc/sa/sa.cpp"

Timer timer;
auto tsp = problem.make_nearest_neighbor_state(0);
improve_tsp_initial_state(
    problem, candidates, tsp, initial_options);
State state(problem, candidates, move(tsp), 23);
double remaining = max(0.0, 1900.0 - timer.elapsed());
auto result = sa::sa_run<State>(remaining, state, true);
```

`initial_options.search = TspInitialSearch::none` の場合は初期状態を改善しません。上の例では、状態の構築にかかった時間を全体の 1900 ms から引いています。ただし、状態の構築は途中で中断しないため、時間のかかる初期探索には個別に時間または評価回数の上限を設定してください。

SA 側の `State` は、確定済みの `TspState` と未適用の操作 1 つを保持します。`modify` ではコストの差分だけを `score` に反映します。`advance` では `TspState::apply` を呼び出し、`rollback` では未適用の操作を破棄します。反復の境界では未適用の操作を残しません。

## 複数巡回路

```cpp
#include "titan_cpplib/ahc/tsp/multiple_tours.cpp"

vector<int> depots = {0, 1};
vector<vector<int>> routes = {
    {0, 2, 3},
    {1, 4, 5},
};
auto state = make_multiple_tsp_state(
    problem, depots, routes);
```

各巡回路の 0 番目は固定デポです。全巡回路を通じて、すべての点を一度ずつ含める必要があります。顧客が 0 点でデポだけからなる巡回路も許容します。

```cpp
auto intra = state.make_two_opt(
    problem, route, left, right);
auto shift = state.make_block_shift(
    problem, source, first, length, target, after);
auto swap = state.make_block_swap(
    problem, route1, first1, length1,
    route2, first2, length2);
```

区間の移動と交換は、影響を受ける各巡回路のコストの差分を個別に返します。総距離、距離の二乗和、最大距離などの目的関数は利用側で選択します。

## 計算量

点数を `n`、候補数を `c`、適用した操作の数を `A`、反転区間の長さを `s_i` とします。

| 処理 | 時間 | 追加メモリ |
|---|---:|---:|
| 状態の作成とコストの計算 | `O(n)` | `O(n)` |
| 最近傍法による初期解 | `O(n²)` | `O(n)` |
| 候補表 | 平均 `O(n² + nc log(c+1))` | `O(nc+n)` |
| 2-opt の評価 | `O(1)` | `O(1)` |
| 2-opt の適用 | `O(s_i)` | `O(1)` |
| Or-opt の評価 | `O(1)` | `O(1)` |
| Or-opt の適用 | `O(n)` | `O(1)` |
| Double Bridge の評価 | `O(1)` | `O(1)` |
| Double Bridge の適用 | `O(n)` | State 内の `O(n)` の作業領域 |
| 全 2-opt の一巡 | `O(n²)` | `O(1)` |
| 候補表で制限した一巡 | `O(n(c+1))` | `O(1)` |

候補表で制限した局所探索の時間計算量の単純な上界は `O((A+1)n(c+1) + Σs_i)` です。

有効な 2-opt の評価数を `Q`、候補表の参照回数を `H`、ペナルティの更新回数を `R`、最良解の更新回数を `B`、ペナルティを加えた異なる辺の数を `P` とします。誘導局所探索の計算量は、候補表を除いて平均 `O(H + Q + Σs_i + Rn + (B+1)n)` 時間、追加メモリ `O(n+P)` です。`H` には隣接辺や部分巡回路の外にある候補をスキップする処理も含みます。

`tsp2.py` との比較用の探索の計算量は、同様に候補表の参照回数を `H` とすると、平均 `O(Rn + H + Q + Σs_i + (B+1)n)` 時間、追加メモリ `O(n+P)` です。

複数巡回路で長さ `L` の区間を移動または交換する操作は、評価が `O(L)` 時間で、適用にかかる時間は関係する巡回路の長さに比例します。長さを 1～3 に制限すると、評価は定数時間になります。

## 利用条件

- C++20
- 点番号は `[0, node_count)` の範囲
- 辺のコストは有限、0 以上、対称
- `Cost` は符号付き整数または浮動小数点数
- 総コストと差分計算の中間値が `Cost` の範囲内
- 問題の寿命は候補表と状態より長くする
- ペナルティを使用する探索では、時間またはペナルティの更新回数の上限を有効にする

辺のコストが三角不等式を満たす必要はありません。非対称 TSP には対応していません。
