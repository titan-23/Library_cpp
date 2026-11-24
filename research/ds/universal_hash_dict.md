# universal_hash_dict.cpp

## 構造

- [universal_hash_dict.cpp](../../titan_cpplib/ds/universal_hash_dict.cpp) は u64 のキーから値 V への対応を連鎖法のハッシュ表で保持します。
- 各 bucket の先頭の要素番号は `head` に、要素は `nodes` に格納します。

## 計算量

- 乱数と独立な固定入力に対し、検索と更新は期待償却 O(1) です。
- `clear` と `values` と `items` は O(size) です。

## ハッシュ関数と負荷率

- `USE_HASH_FUNC=true` では random odd multiply-shift で bucket を決定します。乗数 `mul` は乱数で選んだ奇数です。
- random odd multiply-shift の衝突確率は 2 / bucket 数以下です。
- 負荷率は 1/2 以下に維持します。追加の際に要素数が bucket 数の半分に等しければ、bucket 数を 2 倍にして再構築します。
- `USE_HASH_FUNC=false` では、キーの下位ビットで bucket を決定します。同じ bucket に k 個集中した場合の検索は O(k) です。

## 位置の有効範囲

- `get_pos` と `pos` の結果は、次の変更操作まで有効です。
- `inner_get` と `inner_set` はこの結果を受け取ります。

## 削除と格納

- erase は提供しません。
- 要素は連続領域 `nodes` へ追加します。
