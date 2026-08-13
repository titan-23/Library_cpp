# AHC向けTSPライブラリ

対称TSPの巡回路状態、近傍操作、局所探索、誘導局所探索を提供する。末尾に開始点を重ねず、最後の点から先頭の点への辺を自動で数える。

## ファイル

| ファイル | 内容 |
|---|---|
| `tsp.cpp` | `TspProblem`、`TspCandidates`、`TspState`、最近傍初期解 |
| `tsp_symmetric_moves.cpp` | 2-opt、Or-opt、反転Or-opt、Double Bridge |
| `tsp_local_search.cpp` | 2-opt局所探索 |
| `tsp_guided_local_search.cpp` | 標準的な誘導局所探索 |
| `tsp_edge_penalty_search.cpp` | 旧 `tsp2.py` と同じ処理順の比較用探索 |
| `tsp_initial_state.cpp` | SA初期状態の改善方法を選ぶ補助関数 |
| `multiple_tours.cpp` | 固定デポを持つ複数巡回路 |
| `test/tsp_test.cpp` | 差分、状態の不変条件、探索結果の自動確認 |
| `test/tsp_sa_test.cpp` | `TspState` を焼きなましへ組み込む確認 |

必要な機能のファイルだけを読み込む。後ろのファイルは、表で上にある必要なファイルを内部で読み込む。

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

`TspProblem` は辺費用関数を型のまま保持する。距離行列を参照するラムダも使えるが、参照先は `TspProblem` より長く生存させる。

## TspState

```cpp
auto state = problem.make_state(order);
auto all_nodes = problem.make_nearest_neighbor_state(start);
auto subset = problem.make_nearest_neighbor_state(
    start, nodes_to_visit);
```

- `order()`: 現在の巡回順
- `position(node)`: 巡回路内の位置。部分巡回路に含まれない点は `-1`
- `total_cost()`: 現在の閉路費用
- `reset(problem, order)`: 巡回順全体を置き換える

巡回順、逆引き位置、費用は常に一緒に更新される。`order()[0]` は近傍操作後も変わらない。

点が1個だけの巡回路費用は0で、`edge_cost(v, v)` は呼ばない。点が2個なら往路と復路の2辺を数える。

## 近傍操作

操作の作成時には状態を変更せず、採択したときだけ `apply` する。

```cpp
auto move = state.make_two_opt(problem, left, right);
if (move) {
    auto next_cost = state.total_cost() + move->delta();
    if (accept(next_cost)) state.apply(problem, *move);
}
```

- 2-opt: `[left, right]` を反転する。`left >= 1`
- Or-opt: `[first, first+length)` を、変更前の `after` の直後へ移す
- 反転Or-opt: `make_or_opt(..., true)`
- Double Bridge: `cut1 < cut2 < cut3 < cut4` の4辺を切る。各 `cut` は削除する辺の始点位置

操作は作成元の問題、状態、版番号を保持する。別の状態から作った操作や、状態変更前に作った古い操作は適用できない。

状態のコピーは独立した確定状態を作る。コピー元で作った操作をコピー先へ適用することはできない。状態を代入した場合も、代入前に作った操作は無効になる。

## 2-opt局所探索

```cpp
TspLocalSearchOptions options;
options.max_evaluated_moves = 200000;
options.first_improvement = true;
auto result = tsp_two_opt_local_search(
    problem, candidates, state, options);
```

候補表を渡さない版は全ての2-optを調べる。候補表を渡す版は各候補点の前後辺を調べる。候補数を `n-1` にすると全探索と同じ2-opt集合を調べる。

`first_improvement = false` では、一巡中で費用差が最小の手を適用する。評価上限で止まった場合は `locally_optimal == false` になる。

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

真の費用で候補内局所最適まで進めた後、評価値

```text
edge_cost(u,v) / (1 + penalty(u,v))
```

が最大の現在辺へ罰則を加え、罰則付き費用で再び候補内局所最適まで進める。返却時の `state` は探索中に見つけた真の費用が最小の巡回路である。

時間または罰則周回数の少なくとも一方へ上限を設定する。候補評価数上限は、一周の途中で計算を打ち切る補助上限として併用できる。罰則は探索関数内の疎な表にだけ保存し、`TspState` には残さない。

## tsp2.py比較版

```cpp
TspEdgePenaltySearchOptions options;
options.max_penalty_rounds = 10000;
options.max_evaluated_moves = 2000000;
options.time_limit_ms = 100;
auto result = tsp_edge_penalty_search(
    problem, candidates, state, options);
```

