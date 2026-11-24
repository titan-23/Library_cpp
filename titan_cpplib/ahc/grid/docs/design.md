# 一般グリッドの設計

## 目的

AHC で繰り返し使用する 2 次元の盤面を、セルの型や壁の表現に依存しない形で扱います。盤面は `vector<vector<Cell>>` で保持し、文字、整数、構造体を同じ `Grid<Cell>` で扱います。

このライブラリの対象は次の範囲とします。

- 長方形の盤面の保持と座標によるアクセス
- 範囲内の判定、座標と 1 次元の番号の相互変換
- 4 近傍、8 近傍、任意の移動の集合による近傍の列挙
- メモリを再利用する幅優先探索と経路復元
- 連結成分、0-1 幅優先探索、ダイクストラ法
- 切り出し、回転、反転などの盤面変換

壁の文字、通行条件、移動コストは問題ごとに異なるため `Grid` には保持せず、探索時に関数として渡します。幅 128 以下の盤面をビット並列で処理する `bitboard.cpp` と `flat_bitboard.cpp` は置き換えの対象外とします。

## 配置

```text
titan_cpplib/ahc/grid/
  README.md
  grid_common.cpp           方向、近傍の種類、アサーション用マクロ
  row_bitboard_base.cpp     1 行を 1 つの整数で保持する集合の演算
  bitboard.cpp              1 行を 1 つの整数で保持する Bitboard
  flat_bitboard.cpp         盤面全体を 1 つの整数で保持する Bitboard
  edge_wall_bitboard.cpp    セル間に壁がある Bitboard
  mirror_grid.cpp           鏡とブロックを配置するグリッド
  grid.cpp                  盤面、座標、近傍列挙
  grid_search.cpp           幅優先探索、連結成分、重み付き最短路
  grid_transform.cpp        切り出し、回転、反転
  docs/
    bitboard.md             Bitboard の使用方法
    design.md               本書
    edge_wall_bitboard.md   辺に壁があるグリッド用 Bitboard
    grid_common.md          グリッドの共通部品
    mirror_grid.md          鏡とブロックを配置するグリッド
  test/
    edge_wall_bitboard_test.cpp
    grid_test.cpp
```

このうち `grid.cpp`, `grid_search.cpp`, `grid_transform.cpp`, `test/grid_test.cpp` が本書で設計する未実装のファイルです。ほかのファイルは実装済みであり、`grid_common.cpp` と `row_bitboard_base.cpp` の説明は[グリッドの共通部品](grid_common.md)、`mirror_grid.cpp` の説明は [MirrorGrid](mirror_grid.md) に記載しています。

初版では `grid.cpp` と `grid_search.cpp` の幅優先探索までを実装します。入力、出力、盤面変換は `grid.cpp` に含めません。

## 座標と移動

行と列を名前で参照できるように、座標を構造体で表します。

```cpp
struct GridPosition {
    int row;
    int column;

    bool operator==(const GridPosition&) const = default;
};

struct GridOffset {
    int row_delta;
    int column_delta;
};
```

既定の 4 方向の順序は上、右、下、左とします。この順序では反対方向が `(direction + 2) % 4` で求まります。

```cpp
inline constexpr array<GridOffset, 4> grid_directions4 = {{
    {-1, 0},
    {0, 1},
    {1, 0},
    {0, -1},
}};

inline constexpr array<char, 4> grid_direction_chars4 = {'U', 'R', 'D', 'L'};
```

`GridOffset`, `grid_directions4`, `grid_direction_chars4` は `grid_common.cpp` で定義済みです。Bitboard や `MirrorGrid` が使用する `GridDirection` と同じ順序を 1 箇所で定義するためです。`GridPosition` は `grid.cpp` で定義します。

8 方向も上から時計回りに定義します。ナイトの移動、六角形の盤面、距離 2 の移動などは、利用側が `array<GridOffset, N>` を渡して指定します。方向の個数を実行時に定める場合は `vector<GridOffset>` を渡します。

