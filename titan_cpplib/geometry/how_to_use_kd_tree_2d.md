# 2次元k-d tree

## 選び方

- 全点が最初から分かる場合は `KdTree2D` を使う。
- 検索の途中で点を追加する場合は `IncrementalKdTree2D` を使う。
- 3次元以上や実行時次元数が必要な場合は `KdTree` を使う。

2次元版は座標を直接保持するため、
任意次元版より構築、検索、メモリ使用量の定数倍が小さい。
追加対応のmetadataを静的版へ持たせないため、2つのクラスは分離している。

## 静的2D版

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

任意の点型から構築する場合は、点と軸番号から座標を返す関数を渡す。
木が保持するのは変換後の座標だけで、元のpayloadは保持しない。
結果の点番号は入力順に対応する。

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

## 追加対応2D版

```cpp
#include "titan_cpplib/geometry/incremental_kd_tree_2d.cpp"

titan23::IncrementalKdTree2D<double> tree;
tree.reserve(max_point_count);

int id1 = tree.add(0.0, 0.0);
int id2 = tree.add(3.0, 4.0);
auto nearest = tree.nearest_neighbor(1.0, 1.0);
```

点番号は追加順で、部分再構築や `rebuild()` の後も変わらない。
`point(id)` と `points()` から座標を読める。
ただし、返された座標への参照、iterator、spanは、
`reserve`、`add`、`add_all` によるvector再配置で無効になる場合がある。
`rebuild()` だけなら無効にならない。
変換factoryから構築した場合も、保持するのは変換後の座標だけで、
元のpayloadは保持しない。

複数点をまとめて追加する場合は `add_all` を使う。
全体を一度だけ構築し直すため、1点ずつ追加するより速い。

```cpp
vector<array<double, 2>> added = {{1, 2}, {5, 8}, {13, 21}};
int first_id = tree.add_all(added);
```

`rebuild()` は検索結果と点番号を変えず、木全体を平衡に作り直す。
通常は自動的な部分再構築だけでよいが、
大量追加の後に検索だけを繰り返すフェーズへ移る場合は、
全体再構築で検索時間が安定することがある。

削除、登録済み座標の変更、任意距離関数には対応しない。
追加と検索を同時に別threadから実行してはならない。
追加を行わない `const` 検索同士は、呼び出し側が別の結果vectorを使えば並行実行できる。

## 結果vectorの再利用

`k_nearest_neighbors` と `radius_neighbors` には、結果vectorを渡す形式がある。

```cpp
using Neighbor = decltype(tree)::Neighbor;
vector<Neighbor> out;
out.reserve(32);

tree.k_nearest_neighbors(x, y, 20, out);
tree.radius_neighbors(x, y, radius, out);
```

関数は `out` を空にしてから書き込むが、capacityは保持する。
最近傍検索は常にメモリ確保を行わない。

## 結果と数値

- 距離は2次元ユークリッド距離。
- 結果は距離の二乗が小さい順で、同距離なら点番号が小さい順。
- 半径検索は境界上の点を含む。
- 同じ座標の点も別の点番号として保持する。
- `_of(id)` は指定した点番号だけを除外する。
- 全座標、クエリ、半径、距離の二乗は内部スカラー型の有限値に収まる必要がある。

既定の内部スカラー型は `double` である。
精度や値域が必要な場合は `long double` を指定する。
`vector<array<Scalar, 2>>` を直接渡すfactoryでは、その `Scalar` を保つ。
任意の点型を変換するfactoryでは、指定しなければ `double` になる。

```cpp
auto tree = titan23::make_kd_tree_2d_as<long double>(points, get_coordinate);
auto incremental = titan23::make_incremental_kd_tree_2d_as<long double>(points, get_coordinate);
```

## 計算量

点数を `n`、返す点数を `m` とする。

| 処理 | 低次元での典型的な時間 | 最悪時間 |
|---|---:|---:|
| 静的構築 | `O(n log n)` | `O(n²)` |
| 最近傍 | `O(log n)` | `O(n)` |
| k近傍 | `O(log n + k log k)` | `O(n log k)` |
| 半径検索 | `O(log n + m log m)` | `O(n + m log m)` |
| 1点追加 | 償却 `O(log² n)` | 1回の再構築で `O(n log n)` |
| `add_all` / `rebuild` | `O(n log n)` | `O(n²)` |

追加版は木の高さを抑えるが、
最近傍検索自体の最悪時間が `O(n)` であることは変わらない。
部分再構築には単発の停止時間があるため、
厳しい時間管理では追加時間の最大値も確認する。
