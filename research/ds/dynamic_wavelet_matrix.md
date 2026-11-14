# dynamic_wavelet_matrix.cpp

## 他の構造との比較

- [DynamicWaveletTree](../../titan_cpplib/ds/dynamic_wavelet_tree.cpp) の方が平均的には高速かもしれない。
- 動的な 3 構造の測定記録は [wavelet_family_speed_benchmark.md](../../test/ds/benchmark/wavelet_family_speed_benchmark.md) にある。

## set の実装

- 上位の段から、元の値と bit が等しい間は各段での位置の対応が変わらない。そのため書き換えだけで済む。
- bit が変わる段より下では、変更前の位置から取り除き、変更後の位置へ挿入する。
- new_k は挿入後の並びでの位置である。そのため取り除いた後にそのまま挿入できる。

## 例

- 列 [5, 1, 4, 1, 9] で [lower, upper) = [1, 5) とする。
- kth_index_in_value_range は k = 2 なら 3 を返す。
- next_index_in_value_range は 1 を返す。
- prev_index_in_value_range は 3 を返す。
