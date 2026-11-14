# bit.cpp

## PDEP を使う条件

- PDEP は Zen 1 と Zen 2 ではマイクロコードで実装されている。1 回に数十から数百サイクルかかる。
- そのため PDEP は速い CPU 向けにビルドするときだけ使う。
- 具体的には、`__BMI2__` と `__x86_64__` が定義され、`__znver1__` と `__znver2__` が定義されていないときに `TITAN23_FAST_PDEP` を定義する。
- `TITAN23_FAST_PDEP` を使う側の記述は [ds/b_tree_bit_vector.md](../ds/b_tree_bit_vector.md) にある。

## select64

- `TITAN23_FAST_PDEP` が定義されていれば、`_pdep_u64(1ull << k, x)` の最下位の 1 の位置を返す。
- 定義されていなければ、下半分の 1 の個数を見て、k 番目が上下どちらにあるかを 32 bit、16 bit、8 bit の幅で 3 回絞り込む。最後の 1 バイトは 256 要素の表で引く。
- 表 `select_in_byte_table[b][j]` は、1 バイトの値 b で下から j 番目に立っている bit の位置を持つ。
