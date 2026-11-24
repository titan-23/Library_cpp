# simd_int.cpp

## ファイルの構成

- 先頭は SimdInt の定義です。続けて、simd_find_first_greater、simd_equal_bits などの固定長の小さい配列向けの関数を定義しています。
- その後に、64 bit 語の配列をビット演算で扱う SimdBits と、比較の結果を要素ごとのマスクで表す SimdVec を定義しています。

## SimdInt

- 32 bit と 64 bit の符号付き整数の SIMD 演算を、命令セットに依存しない名前でまとめます。
- コンパイル時に AVX-512 (F, VL)、AVX2、SSE2、逐次処理の順で実装を選択します。SSE2 版は 32 bit 整数だけを扱い、64 bit 整数は逐次処理になります。
- T の配列は Vec 1 本分にあたる LANES 要素ずつ扱います。int_* は T の配列と同じ要素数の int 配列を扱います。
- Mask は要素 i を bit i に対応させた整数です。lane_mask(rest) は先頭の min(rest, LANES) 要素を表します。
- load、store、int_store、int_nonzero は Mask で指定した要素だけを読み書きします。load はそれ以外の要素を 0 にします。
- add、sub、reduce_add は 2 の補数でラップアラウンドします。オーバーフローした要素は使用しないことを前提としています。
- cmpgt_bits は全要素を比較し、結果を unsigned の bit 列で返します。
- simd_int_supported_v は SimdInt で扱うことができる型かどうかを表します。比較を使用する関数では、さらに型が符号付きである必要があります。
- AVX2 には比較の結果を格納するマスクレジスタがありません。そのため、比較の結果は movemask で bit 列に変換します。マスク付きの読み書きでは、bit 列を要素ごとに全ビットが 1 か 0 のマスクに戻してから maskload と maskstore を使用します。
- SSE2 には 32 bit の min と max、およびマスク付きの読み書きがありません。そのため、min と max は比較と論理演算で実装します。一部の要素だけの読み書きは一時配列を経由します。
- 使用例を次に示します。

```cpp
using V = SimdInt<long long>;
for (int j = 0; j < n; j += V::LANES) { auto m = V::lane_mask(n - j); auto x = V::load(m, a + j); ... }
```

## 固定長の小さい配列向けの関数

- simd_find_first_greater、simd_find_first_greater_diff、simd_masked_sum、simd_equal_bits では 0 < N <= 63 が必要です。
- _unchecked の付く版は、該当する i が必ず存在することを前提として番兵を省きます。
- simd_find_first_greater_diff は、差 a[i] - b[i] が T に収まることを前提としています。
- simd_masked_sum の逐次版は i の昇順に加算します。
- simd_equal_bits は符号の有無を問いません。a[0, N) はすべて読み取り可能である必要があります。
- SimdInt の SSE2 版は 32 bit 整数だけを扱います。そのため、SSE2 だけを使用できる環境での 64 bit 整数の一致判定は simd_equal_bits の中で別に実装しています。

## SimdBits

- 64 bit 語の配列をビット演算で扱います。コンパイル時に AVX2、SSE2、逐次処理の順で実装を選択します。AVX-512 のターゲットでも 256 bit の AVX2 版を使用します。
- Vec は 64 bit 語を LANES 個保持します。loadu と storeu はアラインメントされていないアドレスも扱います。
- shl と shr は各語を r (0 <= r < 64) ビットシフトし、語の外に出たビットは破棄します。
- nonzero は 1 のビットがあるかどうかを返します。
- 1 のビットの個数は simd_bits_popcount で求めます。byte_popcount は各バイトの 1 のビットの個数を求め、sum_bytes は 8 バイトごとの和を 64 bit 語に格納します。HAS_POPCOUNT64 が真の場合は popcount64 で語ごとに求めます。
- SSE2 には表引きに使用するバイト単位のシャッフルと ptest がありません。そのため、1 のビットの個数はビット演算で求め、非 0 の判定は比較で行います。
- simd_bits_popcount の S は既定のまま使用します。S をテンプレート引数にしているのは、使用しない側の分岐がコンパイル時にチェックされないようにするためです。

## SimdVec

- 符号付き整数 (32 bit, 64 bit) と float, double を扱います。比較の結果を要素ごとのマスクで返します。このマスクは select で直接使用できます。
- SimdInt は比較の結果を bit 列で返します。選択のたびに bit 列をマスクに戻すと AVX2 では 3 命令増えるため、最小値と番号を同時に求める走査用に SimdVec を別に作成しました。
- 実装は AVX2、SSE2、逐次処理の順に選択します。AVX-512 のターゲットでも AVX2 版を使用します。
- SSE2 版は 64 bit 整数を扱いません。SSE2 には 64 bit の比較がなく、他の命令を組み合わせて実装すると逐次処理より命令数が多くなるためです。
- 符号なし整数は逐次処理になります。符号なし整数が符号付きの比較を使用する実装で処理されないように、整数の特殊化は符号付きに限定しています。
- mul_rounded は積の直後に空の asm を配置し、積と後続の加算が FMA に融合されないようにします。GCC の組み込み関数は内部でベクトルの演算子を使用するため、積と和が FMA にまとめられる場合があります。
- argmin は要素ごとの最小値と番号から、値が最小の組のうち番号が最小のものを選択します。各要素が、その要素で最初に現れた最小値を保持していれば、逐次処理で最初の最小値を選択する場合と同じ結果になります。
- Vec は T を LANES 個保持します。IVec は T と同じ幅の符号付き整数 Idx を LANES 個保持し、番号の格納に使用します。
- lt(a, b) は要素ごとの a < b の結果を Mask で返します。NaN を含む要素は偽になります。select(m, a, b) は m が真の要素で a を選択します。
- mul_rounded、div、sqrt があるのは浮動小数点型だけです。mul_rounded は積を丸めてから返します。積と後続の加算が FMA に融合されないため、逐次処理の計算と同じ値になります。
- argmin は値が最小の要素の値と番号を返します。同じ値が複数ある場合は、番号が最小のものを返します。
- 整数の add と sub は 2 の補数でラップアラウンドします。
- 対応していない型は逐次処理になります。
- SSE2 には blendv と 64 bit 整数の比較がありません。そのため、選択は論理演算で実装し、番号の一致は 32 bit ずつ比較して判定します。
- SimdVec は Hungarian の走査とクラスタリングの距離の計算で使用しています。詳細は [graph/hungarian.md](../graph/hungarian.md) と [ahc/clustering/emeans.md](../ahc/clustering/emeans.md) に記載しています。
