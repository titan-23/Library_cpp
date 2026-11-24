# グリッドの共通部品

グリッドの各ファイルが共通で使用する定義と処理を説明します。方向などの基本的な定義は `grid_common.cpp`、1 行を 1 つの整数で保持する集合の演算は `row_bitboard_base.cpp` にあります。

言語は C++20、名前空間は `titan23` であり、各ファイルの先頭で `using namespace std;` を宣言しています。

## ファイルの依存関係

```text
titan_cpplib/ahc/grid/
  grid_common.cpp          方向、近傍の種類、アサーション用マクロ
  row_bitboard_base.cpp    1 行を 1 つの整数で保持する集合の演算
  bitboard.cpp             セルが壁か道かを保持する Bitboard
  flat_bitboard.cpp        盤面全体を 1 つの整数で保持する Bitboard
  edge_wall_bitboard.cpp   セル間に壁がある Bitboard
  mirror_grid.cpp          鏡とブロックを配置するグリッド
```

| ファイル | include するファイル |
|---|---|
| `row_bitboard_base.cpp` | `grid_common.cpp` |
| `bitboard.cpp`, `edge_wall_bitboard.cpp` | `row_bitboard_base.cpp` |
| `flat_bitboard.cpp`, `mirror_grid.cpp` | `grid_common.cpp` |

利用側は使用するクラスのファイルだけを include します。共通ファイルは `cpp_expander.py` が 1 回だけ展開します。

## `grid_common.cpp`

```cpp
enum class GridDirection : uint8_t { Up, Right, Down, Left };

constexpr GridDirection opposite(GridDirection direction);  // 上と下、右と左を入れ替える

struct GridOffset {
    int row_delta;
    int column_delta;
};

inline constexpr array<GridOffset, 4> grid_directions4;      // 上、右、下、左
inline constexpr array<GridOffset, 8> grid_directions8;      // 上から時計回り
inline constexpr array<char, 4> grid_direction_chars4;       // 'U', 'R', 'D', 'L'

constexpr GridOffset grid_direction(Neighborhood neighborhood, int k);

enum class Neighborhood { Four, Eight };
```

4 方向の順序は上、右、下、左であり、`GridDirection` の値と `grid_directions4`, `grid_direction_chars4` の添字が一致します。反対方向の添字は、添字に 2 を加えて 4 で割った余りです。

`grid_directions8` の順序は上から時計回りに上、右上、右、右下、下、左下、左、左上です。`grid_directions4` は `grid_directions8` の先頭の 4 個とは一致しません。近傍の種類に応じて表を選択する場合は `grid_direction(neighborhood, k)` を使用します。`Four` では `k < 4`、`Eight` では `k < 8` の範囲で呼び出します。

### 例: 4 方向の列挙

```cpp
string moves;
int row = 1, column = 1;
for (int d = 0; d < 4; ++d) {
    int next_row = row + grid_directions4[d].row_delta;
    int next_column = column + grid_directions4[d].column_delta;
    if (next_row == 0 || next_column == 2) moves += grid_direction_chars4[d];
}
// moves == "UR"

assert(opposite(GridDirection::Up) == GridDirection::Down);
assert((GridDirection)1 == GridDirection::Right);
```

### アサーション用マクロ

`TITAN_GRID_ASSERT(expr)` は `TITAN_DEBUG` が定義されている場合は `assert(expr)` に展開され、定義されていない場合は式を評価しません。グリッドの各クラスは引数の範囲チェックにこのマクロを使用します。開発時は include の前に `#define TITAN_DEBUG` を記述します。

このマクロは `#undef` されないため、提出コードの末尾まで定義された状態になります。

## `row_bitboard_base.cpp`

`RowBitboardBase<Word>` は `Bitboard` と `EdgeWallBitboard` の基底クラスです。どちらのクラスも 1 行を 1 つの `Word` で保持する `Set = vector<Word>` で集合を表すため、盤面の形 (`H`, `W`) だけで結果が定まる演算を基底クラスに実装しています。利用側は基底クラスを直接使用せず、`board.count(s)` のように派生クラスのメソッドとして呼び出します。

### 基底クラスの演算

| 分類 | メソッド |
|---|---|
| 形 | `height`, `width`, `cell_count`, `inside` |
| ビット操作 | `popcount`, `ctz`, `msb` |
| 作成と 1 つのセルの操作 | `new_set`, `clear(s)`, `test`, `set`, `reset`, `flip` |
| 集計と比較 | `count`, `count_and`, `any`, `none`, `equals`, `intersects`, `disjoint`, `is_subset`, `hash64(s)` |
| 集合どうしの演算 | `band`, `bor`, `bxor`, `bdiff`, `iand`, `ior`, `ixor`, `idiff` |
| 列挙と位置 | `for_each`, `cells`, `first`, `kth_cell`, `bounding_box`, `rect` |
| 平行移動 | `shift`, `shift_left`, `shift_right`, `shift_up`, `shift_down` |
| 出力 | `to_grid`, `print` |

