# Kruskal 再構築木

`kruskal_reconstruction_tree.cpp` の `titan23::KruskalReconstructionTree<T>` は、
重み付き無向グラフの辺を重みの昇順に追加したときの連結成分の合併を木で表します。
元の頂点が葉、各合併が内部頂点になります。非連結グラフでは森になります。

## 構築と基本クエリ

```cpp
#include "titan_cpplib/graph/tree/kruskal_reconstruction_tree.cpp"

int main() {
    using Tree = titan23::KruskalReconstructionTree<long long>;
    // 無向辺は (u, v, weight) の形式とする
    // 各辺を一度だけ渡す
    std::vector<std::tuple<int, int, long long>> edges{
        {0, 1, 2}, {2, 3, 3}, {1, 2, 5}, {0, 3, 9}
    };
    Tree tree(4, edges);

    auto w = tree.merge_threshold(0, 3);  // optional<long long>{5}
    assert(w && *w == 5);
    assert(!tree.same(0, 3, 4));
    assert(tree.same(0, 3, 5));
    assert(tree.component_size(0, 3) == 2);  // {0, 1}
    assert(tree.component_size(0, 5) == 4);  // {0, 1, 2, 3}

    // 同一頂点は辺なしで連結するため、有限の合併閾値は存在しない
    assert(!tree.merge_threshold(0, 0));
    assert(tree.same(0, 0, -100));
}
```

`T` はデフォルト構築、コピー、`<` による大小比較ができる型である必要があります。既定値は `long long` です。
比較は strict weak ordering を満たす必要があり、浮動小数点の NaN は使用できません。
数値の番兵や重みの加減算は使用せず、負の重みや整数型の最小値と最大値も扱うことができます。
コンストラクタは辺列を値で受け取り、そのコピーをソートします。
辺列が不要な場合は `Tree tree(n, std::move(edges));` でコピーを省くことができます。

元の頂点の番号は `[0, N)`、合併頂点の番号は `[N, node_count())` です。
自己ループ、多重辺、同じ重みの辺、孤立点に対応します。`N=0` も指定できます。
頂点数は `0 <= N <= INT_MAX / 2`、辺の端点は `[0, N)` を満たす必要があります。
構築後の辺の追加、削除、重みの変更には対応していません。

## API

| API | 内容 | 計算量 |
| --- | --- | --- |
| `vertex_count()` | 元の頂点数 `N` | `O(1)` |
| `node_count()` | 合併頂点も含む頂点数 `2N-C`。`C` は連結成分数 | `O(1)` |
| `component_count()` | 全ての辺を使用した場合の連結成分数 | `O(1)` |
| `roots()` | 森の根の列への const 参照 | `O(1)` |
| `parent(node)` | 再構築木上の親。根では `-1` | `O(1)` |
| `children(node)` | 2 つの子。葉では `{-1, -1}` | `O(1)` |
| `merge_weight(node)` | 合併頂点に記録された重みへの const 参照。葉は指定不可 | `O(1)` |
| `lca(a,b)` | 再構築木上の LCA。内部頂点も指定可。異なる木なら `-1` | `O(log N)` |
| `same(u,v)` | 全ての辺を使用したときに元の頂点 `u,v` が連結か | `O(1)` |
| `same(u,v,x)` | 重み `<= x` の辺だけで連結か | `O(log N)` |
| `merge_threshold(u,v)` | 初めて連結になる重みを `optional<T>` で返す | `O(log N)` |
| `component_node(v,x)` | 重み `<= x` の辺で `v` が属する成分を表す頂点 | `O(log N)` |
| `component_size(v,x)` | その成分に含まれる元の頂点数。`v` 自身も含む | `O(log N)` |
| `leaf_order()` | 元の頂点を DFS 順に並べた列への const 参照 | `O(1)` |
| `leaf_index(v)` | 元の頂点 `v` の `leaf_order()` 上の位置 | `O(1)` |
| `subtree_range(node)` | 子孫の葉に対応する `leaf_order()` 上の半開区間 `[l,r)` | `O(1)` |
| `leaf_count(node)` | 子孫の葉の数 | `O(1)` |

`merge_threshold(u,v)` は同一頂点の場合と非連結の場合のどちらでも `nullopt` を返します。
同一頂点は全ての閾値で連結し、非連結の異なる頂点は全ての閾値で非連結となります。
両者は `u == v` または `same(u,v)` で区別できます。
`u != v` かつ連結の場合、戻り値は各 `u-v` 経路上の辺重みの最大値のうち最小のものです。

同じ重みの辺の処理順によって、内部頂点の番号、木の形、葉の順番は変化する場合があります。
`component_node(v,x)` は重み `<= x` を満たす最上位の祖先まで登るため、
同じ重みでの合併を全て含んだ成分を返します。

## 成分内の総和・最大値など

各部分木の葉は `leaf_order()` 上で連続します。元の頂点の値をこの順に並べると、
閾値で指定した成分の集計を累積和、Fenwick Tree、Segment Tree で処理できます。
例えば、上の構築例に次のコードを続けると成分内の総和を求めることができます。

```cpp
std::vector<long long> values{10, 20, 30, 40};
const auto &order = tree.leaf_order();
std::vector<long long> prefix(order.size() + 1, 0);
for (int i = 0; i < tree.vertex_count(); ++i) {
    prefix[i + 1] = prefix[i] + values[order[i]];
}
int node = tree.component_node(0, 3);
auto [l, r] = tree.subtree_range(node);
assert(prefix[r] - prefix[l] == 30);  // 頂点 0, 1 の値の総和
```

頂点の値を更新する場合は `leaf_index(v)` の位置を Fenwick Tree などで更新します。
また、親の番号は必ず子の番号より大きいので、頂点番号の昇順に部分木 DP を行うことができます。

## 計算量と検証

`N` を元の頂点数、`M` を辺数とします。
構築の時間計算量は `O(M log M + M alpha(N) + N log N)`、
保持する領域は `O(N log N)` です。構築時には辺列のために追加で `O(M)` の領域を使用します。
再帰を使用しないため、合併木の高さが `O(N)` になる場合も扱うことができます。

`test/graph/tree/kruskal_reconstruction_tree_random.cpp` では、
Floyd-Warshall で求めた minimax 距離および閾値ごとの BFS の結果と照合します。
次の場合も検証します。

- 空グラフ
- 非連結のグラフ
- 負の重み
- 同じ重みの辺
- 自己ループ
- 多重辺
- 型の最小値と最大値の重み
- 符号なし整数と小数の重み
- 20 万頂点のパスグラフ

```sh
g++ -std=c++17 -O2 -Wall -Wextra -I. test/graph/tree/kruskal_reconstruction_tree_random.cpp -o /tmp/kruskal_reconstruction_tree_random
/tmp/kruskal_reconstruction_tree_random
```
