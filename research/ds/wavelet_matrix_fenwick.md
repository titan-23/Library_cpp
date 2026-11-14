# wavelet_matrix_fenwick.cpp

## 概要

- 静的なキーと、更新できる重みを持つウェーブレット行列である。
- テンプレート引数 `T` はキーの型、`W` は重みの型である。

## topk の計算量

- 訪問する節点の数を `p` とすると、計算量は O(plog(p)) である。
- 区間内の相異なるキーの数を `D` とすると、最悪で `p = O(Dlog(σ))` である。

## max_right_smallest と max_right_largest

- 返り値は `(count, boundary_value, aggregate)` である。
- `boundary_value` は最初に採れなかった要素のキーである。全要素を採れた場合は `-1` を返す。
- `pred(0) == true` であり、要素を加える順に一度偽になると真へ戻らないことを前提とする。

## 位置を求める関数の使用例

- キー列 `[5, 1, 4, 1, 9]` で `[lower, upper) = [1, 5)`、`k = 2` とすると、`kth_index_in_value_range` は `3` を返す。
- キー列 `[5, 1, 4, 1, 9]` で `[lower, upper) = [1, 5)` とすると、`next_index_in_value_range` は `1` を返す。
- 同じ条件で、`prev_index_in_value_range` は `3` を返す。

## 重みを使う関数の使用例

以下の例では、`(key, weight)` の列を `[(1, 2), (2, 3), (3, 5)]` とする。

- `pred(sum) = (sum <= 4)` とすると、`max_right_smallest` は `(1, 2, 2)` を返す。
- `pred(sum) = (sum <= 6)` とすると、`max_right_largest` は `(1, 2, 5)` を返す。
- `budget = 4` とすると、`max_count_smallest_sum_le` は `1` を返す。
- `budget = 6` とすると、`max_count_largest_sum_le` は `1` を返す。
- `target = 4` とすると、`weighted_quantile` は `2` を返す。
- `numerator / denominator = 1 / 2` とすると、`weighted_quantile` は `2` を返す。
- `weighted_median` は `2` を返す。
- `target = 4` とすると、`min_count_smallest_sum_ge` は `2` を返す。
- `target = 4` とすると、`min_count_largest_sum_ge` は `1` を返す。
