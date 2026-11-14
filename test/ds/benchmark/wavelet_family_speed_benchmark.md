# ウェーブレット木・行列系の高速化の測定記録

2026-11-13。動的な 3 構造 (DynamicWaveletTree, DynamicWaveletTreeSum, DynamicWaveletMatrix) と、静的なウェーブレット行列 7 種を高速化した。

## 測定条件

- CPU は AMD Ryzen Threadripper 3990X (Zen 2)、コンパイラは GCC 12.3.0、OS は Linux。
- オプションは `-std=c++17 -O2 -DNDEBUG -march=native`。動的な 3 構造は、`-march` を付けない既定ビルド (SSE2 まで) でも測った。
- 変更前は HEAD (`9634bd5`) のヘッダ、変更後は今回の版。同じベンチマークをそれぞれのヘッダでコンパイルした。
- CPU 4 に固定し、変更前後を交互に 3 回ずつ実行して中央値を取った。
- 入力は N=Q=300,000、σ=N、一様分布。全体がキャッシュに収まる条件として N=20,000、Q=300,000 も測った。
- 表の「前 ns」「後 ns」は 1 操作あたりのナノ秒で、最初の条件 (動的版は native、静的版は N=300,000) の値である。「短縮」は `1 - 後 / 前` で、正の値が高速化を表す。右の列は他の条件の短縮率である。
- チェックサムは、全条件・全操作で変更前後が一致した。

ベンチマークは次のファイルにある。

- `dynamic_wavelet_tree_speed_benchmark.cpp` と、それを行列用に読み込む `dynamic_wavelet_matrix_speed_benchmark.cpp`
- `dynamic_wavelet_tree_sum_speed_benchmark.cpp`
- `wavelet_matrix_speed_benchmark.cpp`、`wavelet_matrix_sum_speed_benchmark.cpp`、`wavelet_matrix_fenwick_speed_benchmark.cpp`、`wavelet_matrix_bit_speed_benchmark.cpp`、`wavelet_matrix_2d_speed_benchmark.cpp` (共通部分は `wavelet_matrix_speed_common.hpp`)

## 採用した変更

### 共通の部品

- `others/simd_int.cpp` に、32 bit 整数用の SSE2 段と、`zero`、`sub`、`reduce_add`、`cmpgt_bits` を加えた。固定長の小さい配列向けに `simd_find_first_greater` (番兵を省く `_unchecked` 版と差を比べる `_diff` 版を含む) と `simd_masked_sum` を加えた。
- `others/bit.cpp` に `select64` を加えた。PDEP は Zen 1/2 ではマイクロコードで実装され、この計算機では 1 回約 45ns かかった。そのため `__znver1__` と `__znver2__` が定義されているときは使わず、popcount による絞り込みと 1 バイトの表引きで求める。
- `ds/bit_vector.cpp` は、64 bit ごとにビット列とそこまでの 1 の個数を同じブロックに並べ、rank が 1 本のキャッシュ行で済むようにした。select は 256 個ごとの位置の表で二分探索の範囲を絞り、語の中は `select64` で求める。各位置の bit を返す関数から 64 bit ずつ組み立てるコンストラクタも加えた。

### 動的な構造

- BTreeBitVector と BTreeBitVectorSum の子の探索を `simd_find_first_greater_unchecked` に置き換えた。BTreeBitVectorSum は、線形走査から SIMD の比較に変わった。
- BTreeBitVectorSum の葉の部分和は、W が SIMD で扱える整数型のとき `simd_masked_sum` で求める。
- BTreeBitVector の PDEP は、Zen 1/2 向けのビルドでは使わない。
- DynamicWaveletTreeSum は、bit 1 以上の段のノードを、BTreeBitVector のビット列と、新しい `ds/b_tree_sum_sequence.cpp` の重みの列に分けた。値の bit が c の要素の重みの和は、子 c の重みの列の区間和として求める。bit 0 の段は BTreeBitVectorSum のままである。
- DynamicWaveletMatrix のビット列を AVLTreeBitVector から BTreeBitVector に替えた。あわせて次の変更を行った。
  - 区間の両端の rank0 を、`_rank0_pair` で 1 回の降下にまとめて求める。
  - select の rank0(n) を、各段の 0 の個数で置き換える。
  - prev_value と next_value を 1 回の降下で求める。
  - set は、上位の段で値の bit が等しい間は書き換えだけで済ませる。

### 静的な構造

