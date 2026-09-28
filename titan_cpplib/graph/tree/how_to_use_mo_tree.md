# 木上の Mo

静的な無向木のパスクエリをオフラインで処理する
`alg/mo.cpp` の `titan23::Mo` を使用し、頂点用・頂点と辺の両方を扱う版では
`graph/tree/offline_lca.cpp` で LCA を一括計算する

| include（`titan_cpplib/graph/tree/` 以下） | クラス | 対象 |
| --- | --- | --- |
| `mo_tree_vertex.cpp` | `titan23::MoTreeVertex` | 両端点を含むパス上の頂点 |
| `mo_tree_edge.cpp` | `titan23::MoTreeEdge` | パス上の辺 |
| `mo_tree_vertex_edge.cpp` | `titan23::MoTreeVertexEdge` | パス上の頂点と辺 |

属性値は利用側の配列で保持し、コールバックには頂点番号・辺番号を渡す

## 構築とクエリ登録

3 クラスとも頂点数と根（省略時は 0）を渡し、辺を登録できる

```cpp
titan23::MoTreeVertex mo(n, root);
for (auto [u, v] : edges) {
    int e = mo.add_edge(u, v);  // 登録順の辺番号
}
int i = mo.add_query(u, v);  // 登録順のクエリ番号
```

辺番号は登録順に 0, 1, ... と付く
頂点数 1 以上の連結な無向木を登録し、最初の `add_query()` または `run()` で Euler walk を構築する
以降の辺追加は不可

隣接リストからの `mo(G, root)` も利用でき、Euler walk はコンストラクタで構築する
`G` は各辺を両方向に含める
この場合の辺番号は `u` の昇順に `G[u]` を走査し、`u < v` の辺を取った順

頂点を扱う版は `run()` の最初に全クエリの LCA を計算する

## 頂点属性

頂点 `v` の色を `color[v]` とし、パス上の色の種類数を求める例

```cpp
#include "titan_cpplib/graph/tree/mo_tree_vertex.cpp"

titan23::MoTreeVertex mo(G);
for (auto [u, v] : queries) mo.add_query(u, v);

vector<int> count(color_count, 0), ans(queries.size());
int distinct = 0;
mo.run(
    [&] (int v) { if (count[color[v]]++ == 0) ++distinct; },
    [&] (int v) { if (--count[color[v]] == 0) --distinct; },
    [&] (int i) { ans[i] = distinct; }
);
```

LCA の頂点は回答直前に一時的に追加され、回答直後に削除される
`add_query(u, u)` では頂点 `u` だけが含まれる

色ごとの出現回数が `[lo[i], hi[i]]` に入る色の数を求める場合は、
加算専用の平方分割 `SqrtSegmentTreeSum` と組み合わせられる
次は上の `run()` を置き換える例で、`count` はすべて 0 から始める

```cpp
#include "titan_cpplib/ds/sqrt_segment_tree_sum.cpp"

titan23::SqrtSegmentTreeSum<int> freq(n + 1);
freq.set(0, color_count);
auto change = [&] (int v, int delta) {
    int &c = count[color[v]];
    freq.add(c, -1);
    c += delta;
    freq.add(c, 1);
};
mo.run(
    [&] (int v) { change(v, 1); },
    [&] (int v) { change(v, -1); },
    [&] (int i) { ans[i] = freq.prod(lo[i], hi[i] + 1); }
);
```

`0 <= lo[i] <= hi[i] <= n` とする
各追加・削除は `O(1)`、回答は `O(sqrt(N))`
出現回数 0 も数える場合、対象は `color_count` 個の色全体となる

## 辺属性

`add_edge()` が返す辺番号に重みを対応させる

```cpp
#include "titan_cpplib/graph/tree/mo_tree_edge.cpp"

titan23::MoTreeEdge mo(n, root);
vector<long long> weight(n - 1);
for (auto [u, v, w] : edges) {
    int e = mo.add_edge(u, v);
    weight[e] = w;
}
for (auto [u, v] : queries) mo.add_query(u, v);

long long sum = 0;
vector<long long> ans(queries.size());
mo.run(
    [&] (int e) { sum += weight[e]; },
    [&] (int e) { sum -= weight[e]; },
    [&] (int i) { ans[i] = sum; }
);
```

`add_query(u, u)` では辺の集合は空
辺用は LCA を前計算しない

## 頂点属性と辺属性の両方

`MoTreeVertexEdge` は 1 回の Mo で頂点と辺を処理する

```cpp
titan23::MoTreeVertexEdge mo(G, root);
for (auto [u, v] : queries) mo.add_query(u, v);
mo.run(add_vertex, del_vertex, add_edge, del_edge, out);
```

`add_vertex(v)` / `del_vertex(v)` には頂点番号、
`add_edge(e)` / `del_edge(e)` には辺番号が渡される
`out(i)` の時点で両方の集計がクエリのパスと一致する
LCA を補うときは頂点用のコールバックだけを呼ぶ

## 実行時の条件と計算量

- 属性値は `run()` 中は固定し、追加・削除の順序に依存しない集計を使う
- `out(i)` の呼び出し順は任意なので、回答は番号 `i` で保存する
- `run()` の前に利用側の集計状態を空に初期化する（終了時は空とは限らない）
- `N = 1`、`Q = 0`、同じ頂点間のクエリ、重複クエリに対応する
- Euler walk の構築は非再帰で `O(N)`
- 頂点を扱う版は各 `run()` で LCA を一括計算し、`O((N+Q)α(N))` が加わる
- `Q >= 1` で追加・削除は合計 `O(N sqrt(Q) + Q)` 回程度、ソートは `O(Q log(Q))`
- 空間計算量は `O(N + Q)`