## `Grid<Cell>`

### 内部表現

```cpp
template<class Cell>
class Grid {
public:
    using Row = vector<Cell>;
    using Storage = vector<Row>;
    using reference = typename Row::reference;
    using const_reference = typename Row::const_reference;

private:
    Storage cells_;
    int height_ = 0;
    int width_ = 0;
};
```

盤面を 1 次元配列に変換せず、`vector<vector<Cell>>` のまま保持します。利用側が 2 次元配列で作成した入力や状態は、コピーまたはムーブでそのまま渡すことができます。

`reference` は `Cell&` に固定しません。これにより `Grid<bool>` も `vector<bool>` の参照型を通して扱うことができます。

探索用の距離、親、訪問番号は 1 次元配列で保持します。

### 構築

```cpp
Grid();

explicit Grid(Storage cells);

Grid(int height, int width, const Cell& initial_value);

template<class MakeCell>
static Grid generate(int height, int width, MakeCell make_cell);
```

`generate` は `make_cell(row, column)` の戻り値で各セルを作成します。座標ごとに異なる初期値を設定でき、`Cell` に既定のコンストラクタを必要としません。

```cpp
vector<vector<Cell>> cells(height, vector<Cell>(width, initial_cell));
Grid<Cell> grid(move(cells));
```

構築時に全行の長さが同じであることを確認し、異なる場合は `invalid_argument` を送出します。探索用の 1 次元の番号を `int` で保持するため、`height * width <= INT_MAX` も構築の条件とします。積は 64 ビット整数で計算して判定します。

空の盤面を許可します。`height == 0` の場合は `width == 0` とします。`height > 0, width == 0` の盤面も許可しますが、探索の始点は指定できません。

### 基本操作

```cpp
int height() const;

int width() const;

int cell_count() const;

bool empty() const;

bool inside(int row, int column) const;

int index(int row, int column) const;

GridPosition position(int index) const;

reference operator()(int row, int column);

const_reference operator()(int row, int column) const;

const Storage& cells() const;

void replace_cells(Storage cells);

Storage take_cells();
```

`operator()`, `index`, `position` は呼び出し回数が多いため、範囲チェックは `TITAN_DEBUG` の定義時だけ行います。座標が盤面内か不明な場合は先に `inside` を呼び出します。

`cells()` は読み取り専用の参照を返します。変更可能な `vector<vector<Cell>>&` を公開すると、行の長さだけを変更して長方形の条件を満たさない状態にできるためです。セルの内容は `grid(row, column)` または全セルの走査で変更します。盤面全体を置き換える場合は `replace_cells`、盤面を外部へ取り出す場合は `take_cells` を使用します。

`take_cells` の呼び出し後、`Grid` は高さ 0、幅 0 の空の盤面になります。

### 全セルの走査

```cpp
template<class Function>
void for_each_cell(Function function);

template<class Function>
void for_each_cell(Function function) const;
```

関数は `function(row, column, cell)` の形式で呼び出されます。座標の配列は作成しません。変更可能な `Grid` では `cell` を変更できます。

条件を満たす座標を保存する場合は、`vector<GridPosition>` を返す次の関数を使用します。

```cpp
template<class Predicate>
vector<GridPosition> positions_if(Predicate predicate) const;
```

`predicate(row, column, cell)` が真になる座標を行優先の順で返します。

### 近傍の列挙

```cpp
template<class Directions, class Function>
void for_each_neighbor(
    int row,
    int column,
    const Directions& directions,
    Function function
) const;
```

移動先が盤面内である移動について `function(direction, next_row, next_column)` を呼び出します。`direction` は渡した移動の集合の中での番号です。戻り値用の `vector` は作成しません。

```cpp
grid.for_each_neighbor(row, column, grid_directions4,
    [&](int direction, int next_row, int next_column) {
        // 使用する近傍だけをここで処理する
    });
```

トーラスの盤面には別の関数を用意します。

