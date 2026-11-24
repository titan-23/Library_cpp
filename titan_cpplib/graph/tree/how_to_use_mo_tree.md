# 木上の Mo

木上の Mo の各クラスは、静的な無向木のパスクエリをオフラインで処理します。
`alg/mo.cpp` の `titan23::Mo` を使用します。頂点を扱う版と、頂点と辺の両方を扱う版では
`graph/tree/offline_lca.cpp` で LCA を一括計算します。

| include (`titan_cpplib/graph/tree/` 以下) | クラス | 対象 |
| --- | --- | --- |
| `mo_tree_vertex.cpp` | `titan23::MoTreeVertex` | 両端点を含むパス上の頂点 |
| `mo_tree_vertex_value.cpp` | `titan23::MoTreeVertexValue<T>` | パス上の頂点値 |
| `mo_tree_edge.cpp` | `titan23::MoTreeEdge` | パス上の辺 |
| `mo_tree_vertex_edge.cpp` | `titan23::MoTreeVertexEdge` | パス上の頂点と辺 |

`MoTreeVertexValue` は頂点の値を保持し、コールバックにも値を渡します。
ほかのクラスは頂点番号や辺番号を渡します。

## 構築とクエリ登録

番号を渡す 3 クラスは、頂点数と根 (省略時は 0) を渡して構築し、辺を登録できます。

```cpp
titan23::MoTreeVertex mo(n, root);
for (auto [u, v] : edges) {
    int e = mo.add_edge(u, v);  // 登録順の辺番号
}
int i = mo.add_query(u, v);  // 登録順のクエリ番号
```

辺番号は登録順に 0, 1, ... と割り当てられます。
登録する木は頂点数 1 以上の連結な無向木とします。最初の `add_query()` または `run()` で Euler walk を構築します。
それ以降は辺を追加できません。

隣接リストを渡す `mo(G, root)` も利用でき、Euler walk はコンストラクタで構築します。
`G` には各辺を両方向に含めます。
この場合の辺番号は、`u` の昇順に `G[u]` を走査して `u < v` の辺を取り出した順です。

番号を渡す 3 クラスには `vector<vector<pair<int, T>>>` の隣接リストも渡すことができます。
ペアの先頭を隣接頂点として使用し、重みは参照しません。
`MoTreeEdge` のコールバックには、この場合も辺番号を渡します。

頂点を扱う版は `run()` の最初に全クエリの LCA を計算します。

## 頂点属性

次は、頂点 `v` の色を `color[v]` として、パス上の色の種類数を求める例です。

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

LCA の頂点は回答の直前に一時的に追加され、回答の直後に削除されます。
`add_query(u, u)` のパスには頂点 `u` だけが含まれます。

色ごとの出現回数が `[lo[i], hi[i]]` に入る色の数を求める場合は、
加算専用の平方分割 `SqrtSegmentTreeSum` と組み合わせることができます。
次は上の `run()` を置き換える例です。`count` の初期値はすべて 0 とします。

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

`0 <= lo[i] <= hi[i] <= n` とします。
各追加と削除は `O(1)`、回答は `O(sqrt(N))` です。
出現回数 0 の色も集計に含める場合は、`color_count` 個の色全体が対象となります。

## 頂点値を直接渡す

`MoTreeVertexValue` は構築時に頂点の値をコピーします。
各 `run()` で Euler walk の各位置に値をコピーし、端点の移動時に `color[v]` を参照する処理を省きます。

```cpp
#include "titan_cpplib/graph/tree/mo_tree_vertex_value.cpp"

titan23::MoTreeVertexValue<int> mo(G, color, root);
for (auto [u, v] : queries) mo.add_query(u, v);

vector<int> count(color_count, 0), ans(queries.size());
int distinct = 0;
mo.run(
    [&] (int x) { if (count[x]++ == 0) ++distinct; },
    [&] (int x) { if (--count[x] == 0) --distinct; },
    [&] (int i) { ans[i] = distinct; }
);
```

`mo(color, root)` で初期化して `add_edge(u, v)` で辺を登録することもできます。
頂点数は `color.size()` とし、`G` を渡す場合も頂点数を合わせます。
値が同じ頂点もそれぞれ追加、削除されます。
値のコピーにかかる時間と空間は `O(N)` です。ここで、各値のコピーは `O(1)` とします。

## 辺属性

`add_edge()` が返す辺番号に重みを対応させます。

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

`add_query(u, u)` では辺の集合は空です。
辺だけを扱う版は LCA を前計算しません。

## 頂点属性と辺属性の両方

`MoTreeVertexEdge` は 1 回の Mo で頂点と辺を処理します。

```cpp
titan23::MoTreeVertexEdge mo(G, root);
for (auto [u, v] : queries) mo.add_query(u, v);
mo.run(add_vertex, del_vertex, add_edge, del_edge, out);
```

`add_vertex(v)` / `del_vertex(v)` には頂点番号、
`add_edge(e)` / `del_edge(e)` には辺番号が渡されます。
`out(i)` の時点では、頂点と辺の両方の集計がクエリのパスと一致します。
LCA を補うときは頂点用のコールバックだけを呼び出します。

## 実行時の条件と計算量

- `run()` の実行中は属性値を固定し、追加と削除の順序に依存しない集計を使用します。
- `out(i)` の呼び出し順は定まっていないため、回答は番号 `i` に対応する位置に保存します。
- `run()` の前に利用側の集計状態を空に初期化します。終了時の集計状態は空とは限りません。
- `N = 1`、`Q = 0`、同じ頂点間のクエリ、重複クエリに対応します。
- Euler walk の構築は非再帰で、計算量は `O(N)` です。
- 頂点を扱う版は各 `run()` で LCA を一括計算し、`O((N+Q)α(N))` が加わります。
- `Q >= 1` の場合、追加と削除は合計 `O(N sqrt(Q) + Q)` 回程度で、ソートは `O(Q log(Q))` です。
- 空間計算量は `O(N + Q)` です。
