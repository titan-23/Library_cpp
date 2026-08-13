# k-d tree

## 概要

`kd_tree.cpp` は、多次元の点からユークリッド距離が近い点を探す。

- 最近点
- 近い順の `k` 点
- 指定半径以内の点
- 登録済みの点から自分自身を除いた近傍

点型は固定しない。点と座標番号から座標を返す関数を渡す。

## 使用例

```cpp
#include <bits/stdc++.h>
#include "titan_cpplib/geometry/kd_tree.cpp"
using namespace std;

using Point = array<int, 2>;
vector<Point> points = {{0, 0}, {3, 4}, {10, 0}, {2, 1}};
auto tree = titan23::make_kd_tree(
    points, 2,
    [](const Point& point, int axis) { return point[axis]; });

Point query = {1, 1};
auto nearest = tree.nearest_neighbor(query);
if (nearest) {
    int point = nearest->index;
    long double squared_distance = nearest->squared_distance;
    long double distance = nearest->distance();
}

auto nearest_five = tree.k_nearest_neighbors(query, 5);
auto within_ten = tree.radius_neighbors(query, 10);
```

結果は距離が小さい順で、同距離なら元の点番号が小さい順になる。`k` が点数より大きい場合は、存在する全点を返す。

## 登録済みの点から探す

```cpp
auto nearest = tree.nearest_neighbor_of(point_index);
auto nearest_ten = tree.k_nearest_neighbors_of(point_index, 10);
auto within_radius = tree.radius_neighbors_of(point_index, radius);
```

これらは指定した点自身を結果から除く。同じ座標の別の点は距離0の近傍として返す。

一般の検索にも、除外する点番号を任意で渡せる。

```cpp
tree.nearest_neighbor(query, excluded_index);
tree.k_nearest_neighbors(query, 10, excluded_index);
tree.radius_neighbors(query, radius, excluded_index);
```

## 座標配列から直接探す

点型から座標を読み出す検索では、各検索時に長さ `d` の座標配列を作る。短い探索を大量に行う場合は、`long double` の座標列を直接渡すと、この一時配列を省ける。

```cpp
array<long double, 2> query = {1, 1};
auto nearest = tree.nearest_neighbor(span<const long double>(query));
auto nearest_five = tree.k_nearest_neighbors(span<const long double>(query), 5);
```

## 半径検索

`radius_neighbors` は境界上の点も含み、通常は距離順に並べる。順序が不要なら最後の引数を `false` にすると、結果の並べ替えを省ける。

```cpp
auto neighbors = tree.radius_neighbors(query, radius, -1, false);
```

## 所有関係

木は渡された点列を所有する。左辺値の `vector` を渡すと複製し、`move(points)` を渡すと移動する。

```cpp
auto tree = titan23::make_kd_tree(
    move(points), dimension, get_coordinate);
```

構築後は点の追加・削除に対応しない。点が変わった場合は木を作り直す。登録した点は `tree.points()` と `tree.point(index)` から読み取れる。

座標取得関数も木が保持する。ラムダが外部データを参照で捕捉する場合、そのデータは木より長く生存し、検索中に変更されないようにする。

## 数値と距離

座標は内部で `long double` へ変換する。全座標と検索座標は有限で、距離の二乗和も `long double` の有限値に収まる必要がある。

距離は通常のユークリッド距離である。軸ごとに重みを付けたい場合は、座標取得関数であらかじめ各座標を重みの平方根倍する。

## 計算量

点数を `n`、次元数を `d`、返す点数を `m` とする。

| 処理 | 低次元での典型的な時間 | 最悪時間 |
|---|---:|---:|
| 構築 | `O(dn log n)` | `O(dn²)` |
| 最近点 | `O(d log n)` | `O(dn)` |
| 近い順のk点 | `O(d log n + k(d+log k))` | `O(n(d+log k))` |
| 半径検索 | `O(d log n + dm + m log m)` | `O(dn + m log m)` |

構築では `nth_element` を使うため、`O(dn log n)` は平均計算量である。追加メモリは座標の保存を含めて `O(dn)`、検索中の再帰は `O(log n)` である。半径検索で並べ替えを省く場合は、結果の `m log m` も不要になる。

k-d treeは次元数が増えるほど枝を省きにくくなる。目安として数十次元以上では全探索に近づく可能性がある。高次元では近似近傍探索や、問題固有の候補削減を検討する。

## AHCでの用途

- K-meansやK-medoidsの初期候補
- SAで移動先クラスタや交換点を絞る
- DBSCANの半径検索
- 点追加や施設配置の近傍候補
- 二次元以外でドロネー三角形分割の代わりに近傍を作る

近傍候補をk-d treeだけに限定すると、問題固有の費用では有力な遠方候補を見落とすことがある。局所探索では一部の無作為候補も残す方が安全である。
