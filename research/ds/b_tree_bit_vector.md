# b_tree_bit_vector.cpp

## 構成

- `BTreeBitVector` は、長さが 127 bit 以下のとき 2 個の `uint64_t` にビット列を直接持つ。これを小表現と呼ぶ。
- 長さが 127 bit を超えると、`BTreeBitVectorCore` を確保して木表現へ移る。`_bit_len` が `UINT8_MAX` のとき木表現である。
- `BTreeBitVectorCore` は B 木である。葉は最大 127 bit の区画を最大 8 個並べ、内部節点は最大 8 個の子を持つ。コードでは区画を chunk と呼ぶ。
- 根以外の葉と内部節点は、区画の数または子の数を容量の半分の 4 以上に保つ。
- 内部節点は、子ごとの長さと 1 の個数を累積値の配列 `child_size` と `child_total` で持つ。
- 葉は `next` で次の葉とつなぐ。`tovector` と小表現への戻しは、この連結をたどる。
- 葉と内部節点は配列に置き、番号 0 を空の印に使う。解放した節点は空きリストでつなぎ、再利用する。
- 127 bit の区画に挿入すると 128 bit になるので、64 bit ずつの 2 個の区画に分ける。
- 削除の後、隣の区画との合計が 127 bit 以下になれば 2 個を併合する。

## 小表現と木表現の切替え

- 削除の後で長さが `_DEMOTE_W` の 96 bit 以下になれば、小表現へ戻す。
- 戻す長さを 127 bit より小さくしたのは、小表現の容量の境界をまたぐ更新で確保が繰り返されるのを避けるためである。
- `reserve(n)` で n が 127 を超えるときは、その時点で木表現へ移る。

## bit の順序

- 小表現は、論理上の先頭の bit を bit 0 に置き、下位 bit から並べる。この順にすると rank と access が単純になる。
- Core の区画は向きが逆で、論理上の先頭を上位 bit に置く。
- 2 つの順序の変換は `_reverse_key` で行う。bit を 1 個ずつ取り出さず、64 bit の反転を 2 回行ってからずらす。

## 子の探索と SIMD

- `_find_size` と `_find_count` は、累積値が k を超える最初の子を探す。探索には [simd_int.cpp](../../titan_cpplib/others/simd_int.cpp) の `simd_find_first_greater_unchecked` と `simd_find_first_greater_diff_unchecked` を使う。
- 使用中の子のどれかは必ず条件を満たす。未使用の要素はその後ろにあるので、節点の再利用で古い値が残っていても結果は変わらない。
- unchecked 版は該当する要素があることを前提にする関数で、上の性質がこの前提を満たす。
- `_find_count` で bit が 0 のときは、`child_size` と `child_total` の差を比べる。
- SIMD の探索は未使用の要素も読む。このため `Internal` のコンストラクタで `child_size` と `child_total` をすべて初期化する。

## 削除での祖先の更新

- `_pop_leaf_bit` は葉から bit を 1 個除き、必要なら葉を再平衡する。
- select で位置を探して削除する `_select_pop` は、根から降りる途中で祖先の累積値を更新する。そのため `_pop_leaf_bit` を `UpdateAncestors = false` で呼ぶ。
- 位置で削除する `_access_pop_and_rank1` は、除く bit の値を葉に着くまで知らない。そのため `_pop_leaf_bit` の中で祖先を更新する。

## 領域の確保

- 葉と内部節点の配列は、初めて使うときに確保する。葉が 1 個だけの木は内部節点を使わない。
- `reserve(n)` は、区画を 64 bit、葉と内部節点を半分だけ埋まった状態と見て、必要な節点の数を見積もる。

## 64 bit 語の中での select

- `_select64` は `TITAN23_FAST_PDEP` が定義されていれば PDEP を使う。定義されていなければ、32 bit、16 bit、8 bit と範囲を絞ってから 256 要素の表を引く。
- `TITAN23_FAST_PDEP` は [bit.cpp](../../titan_cpplib/others/bit.cpp) で定める。Zen 1 と Zen 2 では PDEP がマイクロコードで遅いので定めない。

## 関連するファイル

- テスト: [test/dwm/b_tree_bit_vector_random.cpp](../../test/dwm/b_tree_bit_vector_random.cpp)
- 計測: [test/ds/benchmark/dynamic_wavelet_tree_speed_benchmark.md](../../test/ds/benchmark/dynamic_wavelet_tree_speed_benchmark.md)
