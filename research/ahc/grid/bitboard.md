# bitboard.cpp

[bitboard.cpp](../../../titan_cpplib/ahc/grid/bitboard.cpp) のクラス先頭にあった説明を移した。使い方は [docs/bitboard.md](../../../titan_cpplib/ahc/grid/docs/bitboard.md) にもまとめてある。

## 概要

- 壁と道からなる盤面に対し、flood、BFS、連結成分などをビット並列で求める。
- グリッドの 1 行を 1 つの Word に持つ。W <= sizeof(Word)*8 が前提である。
- Word には uint64_t か __uint128_t を使う。uint64_t なら W <= 64、__uint128_t なら W <= 128 まで扱える。
- 近傍はテンプレート引数 NB で 4 近傍か 8 近傍を選ぶ。

## マスクと出力先

- マスク Set は長さ H の Word 配列で、1 ビットが 1 セルに対応する。
- 結果は new_set() で用意した out に書き込む。メソッドは内部で out を再確保しない。
- 探索系のメソッドは内部の作業領域を共有する。このため同じインスタンスで並列に実行できない。

## 主なメソッド

| メソッド | 内容 |
|---|---|
| `from_grid(grid, '#')` | grid から壁と道を読む。 |
| `flood(sources, out)` | 多始点の到達集合を求める。 |
| `bfs_dist(sources, dist)` | 多始点最短距離を dist[r*W+c] に求める。未到達は -1 になる。 |
| `bfs_nearest(srcs, dist, id)` | 最短距離と最寄りの始点番号を求める。 |
| `connected(a, b)` | 2 点が連結かを調べる。 |
| `distance(a, b)` | 2 点間の最短距離を求める。 |
| `nearest_in_set(p, s, hit)` | 集合 s のうち最も近いセルを求める。 |
| `components()` / `label(lab)` | 連結成分の数を数える。または各セルに成分番号を振る。 |
| `largest_component(out)` | 最大の連結成分を求める。 |
| `component(r, c, out)` | 指定セルを含む連結成分を求める。 |
| `expand` / `border` / `shift` | 近傍への展開、外周、平行移動を求める。 |
| `dilate` / `erode` | 膨張と収縮を求める。 |
| `band` / `bor` / `bxor` / `bdiff` など | マスクどうしの集合演算を行う。 |
