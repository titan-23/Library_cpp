# Steiner木の近似解

`minimum_steiner_tree.cpp` は、非負重みの無向グラフで、指定した端子をすべて含む木を求める近似ライブラリです。厳密な最小Steiner木を求めるものではありません。

頂点数100前後で同じグラフを何度も使う場合は、既存APIの `build_prim` をまず使います。初回から軽く解きたい場合は `incremental_prim`、別の構成法も試したい場合は `mehlhorn` を選べます。[性能比較](minimum_steiner_tree_benchmark_2026-11-12.md)では、前計算と繰り返し呼び出しを分けて測っています。

## 基本の使い方

```cpp
#include <vector>
#include <utility>
#include "titan_cpplib/graph/minimum_steiner_tree.cpp"

using Weight = long long;
std::vector<std::vector<std::pair<int, Weight>>> graph(4);
auto add_edge = [&](int u, int v, Weight w) {
    graph[u].emplace_back(v, w);
    graph[v].emplace_back(u, w);
};
add_edge(0, 1, 2);
add_edge(1, 2, 3);
add_edge(1, 3, 1);

titan23::MinimumSteinerTree<Weight> solver(graph, 1LL << 60);
std::vector<int> terminals = {0, 2, 3};
auto edges = solver.build_prim(terminals); // 従来と同じ呼び出し方

auto result = solver.solve(terminals);
if (result.feasible) {
    // result.edges: 元グラフの辺 (u, v)、常に u < v
    // result.total_cost: 採用した実際の辺の重みの合計
}
```

端子が0個または重複除去後に1個なら、費用0・辺なしで成功します。通常の結果は全端子をつなぐ木で、閉路や非端子の葉は残しません。平行辺は同じ端点間の最小重みにまとめます。返すのは辺番号ではなく端点なので、入力に平行辺があれば最小重みの辺を採用したものとして扱います。

## 選べる方式

```cpp
using Method = titan23::SteinerTreeMethod;
auto cached = solver.solve(terminals, Method::cached_prim);
auto incremental = solver.solve(terminals, Method::incremental_prim);
auto voronoi = solver.solve(terminals, Method::mehlhorn);
auto pruned_mst = solver.solve(terminals, Method::mst_prune);

// 辺集合だけを返す便利関数
auto e1 = solver.build_prim_dijkstra(terminals);
auto e2 = solver.build_mehlhorn(terminals);
```

| 方式 | 主な用途 | 処理 |
|---|---|---|
| `cached_prim`（既定） | 同じグラフで端子集合を変えて繰り返し解く | 端子から全頂点への最短路を必要時にキャッシュし、現在の木に最も近い未接続端子を順に追加 |
| `incremental_prim` | 初回の待ち時間を抑える、大きいグラフで距離表を持たない | 木に追加した経路の頂点を距離0の始点としてDijkstraを更新。探索の未処理部分を再利用 |
| `mehlhorn` | 別の初期解を作る、多始点Dijkstra1回で構成する | 最近傍端子による領域を作り、領域境界の候補辺をMSTで選んで元の経路へ戻す |
| `mst_prune` | 端子が多い入力で、費用の悪化を許して反復を速くする | 全体のMSTを一度だけ作り、その中で端子間の接続に必要な辺だけを返す |

Prim系の始点は `terminals.back()` です。途中で通った端子も接続済みとして扱います。最短路の同点処理や復元方向は旧実装と異なるため、旧版と同じ辺集合・同じ費用になる保証はありません。同じ入力・同じ方式では決定的に動作します。

`incremental_prim` は全頂点を一度確定して終わる普通のDijkstraとは異なり、新しい始点の追加後に頂点を再処理します。そのため、全体の計算量をDijkstra1回分とみなしてはいけません。優先度付きキューには頂点ごとの位置を持つ二分ヒープを使い、未処理要素を最大 `n` 個に抑えています。

Mehlhorn法では、復元用の追加Dijkstraは行いません。最初の探索で保存した親辺を使い、共有する経路は一度だけ展開します。異なる領域の最短路木を領域間MSTで結ぶため、展開後も閉路は生じません。最後に不要な非端子の葉を除きます。

`mst_prune` は、固定したMST上の各部分木に含まれる端子数を集計し、その辺の両側に端子がある辺だけを残します。繰り返し呼び出しは `O(n+k)` ですが、Steiner木に対する定数近似保証はありません。例えば、100頂点の単位重みの鎖と両端を結ぶ重み2の辺があると、両端2点に対して費用99を返し、最適値2より大幅に悪化します。端子が少ないときの既定方式にはしません。

全頂点が端子ならSteiner木は通常のMSTになるため、どの方式を指定しても、キャッシュしたMSTから厳密解を返します。

## 時間を追加して品質を上げる

```cpp
// 全端子を始点として試す。距離キャッシュを共有する。
auto all_roots = solver.build_prim_fullsearch(terminals);

// Mehlhorn法と、最大8個の異なる始点からの逐次Dijkstra法を比較する。
auto best = solver.solve_best(terminals, 8, 23);

// 頂点数100前後で距離キャッシュを再利用する場合。
auto best_cached = solver.solve_best(terminals, 8, 23, Method::cached_prim);
```

