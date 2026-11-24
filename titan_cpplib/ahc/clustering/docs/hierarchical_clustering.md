# 階層型クラスタリング

## 概要

`hierarchical_clustering.cpp` は、各点を 1 つのクラスタとして開始し、近い 2 つのクラスタを順に統合します。
クラスタ数を事前に決定する必要はありません。統合木を一度作成すれば、後から任意のクラスタ数または統合距離で切ることができます。

二次元の整数座標で単連結法だけが必要な場合は、`euclidean_single_linkage.cpp` を使用すると、時間を `O(n log n)`、メモリを `O(n)` に減らすことができます。この実装はドロネー三角形分割に Boost.Polygon を使用します。AtCoder の環境には Boost.Polygon がインストールされています。

対応する方式は次の 4 種類です。

| 設定 | クラスタ間距離 | 傾向 |
|---|---|---|
| `single` | 最も近い点対の距離 | 細長くつながるクラスタを作りやすい |
| `complete` | 最も遠い点対の距離 | 直径の小さいまとまりを作りやすい |
| `average` | 全点対の距離の平均 | 単連結法と完全連結法の中間 |
| `ward` | 統合による平方誤差の増加量 | K-means に近い丸いクラスタを作りやすい |

AHC では次の場合に適しています。

- クラスタ数の候補を一度に作成する場合
- 近い点を段階的にまとめる場合
- SA の初期解を複数の粗さで用意する場合

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

点番号から直接距離を求める場合は、点の配列を用意せずに使用できます。

```cpp
auto result = titan23::hierarchical_clustering_by_index(
    n,
    [&](int a, int b) { return distance_matrix[a][b]; },
    titan23::HierarchicalLinkage::average);
```

## 結果

`result.merges[i]` は、`n+i` 番の新しいクラスタを作成した統合を表します。

```cpp
merge.left       // 左の子 / 点の場合は [0,n)、統合後のクラスタの場合は [n,2n-1)
merge.right      // 右の子
merge.distance   // この統合が行われた距離
merge.size       // 統合後の点数
```

`labels(k)` は、統合木を `k` 個のクラスタに切ったときの所属番号を返します。所属番号は、含まれる最小の点番号が小さいクラスタから順に付けます。

`labels_at_distance(limit)` は、統合距離が `limit` 以下の統合をすべて行った後の所属を返します。同じ距離の統合はまとめて反映します。

## Ward 法の距離

Ward 法には、二乗距離ではなくユークリッド距離を渡してください。実装の内部で二乗し、結果の `merge.distance` は元の距離と同じ尺度に戻します。

一般の距離関数では、Ward 法の統合木には意味がありません。ユークリッド空間以外では、単連結法、完全連結法、群平均法を使用してください。

## 二次元の高速な単連結法

```cpp
#include "titan_cpplib/ahc/clustering/euclidean_single_linkage.cpp"

vector<titan23::IntegerPoint> points = {
    {0, 0}, {1, 0}, {10, 0}, {11, 0}
};
auto result = titan23::euclidean_single_linkage_2d(points);
vector<int> labels = result.labels(2);
```

独自の点の型を使用する場合は、整数の x 座標と y 座標を返す関数を渡します。

```cpp
auto result = titan23::euclidean_single_linkage_2d(
    points,
    [](const Point& point) { return point.x; },
    [](const Point& point) { return point.y; });
```

ユークリッド最小全域木の辺はドロネー三角形分割の辺に含まれるため、内部ではドロネー三角形分割の辺を距離の順に調べて最小全域木を作成します。同じ座標の入力点は削除せず、距離 0 で統合します。そのため、汎用の階層型クラスタリングで単連結法を使用した場合と同じ点数と統合距離を返します。

入力座標は `int` の範囲に収めてください。座標を取得する関数も整数型を返す必要があります。同じ距離の辺が複数あると、統合木の形が汎用の階層型クラスタリングと異なる場合があります。ただし、各点対が同じクラスタになる距離は一致します。

- 時間: `O(n log n)`
- 追加メモリ: `O(n)`

## 入力条件

- 距離は有限かつ 0 以上にしてください。
- 同じ点対には同じ距離を返してください。
- 単連結法、完全連結法、群平均法では対称な距離を使用してください。
- Ward 法ではユークリッド距離を使用してください。
- 距離の戻り値は `long double` へ変換できる必要があります。
- 点数は `int` に収まる必要があります。

同じ距離の候補がある場合は、内部のクラスタ番号が小さい方を優先します。結果は決定的ですが、このときの統合木は一意とは限りません。

## 仕組み

全点対の距離を上三角の表に保持し、最近傍鎖法で統合します。鎖の末尾のクラスタから最も近いクラスタへ鎖を伸ばし、互いに最も近い 2 つのクラスタが見つかった場合はその 2 つを統合します。統合後のクラスタと他のクラスタの距離は、Lance–Williams の更新式で表から求めます。4 種類の方式はいずれも、統合後のクラスタとの距離が統合前の 2 つの距離の小さい方を下回らない性質があるため、この方法で正しい統合木が得られます。最後に、統合を距離の昇順にソートします。

## 計算量

点数を `n` とします。

- 時間: `O(n²)`
- 追加メモリ: `O(n²)`
- `labels`、`clusters`: 1 回につき `O(n)`

距離の表は上三角だけを保存します。AHC で点数が非常に多い場合は、この `O(n²)` のメモリが先に問題になります。その場合は、近傍グラフ上での統合や空間分割など、問題に固有の近似法を使用してください。
