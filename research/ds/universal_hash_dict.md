# universal_hash_dict.cpp

## 構造

- [universal_hash_dict.cpp](../../titan_cpplib/ds/universal_hash_dict.cpp) は u64 のキーから値 V への表を連鎖法で持つ。
- 各 bucket の先頭の要素番号を `head` に持ち、要素は `nodes` に持つ。

## 計算量

- 乱数と独立な固定入力に対し、検索と更新は期待償却 O(1) である。
- `clear` と `values` と `items` は O(size) である。

## ハッシュ関数と負荷率

- `USE_HASH_FUNC=true` では random odd multiply-shift で bucket を決める。乗数 `mul` は乱数で選んだ奇数である。
- random odd multiply-shift の衝突確率は 2 / bucket 数以下である。
- 負荷率は 1/2 以下に保つ。追加のときに要素数が bucket 数の半分に等しければ、bucket 数を 2 倍にして作り直す。
- `USE_HASH_FUNC=false` では、キーの下位ビットで bucket を決める。同じ bucket に k 個集中した場合の検索は O(k) である。

## 位置の有効範囲

- `get_pos` と `pos` の結果は、次の変更操作まで有効である。
- `inner_get` と `inner_set` はこの結果を受け取る。

## 削除と格納

- erase は持たない。
- 要素は連続領域 `nodes` へ追加する。
