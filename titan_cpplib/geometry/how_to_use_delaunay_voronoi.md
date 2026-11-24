# ドロネー三角形分割とボロノイ図

## ファイル

- `delaunay_triangulation.cpp`: 整数座標の点のドロネー三角形分割
- `voronoi_diagram.cpp`: 指定した長方形で切り取ったボロノイ図

どちらのファイルも点を生成点とし、距離には通常のユークリッド距離を使用します。線分から作成するボロノイ図や重み付きボロノイ図は扱いません。

## ドロネー三角形分割

```cpp
#include <bits/stdc++.h>
#include "titan_cpplib/geometry/delaunay_triangulation.cpp"
using namespace std;

vector<titan23::IntegerPoint> points = {
    {0, 0}, {10, 0}, {10, 10}, {0, 10}, {5, 4}
};
auto result = titan23::delaunay_triangulation(points);

for (auto [a, b] : result.edges) {
    // result.points[a] と result.points[b] を結ぶ辺
}
for (auto [a, b, c] : result.triangles) {
    // 反時計回りの三角形
}
for (int next : result.neighbors[0]) {
    // 点 0 とドロネー辺で隣接する点
}
```

独自の点型を使用する場合は、整数の x 座標と y 座標を返す関数を渡します。

```cpp
auto result = titan23::delaunay_triangulation(
    points,
    [](const Point& point) { return point.x; },
    [](const Point& point) { return point.y; });
```

関数の戻り値は整数型で、値が `int` に収まる必要があります。

### 重複点

同じ座標の点は 1 点にまとめます。

```cpp
result.input_to_point[input_index] // 入力点に対応する、重複を除いた点番号
result.point_to_input[point]       // その座標が最初に現れた入力点番号
result.points                      // 重複を除いた点列
```

辺、三角形、隣接点は `result.points` の番号で表します。

### 退化した入力

- 0 点または 1 点の場合は辺と三角形が空です。
- 2 点の場合は辺が 1 本です。
- 全点が一直線上にある場合は、座標の順で隣り合う点を結ぶ辺だけを返し、三角形は空です。
- 4 点以上が同一円周上にある場合は分割が一意でないため、そのうちの 1 つを決定的に返します。

4 点以上が同一円周上にある場合は、返されるドロネー辺自体も一意ではありません。どの分割でも必ず成り立つ性質だけに依存するようにします。

### 計算量

重複を除いた点の個数を `n` とします。

- 時間: `O(n log n)`
- 追加メモリ: `O(n)`

実装では、整数座標のボロノイ図を構築する Boost.Polygon の機能を使用しています。座標は 32 ビット符号付き整数の範囲に限ります。

## 長方形内のボロノイ図

```cpp
#include "titan_cpplib/geometry/voronoi_diagram.cpp"

auto result = titan23::voronoi_diagram(
    points,
    {.min_x=0, .min_y=0, .max_x=100, .max_y=100});

for (const auto& vertex : result.cells[site]) {
    // site のボロノイ領域の頂点を反時計回りに列挙する
}
```

`cells` の各要素は、無限に広がる通常のボロノイ領域を指定した長方形で切り取った凸多角形です。長方形の外にある生成点も入力でき、その生成点の領域が長方形と交わらない場合は空になります。

重複点はドロネー三角形分割と同じ規則でまとめます。`cells` の添字は `result.delaunay.points` の点番号です。

独自の点型を使用する場合は、引数を次の順に渡します。

```cpp
auto result = titan23::voronoi_diagram(
    points,
    [](const Point& point) { return point.x; },
    [](const Point& point) { return point.y; },
    bounds);
```

### 計算量

点の個数を `n` とします。ドロネー辺の本数は平面的な入力では `O(n)` です。各領域はドロネー隣接点との垂直二等分線で切り取ります。

- 平均時間: `O(n log n)`
- 最悪時間: `O(n²)`
- 追加メモリ: 返す多角形の頂点を含めて `O(n)`

通常は各点の隣接点が少ないため高速です。ただし、1 つの点に `O(n)` 個のドロネー隣接点が集中する入力では、その領域の切り取りに `O(n²)` の時間がかかる場合があります。

### 数値計算

ドロネー三角形分割の辺と三角形は整数座標から構築します。ボロノイ領域の頂点は垂直二等分線と長方形の辺の交点であるため、`long double` で返します。
極端に大きい長方形や非常に近い交点では丸め誤差が生じます。点の隣接関係だけが必要な場合はドロネー辺を使用します。

## AHC での主な用途

- 各点について空間的に有力な少数の点だけを列挙します。
- クラスタ間や施設間の近傍グラフを作成します。
- 点の追加、点の交換、局所探索の候補を絞り込みます。
- 勢力圏の面積や境界を評価します。
- 可視化用の領域を作成します。

ドロネー隣接点だけを探索の対象にすると、候補を大きく減らすことができます。ただし、問題に固有のコストがユークリッド距離と一致しない場合は、最良の操作が候補に含まれないことがあります。ランダムに選んだ候補も一部残すことを推奨します。
