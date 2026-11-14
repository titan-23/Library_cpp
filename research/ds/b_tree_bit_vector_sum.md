# b_tree_bit_vector_sum.cpp

## 子の探索

- `_find_size` は `child_size[pos] > k` となる最初の pos を SIMD で探す。
- 使用中の最後の子は必ずこの条件を満たす。未使用の要素はその後ろにあるので、ノードを再利用した後でも結果は変わらない。

## 祖先の累積値の更新

- `_add_path` は `child_size`、`child_ones`、`child_sum`、`child_sum1` の 4 配列を 1 つのループで更新する。
- 配列ごとにループを分けると、終了判定の分岐予測の失敗が増えて遅くなった。