- WaveletMatrix、WaveletMatrixSum、WaveletMatrixFenwick、wavelet_matrix_bit.cpp の WaveletMatrix で、次の変更を共通に行った。
  - range_count と rank は 1 回の降下で求め、区間が空になったら打ち切る。
  - prev_value と next_value は、降下中に候補を 1 つ覚え、その位置から最大値か最小値をたどる。
  - select は rank0(n) の代わりに 0 の個数を使う。
  - tovector は、段ごとに位置の並び替えをたどって復元する。
- 7 種すべての構築で、BitVector を関数から作るコンストラクタを使う。

## 採用しなかった変更

- B 木の節点を 64 バイト境界にそろえる案は、Sum 版で 2〜10% 遅くなった。使用量が増えた影響とみられる。非 Sum 版では差が測定の揺れと区別できなかった。
- 祖先の累積値の更新を配列ごとの SIMD ループにする案は、ループを抜ける分岐の予測外れが増え、更新系が 7〜11% 遅くなった。
- 重みの列で、葉の部分和を半分を超えたら残りから求める案と、要素の少ない側の和だけを求める案は、どちらも遅くなった。
- 重みの列の分岐数を 32 にする案は、効果がなかった。
- BitVector のビット列と累積値を別の配列に戻す案は、N=300,000 で個数の質問が 4〜6% 遅く、和の質問が 2〜5% 速かった。個数の質問を優先して、同じブロックに並べる形を残した。

## 遅くなった操作

- DynamicWaveletTreeSum の weighted_median は 12〜13% 遅い。min_count と max_count は N=300,000 では 13〜15% 速いが、N=20,000 では 11〜18% 遅い。pop は 7〜8%、set は 4〜7% 遅い。これらは、各段で小さいビット列と子の重みの列の 2 つをたどるようになったためである。
- WaveletMatrixSum と WaveletMatrixFenwick の和の質問は 1〜8% 遅く、N=20,000 の sum_k_smallest は 15% 遅い。2 次元版の sum_k も 6% 遅い。上の BitVector の配置のトレードオフによる。
- WaveletMatrixFenwick の set_weight と add_weight は 4% 遅い。
- WaveletMatrix の rank は、N=20,000 では 8% 遅い。N=300,000 では 6% 速い。
- DynamicWaveletTree の select は、既定ビルドで 4% 遅い。native では PDEP を避けたことで 28% 速くなった。

## 結果

### DynamicWaveletTree

| 操作 | 前 ns | 後 ns | 短縮 | 既定ビルド |
|---|---:|---:|---:|---:|
| build | 139.2 | 138.4 | 0.6% | 0.3% |
| access | 678.1 | 682.0 | -0.6% | 3.2% |
| rank | 514.3 | 514.5 | -0.0% | 8.0% |
| range_count | 940.2 | 940.2 | 0.0% | 4.7% |
| range_freq | 953.6 | 962.0 | -0.9% | 4.5% |
| kth_smallest | 1032.9 | 1020.3 | 1.2% | 4.8% |
| prev_value | 1009.1 | 1006.7 | 0.2% | 4.8% |
| next_value | 1010.9 | 994.6 | 1.6% | 5.9% |
| select | 1324.2 | 952.1 | 28.1% | -4.3% |
| insert | 1036.8 | 1039.9 | -0.3% | 2.0% |
| pop | 960.6 | 945.3 | 1.6% | 0.1% |
| insert_pop | 982.2 | 975.4 | 0.7% | 1.4% |
| set | 1801.2 | 1840.5 | -2.2% | 3.5% |
| select_remove | 1385.0 | 1086.2 | 21.6% | 0.3% |
| mixed | 1223.1 | 1244.2 | -1.7% | 4.4% |

### DynamicWaveletTreeSum (分割版)

