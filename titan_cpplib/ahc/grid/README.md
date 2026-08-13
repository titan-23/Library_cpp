# グリッド

AHCの盤面処理をまとめたディレクトリです。セル壁と辺壁の盤面をビット並列で処理できます。

## 選び方

| 条件 | 実装 |
|---|---|
| 幅が64以下または128以下 | `bitboard.cpp` |
| 全セル数が64以下または128以下 | `flat_bitboard.cpp` |
| 隣接セル間に壁があり、幅が64以下または128以下 | `edge_wall_bitboard.cpp` |

`bitboard.cpp` は1行を一つの整数で持ちます。`flat_bitboard.cpp` は盤面全体を一つの整数で持ちます。どちらも4近傍・8近傍、集合演算、到達判定、最短距離、連結成分などに対応します。

`edge_wall_bitboard.cpp` は方向ごとの通行可能な辺を持つ4近傍専用の実装です。壁に当たった場合の停止を含む集合遷移にも対応します。

## 構成

- `bitboard_common.cpp`: 4近傍と8近傍の共通定義
- `edge_wall_bitboard.cpp`: セル間の壁を持つグリッド
- [Bitboardの使い方](docs/bitboard.md)
- [辺に壁があるグリッド用Bitboard](docs/edge_wall_bitboard.md)
- [一般Gridの設計案](docs/design.md)

探索用の作業領域を内部で共有するため、同じインスタンスで複数の探索を同時に実行しないでください。
