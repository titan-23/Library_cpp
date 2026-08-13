# ドロネー三角形分割とボロノイ図

## ファイル

- `delaunay_triangulation.cpp`: 整数座標点のドロネー三角形分割
- `voronoi_diagram.cpp`: 指定した長方形内へ切り取ったボロノイ図

両方とも点を生成点とする通常のユークリッド距離を使う。線分から作るボロノイ図や重み付きボロノイ図は扱わない。

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
    // 点0とドロネー辺で隣接する点
}
```

独自の点型には、整数のx座標とy座標を返す関数を渡す。

```cpp
auto result = titan23::delaunay_triangulation(
    points,
    [](const Point& point) { return point.x; },
    [](const Point& point) { return point.y; });
```

座標の戻り値は整数型で、値は `int` に収まる必要がある。

### 重複点

同じ座標は一つの点へまとめる。

```cpp
result.input_to_point[input_index] // 入力点から、重複を除いた点番号
result.point_to_input[point]       // その座標が最初に現れた入力点番号
result.points                      // 重複を除いた点列
```

辺、三角形、隣接点は `result.points` の番号を使う。

### 退化した入力

- 0点または1点: 辺と三角形は空
- 2点: 1辺
- 全点が一直線上: 座標順に隣り合う辺だけを返し、三角形は空
- 4点以上が同一円周上: 分割は一意でないため、そのうち一つを決定的に返す

同一円周上では、返されるドロネー辺自体も一意ではない。どの分割でも必要になる性質だけに依存すること。

### 計算量

重複を除いた点数を `n` とする。

- 時間: `O(n log n)`
- 追加メモリ: `O(n)`

実装は Boost.Polygon の整数座標用ボロノイ構築を使う。座標は32ビット符号付き整数の範囲に限る。

## 矩形内ボロノイ図

```cpp
#include "titan_cpplib/geometry/voronoi_diagram.cpp"

auto result = titan23::voronoi_diagram(
    points,
    {.min_x=0, .min_y=0, .max_x=100, .max_y=100});

for (const auto& vertex : result.cells[site]) {
    // site に最も近い領域の頂点。反時計回り
}
```

`cells` は、無限に広がる通常のボロノイ領域を指定長方形で切り取った凸多角形である。長方形外にある生成点も使用でき、その領域が長方形と交わらなければ空になる。

重複点はドロネー三角形分割と同じ規則でまとめる。`cells` の添字は `result.delaunay.points` の点番号である。

独自の点型は次の順で渡す。

```cpp
auto result = titan23::voronoi_diagram(
    points,
    [](const Point& point) { return point.x; },
    [](const Point& point) { return point.y; },
    bounds);
```

### 計算量

点数を `n` とする。ドロネー辺数は平面的な入力では `O(n)` で、各領域をドロネー隣接点の二等分線で切る。

- 平均的な時間: `O(n log n)`
- 最悪時間: `O(n²)`
- 追加メモリ: `O(n)`、返す多角形頂点を含む

通常は各点の隣接数が小さいため高速である。ただし、一つの点に `O(n)` 個のドロネー隣接点が集まる入力では、その領域の切り取りに `O(n²)` かかり得る。

### 数値計算

ドロネーの組合せは整数座標から構築する。ボロノイ領域の頂点は二等分線と矩形辺の交点なので `long double` で返す。
極端に大きい矩形や非常に近い交点では丸め誤差が生じる。組合せだけが必要ならドロネー辺を使う。

## AHCでの主な用途

- 各点について、空間的に有力な少数の相手だけを列挙する
- クラスタ間や施設間の近傍グラフを作る
- 点追加、点交換、局所探索の候補を絞る
- 勢力圏の面積や境界を評価する
- 可視化用の領域を作る

ドロネー隣接点だけで探索すると候補を大きく減らせるが、問題固有の費用がユークリッド距離と一致しない場合、最良の操作を見落とすことがある。一部は無作為な候補も残す方がよい。
