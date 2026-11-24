# wavelet_matrix_sum.cpp

## 位置の検索の例

列 `[5, 1, 4, 1, 9]` で `[lower, upper) = [1, 5)` とします。

- `kth_index_in_value_range` は `k = 2` のとき `3` を返します。
- `next_index_in_value_range` は `1` を返します。
- `prev_index_in_value_range` は `3` を返します。

## topk の計算量

- 訪問したノード数を `p` とすると、計算量は `O(plog(p))` です。
- 区間内の相異なるキーの数を `D` とすると、最悪の場合は `p = O(Dlog(σ))` になります。

## 重みを使用する関数の例

`(key, weight)` の列 `[(1, 2), (2, 3), (3, 5)]` に対する結果を示します。

- `max_right_smallest` は `pred(sum) = (sum <= 4)` のとき `(1, 2, 2)` を返します。
- `max_right_largest` は `pred(sum) = (sum <= 6)` のとき `(1, 2, 5)` を返します。
- `max_count_smallest_sum_le` は `budget = 4` のとき `1` を返します。
- `max_count_largest_sum_le` は `budget = 6` のとき `1` を返します。
- `weighted_quantile` は `target = 4` のとき `2` を返します。
- `weighted_quantile` は `numerator / denominator = 1 / 2` のとき `2` を返します。
- `weighted_median` は `2` を返します。
- `min_count_smallest_sum_ge` は `target = 4` のとき `2` を返します。
- `min_count_largest_sum_ge` は `target = 4` のとき `1` を返します。

## max_right_* の返り値

- 返り値は `(count, boundary_value, aggregate)` です。
- `count` は採用した要素の個数で、`aggregate` はその重みの総和です。
- `boundary_value` は採用できなかった最初の要素のキーです。上の例では、キー 2 の要素を加えると `pred` が偽になるため `2` になります。
- 区間の全要素を採用できた場合、`boundary_value` は `T(-1)` になります。