| 操作 | 前 ns | 後 ns | 短縮 | 既定ビルド | N=20,000 |
|---|---:|---:|---:|---:|---:|
| build | 358.0 | 355.3 | 0.7% | 1.5% | -2.0% |
| access | 2896.5 | 834.4 | 71.2% | 70.1% | 41.6% |
| access_pair | 2969.6 | 890.5 | 70.0% | 68.5% | 40.7% |
| access_weight | 49.0 | 26.0 | 46.9% | 44.3% | 46.6% |
| range_freq | 2839.4 | 1055.5 | 62.8% | 61.6% | 37.1% |
| kth_smallest | 3611.2 | 1114.0 | 69.2% | 66.0% | 46.7% |
| range_sum | 134.9 | 92.0 | 31.8% | 33.2% | 29.1% |
| sum_lt | 4529.2 | 2278.6 | 49.7% | 49.9% | 25.7% |
| sum_range | 8195.0 | 4139.7 | 49.5% | 49.4% | 27.1% |
| sum_k_smallest | 4467.2 | 2241.3 | 49.8% | 49.1% | 23.9% |
| sum_k_largest | 4462.7 | 2535.1 | 43.2% | 41.5% | 19.8% |
| min_count_smallest_sum_ge | 4830.3 | 4194.8 | 13.2% | 12.6% | -17.7% |
| max_count_smallest_sum_le | 4922.1 | 4183.8 | 15.0% | 14.8% | -10.9% |
| weighted_median | 2021.1 | 2284.3 | -13.0% | -12.0% | -6.3% |
| select | 3045.3 | 1185.2 | 61.1% | 54.3% | 47.0% |
| insert | 5859.3 | 5545.5 | 5.4% | 5.1% | 3.9% |
| pop | 4212.8 | 4494.9 | -6.7% | -7.9% | -7.5% |
| set | 8978.8 | 9446.7 | -5.2% | -3.6% | -7.0% |
| set_weight | 3579.4 | 2476.3 | 30.8% | 25.7% | 26.1% |
| select_remove | 4760.9 | 4675.1 | 1.8% | -1.2% | 3.5% |
| mixed | 6819.4 | 4691.0 | 31.2% | 32.3% | 13.8% |

### DynamicWaveletMatrix

| 操作 | 前 ns | 後 ns | 短縮 | 既定ビルド |
|---|---:|---:|---:|---:|
| build | 200.8 | 105.7 | 47.4% | 46.3% |
| access | 3606.5 | 938.0 | 74.0% | 71.7% |
| rank | 3490.6 | 1252.7 | 64.1% | 61.5% |
| range_count | 5840.3 | 1301.9 | 77.7% | 76.7% |
| range_freq | 3773.7 | 1302.8 | 65.5% | 64.0% |
| kth_smallest | 3774.9 | 1400.4 | 62.9% | 61.3% |
| prev_value | 7113.6 | 1508.6 | 78.8% | 77.9% |
| next_value | 7105.9 | 1475.5 | 79.2% | 78.2% |
| select | 4504.6 | 1863.7 | 58.6% | 57.8% |
| insert | 3244.7 | 1768.9 | 45.5% | 45.5% |
| pop | 2678.6 | 1624.3 | 39.4% | 37.1% |
| insert_pop | 2581.6 | 1801.8 | 30.2% | 25.7% |
| set | 5703.2 | 3587.1 | 37.1% | 35.5% |
| mixed | 4394.6 | 2036.5 | 53.7% | 51.4% |

### WaveletMatrix

| 操作 | 前 ns | 後 ns | 短縮 | N=20,000 |
|---|---:|---:|---:|---:|
| build | 95.1 | 80.5 | 15.4% | 18.6% |
| access | 229.4 | 228.4 | 0.4% | -0.2% |
| rank | 174.6 | 164.8 | 5.6% | -8.2% |
| range_count | 279.0 | 186.2 | 33.3% | 44.5% |
| range_freq | 204.9 | 194.2 | 5.2% | -4.1% |
| range_freq2 | 394.6 | 376.6 | 4.6% | -2.5% |
| kth_smallest | 263.0 | 251.6 | 4.3% | -1.4% |
| kth_largest | 264.0 | 252.0 | 4.6% | 0.0% |
| prev_value | 355.5 | 230.8 | 35.1% | 40.0% |
| next_value | 354.2 | 215.8 | 39.1% | 40.0% |
| select | 1697.7 | 912.5 | 46.3% | 30.3% |
| range_select | 1743.3 | 958.3 | 45.0% | 28.2% |
| has_majority | 9.4 | 9.2 | 1.6% | -4.0% |
| topk | 3381344 | 3377364 | 0.1% | 1.5% |
| kth_index_in_value_range | 2709.4 | 2263.4 | 16.5% | -0.6% |
| next_index_in_value_range | 2802.6 | 1956.2 | 30.2% | 16.7% |
| prev_index_in_value_range | 2950.8 | 2563.2 | 13.1% | -7.1% |
| tovector | 175.8 | 72.0 | 59.0% | 38.1% |

### WaveletMatrixSum

