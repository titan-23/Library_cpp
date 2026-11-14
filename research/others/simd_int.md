# simd_int.cpp

## ファイルの構成

- 先頭に SimdInt を置く。続けて、長さが固定の小さい配列向けの関数 simd_find_first_greater、simd_equal_bits などを置く。
- その後に、64 bit 語の配列をビット演算で扱う SimdBits と、比較の結果を要素ごとのマスクで持つ SimdVec を置く。

## SimdInt

- 32 bit と 64 bit の符号付き整数の SIMD 演算を、命令セットによらない名前でまとめる。
- コンパイル時に AVX-512 (F, VL)、AVX2、SSE2、逐次の順で実装を選ぶ。SSE2 は 32 bit 整数だけを扱い、64 bit 整数は逐次になる。
- T の配列は Vec 1 本、つまり LANES 要素ずつ扱う。int_* は T の配列と同じ要素数の int 配列を扱う。
- Mask は要素 i を bit i に対応させた整数である。lane_mask(rest) は先頭 min(rest, LANES) 要素を表す。
- load、store、int_store、int_nonzero は Mask の要素だけを読み書きする。load は他の要素を 0 にする。
- add、sub、reduce_add は 2 の補数で巡回する。あふれた要素は使わない前提である。
- cmpgt_bits は全要素を比べ、結果を unsigned の bit 列で返す。
- simd_int_supported_v は SimdInt で扱える型かを表す。比較を使う関数は、さらに符号付きであることを要求する。
- AVX2 には比較の結果を入れるマスクレジスタがない。そのため比較は movemask で bit 列に変換する。マスク付きの読み書きでは、bit 列を要素ごとの全ビット 1 か 0 に戻して maskload と maskstore を使う。
- SSE2 には 32 bit の min と max、マスク付きの読み書きがない。そのため比較と論理演算で組み立てる。一部の要素だけの読み書きは一時配列を経由する。
- 使い方の例を示す。

```cpp
using V = SimdInt<long long>;
for (int j = 0; j < n; j += V::LANES) { auto m = V::lane_mask(n - j); auto x = V::load(m, a + j); ... }
```

## 固定長の小さい配列向けの関数

- simd_find_first_greater、simd_find_first_greater_diff、simd_masked_sum、simd_equal_bits は 0 < N <= 63 を要求する。
- _unchecked の付く版は、該当する i が必ずある前提で番兵を省く。
- simd_find_first_greater_diff は、差 a[i] - b[i] が T に収まる前提である。
- simd_masked_sum の逐次版は i の昇順に足す。
- simd_equal_bits は符号の有無を問わない。a[0, N) はすべて読める必要がある。
- SimdInt の SSE2 段は 32 bit 整数だけを扱う。そのため SSE2 だけの環境での 64 bit 整数の一致判定は、simd_equal_bits の中で別に組む。

## SimdBits

- 64 bit 語の配列をビット演算で扱う。コンパイル時に AVX2、SSE2、逐次の順で実装を選ぶ。AVX-512 のターゲットでも 256 bit の AVX2 段を使う。
- Vec は 64 bit 語を LANES 個持つ。loadu と storeu は整列していない番地も扱う。
- shl と shr は各語を r (0 <= r < 64) ビットずらし、語の外へ出たビットは捨てる。
- nonzero は 1 のビットがあるかを返す。
- 1 の数え上げには simd_bits_popcount を使う。byte_popcount は各バイトの 1 の数を、sum_bytes は 8 バイトごとの和を 64 bit 語に入れる。HAS_POPCOUNT64 なら popcount64 で語ごとに数える。
- SSE2 には表引きに使うバイト単位のシャッフルと ptest がない。そのため 1 の数え上げはビット演算で、非 0 判定は比較で行う。
- simd_bits_popcount の S は既定のまま使う。S をテンプレート引数にしているのは、使わない側の分岐を検査させないためである。

## SimdVec

- 符号付き整数 (32 bit, 64 bit) と float, double を扱う。比較の結果を要素ごとのマスクで返し、select で直接使える。
- SimdInt は比較の結果を bit 列で返す。選択のたびにマスクへ戻すと AVX2 で 3 命令増えるので、最小値と番号を同時に求める走査のために別に作った。
- 実装は AVX2、SSE2、逐次の順に選ぶ。AVX-512 のターゲットでも AVX2 段を使う。
- SSE2 段は 64 bit 整数を扱わない。SSE2 には 64 bit の比較がなく、組み立てると逐次より命令が多い。
- 符号なし整数は逐次になる。符号付きの比較を使う実装に入れないため、整数の特殊化は符号付きに限った。
- mul_rounded は積の後ろに空の asm を置き、続く加算と FMA に融合させない。GCC の組み込み関数は内部でベクトルの演算子を使うので、積と和を FMA にまとめることがある。
- argmin は要素ごとの最小値と番号から、値が最小で番号が最小の組を選ぶ。各要素で最初に現れた最小値を持てば、逐次で最初の最小値を選ぶのと同じ結果になる。
- Vec は T を LANES 個持つ。IVec は T と同じ幅の符号付き整数 Idx を LANES 個持ち、番号を入れるのに使う。
- lt(a, b) は要素ごとの a < b を Mask で返す。NaN を含む要素は偽になる。select(m, a, b) は m が真の要素で a を選ぶ。
- 浮動小数点だけが mul_rounded、div、sqrt を持つ。mul_rounded は積を丸めてから返す。続く加算と FMA に融合しないので、逐次の計算と同じ値になる。
- argmin は値が最小の要素の値と番号を返す。同じ値が複数あれば、番号が最小のものを返す。
- 整数の add と sub は 2 の補数で巡回する。
- 対応しない型は逐次になる。
- SSE2 には blendv と 64 bit 整数の比較がない。そのため選択は論理演算で組み、番号の一致は 32 bit ずつ比べる。
- 使っている箇所は Hungarian の走査と、クラスタリングの距離の計算である。詳細は [graph/hungarian.md](../graph/hungarian.md) と [ahc/clustering/emeans.md](../ahc/clustering/emeans.md) に書いた。
