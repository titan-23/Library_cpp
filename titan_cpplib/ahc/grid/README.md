# グリッド

AHC の盤面処理のライブラリを格納したディレクトリです。壁と道の盤面をビット並列で処理する Bitboard と、鏡で光を反射させる盤面を扱う MirrorGrid があります。

## 実装の選択

| 盤面 | 実装 | 文書 |
|---|---|---|
| セルが壁か道で、幅が 64 以下または 128 以下 | `bitboard.cpp` | [Bitboard](docs/bitboard.md) |
| セルが壁か道で、全セル数が 64 以下または 128 以下 | `flat_bitboard.cpp` | [Bitboard](docs/bitboard.md) |
| 隣接するセルの間に壁があり、幅が 64 以下または 128 以下 | `edge_wall_bitboard.cpp` | [EdgeWallBitboard](docs/edge_wall_bitboard.md) |
| 鏡やブロックを配置し、光の終点を求める | `mirror_grid.cpp` | [MirrorGrid](docs/mirror_grid.md) |

include するファイルは、使用するクラスのファイル 1 つだけです。

```cpp
#include "titan_cpplib/ahc/grid/mirror_grid.cpp"
using namespace titan23;
```

- `bitboard.cpp` は 1 行を 1 つの整数で保持します。
- `flat_bitboard.cpp` は盤面全体を 1 つの整数で保持します。
- `bitboard.cpp` と `flat_bitboard.cpp` は 4 近傍と 8 近傍、集合演算、到達判定、最短距離、連結成分などに対応します。
- `edge_wall_bitboard.cpp` は方向ごとに通行可能な辺を保持する 4 近傍専用の実装です。壁があるセルが元の位置に残る動作を含む、集合の移動にも対応します。
- `mirror_grid.cpp` は直進、`/`、`\`、ブロックの 4 種類のセルを扱います。外周のポートから入射した光の終点を定数時間で返します。1 つのセルを変更した場合に終点が変化するポートも、盤面を変更せずに求めることができます。

## 構成

| ファイル | 内容 |
|---|---|
| `grid_common.cpp` | 方向 `GridDirection`、4 近傍と 8 近傍の移動、アサーション用マクロ |
| `row_bitboard_base.cpp` | `Bitboard` と `EdgeWallBitboard` に共通の集合演算 |
| `bitboard.cpp` | 1 行を 1 つの整数で保持する Bitboard |
| `flat_bitboard.cpp` | 盤面全体を 1 つの整数で保持する Bitboard |
| `edge_wall_bitboard.cpp` | セル間に壁がある Bitboard |
| `mirror_grid.cpp` | 鏡とブロックを配置するグリッド |
| [local_connectivity.cpp](local_connectivity.cpp) | 色付き盤面の周囲 8 マスから、1 マスを削除した後の連結性を局所的に判定する |
| [euler_contribution.cpp](euler_contribution.cpp) | 2 × 2 の配置からオイラー数の 4 倍に対する寄与を求める |

`grid_common.cpp` と `row_bitboard_base.cpp` は、ほかのファイルが include する共通部品です。内容は[グリッドの共通部品](docs/grid_common.md)に記載しています。`clear()` と `fill()` の廃止など、以前の版からの変更点も同じ文書に記載しています。

[一般 Grid の設計案](docs/design.md)は、セルの型を限定しない `Grid<Cell>` の設計案であり、未実装です。

### 色付き盤面の探索

[盤面の 1 マス変更に対する連結性判定と差分評価](docs/local_update_tips.md)では、4 近傍と 8 近傍での連結性判定、削除候補の更新、空きマスの次数による評価、色間の接触辺数と周長、穴の個数を扱います。

## 共通の注意点

- Bitboard の探索は内部の作業領域を共有します。同じインスタンスで複数の探索を同時に実行しないでください。
- 引数の範囲チェックは `TITAN_DEBUG` の定義時だけ行います。開発時は include の前に `#define TITAN_DEBUG` を記述します。
- 方向の順序はどのファイルでも上、右、下、左です。
