# 一般グリッド設計

## 目的

AHCで繰り返し使う二次元盤面を、セルの型や壁の表現に依存しない形で扱う。
盤面の実体は `vector<vector<Cell>>` とし、文字、整数、構造体を同じ `Grid<Cell>` で保持する。

このライブラリが担当するのは次の範囲とする。

- 矩形な盤面の保持と座標アクセス
- 範囲内判定、座標と一次元番号の相互変換
- 4近傍、8近傍、任意の移動集合の列挙
- メモリを使い回す幅優先探索と経路復元
- 連結成分、0-1幅優先探索、ダイクストラ法
- 切り出し、回転、反転などの盤面変換

壁文字、通行条件、移動費用は問題ごとに異なるため、`Grid` 自体には持たせない。探索時に関数として渡す。
幅128以下の盤面をビット並列に処理する `bitboard.cpp` と `flat_bitboard.cpp` は置き換えず、用途で使い分ける。

## 配置

```text
titan_cpplib/ahc/grid/
  README.md
  bitboard_common.cpp       4近傍と8近傍の共通定義
  bitboard.cpp              1行を一つの整数で持つBitboard
  flat_bitboard.cpp         盤面全体を一つの整数で持つBitboard
  grid.cpp                  盤面、座標、近傍列挙
  grid_search.cpp           幅優先探索、連結成分、重み付き最短路
  grid_transform.cpp        切り出し、回転、反転
  docs/
    bitboard.md             Bitboardの使い方
    design.md               本書
  test/
    grid_test.cpp
```

初版は `grid.cpp` と、`grid_search.cpp` の幅優先探索までを実装する。入力、出力、盤面変換を中心部分へ混ぜない。

## 座標と移動

`pair<int, int>` の `first` と `second` を毎回読み解かなくてよいように、行と列を名前で持つ。

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

既定の4方向は上、右、下、左の順に固定する。この順なら、反対方向は `(direction + 2) % 4` で得られる。

```cpp
inline constexpr array<GridOffset, 4> grid_directions4 = {{
    {-1, 0},
    {0, 1},
    {1, 0},
    {0, -1},
}};

inline constexpr array<char, 4> grid_direction_chars4 = {'U', 'R', 'D', 'L'};
```

8方向も上から時計回りに定義する。ナイト移動、六角形、距離2の移動などは、利用側が `array<GridOffset, N>` を渡せばよい。方向数を実行時に決めたい場合は `vector<GridOffset>` も使える。

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

内部表現を一次元配列へ変換せず、`vector<vector<Cell>>` のまま保持する。既に二次元配列で入力や状態を作っている場合は、コピーまたはムーブでそのまま渡せる。

`reference` を `Cell&` と決め打ちしない。これにより `Grid<bool>` も `vector<bool>` の参照型を通して扱える。

この表現は行ごとにメモリを確保するため、盤面全体を一次元配列で持つ実装よりメモリの連続性では劣る。一方、既存の二次元配列をそのまま受け取れ、行と列を取り違えにくい利点がある。探索用の距離、親、訪問番号は一次元配列で持ち、頻繁に走査する作業領域の定数倍は抑える。

### 構築

```cpp
Grid();

explicit Grid(Storage cells);

Grid(int height, int width, const Cell& initial_value);

template<class MakeCell>
static Grid generate(int height, int width, MakeCell make_cell);
```

`generate` は `make_cell(row, column)` の戻り値で各セルを作る。座標ごとに異なる初期値を持たせられ、`Cell` の既定構築も要求しない。

```cpp
vector<vector<Cell>> cells(height, vector<Cell>(width, initial_cell));
Grid<Cell> grid(move(cells));
```

全行の長さが同じことを構築時に確認する。不揃いなら `invalid_argument` とする。探索用の一次元番号を `int` で持つため、`height * width <= INT_MAX` も構築条件にする。積は広い整数型で検査してから `int` へ変換する。

空の盤面は許す。`height == 0` のとき `width == 0` とする。`height > 0, width == 0` も矩形として許すが、探索の始点は置けない。

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

`operator()`、`index`、`position` は頻繁に呼ぶため、範囲検査は `TITAN_DEBUG` 時だけ行う。範囲が不確かなときは先に `inside` を呼ぶ。

`cells()` は読み取り専用で返す。変更可能な `vector<vector<Cell>>&` を公開すると、行の長さだけを変更して矩形条件を壊せるためである。セルの内容は `grid(row, column)` または全セル走査から変更する。盤面全体を置き換える場合は `replace_cells`、外へ取り出す場合は `take_cells` を使う。

`take_cells` 後の `Grid` は高さ0、幅0の空盤面へ戻す。

