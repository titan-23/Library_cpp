# beam_search_turn_old.cpp

## 部分木のスキップ

- TreeNode の `subtree_min_turn` と `subtree_end` は PRE_ORDER の節点でのみ使用します。
- `subtree_min_turn` は部分木に含まれる葉の target_turn の最小値です。
- `subtree_end` は対応する POST_ORDER の次の添字です。この添字は部分木の範囲に含みません。
- get_next_beam は、現在のターンに該当する葉がない部分木をまとめてスキップします。
  - 部分木の葉がすべて先のターンにある場合は、state を進めずに `subtree_end` へ移動します。

## 木の更新での pre_stack

- update_tree の `pre_stack` は、nxt_tree に追加した PRE_ORDER のうち、まだ閉じていないものの添字を保持します。
- POST_ORDER を追加して部分木を閉じるとき、対応する PRE_ORDER の `subtree_end` と `subtree_min_turn` を設定します。
- 閉じた部分木の `subtree_min_turn` は、親の PRE_ORDER に最小値として反映します。
- 部分木が空になったときは、追加した PRE_ORDER を取り消し、POST_ORDER も追加しません。

## ターンをまたぐ重複除去

- Candidates の reset で `clear_hash` が false のとき、現在のビームにあるハッシュの値を -2 にします。
  - -2 は過去のターンで採用済みのハッシュを表します。
- push は、値が -2 であるハッシュの候補を登録せずに破棄します。
