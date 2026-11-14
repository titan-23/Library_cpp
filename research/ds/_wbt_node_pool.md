# _wbt_node_pool.cpp

## 構築済みフラグの置き場所

- スロットは union Slot で、Node、空きリスト用の next、sizeof(Node) + 1 バイトの extent を重ねる。
- Node の直後の 1 バイトを、スロットにノードが構築済みかを表すフラグに使う。
- この 1 バイトは union の表現に含まれる。一方で、有効になる二つのメンバー Node と next のどちらの領域よりも後ろにある。
- そのため Node や next を構築しても、この 1 バイトは上書きされない。

## release でのキャスト

- release は Node* を reinterpret_cast で Slot* に戻す。
- union とその直接のメンバーはポインタ相互変換可能である。
- この性質は Node 自体が標準レイアウトのクラスでなくても成り立つ。
