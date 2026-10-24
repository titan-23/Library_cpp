# DynamicWaveletTree 高速化の測定記録

2026-10-24。N=Q=300,000、主な値域を σ=N、追加の値域を σ=10⁹ として比較した。測定対象は重みなしの `DynamicWaveletTree<uint64_t>`。`DynamicWaveletTreeSum` は変更していない。

変更前はコミット `ff45365` の `b_tree_bit_vector.cpp` と `dynamic_wavelet_tree.cpp`、変更後は今回の実装。同一の[ベンチマーク](dynamic_wavelet_tree_speed_benchmark.cpp)を、それぞれのヘッダでコンパイルした。

測定条件は AMD Ryzen AI 9 HX 370、Linux/WSL、GCC 15.2.0。`-std=c++17 -O3 -DNDEBUG -march=native` を使用し、CPU 4 に固定。変更前後を交互に実行し、主測定は5回、追加条件は3回の中央値を採用した。並行するベンチマーク・コンパイルは停止して測定した。

乱数は `mt19937_64`、seed=23。入力・操作の生成、更新前の構築、更新後の状態ハッシュは更新の計測外。各測定のチェックサムは変更前後で一致した。`insert` は N→N+Q に増加、`pop` と `select_remove` は全要素を削除する。`insert_pop` は30万組、計60万操作。`mixed` は set / kth_smallest / range_freq / access を各25%実行する。

表の時間は各測定の合計ミリ秒。「短縮」は `1 - 変更後 / 変更前`。正の値が高速化を表す。

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

追加条件の「偏り」は90%の値を [0,16)、10%を全値域から生成する。「汎用」は `-march=native` を省略したビルド。9項目の参考条件は build / rank / range_count / kth_smallest / select / insert / pop / set / mixed を測定した。

| 条件 | 測定項目数 | 各項目の短縮率の範囲 | mixed の短縮率 |
|---|---:|---:|---:|
| 汎用・300,000・一様 | 15 | 4.0% ～ 27.8% | 8.3% |
| native・300,000・偏り | 15 | 1.2% ～ 46.3% | 4.3% |
| native・256・一様 | 9 | 6.0% ～ 44.4% | 12.4% |
| native・2²⁰・一様 | 15 | 5.8% ～ 41.8% | 13.3% |
| native・2²⁰・偏り | 9 | -1.5% ～ 43.5% | -1.5% |
| native・2³⁰・一様 | 9 | 2.0% ～ 26.6% | 12.7% |

σ=2²⁰ の偏り条件では mixed が1.5%増加した。入力分布やCPUによって効果は変わり、すべての条件で一律に改善するとは限らない。

採用した変更:

- 内部ノードを8分岐にし、AVX2/SSE2で累積値を比較する。未対応環境には通常の走査を残す。
- 小さいビット列を下位ビットから格納し、rank・access・更新の演算を簡単にする。
- select は BMI2 が有効なら PDEP、無効なら小さなテーブルを使う。
- 挿入と select-pop は降下中に祖先の集計値を更新する。通常の pop は従来の更新順を維持する。
- 小さい表現と木の変換はビットを展開せずに行い、縮小の閾値を96 bitにする。ノードの領域は必要になってから確保する。
- ウェーブレット木の構築では値を直接安定分割する。候補が空・1要素になった検索を短縮し、topk の1要素部分木も直接確定する。

64 bitチャンク、葉への累積メタデータ追加、幅優先配置、通常の削除までの先行更新、一様ビット列の確認、挿入・削除へのPDEP/PEXT使用も比較したが、広い条件で安定した改善が得られず採用していない。APIと計算量は維持している。64 bitキーでは構築用一時バッファがN=300,000で約2.4 MB増える。32 bitキーではこの増加はない。

再現例（リポジトリ直下で実行）:

```sh
mkdir -p /tmp/dwt-before/titan_cpplib/ds
git show ff45365:titan_cpplib/ds/b_tree_bit_vector.cpp > /tmp/dwt-before/titan_cpplib/ds/b_tree_bit_vector.cpp
git show ff45365:titan_cpplib/ds/dynamic_wavelet_tree.cpp > /tmp/dwt-before/titan_cpplib/ds/dynamic_wavelet_tree.cpp
g++ -std=c++17 -O3 -DNDEBUG -march=native -I/tmp/dwt-before -I. test/ds/benchmark/dynamic_wavelet_tree_speed_benchmark.cpp -o /tmp/dwt-before-bench
g++ -std=c++17 -O3 -DNDEBUG -march=native -I. test/ds/benchmark/dynamic_wavelet_tree_speed_benchmark.cpp -o /tmp/dwt-after-bench
taskset -c 4 /tmp/dwt-before-bench --n 300000 --q 300000 --sigma 300000
taskset -c 4 /tmp/dwt-after-bench --n 300000 --q 300000 --sigma 300000
```

値域10⁹は `--sigma 1000000000`、偏り条件は `--distribution skew` を指定する。`--operations rank,select,mixed` で対象を絞れる。複数回、実行順を入れ替えて比較する。

検証:

- 拡充した `test/dwm/b_tree_bit_vector_random.cpp`：30万bitの構築、64/128 bit境界、全長0..128のパターン、reserve・copy・move、Coreの空状態・再利用、96万回のランダム操作。
- 拡充した `test/dwm/dynamic_wavelet_random.cpp`：既存の差分テストに加え、候補1要素、非2冪の値域、64 bitの上位ビット、topk同頻度順序。
- `test/dwm/wavelet_matrix_extended_random.cpp`。
- 上記ビット列・動的ウェーブレット木の native ASan/UBSan、通常ビルド、SSE2無効・BMI2有効のビット列テスト。

通常のテストは `assert` 内の更新を含むため `-DNDEBUG` を付けずに実行した。この環境では LeakSanitizer が ptrace の制約で動作しないため、ASan/UBSan 実行には `ASAN_OPTIONS=detect_leaks=0` を使用した。

