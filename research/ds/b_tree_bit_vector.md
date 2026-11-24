# b_tree_bit_vector.cpp

## 構成

- `BTreeBitVector` は、長さが 127 bit 以下であれば 2 個の `uint64_t` にビット列を直接保持します。この形式を小表現と呼びます。
- 長さが 127 bit を超えると `BTreeBitVectorCore` を確保して木表現へ移行します。`_bit_len` が `UINT8_MAX` の場合は木表現です。
- `BTreeBitVectorCore` は B 木です。葉は最大 127 bit の区画を最大 8 個保持し、内部節点には最大 8 個の子があります。コードでは区画を chunk と呼んでいます。
- 根以外の葉と内部節点では、区画の数または子の数を容量の半分である 4 以上に維持します。
- 内部節点は子ごとの長さと 1 の個数を累積値の配列 `child_size` と `child_total` で保持します。
- 各葉は `next` で次の葉を指します。`tovector` と小表現へ戻す処理は、この参照を順にたどります。
- 葉と内部節点は配列に格納し、番号 0 は節点がないことを表す値として使用します。解放した節点は空きリストに連結して再利用します。
- 127 bit の区画に挿入すると 128 bit になるため、64 bit ずつの 2 個の区画に分割します。
- 削除の後に隣の区画との合計が 127 bit 以下になった場合は 2 個を併合します。

## 小表現と木表現の切替え

- 削除の後で長さが `_DEMOTE_W` の 96 bit 以下になった場合は小表現へ戻します。
- 小表現へ戻す基準の長さを 127 bit より小さくしているのは、小表現の容量の境界をまたぐ更新で確保が繰り返されることを避けるためです。
- `reserve(n)` で n が 127 を超える場合は、その時点で木表現へ移行します。

## bit の順序

- 小表現では論理上の先頭の bit を bit 0 に配置し、下位 bit から順に格納します。この順序にすると rank と access が単純になります。
- Core の区画は向きが逆で、論理上の先頭を上位 bit に配置します。
- 2 つの順序の変換は `_reverse_key` で行います。bit を 1 個ずつ取り出さず、64 bit の反転を 2 回行ってからシフトします。

## 子の探索と SIMD

- `_find_size` と `_find_count` は、累積値が k を超える最初の子を探します。探索には [simd_int.cpp](../../titan_cpplib/others/simd_int.cpp) の `simd_find_first_greater_unchecked` と `simd_find_first_greater_diff_unchecked` を使用します。
- 使用中の子のいずれかは必ず条件を満たします。未使用の要素はその後ろにあるため、節点の再利用で古い値が残っていても結果には影響しません。
- unchecked 版は該当する要素があることを前提とする関数であり、上記の性質によりこの前提が満たされます。
- `_find_count` で bit が 0 の場合は、`child_size` と `child_total` の差を k と比較します。
- SIMD の探索は未使用の要素も参照します。このため `Internal` のコンストラクタで `child_size` と `child_total` をすべて初期化します。

## 削除での祖先の更新

- `_pop_leaf_bit` は葉から bit を 1 個削除し、必要な場合は葉を再平衡します。
- select で位置を求めて削除する `_select_pop` は、根から降りる途中で祖先の累積値を更新します。そのため `_pop_leaf_bit` を `UpdateAncestors = false` で呼び出します。
- 位置を指定して削除する `_access_pop_and_rank1` では、葉に到達するまで削除する bit の値が判明しません。そのため `_pop_leaf_bit` の中で祖先を更新します。

## 領域の確保

- 葉と内部節点の配列は初めて使用するときに確保します。葉が 1 個だけの木は内部節点を使用しません。
- `reserve(n)` は、各区画の長さを 64 bit とし、葉と内部節点は容量の半分だけ使用している状態と仮定して、必要な節点の数を見積もります。

## 64 bit 語の中での select

- `TITAN23_FAST_PDEP` が定義されている場合、`_select64` は PDEP を使用します。定義されていない場合は 32 bit、16 bit、8 bit と範囲を絞り込んでから 256 要素の表を参照します。
- `TITAN23_FAST_PDEP` は [bit.cpp](../../titan_cpplib/others/bit.cpp) で定義します。Zen 1 と Zen 2 では PDEP がマイクロコードによる実装で遅いため定義しません。

## 関連するファイル

- テスト: [test/dwm/b_tree_bit_vector_random.cpp](../../test/dwm/b_tree_bit_vector_random.cpp)
- 計測: [test/ds/benchmark/dynamic_wavelet_tree_speed_benchmark.md](../../test/ds/benchmark/dynamic_wavelet_tree_speed_benchmark.md)
