# wavelet_matrix_sum.cpp

## 位置の検索の例

列 `[5, 1, 4, 1, 9]` で `[lower, upper) = [1, 5)` とする。

- `kth_index_in_value_range` は `k = 2` のとき `3` を返す。
- `next_index_in_value_range` は `1` を返す。
- `prev_index_in_value_range` は `3` を返す。

## topk の計算量

- 訪問したノード数を `p` とすると、計算量は `O(plog(p))` である。
- 区間内の相異なるキーの数を `D` とすると、最悪で `p = O(Dlog(σ))` になる。

## 重みを使う関数の例

`(key, weight)` の列 `[(1, 2), (2, 3), (3, 5)]` に対する結果を示す。

- `max_right_smallest` は `pred(sum) = (sum <= 4)` のとき `(1, 2, 2)` を返す。
- `max_right_largest` は `pred(sum) = (sum <= 6)` のとき `(1, 2, 5)` を返す。
- `max_count_smallest_sum_le` は `budget = 4` のとき `1` を返す。
- `max_count_largest_sum_le` は `budget = 6` のとき `1` を返す。
- `weighted_quantile` は `target = 4` のとき `2` を返す。
- `weighted_quantile` は `numerator / denominator = 1 / 2` のとき `2` を返す。
- `weighted_median` は `2` を返す。
- `min_count_smallest_sum_ge` は `target = 4` のとき `2` を返す。
- `min_count_largest_sum_ge` は `target = 4` のとき `1` を返す。

## max_right_* の返り値

- 返り値は `(count, boundary_value, aggregate)` である。
- `count` は採用した要素の個数で、`aggregate` はその重みの総和である。
- `boundary_value` は採用できなかった最初の要素のキーである。上の例では、キー 2 の要素を加えると `pred` が偽になるので `2` になる。
- 区間の全要素を採用できたとき、`boundary_value` は `T(-1)` になる。
