# k-d tree

## 概要

`kd_tree.cpp` は、多次元の点の集合からユークリッド距離が近い点を探索します。

- 最近点
- 近い順の `k` 点
- 指定した半径以内の点
- 登録済みの点の、その点自身を除いた近傍

点の型は任意で、点と座標番号を受け取って座標を返す関数を渡します。

2 次元の点だけを扱う場合は、計算時間の定数倍とメモリ使用量を抑えた `KdTree2D` を優先して使用します。
検索中に 2 次元の点を追加する場合は `IncrementalKdTree2D` を使用します。
詳細は `how_to_use_kd_tree_2d.md` に記載しています。

## 使用例

```cpp
#include <array>
#include <span>
#include <utility>
#include <vector>
#include "titan_cpplib/geometry/kd_tree.cpp"
using namespace std;

using Point = array<int, 2>;
vector<Point> points = {{0, 0}, {3, 4}, {10, 0}, {2, 1}};
auto tree = titan23::make_kd_tree(points, 2, [](const Point& point, int axis) { return point[axis]; });

Point query = {1, 1};
auto nearest = tree.nearest_neighbor(query);
if (nearest) {
    int id = nearest->index;
    long double dist2 = nearest->squared_distance;
    long double dist = nearest->distance();
}

auto nearest_five = tree.k_nearest_neighbors(query, 5);
auto within_ten = tree.radius_neighbors(query, 10);
```

結果は距離が小さい順に並び、距離が等しい場合は元の点番号が小さい順になります。
`k` が点の個数より大きい場合は、存在するすべての点を返します。

## 登録済みの点の近傍検索

```cpp
auto nearest = tree.nearest_neighbor_of(point_index);
auto nearest_ten = tree.k_nearest_neighbors_of(point_index, 10);
auto within_radius = tree.radius_neighbors_of(point_index, radius);
```

これらの関数は指定した点自身を結果から除きます。同じ座標にある別の点は距離 0 の近傍として返します。

通常の検索関数にも、除外する点番号を省略可能な引数として渡すことができます。

```cpp
tree.nearest_neighbor(query, excluded_index);
tree.k_nearest_neighbors(query, 10, excluded_index);
tree.radius_neighbors(query, radius, excluded_index);
```

## 座標配列を直接渡す検索

点型から座標を取得する検索では、検索のたびに長さ `d` の座標配列を作成します。
短い探索を大量に行う場合は、内部のスカラー型の座標列を直接渡すと
この一時配列の作成を省略できます。

```cpp
array<long double, 2> query = {1, 1};
auto nearest = tree.nearest_neighbor(span<const long double>(query));
auto nearest_five = tree.k_nearest_neighbors(span<const long double>(query), 5);
```

## 結果配列の再利用

`k_nearest_neighbors` と `radius_neighbors` には、結果を格納する `vector` を渡す形式もあります。
同じ木を繰り返し検索する場合は、あらかじめ必要な要素数を `reserve` しておくと
確保したメモリを再利用できます。

```cpp
vector<titan23::KdNeighbor> neighbors;
neighbors.reserve(20);

tree.k_nearest_neighbors(query, 20, neighbors);
tree.radius_neighbors(query, radius, neighbors);
tree.k_nearest_neighbors_of(point_index, 20, neighbors);
tree.radius_neighbors_of(point_index, radius, neighbors);
```

関数は最初に結果配列を空にしますが、確保済みの容量は維持します。
結果の内容と並び順は、戻り値で結果を返す形式と同じです。

## 半径検索

`radius_neighbors` の結果は境界上の点も含み、既定では距離の昇順に並びます。
順序が不要な場合は、最後の引数を `false` にすると結果の並べ替えを省略できます。

```cpp
auto neighbors = tree.radius_neighbors(query, radius, -1, false);
```

## 所有関係

木は渡された点列を所有します。
左辺値の `vector` を渡すとコピーし、`move(points)` を渡すとムーブします。

```cpp
auto tree = titan23::make_kd_tree(move(points), dimension, get_coordinate);
```

構築後は点の追加と削除に対応していません。点を変更した場合は木を再構築します。
登録した点は `tree.points()` と `tree.point(index)` で取得できます。

座標を取得する関数も木が保持します。
ラムダが外部のデータを参照でキャプチャする場合は、そのデータを木より長く生存させ、
木を使用している間は変更しないようにします。
変換係数などを変更すると、木がキャッシュした登録済みの点の座標とクエリの座標とで、変換が一致しなくなります。
その場合は木を再構築します。
コピー代入を使用する場合、座標を取得する関数は例外を送出せずに swap できる必要があります。
キャプチャのないラムダと `std::function` はこの条件を満たします。
コピー代入できないキャプチャ付きのラムダでは、木のコピー構築を使用します。

## 数値と距離

既定では座標を内部で `long double` に変換します。
速度とメモリ使用量を優先して `double` や `float` を使用する場合は、
`make_kd_tree_as` で型を指定します。

```cpp
auto tree = titan23::make_kd_tree_as<double>(points, dimension, get_coordinate);

array<double, 2> query = {1, 1};
auto nearest = tree.nearest_neighbor(span<const double>(query));
vector<titan23::KdNeighborT<double>> neighbors =
    tree.k_nearest_neighbors(span<const double>(query), 10);
```

`make_kd_tree` は `long double` を使用し、結果の型は `KdNeighbor` です。
指定できる内部のスカラー型は浮動小数点型に限ります。

登録するすべての点の座標とクエリの座標は、内部のスカラー型に変換した後も有限である必要があります。
座標の幅、半径の二乗、距離の二乗和が有限値に収まらない場合は
`overflow_error` を送出します。

距離は通常のユークリッド距離です。
軸ごとに重みを付ける場合は、座標を取得する関数で
あらかじめ各座標に重みの平方根を掛けます。

## 計算量

点の個数を `n`、次元数を `d`、返す点の個数を `m` とします。

| 処理 | 低次元での典型的な時間 | 最悪時間 |
|---|---:|---:|
| 構築 | `O(dn log n)` | `O(dn²)` |
| 最近点 | `O(d log n)` | `O(dn)` |
| 近い順の k 点 | `O(d log n + k(d+log k))` | `O(n(d+log k))` |
| 半径検索 | `O(d log n + dm + m log m)` | `O(dn + m log m)` |

構築では `nth_element` を使用するため、`O(dn log n)` は平均計算量です。
追加メモリは座標の保存を含めて `O(dn)` で、検索中の再帰の深さは `O(log n)` です。
構築に使用する点番号の作業配列は、構築後には保持しません。
半径検索で並べ替えを省略する場合は、計算量の `m log m` の項もなくなります。

k-d tree は次元数が増えるほど枝刈りしにくくなります。
目安として、数十次元以上では検索が全探索に近づく可能性があります。
高次元の場合は、近似近傍探索や、問題に固有の方法で候補を減らすことを検討します。

## AHC での用途

- K-means や K-medoids の初期候補
- SA での移動先のクラスタや交換する点の絞り込み
- DBSCAN の半径検索
- `IncrementalKdTree2D` による、追加した施設や点の近傍候補
- 2 次元以外の場合の、ドロネー三角形分割に代わる近傍の作成

近傍候補を k-d tree の検索結果だけに限定すると、
問題に固有のコストでは有力な遠方の点が候補に含まれないことがあります。
局所探索では、ランダムに選んだ候補も一部残すほうが安全です。