```cpp
template<class Directions, class Function>
void for_each_wrapped_neighbor(
    int row,
    int column,
    const Directions& directions,
    Function function
) const;
```

高さ 1 や幅 1 の盤面では、異なる方向の移動先が同じセルになる場合があります。この関数は方向を列挙するため、重複を除外しません。行だけ、または列だけが循環する盤面では、利用側が座標を補正します。

## 探索と盤面の分離

`Grid<Cell>` はセルの保持だけを行います。幅優先探索の訪問配列やキューを `Grid` のメンバにすると、次の問題が発生します。

- `Grid` をコピーするたびに大きな作業配列も複製されます。
- 同じ盤面に対する 2 つの探索を同時に保持できません。
- 盤面を参照するだけの処理でも `mutable` の内部状態が必要になります。

このため探索用の配列は `GridBfsWorkspace` に分離します。同じ作業領域を使用する探索は逐次実行し、並列に探索する場合はスレッドごとに作業領域を用意します。

## 幅優先探索

### 作業領域

```cpp
class GridBfsWorkspace {
public:
    GridBfsWorkspace();

    GridBfsWorkspace(int height, int width);

    void resize(int height, int width);

    template<class Cell, class Directions, class CanMove>
    void bfs(
        const Grid<Cell>& grid,
        const vector<GridPosition>& starts,
        const Directions& directions,
        CanMove can_move
    );

    template<class Cell, class Directions, class CanMove>
    void bfs_with_path(
        const Grid<Cell>& grid,
        const vector<GridPosition>& starts,
        const Directions& directions,
        CanMove can_move
    );

    template<class Cell, class Directions, class CanMove>
    void bfs_nearest_source(
        const Grid<Cell>& grid,
        const vector<GridPosition>& starts,
        const Directions& directions,
        CanMove can_move
    );

    template<class Cell, class EnumerateNeighbors>
    void bfs_custom(
        const Grid<Cell>& grid,
        const vector<GridPosition>& starts,
        EnumerateNeighbors enumerate_neighbors
    );

    bool reached(int row, int column) const;

    int distance(int row, int column) const;

    int source(int row, int column) const;

    vector<GridPosition> restore_path(int row, int column) const;
};
```

`can_move` は次の形式で呼び出されます。

```cpp
bool can_move(int row, int column, int next_row, int next_column);
```

セル自体は引数に含めません。必要な場合はラムダ式から `grid` を参照します。この形式では、移動先が壁でないという条件に加えて、高低差、一方通行、現在のセルとの組み合わせによる条件も表現できます。

`bfs` は指定された移動の集合のうち移動先が盤面内である移動を探索します。上下の端が接続する盤面、ワープ、セルごとに異なる移動などには `bfs_custom` を使用します。

`enumerate_neighbors` は `enumerate_neighbors(row, column, visit)` の形式で呼び出され、移動可能な移動先ごとに `visit(next_row, next_column)` を呼び出します。移動先は盤面内でなければなりません。`bfs` は `bfs_custom` と同じ探索を、メモリ確保を伴わずに行う関数として実装します。

```cpp
GridBfsWorkspace workspace;
workspace.bfs_with_path(
    grid,
    {{start_row, start_column}},
    grid_directions4,
    [&](int row, int column, int next_row, int next_column) {
        return !grid(next_row, next_column).wall;
    }
);

int distance = workspace.distance(goal_row, goal_column);
vector<GridPosition> path = workspace.restore_path(goal_row, goal_column);
```

`bfs` は距離だけを保存します。`bfs_with_path` は親の 1 次元の番号も保存し、`bfs_nearest_source` は各セルに最初に到達した始点の番号も保存します。使用しない配列は確保しません。一度確保した配列は次回以降も再利用します。

始点が複数ある場合、`source` は `starts` の中での番号を返します。距離が同じ場合は、キューに先に追加された始点と、先に列挙された方向が選択されます。始点と方向の順序が同じであれば結果も同じになります。

