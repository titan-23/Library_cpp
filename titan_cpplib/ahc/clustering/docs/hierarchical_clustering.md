# 階層型クラスタリング

## 概要

`hierarchical_clustering.cpp` は、各点を一つのクラスタとして開始し、近い二クラスタを順に統合する。
クラスタ数を事前に決めなくても統合木を一度作れば、後から任意のクラスタ数または統合距離で切れる。

二次元整数座標の単連結法だけが必要な場合は、`euclidean_single_linkage.cpp` を使うと時間とメモリを `O(n log n)` と `O(n)` に減らせる。

対応する方式は次の4種類である。

| 設定 | クラスタ間距離 | 傾向 |
|---|---|---|
| `single` | 最も近い点対の距離 | 細長くつながるクラスタを作りやすい |
| `complete` | 最も遠い点対の距離 | 直径の小さいまとまりを作りやすい |
| `average` | 全点対距離の平均 | 単連結法と完全連結法の中間 |
| `ward` | 統合による平方誤差の増え方 | K-meansに近い丸いクラスタを作りやすい |

AHCでは、クラスタ数の候補を一度に作る、近い点を段階的にまとめる、SAの初期解を複数の粗さで用意する用途に向く。

## 使用例

```cpp
#include <bits/stdc++.h>
#include "titan_cpplib/ahc/clustering/hierarchical_clustering.cpp"
using namespace std;
using Point = pair<double, double>;

vector<Point> points = {{0, 0}, {1, 0}, {10, 0}, {11, 0}};
auto distance = [](const Point& a, const Point& b) {
    return hypot(a.first - b.first, a.second - b.second);
};
auto result = titan23::hierarchical_clustering(
    points, distance, titan23::HierarchicalLinkage::ward);
vector<int> labels = result.labels(2);
vector<vector<int>> clusters = result.clusters(2);
```

点番号から直接距離を求める場合は、点配列を持たずに使える。

```cpp
auto result = titan23::hierarchical_clustering_by_index(
    n,
    [&](int a, int b) { return distance_matrix[a][b]; },
    titan23::HierarchicalLinkage::average);
```

## 結果

`result.merges[i]` は、`n+i` 番の新しいクラスタを作った統合を表す。

```cpp
merge.left       // 左の子。点なら [0,n)、統合後のクラスタなら [n,2n-1)
merge.right      // 右の子
merge.distance   // この統合が起きた距離
merge.size       // 統合後の点数
```

`labels(k)` は `k` クラスタへ切った所属番号を返す。所属番号は、含まれる点番号が小さいクラスタから順に付く。

`labels_at_distance(limit)` は、統合距離が `limit` 以下の統合をすべて行った所属を返す。同じ距離の統合はまとめて反映する。

## Ward法の距離

Ward法にはユークリッド距離を渡す。二乗距離を渡してはいけない。実装内部で二乗し、結果の `merge.distance` は元の距離と同じ尺度へ戻す。

Ward法の統合木は、一般の距離関数では意味を持たない。ユークリッド空間以外では、単連結法、完全連結法、群平均法を使う。

## 二次元の高速な単連結法

```cpp
#include "titan_cpplib/ahc/clustering/euclidean_single_linkage.cpp"

vector<titan23::IntegerPoint> points = {
    {0, 0}, {1, 0}, {10, 0}, {11, 0}
};
auto result = titan23::euclidean_single_linkage_2d(points);
vector<int> labels = result.labels(2);
```

独自の点型には、整数のx座標とy座標を返す関数を渡す。

```cpp
auto result = titan23::euclidean_single_linkage_2d(
    points,
    [](const Point& point) { return point.x; },
    [](const Point& point) { return point.y; });
```

内部ではドロネー辺を距離順に調べ、最小全域木を作る。同じ座標の入力点は削除せず、距離0で統合する。そのため、一般版の単連結法と同じ点数・同じ統合距離を返す。

入力座標は `int` の範囲に収める。座標取得関数も整数型を返す必要がある。同じ距離の辺が複数ある場合、統合木の形は一般版と異なる可能性があるが、各点対が同じクラスタになる距離は一致する。

- 時間: `O(n log n)`
- 追加メモリ: `O(n)`

## 入力条件

- 距離は有限かつ0以上
- 同じ点対には同じ距離を返す
- 単連結法、完全連結法、群平均法では対称な距離を使う
- Ward法ではユークリッド距離を使う
- 距離の戻り値は `long double` へ変換できる
- 点数は `int` に収まる

同じ距離の候補がある場合は、内部のクラスタ番号が小さい方を優先する。結果は決定的だが、同距離時の統合木は一意とは限らない。

## 計算量

点数を `n` とする。

- 時間: `O(n²)`
- 追加メモリ: `O(n²)`
- `labels`、`clusters`: 1回につき `O(n)`

距離表は上三角だけを保存する。AHCで点数が非常に多い場合は、この `O(n²)` メモリが先に問題になる。その場合は、近傍グラフ上の統合や空間分割など、問題固有の近似法を使う。