### 全セル走査

```cpp
template<class Function>
void for_each_cell(Function function);

template<class Function>
void for_each_cell(Function function) const;
```

関数は `function(row, column, cell)` の形で呼ぶ。中間の座標配列を作らない。変更可能な `Grid` では `cell` も変更できる。

条件を満たす座標を保存したい場合だけ、別の関数で `vector<GridPosition>` を返す。

```cpp
template<class Predicate>
vector<GridPosition> positions_if(Predicate predicate) const;
```

`predicate(row, column, cell)` が真の座標を行優先で返す。

### 近傍列挙

```cpp
template<class Directions, class Function>
void for_each_neighbor(
    int row,
    int column,
    const Directions& directions,
    Function function
) const;
```

盤面内に入る移動だけについて、`function(direction, next_row, next_column)` を呼ぶ。`direction` は渡された移動集合内の番号である。戻り値用の `vector` は作らない。

```cpp
grid.for_each_neighbor(row, column, grid_directions4,
    [&](int direction, int next_row, int next_column) {
        // 使用する近傍だけここで処理する
    });
```

トーラス盤面用には別関数を用意する。

```cpp
template<class Directions, class Function>
void for_each_wrapped_neighbor(
    int row,
    int column,
    const Directions& directions,
    Function function
) const;
```

高さ1や幅1では、異なる方向が同じセルへ到達することがある。この関数は「方向の列挙」なので重複を除かない。行だけ、または列だけを循環させる盤面は、利用側で座標を補正する。

## 探索と盤面を分離する

`Grid<Cell>` はセルを保持するだけにする。幅優先探索の訪問配列やキューを `Grid` のメンバにすると、次の問題が起きる。

- `Grid` をコピーするたびに大きな作業配列まで複製される
- 同じ盤面に対する二つの探索を同時に持てない
- 盤面を読むだけの処理でも `mutable` な内部状態が必要になる

そのため探索用配列は `GridBfsWorkspace` に分ける。同じ作業領域を使う探索は逐次実行し、並列に探索する場合はスレッドごとに作業領域を持つ。

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

`can_move` は次の形で呼ぶ。

```cpp
bool can_move(int row, int column, int next_row, int next_column);
```

セルそのものを引数へ固定しない。必要ならラムダから `grid` を参照する。この形なら、到着先が壁でないという条件だけでなく、高低差、一方通行、現在セルとの組み合わせも表現できる。

通常の `bfs` は、指定された移動集合のうち盤面内に入る移動を調べる。上下端がつながる盤面、ワープ、セルごとに異なる移動などは `bfs_custom` を使う。

`enumerate_neighbors` は `enumerate_neighbors(row, column, visit)` の形で呼ばれ、移動可能な行き先ごとに `visit(next_row, next_column)` を呼ぶ。行き先は盤面内でなければならない。これにより、探索側をトーラス盤面や問題固有の移動規則へ依存させずに済む。通常の `bfs` は、この一般形に対するメモリ確保を伴わない薄い入口として実装する。

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

`bfs` は距離だけを保存する。`bfs_with_path` は親の一次元番号も保存し、`bfs_nearest_source` は各セルへ最初に到達した始点番号も保存する。使わない配列は確保しない。一度必要になった配列は次回以降も再利用する。

始点が複数ある場合、`source` は `starts` 内の番号を返す。同じ距離なら、キューへ先に入った始点と、先に列挙された方向が選ばれる。始点や方向の順序を固定すれば結果も決定的になる。

### 初期化の定数倍

各探索の前に距離配列全体を `-1` で埋めない。各セルに最後に訪問した探索番号を持たせ、今回の番号と一致するセルだけを訪問済みとみなす。

```text
visited_generation[index] == current_generation
```

探索番号が上限へ達したときだけ配列全体を0へ戻す。この方式により、小さい範囲しか到達しない探索を繰り返す場合、毎回の `O(height * width)` 初期化を避けられる。

キューは `vector<int>` と先頭位置で管理し、`queue` や `deque` の要素ごとの処理を避ける。前回確保した容量を再利用する。

### 簡易関数

一度しか探索しない場合のため、距離を `Grid<int>` として返す関数も用意する。

```cpp
template<class Cell, class Directions, class CanMove>
Grid<int> grid_bfs_distances(
    const Grid<Cell>& grid,
    const vector<GridPosition>& starts,
    const Directions& directions,
    CanMove can_move
);
```

未到達は `-1` とする。この関数は結果を作るため必ず全セル分の初期化とメモリ確保を行う。探索を繰り返すAHCでは `GridBfsWorkspace` を優先する。

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