### 初期化の定数倍

探索のたびに距離配列の全体を `-1` で初期化することは行いません。各セルに最後に訪問した探索の番号を保持し、現在の探索の番号と一致するセルだけを訪問済みと判定します。

```text
visited_generation[index] == current_generation
```

探索の番号が上限に達した場合だけ配列全体を 0 に初期化します。この方式では、到達範囲が小さい探索を繰り返す場合に毎回の `O(height * width)` の初期化が不要になります。

キューは `vector<int>` と先頭位置で管理し、前回確保した容量を再利用します。

### 簡易関数

探索を 1 回だけ行う場合のために、距離を `Grid<int>` で返す関数も用意します。

```cpp
template<class Cell, class Directions, class CanMove>
Grid<int> grid_bfs_distances(
    const Grid<Cell>& grid,
    const vector<GridPosition>& starts,
    const Directions& directions,
    CanMove can_move
);
```

到達できないセルの距離は `-1` とします。この関数は結果を作成するため、毎回全セル分の初期化とメモリ確保を行います。探索を繰り返す場合は `GridBfsWorkspace` を使用します。

## 連結成分

```cpp
struct GridComponents {
    Grid<int> label;
    vector<int> sizes;
};

template<class Cell, class Directions, class IsActive>
GridComponents grid_connected_components(
    const Grid<Cell>& grid,
    const Directions& directions,
    IsActive is_active
);
```

`is_active(row, column)` が真になるセルだけを対象とし、隣接する対象のセルを同じ連結成分にします。対象外のセルの連結成分の番号は `-1` とします。

辺ごとに通行可否が異なる場合と有向の移動の場合は、この関数の対象外とします。必要な場合は利用側が幅優先探索を繰り返します。

## 重み付き最短路

幅優先探索の実装後に、幅優先探索の作業領域と同じ方式で次のクラスを追加します。

```cpp
template<class Distance>
class GridShortestPathWorkspace {
public:
    template<class Cell, class Directions, class CanMove, class EdgeCost>
    void zero_one_bfs(...);

    template<class Cell, class Directions, class CanMove, class EdgeCost>
    void dijkstra(...);
};
```

`edge_cost(row, column, next_row, next_column)` は移動コストを返します。0-1 幅優先探索はコストが 0 または 1 であること、ダイクストラ法はコストが非負であることを使用条件とします。通行可否とコストは別の関数で指定し、通行できない辺を巨大な値のコストで表すことは行いません。

距離の型、無限大の値、加算時の値の範囲は呼び出し側が指定します。

## 盤面変換

盤面変換は `Grid` の状態管理と独立しているため、メンバではない関数として実装します。

```cpp
template<class Cell>
Grid<Cell> grid_subgrid(
    const Grid<Cell>& grid,
    int top,
    int left,
    int height,
    int width
);

template<class Cell>
Grid<Cell> grid_transpose(const Grid<Cell>& grid);

template<class Cell>
Grid<Cell> grid_rotate_right(const Grid<Cell>& grid);

template<class Cell>
Grid<Cell> grid_rotate_180(const Grid<Cell>& grid);

template<class Cell>
Grid<Cell> grid_rotate_left(const Grid<Cell>& grid);

template<class Cell>
Grid<Cell> grid_flip_rows(const Grid<Cell>& grid);

template<class Cell>
Grid<Cell> grid_flip_columns(const Grid<Cell>& grid);
```

これらの関数を呼び出す場合だけ `Cell` のコピー構築を必要とします。

回転と反転で得られる 8 通りを列挙する関数は重複を除外しません。重複の除外には `Cell` の比較が必要になるためです。

## 文字の盤面

`Grid<Cell>` は標準入力の形式を規定しません。文字の盤面は使用頻度が高いため、次の補助関数を用意します。

```cpp
Grid<char> make_char_grid(const vector<string>& rows);

vector<string> make_strings(const Grid<char>& grid);
```

