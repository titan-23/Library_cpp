# _wbt_node_pool.cpp

## 構築済みフラグの位置

- スロットは union Slot で、メンバーの Node、空きリスト用の next、sizeof(Node) + 1 バイトの extent が同じ領域を共有します。
- Node の直後の 1 バイトを、スロットにノードが構築済みかを表すフラグとして使用します。
- この 1 バイトは union の表現に含まれます。一方で、有効になる二つのメンバー Node と next のどちらの領域よりも後ろにあります。
- そのため Node や next を構築しても、この 1 バイトは変更されません。

## release でのキャスト

- release は Node* を reinterpret_cast で Slot* に戻します。
- union とその直接のメンバーはポインタ相互変換可能です。
- この性質は Node 自体が標準レイアウトのクラスでなくても成り立ちます。
