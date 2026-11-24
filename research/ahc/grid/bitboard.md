# bitboard.cpp

[bitboard.cpp](../../../titan_cpplib/ahc/grid/bitboard.cpp) のクラスの説明です。使用方法は [docs/bitboard.md](../../../titan_cpplib/ahc/grid/docs/bitboard.md) にも記載しています。

## 概要

- 壁と道からなる盤面に対し、flood、BFS、連結成分などをビット並列で求めます。
- グリッドの 1 行を 1 つの Word で保持します。W <= sizeof(Word)*8 が前提です。
- Word には uint64_t か __uint128_t を使用します。uint64_t では W <= 64、__uint128_t では W <= 128 まで扱うことができます。
- 近傍はテンプレート引数 NB で 4 近傍か 8 近傍を選択します。

## マスクと出力先

- マスク Set は長さ H の Word 配列で、1 ビットが 1 セルに対応します。
- 結果は new_set() で用意した out に格納します。メソッドは内部で out を再確保しません。
- 探索系のメソッドは内部の作業領域を共有します。このため同じインスタンスで並列に実行することはできません。

## 主なメソッド

| メソッド | 内容 |
|---|---|
| `from_grid(grid, '#')` | grid から壁と道を読み込む |
| `flood(sources, out)` | 多始点の到達集合を求める |
| `bfs_dist(sources, dist)` | 多始点の最短距離を dist[r*W+c] に格納する / 未到達のセルは -1 になる |
| `bfs_nearest(srcs, dist, id)` | 最短距離と最も近い始点の番号を求める |
| `connected(a, b)` | 2 点が連結であるかを判定する |
| `distance(a, b)` | 2 点間の最短距離を求める |
| `nearest_in_set(p, s, hit)` | 集合 s のうち最も近いセルを求める |
| `components()` / `label(lab)` | 連結成分の個数を求める / 各セルに成分番号を割り当てる |
| `largest_component(out)` | 最大の連結成分を求める |
| `component(r, c, out)` | 指定したセルを含む連結成分を求める |
| `expand` / `border` / `shift` | 近傍への展開、外周、平行移動を求める |
| `dilate` / `erode` | 膨張と収縮を求める |
| `band` / `bor` / `bxor` / `bdiff` など | マスクどうしの集合演算を行う |
