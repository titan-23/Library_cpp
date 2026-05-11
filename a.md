# ビームサーチ本体ファイル精査メモ（未対応分）

---

## 1. naive_beam_search.cpp

---

## 2. beam_search.cpp

---

## 3. beam_fast.cpp （+ candidates.cpp）

🟢

- cand.clear(); cand.push_back(move(...)) ループ（L87-90, L186-189）→ cand.assign(...) or swap で 1 アロケーション。

---

## 4. beam_search_turn.cpp

🟡 危ない実装

- current_new_candidates は evict された候補も保持（push 成功時に push_back し、その後 evict されると is_survived=0 のまま残る）
  - update_tree は is_survived_node[id] でフィルタするので正しいが、ソート対象に死骸が入る分ソート対象が膨らむ。
  - 一旦そのまま

🟢

- 木が future-turn 候補を全部抱えるため、毎ターンの木走査コストが O(全 pending) に。
  - target_turn で索引したサブツリー集合に分ければ、現ターンに該当するノードだけ触る O(active@this_turn) にできる。AHC で max_turn が大きい問題では効く。
  - 定数倍を落とさずに実装可能ですか
- thresholds を try_op / get_actions に毎回ベクタ参照渡し → 内部実装で必要な分だけ取れるよう「現在の target_turn を引数で渡しその閾値だけ参照」へ縮約できる。
  - vector参照渡しのコストは低いと思っていますが、いかがでしょう。
- pool の reset(req_w) は target_turn ごとに初回しか呼ばれず、同じ target_turn の途中で req_w が変わる可能性はゼロという前提（実際 get_beam_width が決定的なので OK）。明示コメントが無いと将来事故りやすい。
  - 前提が崩れそうなので、一旦保留

---

## 横断的に共通する事項

| 項目 | 影響 | 対処 |
|---|---|---|
| record_history=false でも node_id_counter を消費 | わずかな定数倍コスト | OK（圧縮判定に使うので必要） |


## その他

- ログ出力を統一したい
