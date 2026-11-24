# b_tree_bit_vector_sum.cpp

## 子の探索

- `_find_size` は `child_size[pos] > k` となる最初の pos を SIMD で探します。
- 使用中の末尾の子は必ずこの条件を満たします。未使用の要素はその後ろにあるため、ノードを再利用した後でも結果には影響しません。

## 祖先の累積値の更新

- `_add_path` は `child_size`、`child_ones`、`child_sum`、`child_sum1` の 4 つの配列を 1 つのループで更新します。
- 配列ごとにループを分けた場合は、終了判定の分岐予測の失敗が増えて遅くなりました。
