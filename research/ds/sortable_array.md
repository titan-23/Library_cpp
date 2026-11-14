# sortable_array.cpp

## 使用上の注意

- T の `<` と `==` が定義され、全順序であることを前提とする。
- コピーすると正しく動かない。元のコメントでは「コピーするとバグる」と書いていた。
- 節点を生ポインタで持つので、コピーした配列は元の配列と節点を共有する。
- 複製が必要なときは tovector() で取り出してから作り直す。

## make_kyokai の考え方

- make_kyokai(k) は位置 k の直前で木を切る。元のコメントでは「..., k, ... で切る」と書いていた。
- 1 つの splay 木はソート済みなので、キー順と添字順が一致している。
- そのため nodeptr[k] で splay() すると、左側は key 以下かつ添字も k 以下になる。
- 元のコメントは「key以下(未満？)」と書いており、以下か未満かは確定していない。
- 反転した木では添字順がキー順と逆になる。split_idx は is_rev[idx] を見て tree_size - tree_idx に読み替える。
- 同じ構造は [sortable_segment_tree.cpp](../../titan_cpplib/ds/sortable_segment_tree.cpp) でも使う。
