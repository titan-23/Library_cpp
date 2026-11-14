# bitset.cpp

使い方と API の一覧は [titan_cpplib/ds/bitset.md](../../titan_cpplib/ds/bitset.md) にある。

## 計算方式

- `BitsetBackend::Auto` は SimdBits の SIMD 実装を使う。SimdBits はコンパイル時に有効な命令から AVX2 か SSE2 を選ぶ。
- `BitsetBackend::Avx2` は AVX2 の SIMD 実装を常に使う。AVX2 向けにコンパイルしないと static_assert で止まる。
- `BitsetBackend::Scalar` は SimdBits を使わない。ただし、コンパイラが自動ベクトル化する場合がある。

## 内部表現

- 64bit 単位の配列 `words_` で持つ。ビット i は `words[i / 64]` の下位から i % 64 番目に格納する。
- 各操作の計算量では W = ceil(N / 64)、K = 入力する Bitset の個数とする。

## count を SimdBits で処理する語数の下限

`Auto` では、語数が `simd_count_min` 以上のときだけ count と count_* を SimdBits で処理する。下限は有効な命令で決める。

- AVX512VPOPCNTDQ が有効なときは SimdBits を使わない。通常のループがベクトルの popcount 命令になるので、コンパイラに任せる。
- AVX2 が有効なときは 16 語以上に限る。小さいサイズでは AVX2 が遅くなる計測結果があった。
- POPCNT が有効で AVX2 がないときは SimdBits を使わない。SSE2 のビット演算による数え上げは POPCNT 命令より遅い。
- POPCNT 命令がないときは、Vec 1 本分の語数から SimdBits を使う。POPCNT 命令がないと std::popcount は関数呼び出しになる。

## 自由関数の共通処理

- `bitset_detail::Access` は、自由関数で使う共通処理をまとめる。
- count、any、all は中間の Bitset を作らずに計算する。
- 演算の種類と入力数はコンパイル時に決まる。
