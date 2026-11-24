# persistent_wbtree_node.cpp

対象のファイルは [persistent_wbtree_node.cpp](../../titan_cpplib/gomi/persistent_wbtree_node.cpp) です。

## ノードの所有と複製

- 公開している操作は、開始時に一度だけ `first = ma.ptr` を取得し、操作全体でこの値を使用します。
- `own()` と `balance()` は `first` を所有の境界とする規則に従います。
- `own()` は番号が `first` より小さいノードを複製し、それ以外のノードはそのまま返します。番号が `first` 以上のノードは同じ操作の中で作成したノードです。
- `balance()` に渡す `node` は所有済みです。その子はまだ古い版に属していることがあります。
- `balance()` は `update()` の後に呼び出します。重みが大きい側の子を複製することがあります。

## 挿入と削除

- `insert_node` などの挿入と削除の内部関数は、探索経路を一度だけ複製して処理します。
- `erase_extreme` は端の値を取り除くときに、切り離した単一ノードを新たに確保しません。
- 端点にある古い未変更の根は、複製せずに取り除くことができます。

## 連結

- `merge` は `split(size-1)` の代わりに、取り出し用の専用関数 `extract_right_node` を使用します。
- 取り出し用の関数は、消費済みの単一ノードを pivot として返します。この pivot はそのまま join() に渡すことができます。コード上で join() に相当する関数は `_merge_with_root` です。