整数の盤面や構造体の盤面は、利用側が `vector<vector<Cell>>` を作成して `Grid<Cell>` に渡します。

## 構造体のセルの使用例

```cpp
struct Cell {
    int height;
    int item_count;
    bool wall;
};

vector<vector<Cell>> cells(height, vector<Cell>(width));
for (int row = 0; row < height; ++row) {
    for (int column = 0; column < width; ++column) {
        cells[row][column] = read_cell();
    }
}

Grid<Cell> grid(move(cells));

grid.for_each_neighbor(row, column, grid_directions4,
    [&](int direction, int next_row, int next_column) {
        const Cell& next = grid(next_row, next_column);
        if (next.wall) return;
        // 候補を評価する
    });
```

`Grid` は `Cell` の内容に関与しません。壁、得点、所有者、高さ、配置された物などを `Cell` のメンバとして保持できます。

## 計算量

方向の個数を `d`、セル数を `n = height * width`、到達したセルの個数を `r`、探索した移動の回数を `e` とします。

| 処理 | 時間計算量 | 追加のメモリ |
|---|---:|---:|
| ムーブによる盤面の構築 | `O(height)` | `O(1)` |
| コピーによる盤面の構築 | `O(n)` | `O(n)` |
| セルへのアクセス、範囲内の判定、番号の変換 | `O(1)` | `O(1)` |
| 1 つのセルの近傍の列挙 | `O(d)` | `O(1)` |
| 全セルの走査 | `O(n)` | `O(1)` |
| 幅優先探索 | `O(r + e)` | 作業領域 `O(n)` |
| 距離の盤面の作成 | `O(n)` | `O(n)` |
| 連結成分 | `O(n d)` | `O(n)` |
| 0-1 幅優先探索 | `O(n + e)` | `O(n)` |
| ダイクストラ法 | `O((n + e) log n)` | `O(n)` |
| 切り出し、回転、反転 | 出力のセル数に比例 | 出力のセル数に比例 |

4 近傍と 8 近傍では `d` が定数であるため、盤面全体の幅優先探索と連結成分の時間計算量は `O(n)` です。

## 使用条件

- 盤面は長方形です。
- 行数、列数、セル数が `int` に収まります。
- `index(row, column)` に渡す座標は盤面内です。
- 探索中に盤面の大きさを変更しません。セルの内容は、探索関数の実行中でなければ変更できます。
- `can_move`, `is_active`, `edge_cost` は、探索中に同じ入力に対して同じ結果を返します。
- 1 つの作業領域を複数の探索で同時に使用しません。
- `Grid<Cell>` の基本操作は `Cell` の比較、ハッシュ、出力演算子を必要としません。

## 初版の対象外

- 壁の文字 `'#'` や道の文字 `'.'` の固定
- セルの読み込みと出力の形式
- 問題固有の得点や状態遷移
- 時刻を状態に含む探索
- 無限の盤面や疎な座標集合
- ビット並列処理
- 盤面全体の暗黙のコピーを伴う探索状態

時刻を含む探索では `(row, column, time)` が頂点となり、2 次元の作業領域では扱うことができません。疎な盤面にはハッシュ表やグラフによる表現が適します。

## 実装順

1. `GridPosition`, `Grid<Cell>`、任意の方向の近傍の列挙
2. `GridBfsWorkspace` の距離だけを保存する探索と経路復元
3. 多始点で最も近い始点を求める探索と連結成分
4. 0-1 幅優先探索とダイクストラ法
5. 盤面変換と文字の盤面用の補助関数

手順 1 では、`GridOffset` と方向の表に `grid_common.cpp` の定義を使用します。

各段階で、単純な 2 次元配列による実装と結果を照合します。次の場合を照合の対象に含めます。

- 空の盤面
- 1 行の盤面
- 1 列の盤面
- `Grid<bool>`
- 構造体のセル
- 作業領域の連続した再利用
- 探索の間でセルの内容を変更した場合