| 操作 | 前 ns | 後 ns | 短縮 | N=20,000 |
|---|---:|---:|---:|---:|
| build | 222.8 | 232.6 | -4.4% | -4.0% |
| access | 230.0 | 229.5 | 0.2% | -1.0% |
| rank | 173.8 | 164.0 | 5.7% | -8.5% |
| range_count | 195.6 | 185.0 | 5.4% | -1.8% |
| range_freq | 201.2 | 186.4 | 7.4% | -2.0% |
| range_freq2 | 383.1 | 362.7 | 5.3% | -2.6% |
| kth_smallest | 267.8 | 251.4 | 6.1% | 2.2% |
| kth_largest | 266.5 | 252.3 | 5.3% | 1.6% |
| prev_value | 351.6 | 231.1 | 34.3% | 39.3% |
| next_value | 351.4 | 216.1 | 38.5% | 40.6% |
| select | 1690.8 | 920.7 | 45.5% | 30.5% |
| range_select | 1726.5 | 973.7 | 43.6% | 27.8% |
| has_majority | 9.4 | 9.2 | 2.0% | -2.5% |
| topk | 3381756 | 3332936 | 1.4% | 1.4% |
| kth_index_in_value_range | 2698.5 | 2096.1 | 22.3% | 6.5% |
| next_index_in_value_range | 2758.2 | 1855.0 | 32.7% | 16.9% |
| prev_index_in_value_range | 2909.8 | 2424.2 | 16.7% | -4.1% |
| access_weight | 6.6 | 6.7 | -0.6% | -1.3% |
| access_pair | 229.7 | 232.7 | -1.3% | -4.6% |
| range_sum | 5.7 | 5.8 | -1.0% | -1.4% |
| count_sum_lt | 413.4 | 427.7 | -3.5% | -8.0% |
| sum_lt | 406.7 | 406.3 | 0.1% | -5.6% |
| sum_range | 804.2 | 796.9 | 0.9% | -2.9% |
| sum_k_smallest | 539.6 | 580.4 | -7.6% | -15.2% |
| sum_k_largest | 539.0 | 579.3 | -7.5% | -7.2% |
| max_right_smallest | 1214.7 | 1190.9 | 2.0% | -0.5% |
| max_right_largest | 1231.4 | 1217.4 | 1.1% | -2.6% |
| max_count_smallest_sum_le | 1206.4 | 1193.4 | 1.1% | -1.5% |
| max_count_largest_sum_le | 1243.7 | 1214.1 | 2.4% | -2.3% |
| weighted_quantile | 1228.1 | 1216.8 | 0.9% | -1.1% |
| weighted_quantile_ratio | 1254.2 | 1240.4 | 1.1% | -0.4% |
| weighted_median | 297.8 | 290.5 | 2.4% | -1.6% |
| min_count_smallest_sum_ge | 1206.7 | 1178.4 | 2.3% | -0.5% |
| min_count_largest_sum_ge | 1223.0 | 1195.9 | 2.2% | -0.3% |
| tovector | 172.8 | 73.2 | 57.7% | 35.1% |
| toweights | 0.6 | 0.6 | -2.6% | -0.1% |
| toitems | 177.0 | 81.4 | 54.0% | 35.3% |

### WaveletMatrixFenwick

| 操作 | 前 ns | 後 ns | 短縮 |
|---|---:|---:|---:|
| build | 251.5 | 257.5 | -2.4% |
| access | 230.1 | 227.0 | 1.3% |
| rank | 174.9 | 163.9 | 6.3% |
| range_count | 195.1 | 186.0 | 4.7% |
| range_freq | 200.7 | 188.4 | 6.1% |
| range_freq2 | 387.0 | 358.5 | 7.4% |
| kth_smallest | 264.5 | 252.2 | 4.6% |
| kth_largest | 267.4 | 254.6 | 4.8% |
| prev_value | 375.1 | 229.9 | 38.7% |
| next_value | 352.5 | 222.3 | 36.9% |
| select | 1693.3 | 934.5 | 44.8% |
| range_select | 1726.8 | 967.5 | 44.0% |
| has_majority | 9.3 | 9.2 | 1.5% |
| topk | 3420182 | 3347752 | 2.1% |
| kth_index_in_value_range | 2699.9 | 2111.7 | 21.8% |
| next_index_in_value_range | 2761.2 | 1846.4 | 33.1% |
| prev_index_in_value_range | 2919.7 | 2418.0 | 17.2% |
| access_weight | 6.7 | 6.6 | 0.7% |
| access_pair | 229.4 | 230.9 | -0.6% |
| range_sum | 25.9 | 26.1 | -0.8% |
| count_sum_lt | 852.9 | 887.6 | -4.1% |
| sum_lt | 845.1 | 870.6 | -3.0% |
| sum_range | 1660.5 | 1710.6 | -3.0% |
| sum_k_smallest | 1358.3 | 1425.7 | -5.0% |
| sum_k_largest | 1403.7 | 1436.6 | -2.3% |
| max_right_smallest | 2152.4 | 2188.7 | -1.7% |
| max_right_largest | 2155.6 | 2192.7 | -1.7% |
| max_count_smallest_sum_le | 2151.3 | 2177.2 | -1.2% |
| max_count_largest_sum_le | 2157.7 | 2195.0 | -1.7% |
| weighted_quantile | 2165.8 | 2186.4 | -1.0% |
| weighted_quantile_ratio | 2216.9 | 2246.8 | -1.4% |
| weighted_median | 924.7 | 923.2 | 0.2% |
| min_count_smallest_sum_ge | 2236.3 | 2264.6 | -1.3% |
| min_count_largest_sum_ge | 2248.4 | 2266.6 | -0.8% |
| tovector | 176.0 | 71.8 | 59.2% |
| toweights | 0.4 | 0.4 | 0.5% |
| toitems | 176.3 | 72.9 | 58.6% |
| set_weight | 995.1 | 1035.7 | -4.1% |
| add_weight | 995.6 | 1036.8 | -4.1% |
| mixed | 1577.7 | 1630.6 | -3.4% |

