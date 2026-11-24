# dynamic_wavelet_tree.cpp

## 例

- 列 [5, 1, 4, 1, 9] で [lower, upper) = [1, 5) とします。
  - kth_index_in_value_range は k = 2 のとき 3 を返します。
  - next_index_in_value_range は 1 を返します。
  - prev_index_in_value_range は 3 を返します。
- 列 [5, 1, 4, 1, 9] で range_select(1, 5, 1, 1) は 3 を返します。
- 列 [1, 2, 1, 3, 1, 2] で topk(0, 6, 2) は {(1, 3), (2, 2)} を返します。

## set の実装

- 上位の段から順に、元の値と新しい値で bit が等しい間はビットを更新するだけで済みます。
- bit が変化する段より下の段では、元の値の側のノードから要素を取り除き、新しい値の側のノードへ挿入します。
- どちらの場合も各段で O(log(n)) の操作を 1 回か 2 回行うため、全体の計算量は O(log(n)log(σ)) です。

## 関連

- 測定記録は [dynamic_wavelet_tree_speed_benchmark.md](../../test/ds/benchmark/dynamic_wavelet_tree_speed_benchmark.md) と [wavelet_family_speed_benchmark.md](../../test/ds/benchmark/wavelet_family_speed_benchmark.md) にあります。
