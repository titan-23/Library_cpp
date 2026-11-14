# persistent_seg_wbtree_node.cpp

対象のファイルは [persistent_seg_wbtree_node.cpp](../../titan_cpplib/gomi/persistent_seg_wbtree_node.cpp) である。

## 所有境界

- 公開操作は開始時に一度だけ `first = ma.ptr` を取得し、操作全体でこの値を使う。
- `own()`、`propagate()`、`balance()` は所有境界の約束に従う。
- `own()` は番号が `first` より小さいノードを複製し、それ以外のノードはそのまま返す。番号が `first` 以上のノードは、同じ操作の中で作ったノードである。
- `balance()` に渡す `node` は所有済みで、反転も解消済みである。その子は、まだ古い版に属することがある。
- `balance()` は `update()` の後に呼ぶ。重い側の子の反転を解消し、複製することがある。

## 挿入と削除

- `insert_node` などの挿入と削除の内部関数は、探索経路を一度だけ複製して処理する。
- `erase_extreme` は端の値を取り除くとき、切り離した単一ノードを新たに確保しない。
- 端点にある古い根は、未処理の反転を持たなければ複製せずに省ける。
- `erase_node` では、葉は反転の影響を受けないので、複製せずに取り除ける。
- 削除するノードが両方の子を持つとき、代わりの要素を重い側から取り出す。このノードでの偏りを広げないためである。

## 連結

- `merge` は `split(size-1)` の代わりに、専用の取り出し関数 `extract_right_node` を使う。
- 取り出し関数は、消費済みで反転も解消済みの単一ノードを pivot として返す。この pivot はそのまま join() に渡せる。コード上で join() に当たる関数は `_merge_with_root` である。
