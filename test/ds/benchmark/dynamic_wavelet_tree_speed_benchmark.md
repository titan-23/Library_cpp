# DynamicWaveletTree 高速化の測定記録

2026-10-24 の記録です。N=Q=300,000 とし、主な値域を σ=N、追加の値域を σ=10⁹ として比較しました。測定対象は重みなしの `DynamicWaveletTree<uint64_t>` です。`DynamicWaveletTreeSum` は変更していません。

変更前はコミット `ff45365` の `b_tree_bit_vector.cpp` と `dynamic_wavelet_tree.cpp`、変更後は今回の実装です。同一の[ベンチマーク](dynamic_wavelet_tree_speed_benchmark.cpp)をそれぞれのヘッダでコンパイルしました。

測定条件は AMD Ryzen AI 9 HX 370、Linux/WSL、GCC 15.2.0 です。`-std=c++17 -O3 -DNDEBUG -march=native` を使用し、CPU 4 に固定しました。変更前後を交互に実行し、主測定は 5 回、追加条件は 3 回の中央値を採用しました。並行して動作するベンチマークとコンパイルは停止して測定しました。

乱数は `mt19937_64` で、seed=23 です。入力と操作の生成、更新前の構築、更新後の状態のハッシュ値の計算は、更新の計測の対象外です。各測定のチェックサムは変更前後で一致しました。`insert` では要素数が N から N+Q に増加し、`pop` と `select_remove` では全要素を削除します。`insert_pop` は 30 万組で、計 60 万操作です。`mixed` は set / kth_smallest / range_freq / access をそれぞれ 25% の割合で実行します。

表の時間は各測定の合計時間で、単位はミリ秒です。「短縮」は `1 - 変更後 / 変更前` で、正の値が高速化を表します。

| 操作 | σ=N 前 ms | σ=N 後 ms | 短縮 | σ=10⁹ 前 ms | σ=10⁹ 後 ms | 短縮 |
|---|---:|---:|---:|---:|---:|---:|
| build | 31.222 | 28.261 | 9.5% | 105.541 | 99.964 | 5.3% |
| access | 176.572 | 155.952 | 11.7% | 327.996 | 290.247 | 11.5% |
| rank | 160.835 | 134.709 | 16.2% | 225.660 | 186.139 | 17.5% |
| range_count | 264.088 | 227.857 | 13.7% | 319.346 | 253.396 | 20.7% |
| range_freq | 270.656 | 233.209 | 13.8% | 290.365 | 255.013 | 12.2% |
| kth_smallest | 294.096 | 260.081 | 11.6% | 423.470 | 377.683 | 10.8% |
| prev_value | 283.195 | 256.066 | 9.6% | 447.831 | 381.597 | 14.8% |
| next_value | 282.092 | 244.669 | 13.3% | 420.787 | 372.921 | 11.4% |
| select | 362.319 | 194.649 | 46.3% | 475.729 | 334.512 | 29.7% |
| insert | 306.244 | 259.407 | 15.3% | 525.671 | 476.981 | 9.3% |
| pop | 247.905 | 233.104 | 6.0% | 400.913 | 372.285 | 7.1% |
| insert_pop | 519.191 | 462.258 | 11.0% | 598.826 | 530.790 | 11.4% |
| set | 508.508 | 449.181 | 11.7% | 678.694 | 615.057 | 9.4% |
| select_remove | 360.543 | 206.943 | 42.6% | 504.352 | 337.515 | 33.1% |
| mixed | 361.927 | 316.227 | 12.6% | 498.157 | 432.338 | 13.2% |

追加条件の「偏り」では、90% の値を [0,16) から、10% の値を全値域から生成します。「汎用」は `-march=native` を省略したビルドです。9 項目の参考条件では build / rank / range_count / kth_smallest / select / insert / pop / set / mixed を測定しました。

| 条件 | 測定項目数 | 各項目の短縮率の範囲 | mixed の短縮率 |
|---|---:|---:|---:|
| 汎用、300,000、一様 | 15 | 4.0% ～ 27.8% | 8.3% |
| native、300,000、偏り | 15 | 1.2% ～ 46.3% | 4.3% |
| native、256、一様 | 9 | 6.0% ～ 44.4% | 12.4% |
| native、2²⁰、一様 | 15 | 5.8% ～ 41.8% | 13.3% |
| native、2²⁰、偏り | 9 | -1.5% ～ 43.5% | -1.5% |
| native、2³⁰、一様 | 9 | 2.0% ～ 26.6% | 12.7% |