`solve_best` の引数は、端子列、試すPrim系始点数、乱数の種、Prim系の方式です。Mehlhorn法を1回実行し、指定した始点数（最大で端子数）の結果と比較して費用が最小のものを返します。始点数0ならMehlhorn法だけです。最初の始点は `terminals.back()`、残りは重複なしで抽選します。

全始点探索とキャッシュ版の複数始点探索では、作成途中の木の費用が現在の最良値以上になった試行を打ち切ります。非負辺を追加するだけなので、その試行を続けても最良値を更新できないためです。これは同じ候補始点集合での最良費用を悪化させない枝刈りです。

Mehlhorn法が特に密グラフで割高になることがあります。頂点数100程度では、既存の `build_prim_fullsearch` も有力な比較対象です。複数方式の併用を常に最速の品質改善だとはみなさず、実際の端子数と時間枠で比較してください。いずれのAPIも壁時計による停止期限は受け取りません。

## キャッシュとグラフの変更

コンストラクタはグラフをコピー・正規化します。全点対のWarshall–Floydは実行しません。`cached_prim` が必要とした端子ごとにDijkstraを行い、その距離と親辺を保存します。

```cpp
int sources = solver.cached_source_count();
solver.clear_cache(); // 距離キャッシュとMSTキャッシュを解放
```

多くの異なる端子を問い合わせ続けると、最終的には全頂点分の行を持つ可能性があります。大きいグラフでは `incremental_prim` や `mehlhorn` を選ぶか、不要になったキャッシュを解放してください。これらの方式は新しい距離行をキャッシュしませんが、以前の呼び出しで作ったキャッシュを自動では消しません。

元の `graph` を変更しても `solver` は変わりません。辺の重みや利用可能な辺を変える場合は、新しいグラフからオブジェクトを作り直します。`clear_cache()` はグラフ更新APIではありません。

## 入力条件と失敗

- `T` は `bool` を除く算術型。整数型、`double` などを使う
- 頂点番号は `[0, n)`。頂点数と正規化後の辺数は `int` に収まる
- 辺重みは有限、非負、`INF` 未満。`INF` 自体も正で有限
- 各辺を両方向へ登録し、方向ごとの最小重みを一致させる。非対称なら例外
- 自己ループは無視する。端子の重複は除去する
- 利用する最短路、Mehlhorn法の境界候補 `dist[u] + w(u,v) + dist[v]`、返却する木の総費用が `INF` 未満に収まる値域を使う

最後の条件を満たす簡単な十分条件は、`INF` を `(2n-1) × 最大辺重み` より大きくすることです。`n=100`、普通の整数コストなら `long long` と十分大きな `INF` を使えば扱いやすくなります。

加算は `INF` で飽和させ、整数オーバーフローを避けます。端子が同じ連結成分にあるのに必要な費用を表現できない場合は `overflow_error` にします。浮動小数点型では通常の丸め誤差があります。

端子が異なる連結成分にある場合、`solve` と `solve_best` は `feasible=false`、空の辺集合、費用0を返します。この費用0を実行可能解として比較してはいけません。辺集合だけを返す `build_*` は `runtime_error` を投げます。旧版が未接続端子を残した部分解を返していた動作は修正しました。

## 計算量

`n` を頂点数、`m` を入力の無向辺数（平行辺を含む）、`k` を重複除去後の端子数、`q` をキャッシュ済み始点数とします。

| 処理 | 時間の上界 | メモリ |
|---|---:|---:|
| 構築・正規化 | `O(n + m log(m+1))` | `O(n+m)` |
| 新しい端子1個の距離キャッシュ | `O((n+m) log(n+1))` | 行ごとに `O(n)` |
| キャッシュ済みの `build_prim` | `O(nk + k²)` | オブジェクト全体で `O(n+m+qn)` |
| `incremental_prim` | `O(k(n+m) log(n+1))` | キャッシュを除き `O(n+m)` |
| `mehlhorn` | `O((n+m) log(n+1) + m log(m+1))` | キャッシュを除き `O(n+m)` |
| `mst_prune` | 初回 `O(n + m log(m+1))`、以後 `O(n+k)` | 距離キャッシュを除き `O(n+m)` |
| 全始点探索 | キャッシュ済みなら `O(nk²+k³)` | キャッシュ版と同じ |

これらは保守的な上界です。逐次Dijkstraは最後の端子へ接続した時点で止まり、実際にはグラフ全体を端子数回探索するとは限りません。キャッシュ版の距離更新も、今回新たに木へ入った頂点だけを使います。

## 別方式をさらに検討する場合

今回実装したMehlhorn法は、多始点最短路とMSTによる別の構成法です。原論文には2未満の近似比が示されていますが、AHCでの実際の費用が逐次接続法より良くなる保証はありません。[Mehlhorn, 1988](https://doi.org/10.1016/0020-0190(88)90066-X)

より長い時間を品質改善に使えるなら、Steiner頂点の挿入・除去や、分岐点間の経路交換が候補になります。これらの近傍を効率よく探索する研究がありますが、今回の実装には含めていません。まず始点探索で得られる品質と比較し、その先の改善が必要な場合に追加する方針です。[Uchoa and Werneck, 2012](https://doi.org/10.1145/2133803.2184448)

端子がごく少ない場合には端子部分集合DPによる厳密解も別の選択肢です。ただし端子数に対して指数的に増えるため、汎用の近似APIへ自動的に組み込むことはしていません。
