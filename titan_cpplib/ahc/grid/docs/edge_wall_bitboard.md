# 辺に壁があるグリッド用 Bitboard

## 目的

`EdgeWallBitboard` はセル自体ではなく隣接するセルの間に壁があるグリッドを扱います。`Bitboard` が各セルを道か壁に分類するのに対し、`EdgeWallBitboard` は辺ごとの通行可否を保持する 4 近傍専用の実装です。4 近傍の無向の壁だけを扱い、8 近傍、一方通行、重み付きの辺には対応しません。

```cpp
#include "titan_cpplib/ahc/grid/edge_wall_bitboard.cpp"
using namespace titan23;
```

## 最小例

```cpp
// 3 行 4 列の盤面
// right_walls[r][c] は (r,c) と (r,c+1) の間の壁、down_walls[r][c] は (r,c) と (r+1,c) の間の壁
vector<string> right_walls = {"010", "000", "100"};
vector<string> down_walls = {"0010", "1100"};
EdgeWallBitboard64 board(3, 4);
board.from_walls(right_walls, down_walls);

// 1 回の移動
assert(board.can_move(0, 0, GridDirection::Right));
assert(!board.can_move(0, 1, GridDirection::Right));

// 現在位置の候補の集合を指示の方向へ移動する
auto now = board.new_set();
auto next = board.new_set();
board.make_set({{0, 0}, {1, 1}}, now);
board.advance(now, GridDirection::Right, next);   // 壁があって移動できないセルは元の位置に残る
assert(board.count(next) == 2 && board.test(next, 0, 1) && board.test(next, 1, 2));

// 到達できるセルと最短距離
auto start = board.new_set();
auto reach = board.new_set();
board.set(start, 0, 0);
board.flood(start, reach);                         // board.count(reach) == 11
vector<int> dist;
board.bfs_dist(start, dist);                       // 添字は row * width + column で、到達できないセルは -1
vector<pair<int, int>> path;
bool found = board.shortest_path({0, 0}, {2, 3}, path);  // path.size() == dist[2 * 4 + 3] + 1 == 6
```

結果を格納する `Set` は `new_set()` で 1 回作成して再利用します。`advance` や `flood` などの入力と出力には別の変数を渡します。

## 壁の表現

各方向について、その方向へ移動できる移動元のセルの集合を保持します。方向は `grid_common.cpp` の `GridDirection` で表し、順序は上、右、下、左です。

```cpp
array<Set, 4> movable;  // 添字は GridDirection の値
```

例えば `movable[Right]` に `(r, c)` が含まれる場合は、`(r, c)` から `(r, c + 1)` へ移動できます。外周のセルから盤面外へは常に移動できないものとします。

壁の入力は次の 2 種類です。

- `right_walls[r][c]`: `(r, c)` と `(r, c + 1)` の間の壁
- `down_walls[r][c]`: `(r, c)` と `(r + 1, c)` の間の壁

文字が `0` の場合は両方向に移動でき、`1` の場合は両方向に移動できません。

## 集合の移動

方向 `d` へ実際に移動できるセルだけを取り出して 1 セル分シフトします。

```text
moved = shift(s & movable[d], d)
```

次の 3 つの操作を用途に応じて選択します。

| 操作 | 結果 |
|---|---|
| `move_success(s, d, out)` | 移動に成功したセルの移動先だけ |
| `advance(s, d, out)` | 指示を実行した結果。壁があるセルは元の位置に残る |
| `advance_uncertain(s, d, out)` | 指示を実行する場合と実行しない場合の位置の候補の和集合 |

式で表すと次のとおりです。

```text
advance(s, d) = shift(s & movable[d], d) | (s & ~movable[d])
advance_uncertain(s, d) = s | shift(s & movable[d], d)
```

通常の幅優先探索で使用する 1 回の移動先の集合は、移動可能な 4 方向の移動先の和集合です。

```text
expand(s) = union(shift(s & movable[d], d) for d in four directions)
```

壁があって同じセルに残る動作は、グラフ上の隣接セルの列挙には含めません。

## 公開 API

1 行を 1 つの整数で保持します。幅が 64 以下の場合は `EdgeWallBitboard64`、幅が 128 以下の場合は `EdgeWallBitboard128` を使用します。

```cpp
template<class Word>
class EdgeWallBitboard : public RowBitboardBase<Word> {
public:
    using Set = vector<Word>;

    EdgeWallBitboard(int height, int width);

    void from_walls(
        const vector<string>& right_walls,
        const vector<string>& down_walls
    );

    bool can_move(int row, int column, GridDirection direction) const;
    int destination_index(int index, GridDirection direction) const;

    void make_set(const vector<pair<int, int>>& cells, Set& out) const;
    void complement_into(const Set& source, Set& out) const;

    void move_success(
        const Set& source,
        GridDirection direction,
        Set& out
    ) const;

    void advance(
        const Set& source,
        GridDirection direction,
        Set& out
    ) const;

    void advance_uncertain(
        const Set& source,
        GridDirection direction,
        Set& out
    ) const;

    void expand(const Set& source, Set& out) const;
    void flood(const Set& sources, Set& out) const;
    void bfs_dist(const Set& sources, vector<int>& distance) const;
    bool shortest_path(
        pair<int, int> start,
        pair<int, int> goal,
        vector<pair<int, int>>& path
    ) const;
};

using EdgeWallBitboard64 = EdgeWallBitboard<uint64_t>;
using EdgeWallBitboard128 = EdgeWallBitboard<__uint128_t>;
```

`right_walls` は `height` 行で各行が `width - 1` 文字、`down_walls` は `height - 1` 行で各行が `width` 文字とします。

`destination_index` は 1 次元の番号で指定したセルの移動先の番号を返し、壁がある場合は同じ番号を返します。

### 集合演算

盤面の形だけで結果が定まる集合演算は、`Bitboard` と共通の基底クラス `RowBitboardBase` が提供します。`EdgeWallBitboard` でも同じ名前で呼び出すことができます。

| 分類 | メソッド |
|---|---|
| 形 | `height`, `width`, `cell_count`, `inside` |
| 作成と 1 つのセルの操作 | `new_set`, `clear(s)`, `test`, `set`, `reset`, `flip` |
| 集計と比較 | `count`, `count_and`, `any`, `none`, `equals`, `intersects`, `disjoint`, `is_subset`, `hash64(s)` |
| 集合どうしの演算 | `band`, `bor`, `bxor`, `bdiff`, `iand`, `ior`, `ixor`, `idiff` |
| 列挙と位置 | `for_each`, `cells`, `first`, `kth_cell`, `bounding_box`, `rect` |
| 壁を考慮しない平行移動 | `shift`, `shift_left`, `shift_right`, `shift_up`, `shift_down` |
| 出力 | `to_grid`, `print` |

`complement_into(source, out)` は盤面全体に対する補集合を作成し、`make_set(cells, out)` は座標の列で指定したセルをすべて含む集合を作成します。この 2 つのメソッドだけは `Bitboard` と結果が異なります。`Bitboard` では道のセルが全体集合になります。

`Set` の各行で幅 `width` 以上のビットは常に 0 とします。上の表のメソッドだけで集合を作成する場合、この条件は維持されます。詳細は[グリッドの共通部品](grid_common.md)に記載しています。

## ファイル構成

```text
titan_cpplib/ahc/grid/
  grid_common.cpp           GridDirection などの共通の定義
  row_bitboard_base.cpp     Bitboard と共通の集合演算
  edge_wall_bitboard.cpp
  docs/
    edge_wall_bitboard.md
  test/
    edge_wall_bitboard_test.cpp
```