σ=2²⁰ の偏り条件では mixed の時間が 1.5% 増加しました。効果は入力分布や CPU によって変化し、すべての条件で一律に改善するとは限りません。

採用した変更は次のとおりです。

- 内部ノードを 8 分岐にし、AVX2/SSE2 で累積値を比較します。未対応の環境向けに通常の走査も残しています。
- 小さいビット列を下位ビットから格納し、rank、access、更新の演算を簡単にします。
- select は BMI2 が有効な場合は PDEP、無効な場合は小さなテーブルを使用します。
- 挿入と select-pop は降下中に祖先の集計値を更新します。通常の pop では従来の更新順を維持しています。
- 小さい表現と木の間の変換はビットを展開せずに行い、縮小の閾値を 96 bit にします。ノードの領域は必要になってから確保します。
- ウェーブレット木の構築では値を直接安定分割します。候補が空または 1 要素になった検索を短縮し、topk の 1 要素の部分木も直接確定します。

次の変更も比較しましたが、広い条件で安定した改善が得られなかったため採用していません。

- 64 bit のチャンク
- 葉への累積メタデータの追加
- 幅優先の配置
- 通常の削除への先行更新の適用
- 一様なビット列の確認
- 挿入と削除での PDEP/PEXT の使用

API と計算量は維持しています。64 bit のキーでは、構築用の一時バッファが N=300,000 で約 2.4 MB 増加します。32 bit のキーではこの増加はありません。

再現例は次のとおりです。リポジトリの直下で実行します。

```sh
mkdir -p /tmp/dwt-before/titan_cpplib/ds
git show ff45365:titan_cpplib/ds/b_tree_bit_vector.cpp > /tmp/dwt-before/titan_cpplib/ds/b_tree_bit_vector.cpp
git show ff45365:titan_cpplib/ds/dynamic_wavelet_tree.cpp > /tmp/dwt-before/titan_cpplib/ds/dynamic_wavelet_tree.cpp
g++ -std=c++17 -O3 -DNDEBUG -march=native -I/tmp/dwt-before -I. test/ds/benchmark/dynamic_wavelet_tree_speed_benchmark.cpp -o /tmp/dwt-before-bench
g++ -std=c++17 -O3 -DNDEBUG -march=native -I. test/ds/benchmark/dynamic_wavelet_tree_speed_benchmark.cpp -o /tmp/dwt-after-bench
taskset -c 4 /tmp/dwt-before-bench --n 300000 --q 300000 --sigma 300000
taskset -c 4 /tmp/dwt-after-bench --n 300000 --q 300000 --sigma 300000
```

値域 10⁹ では `--sigma 1000000000`、偏り条件では `--distribution skew` を指定します。`--operations rank,select,mixed` のように指定すると測定の対象を限定できます。比較するときは実行順を入れ替えて複数回実行します。

検証の内容は次のとおりです。

- 拡充した `test/dwm/b_tree_bit_vector_random.cpp` では、30 万 bit の構築、64/128 bit の境界、全長 0..128 のパターン、reserve / copy / move、Core の空状態と再利用、96 万回のランダム操作を確認しました。
- 拡充した `test/dwm/dynamic_wavelet_random.cpp` では、既存の差分テストに加えて、候補が 1 要素の場合、2 の冪でない値域、64 bit の上位ビット、topk で頻度が同じ要素の順序を確認しました。
- `test/dwm/wavelet_matrix_extended_random.cpp` も実行しました。
- 上記のビット列と動的ウェーブレット木のテストは、native の ASan/UBSan と通常ビルドで実行しました。ビット列のテストは SSE2 無効と BMI2 有効の構成でも実行しました。

通常のテストは `assert` 内に更新を含むため、`-DNDEBUG` を付けずに実行しました。この環境では LeakSanitizer が ptrace の制約で動作しないため、ASan/UBSan での実行には `ASAN_OPTIONS=detect_leaks=0` を使用しました。

