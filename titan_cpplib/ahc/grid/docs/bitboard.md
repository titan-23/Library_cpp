# Bitboard

壁と道からなる小さな盤面をビット並列で処理するライブラリです。集合演算、到達判定、最短路、連結成分などを提供します。

## 実装の選択

| 実装 | 使用条件 | `Set` の型 |
|---|---|---|
| `Bitboard64` | 幅が 64 以下 | 1 行につき `uint64_t` 1 つ |
| `Bitboard128` | 幅が 128 以下 | 1 行につき `__uint128_t` 1 つ |
| `FlatBitboard64` | 全セル数が 64 以下 | `uint64_t` 1 つ |
| `FlatBitboard128` | 全セル数が 128 以下 | `__uint128_t` 1 つ |

盤面全体が 1 つの整数に収まる場合は `FlatBitboard`、収まらない場合は `Bitboard` を使用します。標準は 4 近傍です。8 近傍を使用する場合は、型名の末尾に `Diag` が付く別名を選択します。

```cpp
using Board4 = titan23::Bitboard64;
using Board8 = titan23::Bitboard64Diag;
using Flat4 = titan23::FlatBitboard64;
using Flat8 = titan23::FlatBitboard64Diag;
```

セル間に壁がある盤面には [EdgeWallBitboard](edge_wall_bitboard.md) を使用します。

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
board.from_grid(grid);  // '#' が壁で、それ以外の文字が道

auto reached = board.new_set();
board.flood(0, 0, reached);
cout << board.count(reached) << '\n';  // 10

vector<pair<int, int>> path;
if (board.shortest_path({0, 0}, {0, 3}, path)) {
    for (auto [row, column] : path) {
        cout << row << ' ' << column << '\n';
    }
}
```

`FlatBitboard` を使用する場合は include と型だけを変更します。公開 API は共通です。

```cpp
#include "titan_cpplib/ahc/grid/flat_bitboard.cpp"
titan23::FlatBitboard64 board(height, width);
```

## 盤面

盤面は道のセルの集合として保持されます。コンストラクタの実行直後は全セルが道です。座標は `(r, c)` で表し、左上が `(0, 0)` です。

| 操作 | 内容 |
|---|---|
| `Board(h, w)`, `resize(h, w)` | 盤面を作成する、再作成する。全セルが道になる |
| `height()`, `width()`, `cell_count()` | 行数、列数、セル数を返す |
| `inside(r, c)` | 盤面内であるかを判定する |
| `from_grid(grid, '#')` | 文字列の配列から壁と道を設定する |
| `set_road(r, c)`, `set_wall(r, c)` | 1 つのセルを道にする、壁にする |
| `is_road(r, c)` | 道であるかを判定する |
| `open_all()`, `block_all()` | 全セルを道にする、壁にする |
| `snapshot(out)`, `restore(saved)` | 盤面を保存する、復元する |
| `road_set()`, `wall_set(out)` | 道の集合、壁の集合を取得する |
| `road_count()`, `wall_count()` | 道のセル数、壁のセル数を返す |
| `hash64()` | 盤面の 64 ビットのハッシュ値を返す |
| `==`, `!=` | 大きさと壁の配置が一致するかを比較する |
| `os << board` | 壁を `#`、道を `.` で出力する |

## 集合

`Set` はセルの集合を表す型です。盤面と同じ大きさの空集合は `new_set()` で作成します。結果を返すメソッドは最後の引数 `out` に結果を格納します。

| 操作 | 内容 |
|---|---|
| `new_set()` | 空集合を作成する |
| `make_set(cells, out)` | 座標の列から集合を作成する。壁のセルは除外する |
| `set`, `reset`, `flip`, `test` | 1 つのセルを追加する、削除する、反転する、含まれるかを判定する。引数は `(s, r, c)` |
| `clear(s)` | 集合を空にする |
| `count(s)`, `any(s)`, `none(s)` | 要素数を返す、空でないかを判定する、空であるかを判定する |
| `count_and(a, b)` | 両方の集合に含まれるセルの個数を返す |
| `equals`, `intersects`, `disjoint`, `is_subset` | 一致するか、共通のセルがあるか、共通のセルがないか、`a` が `b` の部分集合であるかを判定する。引数は `(a, b)` |
| `band`, `bor`, `bxor`, `bdiff` | 積集合、和集合、対称差、差集合を `out` に格納する。引数は `(a, b, out)` |
| `iand`, `ior`, `ixor`, `idiff` | 積集合、和集合、対称差、差集合で `a` を更新する。引数は `(a, b)` |
| `complement_into(a, out)` | 道のセルのうち `a` に含まれないセルの集合を求める |
| `for_each(s, f)` | 含まれるセルごとに `f(r, c)` を行優先の順で呼び出す |
| `cells(s, out)` | 含まれるセルを座標の列に変換する |
| `first(s, r, c)`, `kth_cell(s, k, r, c)` | 行優先の順で先頭のセル、`k` 番目のセルを求める。存在しない場合は `false` を返す |
| `rect(r1, c1, r2, c2, out)` | 半開区間の長方形 `[r1, r2) × [c1, c2)` の集合を作成する |
| `bounding_box(s, r1, c1, r2, c2)` | 集合を含む最小の半開区間の長方形を求める。空集合の場合は `false` を返す |
| `hash64(s)` | 集合の 64 ビットのハッシュ値を返す |
| `to_grid(s, grid, one, zero)`, `print(s, os)` | 文字列の配列に変換する、出力する |

