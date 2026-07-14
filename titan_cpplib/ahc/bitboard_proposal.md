# `bitboard.cpp` 改善結果

## 対象と方針

- 行ごとに 1 `Word` を持つ `bitboard.cpp` と、盤面全体を 1 `Word` に収める `flat_bitboard.cpp` を対象とする。
- 主用途は `H, W <= 128` 程度の盤面を使う SA・ビームサーチのホットループ。
- `Set` と内部作業領域は直接保持し、`shared_ptr` や Workspace 分離は行わない。
- `H * W <= 64/128` では別ファイルの `FlatBitboard` を使う。
- 結果用 `Set` は呼び出し側で確保し、探索のホットパスでは動的確保しない。

## 実装済み

### 探索処理

- flood の収束判定を上下 1 掃引ごとに行い、閉包完成後の余分な掃引を削減した。
- 行版の flood は有効な最小・最大行を追跡し、成分の外側を走査しない。
- `connected` の閉包中に目標行を更新した時点で到達判定する。
- `distance`・`connected`・`nearest_in_set`・`flood_limited`・`bfs_dist`・`bfs_nearest`・`shortest_path` に有効行ウィンドウを導入した。
- 範囲外に残った作業ビットを読まないよう、展開時は現在の frontier 範囲外を 0 として扱う。
- 8 近傍の `expand_into` は横 3 ビット展開をローリング保持する。
- 引数 2 点だけの `shortest_path` は、事前確保した距離バッファと現在の `seen` を使い、毎回の `vector` 確保と全体 `-1` 初期化をなくした。
- 行版の `component_size` は専用作業領域の使用行だけを初期化・集計する。

### 定数倍と安全検査

- ホットな座標・`Set` サイズ・非エイリアス検査を専用マクロへ移した。
- 既定では検査を評価せず、`TITAN_DEBUG` 定義時だけ `assert` を有効にする。
- コンストラクタ、`from_grid`、矩形範囲など低頻度の構造検査は通常の `assert` を維持した。
- `nearest_in_set` の新規セル確定と対象判定を 1 行ループへ統合した。
- `build_runs` の処理済みラン除去を `x &= ~lowmask(rr)` に簡略化した。
- 行版 `hash64` から冗長な行番号ごとの `mix64` を除いた。盤面形状は初期 seed の `H, W` で区別する。
- 4・8 近傍共通になった `expand_into` のコメントを修正した。

### 追加 API

行版・flat版の両方へ次を追加した。

- `count_and(a, b)`：共通セル数を一時 `Set` なしで数える。
- `bounding_box(s, r1, c1, r2, c2)`：空でなければ最小半開矩形を返す。
- `reachable_from_border(out)`：外周に接する道から到達できる道集合。
- `enclosed_road(out)`：外周から到達できない道集合。
- `locally_removable(r, c)`：連結盤面に対する 3x3 LUT の安全側削除判定。
- `articulation_cells(out)`：Tarjan 法による正確な関節点集合。
- `for_each_component(f)`：`f(component_set, size)` を成分ごとに呼ぶ。
- `dilate(s, k, out)`：盤面内の幾何的な k 回膨張。道上の探索である `flood_limited` とは区別する。
- `to_grid(s, grid, one, zero)`：任意マスクの可視化用変換。

`locally_removable` は厳密な `removable` ではない。中心を除いた 3x3 内で中心の道隣接セルが連結なら `true` とする。盤面外を迂回する接続は見ないため削除可能セルを見逃し得るが、変更前の道集合が連結なら `true` 側は安全である。4・8 近傍ごとに別の 256 パターン LUT が生成される。

`for_each_component` が渡す `component_set` はコールバック中だけ有効である。run + DSU の各 run を全体で一度だけ処理し、前成分で触れた行だけをクリアする。同じ `Bitboard` の探索メソッドをコールバック内から呼ぶことはできない。

## `connected` の層数

有効行ウィンドウ導入後に `LAYER_LIMIT = 0, 1, 2, 4, 8` を比較した。`-O2`、開けた 128x128 盤面での参考値である。

| `LAYER_LIMIT` | マンハッタン距離 4 | 盤面両端 |
|---:|---:|---:|
| 0 | 263 ns | 5.51 us |
| 1 | 306 ns | 5.48 us |
| 2 | 369 ns | 5.51 us |
| 4 | 146 ns | 5.59 us |
| 8 | 146 ns | 5.70 us |

距離 4 を層別 BFS だけで完了でき、遠距離での追加コストも小さい `LAYER_LIMIT = 4` を維持する。値は盤面分布と CPU に依存するため、特定問題で `connected` が支配的なら再計測する。

## 検証

- 既存のスカラー BFS とのランダム差分テストを、行版・flat版、4・8 近傍、64・128bit の組み合わせで通過した。
- 追加 API 専用に 2,000 盤面を検査した。
  - 関節点は各道セルを実際に削除した後の成分数と比較した。
  - `for_each_component` はスカラー BFS の成分集合・サイズと比較した。
  - 外周到達、外接矩形、`count_and`、k 回膨張、`to_grid` を単純実装と比較した。
  - 連結盤面で `locally_removable == true` のセルを削除し、連結性が壊れないことを確認した。
- 通常ビルドと `TITAN_DEBUG` ビルドの双方を確認した。
- AddressSanitizer・UndefinedBehaviorSanitizer を通過した。

## 残件

- `path_to_moves` は未実装。4 近傍なら `RLDU` で一意だが、8 近傍時の斜め移動表現が定まっていないため、Bitboard のメンバには加えない。
- `TITAN_DEBUG` の効果は呼び出し方や分岐予測に依存する。実問題のプロファイルで境界検査が支配的でなければ、速度差は小さい。
- 固定長行版、BMI2 専用 `kth_cell`、更新型 Zobrist hash は、現時点では必要性が低いため未実装。