### WaveletMatrix (wavelet_matrix_bit.cpp)

| 操作 | 前 ns | 後 ns | 短縮 |
|---|---:|---:|---:|
| build | 129.4 | 77.7 | 40.0% |
| access | 246.4 | 216.2 | 12.2% |
| rank | 170.3 | 163.2 | 4.2% |
| range_count | 300.1 | 186.0 | 38.0% |
| range_freq | 197.8 | 191.7 | 3.1% |
| range_freq2 | 381.7 | 359.1 | 5.9% |
| kth_smallest | 260.9 | 248.6 | 4.7% |
| kth_largest | 261.5 | 251.0 | 4.0% |
| prev_value | 345.3 | 229.6 | 33.5% |
| next_value | 344.6 | 221.6 | 35.7% |
| select | 1759.2 | 877.3 | 50.1% |
| range_select | 1821.2 | 972.2 | 46.6% |
| has_majority | 9.3 | 9.1 | 2.3% |
| topk | 3526318 | 3522240 | 0.1% |
| kth_index_in_value_range | 2668.8 | 2394.8 | 10.3% |
| next_index_in_value_range | 2769.0 | 2525.6 | 8.8% |
| prev_index_in_value_range | 2959.1 | 2568.1 | 13.2% |
| tovector | 165.9 | 74.1 | 55.3% |

### 2 次元のウェーブレット行列

| 操作 | 前 ns | 後 ns | 短縮 |
|---|---:|---:|---:|
| sum.build | 457.9 | 468.6 | -2.3% |
| sum.range_sum_x | 204.3 | 203.5 | 0.4% |
| sum.range_sum | 1383.3 | 1391.2 | -0.6% |
| sum.count_sum_lt | 728.6 | 737.7 | -1.2% |
| sum.sum_lt | 727.5 | 736.2 | -1.2% |
| sum.range_count | 1380.5 | 1394.6 | -1.0% |
| sum.kth_y | 492.0 | 478.7 | 2.7% |
| sum.sum_k_smallest_y | 783.0 | 827.1 | -5.6% |
| sum.sum_k_largest_y | 781.6 | 828.2 | -6.0% |
| min.build | 1211.3 | 1152.9 | 4.8% |
| min.range_min | 3085.9 | 3140.6 | -1.8% |
| min.range_argmin | 3094.5 | 3146.1 | -1.7% |
| min.range_max | 3087.0 | 3135.6 | -1.6% |
| min.range_argmax | 3091.5 | 3132.6 | -1.3% |
| monoid.build | 650.5 | 539.5 | 17.1% |
| monoid.range_prod_x | 305.4 | 305.0 | 0.1% |
| monoid.range_prod | 2744.8 | 2803.1 | -2.1% |

## 検証

- `test/dwm/` のランダムテスト 11 本を、`-march=native`、既定ビルド、SIMD を無効にしたビルド (`-U__SSE2__ -U__SSE__ -U__BMI2__`) の 3 構成で実行し、すべて通った。
- 新しく `bit_vector_random.cpp` と `b_tree_sum_sequence_random.cpp` を加えた。`wavelet_matrix_sum_random.cpp` と `wavelet_matrix_bit_extended_random.cpp` には、WaveletMatrix の prev_value・next_value・range_count・select などを素朴な計算と比べる検査を加えた。追加した検査は HEAD のヘッダでも通る。
- `select64` は、3 構成で 300 万通りの値を素朴な計算と比べた。
- 展開スクリプトで 1 ファイルにまとめたコードが、AVX2 ありとなしの両方でコンパイルでき、正しい値を返すことを確かめた。
