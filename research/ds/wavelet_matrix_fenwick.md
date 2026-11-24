# wavelet_matrix_fenwick.cpp

## 概要

- 静的なキーと更新可能な重みを保持するウェーブレット行列です。
- テンプレート引数 `T` はキーの型、`W` は重みの型です。

## topk の計算量

- 訪問する節点の数を `p` とすると、計算量は O(plog(p)) です。
- 区間内の相異なるキーの数を `D` とすると、最悪の場合は `p = O(Dlog(σ))` です。

## max_right_smallest と max_right_largest

- 返り値は `(count, boundary_value, aggregate)` です。
- `boundary_value` は最初に採用できなかった要素のキーです。全要素を採用できた場合は `-1` になります。
- `pred(0) == true` であり、要素を順に加えていく過程で一度偽になると真に戻らないことを前提とします。

## 位置を求める関数の使用例

- キー列 `[5, 1, 4, 1, 9]` で `[lower, upper) = [1, 5)`、`k = 2` とすると、`kth_index_in_value_range` は `3` を返します。
- キー列 `[5, 1, 4, 1, 9]` で `[lower, upper) = [1, 5)` とすると、`next_index_in_value_range` は `1` を返します。
- 同じ条件で `prev_index_in_value_range` は `3` を返します。

## 重みを扱う関数の使用例

以下の例では、`(key, weight)` の列を `[(1, 2), (2, 3), (3, 5)]` とします。

- `pred(sum) = (sum <= 4)` とすると、`max_right_smallest` は `(1, 2, 2)` を返します。
- `pred(sum) = (sum <= 6)` とすると、`max_right_largest` は `(1, 2, 5)` を返します。
- `budget = 4` とすると、`max_count_smallest_sum_le` は `1` を返します。
- `budget = 6` とすると、`max_count_largest_sum_le` は `1` を返します。
- `target = 4` とすると、`weighted_quantile` は `2` を返します。
- `numerator / denominator = 1 / 2` とすると、`weighted_quantile` は `2` を返します。
- `weighted_median` は `2` を返します。
- `target = 4` とすると、`min_count_smallest_sum_ge` は `2` を返します。
- `target = 4` とすると、`min_count_largest_sum_ge` は `1` を返します。