`is_active(row, column)` が真のセルだけを対象とし、隣接する対象セルを同じ成分にする。対象外セルの番号は `-1` とする。

辺ごとに通行可否が変わる場合や有向移動の場合は、単純な連結成分とは意味が異なるため、この関数へ設定を増やさない。必要なら幅優先探索を利用側で繰り返す。

## 重み付き最短路

初版の幅優先探索が安定してから、同じ作業領域の考え方で次を追加する。

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

`edge_cost(row, column, next_row, next_column)` が移動費用を返す。0-1幅優先探索は費用が0または1、ダイクストラ法は非負であることを利用条件にする。通行可否と費用を別の関数にし、不通辺を特別な巨大値で表さない。

距離型、無限大、加算時の範囲は呼び出し側が明示する。整数と浮動小数を一つの番兵規則へ無理に統一しない。

## 盤面変換

盤面変換は `Grid` の状態管理とは独立しているため、自由関数にする。

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

これらを呼ぶ場合だけ `Cell` のコピー構築を要求する。中心の `Grid<Cell>` は、変換を使わない限りこの条件を要求しない。

回転と反転から得られる8通りをまとめる関数は、重複除去をしない。重複除去には `Cell` の比較可能性が必要になり、一般のセル型へ不要な条件を課すためである。

## 文字盤

一般の `Grid<Cell>` に標準入力の形式を決めさせない。文字盤だけは使用頻度が高いため、別の補助関数を検討する。

```cpp
Grid<char> make_char_grid(const vector<string>& rows);

vector<string> make_strings(const Grid<char>& grid);
```

整数盤や構造体の盤面は問題側で `vector<vector<Cell>>` を作り、`Grid<Cell>` へ渡す。

## 構造体セルの使用例

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

`Grid` は `Cell` の意味を知らない。壁、得点、所有者、高さ、置かれた物などを自由に持たせられる。

## 計算量

方向数を `d`、セル数を `n = height * width`、到達セル数を `r`、実際に調べた移動数を `e` とする。

| 処理 | 時間 | 追加メモリ |
|---|---:|---:|
| ムーブによる盤面構築 | `O(height)` の矩形確認 | `O(1)` |
| コピーによる盤面構築 | `O(n)` | `O(n)` |
| セルアクセス、範囲判定、番号変換 | `O(1)` | `O(1)` |
| 1セルの近傍列挙 | `O(d)` | `O(1)` |
| 全セル走査 | `O(n)` | `O(1)` |
| 幅優先探索 | `O(r + e)` | 作業領域 `O(n)` |
| 距離盤面の生成 | `O(n)` | `O(n)` |
| 連結成分 | `O(n d)` | `O(n)` |
| 0-1幅優先探索 | `O(n + e)` | `O(n)` |
| ダイクストラ法 | `O((n + e) log n)` | `O(n)` |
| 切り出し、回転、反転 | 出力セル数に比例 | 出力セル数に比例 |

4近傍や8近傍では `d` が定数なので、盤面全体の幅優先探索と連結成分は `O(n)` になる。

## 利用条件

- 盤面は矩形である。
- 行数、列数、セル数が `int` に収まる。
- `index(row, column)` へ渡す座標は盤面内である。
- 探索中に盤面の大きさを変更しない。セル内容の変更は、探索関数を呼んでいない間ならよい。
- `can_move`、`is_active`、`edge_cost` は探索中に同じ入力へ同じ結果を返す。
- 作業領域は一つの探索中に別の探索から使わない。
- `Grid<Cell>` の基本操作は `Cell` の比較、ハッシュ、出力演算子を要求しない。

## 初版に入れないもの

- 壁文字 `'#'` や道文字 `'.'` の固定
- セルの読み込み、出力形式
- 問題固有の得点や状態遷移
- 時刻を状態に含む探索
- 無限盤面や疎な座標集合
- ビット並列処理
- 盤面全体の暗黙コピーを伴う探索状態

時刻付き探索は `(row, column, time)` を頂点とする問題であり、二次元の作業領域へ押し込めない。疎な盤面はハッシュ表やグラフ表現の方が適する。これらまで一つの `Grid` に含めず、矩形盤面の共通部分だけを安定させる。

## 実装順

1. `GridPosition`、`GridOffset`、`Grid<Cell>`、任意方向の近傍列挙
2. `GridBfsWorkspace` の距離版と経路復元版
3. 多始点最近傍と連結成分
4. 0-1幅優先探索とダイクストラ法
5. 盤面変換と文字盤用補助関数

各段階で、単純な二次元配列による実装と結果を照合する。特に空盤面、1行、1列、`Grid<bool>`、構造体セル、作業領域の連続再利用、探索間でセル内容を変更した場合を確認する。