一周ごとに評価値最大の辺を1本だけ罰し、その辺を外す候補から最初の改善2-optを最大1回適用する。初期係数は0.3、最良更新後は0.1で再計算する。標準的な誘導局所探索とは別の探索である。

密な `n*n` 罰則行列は使わないが、元の処理と同じ辺選択、候補順、採択条件になる。

## SA初期状態

```cpp
TspInitialStateOptions options;
options.search = TspInitialSearch::guided_local_search;
options.guided_local_search.time_limit_ms = 50;
options.guided_local_search.max_evaluated_moves = 500000;
auto result = improve_tsp_initial_state(
    problem, candidates, state, options);
```

選べる方法は次の4種類。

- `none`: 改善しない
- `two_opt`: 通常の2-opt局所探索
- `guided_local_search`: 標準的な誘導局所探索
- `edge_penalty_search`: `tsp2.py` 比較版

既定は `none`。誘導局所探索は初期費用を下げる一方でSA本体の時間を減らすため、小さい上限から比較する。候補表は初期状態の改善とSAの近傍選択で共有する。

焼きなましへ渡すときは、初期巡回路と任意の改善を組み立ててから、初期化済みの状態を渡す。

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

`initial_options.search = TspInitialSearch::none` なら初期改善を行わない。上の例では状態構築時間を全体の1900msから引いている。ただし状態構築そのものは途中で中断しないため、重い初期探索には個別の時間または評価回数上限を設定する。

SA側の `State` は、確定済みの `TspState` と未適用の操作を一つ持つ。`modify` では費用差だけを `score` に反映し、`advance` で `TspState::apply`、`rollback` で未適用操作の破棄を行う。反復の境界では未適用操作を残さない。

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

各巡回路の0番目は固定デポで、全点を全巡回路を通じて一度ずつ含める。顧客0点でデポだけの巡回路も許す。

```cpp
auto intra = state.make_two_opt(
    problem, route, left, right);
auto shift = state.make_block_shift(
    problem, source, first, length, target, after);
auto swap = state.make_block_swap(
    problem, route1, first1, length1,
    route2, first2, length2);
```

区間移動と区間交換は、影響する各巡回路の費用差を別々に返す。総距離、距離の二乗和、最大距離などの目的関数は利用側で選ぶ。

## 計算量

点数を `n`、候補数を `c`、適用手数を `A`、反転区間長を `s_i` とする。

| 処理 | 時間 | 追加メモリ |
|---|---:|---:|
| 状態作成・費用計算 | `O(n)` | `O(n)` |
| 最近傍初期解 | `O(n²)` | `O(n)` |
| 候補表 | 平均 `O(n² + nc log(c+1))` | `O(nc+n)` |
| 2-opt評価 | `O(1)` | `O(1)` |
| 2-opt適用 | `O(s_i)` | `O(1)` |
| Or-opt評価 | `O(1)` | `O(1)` |
| Or-opt適用 | `O(n)` | `O(1)` |
| Double Bridge評価 | `O(1)` | `O(1)` |
| Double Bridge適用 | `O(n)` | State内の `O(n)` 作業領域 |
| 全2-optの一巡 | `O(n²)` | `O(1)` |
| 候補制限付き一巡 | `O(n(c+1))` | `O(1)` |

候補制限付き局所探索の単純な上界は `O((A+1)n(c+1) + Σs_i)`。

誘導局所探索は、有効な2-optの評価数を `Q`、候補表の参照数を `H`、罰則周回数を `R`、最良更新数を `B`、罰した異なる辺数を `P` として、候補表を除き平均 `O(H + Q + Σs_i + Rn + (B+1)n)` 時間、`O(n+P)` 追加メモリ。`H` には隣接辺や部分巡回路の外にある候補を読み飛ばす処理も含む。

`tsp2.py` 比較版も候補表の参照数を `H` として、平均 `O(Rn + H + Q + Σs_i + (B+1)n)` 時間、`O(n+P)` 追加メモリ。

複数巡回路の長さ `L` の区間移動・交換は評価 `O(L)`、適用は関係する巡回路長に比例する。長さを1～3へ制限すれば評価は定数時間になる。

## 利用条件

- C++20
- 点番号は `[0, node_count)`
- 辺費用は有限、0以上、対称
- `Cost` は符号付き整数または浮動小数
- 全費用と差分計算の中間値が `Cost` の範囲内
- 問題は候補表と状態より長く生存させる
- 罰則探索では時間または罰則周回数の上限を有効にする

三角不等式は不要。非対称TSPには対応しない。
