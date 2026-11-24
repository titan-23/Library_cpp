# 2 次元 k-d tree

## 選び方

- 全点が最初から定まっている場合は `KdTree2D` を使用します。
- 検索の途中で点を追加する場合は `IncrementalKdTree2D` を使用します。
- 3 次元以上の場合や、次元数を実行時に定める必要がある場合は `KdTree` を使用します。

2 次元版は座標を直接保持するため、任意次元版より構築、検索、メモリ使用量の定数倍が小さくなります。
点の追加に必要な metadata を静的版に保持させないように、2 つのクラスを分離しています。

## 静的な 2D 版

```cpp
#include <array>
#include <utility>
#include <vector>
#include "titan_cpplib/geometry/kd_tree_2d.cpp"
using namespace std;

vector<array<double, 2>> points = {{0, 0}, {3, 4}, {10, 0}, {2, 1}};
auto tree = titan23::make_kd_tree_2d(move(points));

auto nearest = tree.nearest_neighbor(1.0, 1.0);
auto nearest_five = tree.k_nearest_neighbors(1.0, 1.0, 5);
auto within_ten = tree.radius_neighbors(1.0, 1.0, 10.0);
```

任意の点型から構築する場合は、点と軸番号から座標を返す関数を渡します。
木は変換後の座標だけを保持し、元の payload は保持しません。
結果の点番号は入力順に対応します。

```cpp
struct Data {
    int x;
    int y;
    int payload;
};

auto tree = titan23::make_kd_tree_2d(data, [](const Data& p, int axis) {
    return axis == 0 ? p.x : p.y;
});
```

## 点の追加に対応する 2D 版

```cpp
#include "titan_cpplib/geometry/incremental_kd_tree_2d.cpp"

titan23::IncrementalKdTree2D<double> tree;
tree.reserve(max_point_count);

int id1 = tree.add(0.0, 0.0);
int id2 = tree.add(3.0, 4.0);
auto nearest = tree.nearest_neighbor(1.0, 1.0);
```

点番号は追加順で、部分再構築や `rebuild()` の後も変化しません。
`point(id)` と `points()` で座標を参照することができます。
ただし、返された座標への参照、iterator、span は、`reserve`、`add`、`add_all` による vector の再配置で無効になる場合があります。
`rebuild()` を呼び出しただけでは無効になりません。
座標を変換する factory から構築した場合も、木は変換後の座標だけを保持し、元の payload は保持しません。

複数の点をまとめて追加する場合は `add_all` を使用します。
木全体を一度だけ再構築するため、1 点ずつ追加するより高速です。

```cpp
vector<array<double, 2>> added = {{1, 2}, {5, 8}, {13, 21}};
int first_id = tree.add_all(added);
```

`rebuild()` は検索結果と点番号を変更せずに、木全体を平衡になるように再構築します。
通常は自動的な部分再構築だけで十分です。
ただし、大量の点を追加した後に検索だけを繰り返すフェーズに移る場合は、木全体を再構築すると検索時間が安定することがあります。

点の削除、登録済みの座標の変更、任意の距離関数には対応していません。
追加と検索を別のスレッドから同時に実行してはなりません。
追加を行わない `const` の検索同士は、呼び出し側が別々の結果の vector を使用すれば並行して実行することができます。

## 結果の vector の再利用

`k_nearest_neighbors` と `radius_neighbors` には、結果の vector を渡す形式があります。

```cpp
using Neighbor = decltype(tree)::Neighbor;
vector<Neighbor> out;
out.reserve(32);

tree.k_nearest_neighbors(x, y, 20, out);
tree.radius_neighbors(x, y, radius, out);
```

関数は `out` を空にしてから結果を格納し、capacity は維持します。
最近傍検索はメモリ確保を一切行いません。

## 結果と数値

- 距離は 2 次元のユークリッド距離です。
- 結果は距離の二乗が小さい順で、距離が同じ場合は点番号が小さい順です。
- 半径検索の結果は境界上の点を含みます。
- 同じ座標の点も別の点番号として保持します。
- `_of(id)` は指定した点番号だけを除外します。
- 全座標、クエリ、半径、距離の二乗は内部のスカラー型の有限値に収まる必要があります。

内部のスカラー型の既定は `double` です。
高い精度や広い値域が必要な場合は `long double` を指定します。
`vector<array<Scalar, 2>>` を直接渡す factory では、内部のスカラー型はその `Scalar` になります。
任意の点型を変換する factory では、指定しない場合は `double` になります。

```cpp
auto tree = titan23::make_kd_tree_2d_as<long double>(points, get_coordinate);
auto incremental = titan23::make_incremental_kd_tree_2d_as<long double>(points, get_coordinate);
```

## 計算量

点数を `n`、返す点数を `m` とします。

| 処理 | 低次元での典型的な時間 | 最悪時間 |
|---|---:|---:|
| 静的版の構築 | `O(n log n)` | `O(n²)` |
| 最近傍 | `O(log n)` | `O(n)` |
| k 近傍 | `O(log n + k log k)` | `O(n log k)` |
| 半径検索 | `O(log n + m log m)` | `O(n + m log m)` |
| 1 点の追加 | 償却 `O(log² n)` | 1 回の再構築で `O(n log n)` |
| `add_all` / `rebuild` | `O(n log n)` | `O(n²)` |

点の追加に対応する版は木の高さを抑えますが、最近傍検索の最悪時間は `O(n)` のままです。
部分再構築が発生すると、その追加に長い時間がかかるため、時間管理が厳しい場合は追加時間の最大値も確認するようにします。
