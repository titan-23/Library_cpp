# dynamic_wavelet_tree_sum.cpp

## ノードの構成

- bit 1 以上の段のノードは、ビット列 BTreeBitVector と重みの列 BTreeSumSequence を別々に保持します。
- 段 bit のノードで値の bit が c である要素の重みの総和は、子 c の重みの列の対応する区間の総和と一致します。
- そのため個数だけを使用するクエリでは、小さいビット列だけをたどります。
- bit 0 の段のノードには子がありません。そのためビットと重みをまとめて保持する BTreeBitVectorSum を使用します。

## 例

位置を返す関数の例では、列 [5, 1, 4, 1, 9] で [lower, upper) = [1, 5) とします。

- kth_index_in_value_range は k = 2 のとき 3 を返します。
- next_index_in_value_range は 1 を返します。
- prev_index_in_value_range は 3 を返します。
- range_select(1, 5, 1, 1) は 3 を返します。

topk の例では、キー列を [1, 2, 1, 3, 1, 2] とします。

- topk(0, 6, 2) は {(1, 3), (2, 2)} を返します。

重みを使用する関数の例では、(key, weight) の列を [(1, 2), (2, 3), (3, 5)] とします。

- max_right_smallest は pred(sum) = (sum <= 4) のとき (1, 2, 2) を返します。
- max_right_largest は pred(sum) = (sum <= 6) のとき (1, 2, 5) を返します。
- max_count_smallest_sum_le は budget = 4 のとき 1 を返します。
- max_count_largest_sum_le は budget = 6 のとき 1 を返します。
- weighted_quantile は target = 4 のとき 2 を返します。
- weighted_quantile は numerator / denominator = 1 / 2 のとき 2 を返します。
- weighted_median は 2 を返します。
- min_count_smallest_sum_ge は target = 4 のとき 2 を返します。
- min_count_largest_sum_ge は target = 4 のとき 1 を返します。

## 関連

- 測定記録は [wavelet_family_speed_benchmark.md](../../test/ds/benchmark/wavelet_family_speed_benchmark.md) にあります。
