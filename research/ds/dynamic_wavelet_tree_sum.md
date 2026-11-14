# dynamic_wavelet_tree_sum.cpp

## ノードの持ち方

- bit 1 以上の段のノードは、ビット列 BTreeBitVector と重みの列 BTreeSumSequence を別々に持つ。
- 段 bit のノードで値の bit が c の要素の重みの和は、子 c の重みの列の対応する区間の和に等しい。
- そのため個数だけを使う質問は、小さいビット列だけをたどる。
- bit 0 の段のノードは子を持たない。そのためビットと重みを一緒に持つ BTreeBitVectorSum を使う。

## 例

位置を表す例では、列 [5, 1, 4, 1, 9] で [lower, upper) = [1, 5) とする。

- kth_index_in_value_range は k = 2 なら 3 を返す。
- next_index_in_value_range は 1 を返す。
- prev_index_in_value_range は 3 を返す。
- range_select(1, 5, 1, 1) は 3 を返す。

topk の例を示す。

- キー列 [1, 2, 1, 3, 1, 2] で topk(0, 6, 2) は {(1, 3), (2, 2)} を返す。

重みを使う例では、(key, weight) の列を [(1, 2), (2, 3), (3, 5)] とする。

- max_right_smallest は pred(sum) = (sum <= 4) なら (1, 2, 2) を返す。
- max_right_largest は pred(sum) = (sum <= 6) なら (1, 2, 5) を返す。
- max_count_smallest_sum_le は budget = 4 なら 1 を返す。
- max_count_largest_sum_le は budget = 6 なら 1 を返す。
- weighted_quantile は target = 4 なら 2 を返す。
- weighted_quantile は numerator / denominator = 1 / 2 なら 2 を返す。
- weighted_median は 2 を返す。
- min_count_smallest_sum_ge は target = 4 なら 2 を返す。
- min_count_largest_sum_ge は target = 4 なら 1 を返す。

## 関連

- 測定記録は [wavelet_family_speed_benchmark.md](../../test/ds/benchmark/wavelet_family_speed_benchmark.md) にある。
