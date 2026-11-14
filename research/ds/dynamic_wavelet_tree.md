# dynamic_wavelet_tree.cpp

## 例

- 列 [5, 1, 4, 1, 9] で [lower, upper) = [1, 5) とする。
  - kth_index_in_value_range は k = 2 なら 3 を返す。
  - next_index_in_value_range は 1 を返す。
  - prev_index_in_value_range は 3 を返す。
- 列 [5, 1, 4, 1, 9] で range_select(1, 5, 1, 1) は 3 を返す。
- 列 [1, 2, 1, 3, 1, 2] で topk(0, 6, 2) は {(1, 3), (2, 2)} を返す。

## set の実装

- 上位の段から、元の値と bit が等しい間はビットを書き換えるだけで済む。
- bit が変わる段より下では、元の値の側のノードから取り除き、新しい値の側のノードへ挿入する。
- どちらの場合も各段で O(log(n)) の操作を 1 回か 2 回行うので、全体は O(log(n)log(σ)) である。

## 関連

- 測定記録は [dynamic_wavelet_tree_speed_benchmark.md](../../test/ds/benchmark/dynamic_wavelet_tree_speed_benchmark.md) と [wavelet_family_speed_benchmark.md](../../test/ds/benchmark/wavelet_family_speed_benchmark.md) にある。
