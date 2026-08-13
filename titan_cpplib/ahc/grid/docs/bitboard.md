# Bitboard

壁と道からなる小さな盤面をビット並列で処理するライブラリです。集合演算、到達判定、最短路、連結成分などを提供します。

## 選び方

| 実装 | 使用条件 | `Set` の型 |
|---|---|---|
| `Bitboard64` | 幅が64以下 | 1行につき `uint64_t` 一つ |
| `Bitboard128` | 幅が128以下 | 1行につき `__uint128_t` 一つ |
| `FlatBitboard64` | 全セル数が64以下 | `uint64_t` 一つ |
| `FlatBitboard128` | 全セル数が128以下 | `__uint128_t` 一つ |

盤面全体が一つの整数に収まるなら `FlatBitboard`、収まらなければ `Bitboard` を使います。標準は4近傍です。8近傍を使う場合は、型名の末尾に `Diag` が付く別名を選びます。

```cpp
using Board4 = titan23::Bitboard64;
using Board8 = titan23::Bitboard64Diag;
using Flat4 = titan23::FlatBitboard64;
using Flat8 = titan23::FlatBitboard64Diag;
```

## 最小例

```cpp
#include <bits/stdc++.h>
#include "titan_cpplib/ahc/grid/bitboard.cpp"
using namespace std;

vector<string> grid = {
    ".#..",
    ".#..",
    "....",
};

titan23::Bitboard64 board(grid.size(), grid[0].size());
board.from_grid(grid);  // '#' が壁、それ以外が道

auto reached = board.new_set();
board.flood(0, 0, reached);
cout << board.count(reached) << '\n';

vector<pair<int, int>> path;
if (board.shortest_path({0, 0}, {0, 3}, path)) {
    for (auto [row, column] : path) {
        cout << row << ' ' << column << '\n';
    }
}
```

`FlatBitboard` を使う場合は、includeと型だけを変更します。公開APIはほぼ共通です。

```cpp
#include "titan_cpplib/ahc/grid/flat_bitboard.cpp"
titan23::FlatBitboard64 board(height, width);
```

## 盤面と集合

盤面は「道セルの集合」を内部に持ちます。コンストラクタ直後は全セルが道です。

| 操作 | 内容 |
|---|---|
| `from_grid(grid, '#')` | 文字盤から壁と道を設定する |
| `set_road(r, c)` / `set_wall(r, c)` | 1セルを変更する |
| `is_road(r, c)` | 道か調べる |
| `open_all()` / `block_all()` | 全セルを道／壁にする |
| `snapshot(out)` / `restore(saved)` | 盤面を保存／復元する |
| `road_set()` / `wall_set(out)` | 道／壁の集合を得る |

`Set` はセル集合を表す型です。盤面と同じ大きさの空集合は `new_set()` で作ります。

| 操作 | 内容 |
|---|---|
| `make_set(cells, out)` | 座標列から集合を作る。壁セルは除く |
| `set` / `reset` / `flip` / `test` | 1セルを変更・確認する |
| `count` / `any` / `none` | 要素数や空判定 |
| `band` / `bor` / `bxor` / `bdiff` | 積・和・対称差・差集合 |
| `for_each` / `cells` | 含まれるセルを列挙する |
| `rect` / `bounding_box` | 矩形集合の作成／外接矩形の取得 |

## 近傍と探索

| 操作 | 内容 |
|---|---|
| `shift(s, dr, dc, out)` | 壁を無視して集合を平行移動する |
| `step(s, dr, dc, out)` | 平行移動後、道セルだけを残す |
| `expand(s, out)` | 隣接する道セルを求める |
| `border(s, out)` | `s` に隣接し、`s` には含まれない道セルを求める |
| `dilate` / `erode` | 壁を無視して集合を膨張／収縮する |
| `flood(source, out)` | 始点から到達できる道セルを求める |
| `flood_limited(source, steps, out)` | 指定手数以内の到達集合を求める |
| `connected(a, b)` / `distance(a, b)` | 二点の連結性／最短距離 |
| `shortest_path(a, b, path)` | 最短路を座標列で返す |
| `bfs_dist(sources, dist)` | 多始点最短距離を求める |
| `bfs_nearest(sources, dist, src)` | 距離と最寄り始点番号を求める |

`bfs_dist` と `bfs_nearest` の配列位置は `row * width + column`、未到達は `-1` です。

## 連結成分

| 操作 | 内容 |
|---|---|
| `component(r, c, out)` | 指定セルを含む連結成分 |
| `component_size(r, c)` | 指定セルを含む連結成分の大きさ |
| `components()` / `label(labels)` | 成分数／各セルの成分番号 |
| `for_each_component(f)` | 各連結成分について関数を呼ぶ |
| `largest_component(out)` | 最大の連結成分 |
| `articulation_cells(out)` | 道グラフの関節点 |
| `reachable_from_border(out)` | 外周の道から到達できる集合 |
| `enclosed_road(out)` | 外周の道から到達できない集合 |

`locally_removable(r, c)` は、連結な盤面からその道セルを削除しても連結性を保つ十分条件を3×3範囲で判定します。`false` でも削除できないとは限りません。

## 注意点

- 結果を受け取る `Set` は `new_set()` で一度作り、使い回します。
- `shift`、`expand`、`flood` などの入力集合と出力集合には別の変数を渡します。
- 探索処理は内部の作業領域を共有します。同じインスタンスで並列または再帰的に探索しないでください。
- `for_each_component` が渡す集合はコールバック中だけ有効です。コールバック内から同じインスタンスの探索処理を呼ばないでください。
- 座標と集合サイズの検査が必要な開発時は、include前に `TITAN_DEBUG` を定義します。
