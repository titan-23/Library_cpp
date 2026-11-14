# beam_search_turn_old.cpp

## 部分木の読み飛ばし

- TreeNode の `subtree_min_turn` と `subtree_end` は PRE_ORDER の節点でのみ使う。
- `subtree_min_turn` は、部分木に含まれる葉の target_turn の最小値である。
- `subtree_end` は、対応する POST_ORDER の次の添字である。この添字は部分木の範囲に含まない。
- get_next_beam は、現ターンに該当する葉がない部分木をまとめて読み飛ばす。
  - 部分木の葉がすべて先のターンにあれば、state を進めずに `subtree_end` へ飛ぶ。

## 木の更新での pre_stack

- update_tree の `pre_stack` は、nxt_tree に積んだ PRE_ORDER のうち、まだ閉じていないものの添字を持つ。
- POST_ORDER を積んで部分木を閉じるとき、対応する PRE_ORDER の `subtree_end` と `subtree_min_turn` を埋める。
- 閉じた部分木の `subtree_min_turn` は、親の PRE_ORDER に最小値として伝える。
- 部分木が空になったときは、積んだ PRE_ORDER を取り消し、POST_ORDER も積まない。

## ターンをまたぐ重複除去

- Candidates の reset で `clear_hash` が false のとき、現ビームにあるハッシュの値を -2 にする。
  - -2 は、過去のターンで採用済みのハッシュを表す。
- push は、値が -2 のハッシュを持つ候補をその場で捨てる。
