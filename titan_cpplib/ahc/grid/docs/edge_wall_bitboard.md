# 辺に壁があるグリッド用Bitboard

## 目的

セル自体ではなく、隣接セル間に壁があるグリッドを扱う。

現在の `Bitboard` は各セルを道か壁に分ける。`EdgeWallBitboard` は、辺ごとの通行可否を表す4近傍専用の実装である。

## 表現

各方向について「その方向へ移動できる出発セル」の集合を持つ。

```cpp
enum class GridDirection {
    Up,
    Right,
    Down,
    Left,
};

array<Set, 4> movable;
```

例えば `movable[Right]` の `(r, c)` が立っていれば、`(r, c)` から `(r, c + 1)` へ移動できる。外周セルの盤面外方向は常に0にする。

壁の入力は次の二種類に分ける。

- `right_walls[r][c]`: `(r, c)` と `(r, c + 1)` の間の壁
- `down_walls[r][c]`: `(r, c)` と `(r + 1, c)` の間の壁
- 文字が `0` なら両方向を移動可能、`1` なら両方向を移動不可

入力時に上下、左右の両方のマスクを作る。毎回壁文字を調べるより、4方向のマスクを持つ方が探索中の処理を単純にできる。20×20盤面なら追加メモリも小さい。

## 集合の移動

方向 `d` へ実際に移動できるセルだけを取り出し、1マスずらす。

```text
moved = shift(s & movable[d], d)
```

用途ごとに次の三操作を分ける。

| 操作 | 結果 |
|---|---|
| `move_success(s, d, out)` | 移動に成功したセルの移動先だけ |
| `advance(s, d, out)` | 指示を実行する。壁なら元のセルに残る |
| `advance_uncertain(s, d, out)` | 指示を実行する場合と忘れる場合の到達候補の和集合 |

式では次のようになる。

```text
advance(s, d) = shift(s & movable[d], d) | (s & ~movable[d])
advance_uncertain(s, d) = s | shift(s & movable[d], d)
```

通常の幅優先探索で使う1手先の集合は、移動可能な4方向だけを合併する。

```text
expand(s) = union(shift(s & movable[d], d) for d in four directions)
```

壁に当たって同じセルに残る動作は、グラフ上の隣接セル列挙には含めない。

## 公開API

1行を一つの整数で持つ。幅64以下は `EdgeWallBitboard64`、幅128以下は `EdgeWallBitboard128` で扱う。

```cpp
template<class Word>
class EdgeWallBitboard {
public:
    using Set = vector<Word>;

    EdgeWallBitboard(int height, int width);

    void from_walls(
        const vector<string>& right_walls,
        const vector<string>& down_walls
    );

    bool can_move(int row, int column, GridDirection direction) const;
    int destination_index(int index, GridDirection direction) const;

    Set new_set() const;
    void make_set(const vector<pair<int, int>>& cells, Set& out) const;

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

`right_walls` は `height` 行、各行 `width - 1` 文字、`down_walls` は `height - 1` 行、各行 `width` 文字とする。

`destination_index` は一次元番号で指定したセルの移動先を返し、壁があれば同じ番号を返す。

## 既存Bitboardとの関係

既存の `Bitboard` と `FlatBitboard` は変更しない。

| クラス | 表すもの |
|---|---|
| `Bitboard` | 各セルが道か壁か |
| `EdgeWallBitboard` | 隣接セル間を移動できるか |

集合の型、集合演算、出力バッファの使い回し方は既存Bitboardと揃える。共通化のために既存クラスを複雑にすることは避け、重複が大きくなった時点で低水準のビット操作だけを内部部品へ分離する。

## ファイル構成

```text
titan_cpplib/ahc/grid/
  edge_wall_bitboard.cpp
  docs/
    edge_wall_bitboard.md
  test/
    edge_wall_bitboard_test.cpp
```

## 検証

`test/edge_wall_bitboard_test.cpp` で次を通常の全セル列挙や幅優先探索と照合する。

- 外周方向へ移動できないこと
- 一つの壁について両方向が同時に閉じること
- 1行、1列の盤面
- `advance` で壁に当たったセルが残ること
- `advance_uncertain` が単純な全セル列挙と一致すること
- ランダムな壁配置で、`flood`、最短距離、最短路が通常の幅優先探索と一致すること
- 右壁、下壁の文字列を変換せず読み込めること

4近傍の無向壁だけを扱う。8近傍、一方通行、重み付き辺には対応しない。
