# bitset.cpp

使用方法と API の一覧は [titan_cpplib/ds/bitset.md](../../titan_cpplib/ds/bitset.md) に記載しています。

## 計算方式

- `BitsetBackend::Auto` は SimdBits の SIMD 実装を使用します。SimdBits はコンパイル時に有効な命令に応じて AVX2 か SSE2 を選択します。
- `BitsetBackend::Avx2` は AVX2 の SIMD 実装を常に使用します。AVX2 向けにコンパイルしない場合は static_assert によりコンパイルエラーになります。
- `BitsetBackend::Scalar` は SimdBits を使用しません。ただしコンパイラが自動ベクトル化する場合があります。

## 内部表現

- ビット列を 64 bit 単位の配列 `words_` で保持します。ビット i は `words[i / 64]` の下位から i % 64 番目に格納します。
- 各操作の計算量では W = ceil(N / 64)、K = 入力する Bitset の個数とします。

## count を SimdBits で処理する語数の下限

`Auto` では語数が `simd_count_min` 以上の場合だけ count と count_* を SimdBits で処理します。下限は有効な命令に応じて定めます。

- AVX512VPOPCNTDQ が有効な場合は SimdBits を使用しません。通常のループがベクトルの popcount 命令にコンパイルされるため、コンパイラに任せます。
- AVX2 が有効な場合は 16 語以上に限定します。小さいサイズでは AVX2 が遅くなる計測結果がありました。
- POPCNT が有効で AVX2 がない場合は SimdBits を使用しません。SSE2 のビット演算で 1 の個数を求める方法は POPCNT 命令より遅くなります。
- POPCNT 命令がない場合は Vec 1 本分の語数から SimdBits を使用します。POPCNT 命令がないと std::popcount が関数呼び出しになるためです。

## 自由関数の共通処理

- `bitset_detail::Access` には自由関数で使用する共通処理をまとめています。
- count、any、all は中間の Bitset を作成せずに計算します。
- 演算の種類と入力数はコンパイル時に定まります。