### 例: 集合の組み合わせ

```cpp
// 始点から 3 回以内の移動で到達できる道のセルのうち、候補の集合に含まれるセルの個数を求める
auto near = board.new_set();
auto candidates = board.new_set();
auto hit = board.new_set();
vector<pair<int, int>> sources = {{0, 0}};  // 波括弧を直接渡すと、Set を引数とするオーバーロードと区別できない
board.flood_limited(sources, 3, near);
board.make_set({{2, 0}, {2, 1}, {0, 2}}, candidates);
board.band(near, candidates, hit);
int count = board.count(hit);  // 2
board.for_each(hit, [&](int r, int c) {
    // (2, 0), (2, 1) の順に呼び出される
});
```

## 近傍と探索

| 操作 | 内容 |
|---|---|
| `shift(s, dr, dc, out)` | 壁を考慮せずに集合を平行移動する |
| `shift_left`, `shift_right`, `shift_up`, `shift_down` | 壁を考慮せずに集合を 1 セル移動する。引数は `(s, out)` |
| `step(s, dr, dc, out)` | 平行移動した後、道のセルだけを残す |
| `expand(s, out)` | `s` に隣接する道のセルを求める |
| `border(s, out)` | `s` に隣接し、`s` に含まれない道のセルを求める |
| `dilate(s, out)`, `dilate(s, k, out)` | 壁を考慮せずに集合を 1 回、`k` 回膨張させる |
| `erode(s, out)`, `inner_border(s, out)` | 壁を考慮せずに集合を収縮させる、収縮で除かれる部分を求める |
| `flood(source, out)` | 始点から到達できる道のセルを求める。始点には `(r, c)`、座標の列、集合のいずれかを指定する |
| `flood_limited(source, steps, out)` | 指定した回数以内の移動で到達できるセルの集合を求める。始点には座標の列か集合を指定する |
| `connected(a, b)`, `distance(a, b)` | 2 点が連結であるかを判定する、最短距離を返す。到達できない場合の距離は `-1` である |
| `nearest_in_set(r, c, targets, hit)` | `targets` の中で最も近いセルまでの距離を返す。到達できない場合は `-1` を返す |
| `shortest_path(a, b, path)` | 最短路を座標の列で返す |
| `bfs_dist(sources, dist)` | 多始点の最短距離を求める |
| `bfs_nearest(sources, dist, src)` | 最短距離と最も近い始点の番号を求める |

`bfs_dist` と `bfs_nearest` の配列の添字は `row * width + column` で、到達できないセルの値は `-1` です。

## 連結成分

| 操作 | 内容 |
|---|---|
| `component(r, c, out)` | 指定したセルを含む連結成分を求める |
| `component_size(r, c)` | 指定したセルを含む連結成分の大きさを返す |
| `components()`, `label(labels)` | 連結成分の個数を返す、各セルの連結成分の番号を求める |
| `for_each_component(f)` | 各連結成分について `f(component_set, size)` を呼び出す |
| `largest_component(out)` | 最大の連結成分を求める |
| `articulation_cells(out)` | 道のセルを頂点とするグラフの関節点を求める |
| `reachable_from_border(out)` | 外周の道のセルから到達できるセルの集合を求める |
| `enclosed_road(out)` | 外周の道のセルから到達できない道のセルの集合を求める |

`locally_removable(r, c)` は、連結な盤面からその道のセルを削除しても連結性が維持される十分条件を `3 × 3` の範囲で判定します。`false` の場合でも削除できないとは限りません。

### 例: 壁を配置した後の連結性の確認

```cpp
auto still_connected = [&](int r, int c) {
    board.set_wall(r, c);
    bool ok = board.components() == 1;
    board.set_road(r, c);
    return ok;
};
```

## 注意点

- 結果を格納する `Set` は `new_set()` で 1 回作成して再利用します。
- `shift`, `expand`, `flood` などの入力の集合と出力の集合には別の変数を渡します。
- 探索の処理は内部の作業領域を共有します。同じインスタンスで並列または再帰的に探索を実行しないでください。
- `for_each_component` が渡す集合はコールバックの実行中だけ有効です。コールバックの中から同じインスタンスの探索と連結成分の処理を呼び出さないでください。
- `flood`, `flood_limited`, `bfs_dist` には、座標の列を引数とするオーバーロードと集合を引数とするオーバーロードがあります。`{{0, 0}}` のような波括弧を直接渡すと、呼び出すオーバーロードが定まらずコンパイルエラーになります。`vector<pair<int, int>>` の変数に格納してから渡してください。
- `Set` の要素を直接変更する場合は、幅以上のビットを 1 にしないでください。
- 全セルを道にするメソッドは `open_all()`、全セルを壁にするメソッドは `block_all()` です。以前の版にあった引数なしの `clear()` と `fill()` は廃止しました。
- 最短路が複数ある場合、`shortest_path` は近傍を上、右、下、左の順に探索して最初に得られた経路を返します。8 近傍では上から時計回りの順に探索します。
- 座標と集合のチェックを有効にする場合は、include の前に `TITAN_DEBUG` を定義します。

集合の演算は `EdgeWallBitboard` と共通の実装です。実装の構成は[グリッドの共通部品](grid_common.md)に記載しています。
