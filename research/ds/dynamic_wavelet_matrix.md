# dynamic_wavelet_matrix.cpp

## 他の構造との比較

- [DynamicWaveletTree](../../titan_cpplib/ds/dynamic_wavelet_tree.cpp) の方が平均的には高速である可能性があります。
- 動的な 3 つの構造の測定記録は [wavelet_family_speed_benchmark.md](../../test/ds/benchmark/wavelet_family_speed_benchmark.md) にあります。

## set の実装

- 上位の段から順に、元の値と新しい値で bit が等しい間は各段での位置の対応が変化しません。そのためビットの更新だけで済みます。
- bit が変化する段より下の段では、変更前の位置から要素を取り除き、変更後の位置へ挿入します。
- new_k は挿入後の並びでの位置です。そのため取り除いた後にそのまま挿入できます。

## 例

- 列 [5, 1, 4, 1, 9] で [lower, upper) = [1, 5) とします。
- kth_index_in_value_range は k = 2 のとき 3 を返します。
- next_index_in_value_range は 1 を返します。
- prev_index_in_value_range は 3 を返します。
