# b_tree_sum_sequence.cpp

## 用途

- 重みの列を B 木で保持し、位置を指定した挿入と削除、および区間和の計算を行います。
- 複数の列が節点の領域を共有します。各列は `Sequence` で表します。
- [DynamicWaveletTreeSum](../../titan_cpplib/ds/dynamic_wavelet_tree_sum.cpp) の各ノードの重みの列に使用します。

## 子の探索

- `_find_size` は `child_size[pos] > k` となる最初の pos を SIMD で探します。
- 使用中の末尾の子は必ずこの条件を満たします。未使用の要素はその後ろにあるため、未使用の要素を参照しても結果には影響しません。
- SIMD の探索は未使用の要素も参照するため、`Internal` の構築時に探索で使用する累積値を初期化します。

## 挿入での累積値の更新

- `insert` は葉へ降りながら祖先の累積値を更新します。
- 葉を分割する場合も `_set_child` が累積値を実際の値に合わせて更新します。
