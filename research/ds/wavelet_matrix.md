# wavelet_matrix.cpp

## 使用例

- 列 `[5, 1, 4, 1, 9]` で、`range_select(1, 5, 1, 1)` は `3` を返す。
- 列 `[2, 1, 2, 2]` の全体で、`has_majority` は `{true, 2}` を返す。
- 列 `[1, 2, 3]` の全体で、`has_majority` は `{false, 0}` を返す。
- 列 `[5, 1, 4, 1, 9]` で `[lower, upper) = [1, 5)`、`k = 2` とすると、`kth_index_in_value_range` は `3` を返す。
- 列 `[5, 1, 4, 1, 9]` で `[lower, upper) = [1, 5)` とすると、`next_index_in_value_range` は `1` を返す。
- 同じ条件で、`prev_index_in_value_range` は `3` を返す。
