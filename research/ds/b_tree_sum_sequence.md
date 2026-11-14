# b_tree_sum_sequence.cpp

## 用途

- 重みの列を B 木で持ち、位置の挿入・削除と区間和を扱う。
- 複数の列が節点の領域を共有する。各列は `Sequence` で表す。
- [DynamicWaveletTreeSum](../../titan_cpplib/ds/dynamic_wavelet_tree_sum.cpp) の各ノードの重みの列に使う。

## 子の探索

- `_find_size` は `child_size[pos] > k` となる最初の pos を SIMD で探す。
- 使用中の最後の子は必ずこの条件を満たす。未使用の要素はその後ろにあるので、未使用の要素を読んでも結果は変わらない。
- SIMD の探索は未使用の要素も読むので、`Internal` の構築時に探索に使う累積値を初期化する。

## 挿入での累積値の更新

- `insert` は、葉へ降りながら祖先の累積値を更新する。
- 葉を分割する場合も、`_set_child` が累積値を実際の値に合わせ直す。