各メソッドの仕様は [Bitboard](bitboard.md) に記載しています。基底クラスは作業領域を保持しないため、どの演算も同じインスタンスに対して同時に呼び出すことができます。

### 幅 `W` 以上のビット

`Set` の各行で幅 `W` 以上のビットは常に 0 とします。基底クラスのどの演算も、この条件を満たす入力からこの条件を満たす結果を作成します。利用側が `Set` の要素を直接変更する場合は、この条件を満たすようにします。

`to_grid`, `hash64(s)`, `bounding_box` は幅 `W` 以上のビットを除外して参照します。それ以外の演算は入力がこの条件を満たすことを前提とし、`TITAN_DEBUG` の定義時に入力の全行について条件を確認します。

### 派生クラスの演算

次の演算は盤面の内容や近傍の種類によって結果が異なるため、派生クラスに実装しています。

| 演算 | `Bitboard` | `EdgeWallBitboard` |
|---|---|---|
| `complement_into(a, out)` | `road & ~a` | 盤面全体 `& ~a` |
| `make_set(cells, out)` | 壁のセルを除外して追加する | すべてのセルを追加する |
| 近傍に依存する演算 | `step`, `expand`, `border`, `dilate`, `erode`, `inner_border` | `move_success`, `advance`, `advance_uncertain`, `expand` |
| 探索 | `flood`, `bfs_dist`, `shortest_path`、連結成分など | `flood`, `bfs_dist`, `shortest_path` |

`complement_into` と `make_set` は、どちらのクラスでも通行できるセルの全体を全体集合とします。全体集合は `Bitboard` では道のセル、`EdgeWallBitboard` では全セルです。

### 派生クラスの記述方法

`Bitboard<Word, NB>` と `EdgeWallBitboard<Word>` は `RowBitboardBase<Word>` を public 継承しています。

基底クラスの型がテンプレート引数 `Word` に依存するため、派生クラスの中では基底クラスのメンバ (`H`, `bit`, `count` など) を修飾なしの名前で参照できません。このため派生クラスの先頭で `using Base::count;` のような using 宣言によりメンバを導入しています。public のメンバは public の区画に、protected のメンバは private の区画に記述します。using 宣言を記述した区画が、派生クラスでのアクセス指定になるためです。

派生クラスが基底クラスと同じ名前のメンバを定義すると、基底クラスの同名のメンバはすべて隠蔽されます。`Bitboard` は道のセルのハッシュ値を返す引数なしの `hash64()` を定義しているが、`using Base::hash64;` を記述しているため、集合のハッシュ値を返す `hash64(s)` も呼び出すことができます。

### `FlatBitboard`

`FlatBitboard` は `Set = Word` であり、集合の表現が異なります。そのため基底クラスを継承せず、実装も共有していません。メソッドの名前と仕様は基底クラスに合わせているため、利用側は include と型名を変更するだけで `Bitboard` と切り替えることができます。

## 以前の版からの変更点

共通化に伴って次の点を変更しました。以前のコードを再利用する場合は次の表を確認します。

| 対象 | 変更点 |
|---|---|
| `bitboard_common.cpp` | `grid_common.cpp` に名前を変更した。`bitboard_common.cpp` を直接 include しているコードは修正が必要である。`bitboard.cpp` などを include しているだけのコードは修正不要である |
| `Bitboard`, `FlatBitboard` の `clear()`, `fill()` | 引数なしのオーバーロードを廃止した。全セルを道にする場合は `open_all()`、全セルを壁にする場合は `block_all()` を使用する。集合を空にする `clear(s)` は引き続き使用できる |
| `Bitboard`, `FlatBitboard` の `shortest_path` | 近傍を探索する順序を右、左、下、上から共通の順序である上、右、下、左に変更した。同じ長さの最短路が複数ある場合に返す経路が以前と異なる。距離、最も近い始点、関節点の結果は変化しない |
| `EdgeWallBitboard` の集合演算 | `to_grid` 以外の演算で、集合を参照するたびに行っていたマスクを廃止した。幅 `W` 以上のビットが 1 である集合を渡すと、そのビットが結果に含まれる。`TITAN_DEBUG` の定義時はアサーションで停止する |
| `EdgeWallBitboard` に追加した演算 | `hash64`, `first`, `kth_cell`, `bounding_box`, `rect`, `shift`, `shift_left`, `shift_right`, `shift_up`, `shift_down`, `popcount`, `ctz`, `msb` |
| `Bitboard`, `FlatBitboard` に追加した演算 | `cell_count`。`FlatBitboard` には `msb` も追加した |
| アサーション用マクロ | `TITAN_BITBOARD_ASSERT`, `TITAN_FLAT_BITBOARD_ASSERT`, `TITAN_EDGE_WALL_BITBOARD_ASSERT` を `TITAN_GRID_ASSERT` に統合した |
