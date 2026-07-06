# titan_cpplib/ahc/beam_search レビュー

対象は以下の11ファイル。テスト実行はせず、コードを読んで精査した。
old/、test/、ahclib_results、ビルド成果物は対象外。

- beam_search.cpp
- beam_search_compose.cpp
- beam_search_turn.cpp
- candidates.cpp
- beam_param.cpp
- beam_log.cpp
- beam_history.cpp
- naive_beam_search.cpp
- beam_search_state.cpp
- beam_search_state_turn.cpp
- how_to_use_beam_search.md

重要度は次の3段階で付けた。

- **[バグ]** 誤動作・UB・コンパイル不能につながる
- **[注意]** 特定条件で問題になる。仕様として明記すれば許容できる
- **[軽微]** 動作に影響しない指摘

検証方針。tour/leaf/trace のオイラーツアー木、深さの帳尻(rollback と apply の対応)、最終パス再構成の長さ、Candidates の segtree と hash マーカー、compose の ghost 整合、turn 版の update_tree の PRE/POST 整合を読みで検証した。誤りは見つからなかった。以下は個別の指摘。

## ディレクトリ横断の指摘

- **[注意] 同名クラスの3重定義**。`flying_squirrel::BeamSearchWithTree` が beam_search.cpp、beam_search_turn.cpp、beam_search_compose.cpp の3ファイルで定義されている。差し替え前提の設計だが、同一 TU に2つ include すると再定義エラーになる。expander が別経路で両方を取り込む事故も起こりうる。クラス名を分けるか、少なくともファイル冒頭に「排他 include」と明記すべき。
- **[注意] HashDict 事前確保の死文化**。candidates.cpp の Candidates/CandidatesFlat と naive_beam_search.cpp は `if (func.inner_len() == 1)` で初回に `HashDict(beam_width*8)` を確保する意図だが、HashDict::inner_len() は cap を返し初期値は 16 なので、この条件は常に偽。事前確保が効かず、初回ターンに rebuild が繰り返し走る。beam_search_turn.cpp 内の Candidates は `inner_len() < beam_width * 8` で判定しており正しい。同じ形に直すべき。
- **[注意] 候補が空になったときの NDEBUG 時 UB**。beam_search.cpp と compose 版は候補が空だと `assert(candidates.size() > 0)` で停止する。この挙動自体はドキュメントに明記されているが、NDEBUG ビルドでは assert が消え、そのまま進むと `best_idx == -1` で `cand[-1]` にアクセスして UB になる。assert でなく明示的なエラー処理にした方が安全。
- **[軽微]** `DAMMY_ACTION`(DUMMY の typo)、int と size_t の比較警告(`i < history.size()` 等)、`<bits/stdc++.h>` の使用が各ファイルにある。

## beam_search.cpp(base 版)

- DFS の深さ整合を確認した。ターン t 終了時に状態は深さ t にあり、次ターン最初の候補(f=0)と2番目以降(f=1)のどちらの rollback 本数も apply と正確に対応する。
- 最終パス再構成(result_prefix + trace 復元 + 最終 action)の長さが max_turn に一致することを確認した。confirm_and_free による接頭辞確定と slab 再利用も整合している。
- 世代ブロック(gblock)+ ActionId 方式のコメントは設計理由まで書かれており適切。
- **[軽微]** 時間による打ち切りはない(time_limit は幅調整のみ)。ドキュメントに明記済みで仕様どおりだが、TLE 回避は max_turn と幅の設定に依存する。

## candidates.cpp

- push の3経路(未満・同 hash 置換・worst 追い出し)、segtree の遅延構築(is_built)、hash マーカー(-1=削除済み、-2=前ターン survivor)のロジックを確認した。正しい。
- 追い出し時の `func.set(hashidx[i], -1)` は既存キーへの set で rebuild が起きないため、先に取得した `get_pos(hash)` の位置は `inner_set` まで有効。確認済み。
- **[軽微]** Candidates(非 Flat)の reset の `hashidx.size() < w` が符号なし比較で警告が出る。Flat 版はキャストしており不統一。
- **[軽微] CandidatesFlat は現状どのエンジンからも使われていない**。beam_search_turn.cpp は内部に別実装の Candidates を持つ。残すなら用途をコメントに書く、使わないなら削除を検討。

## beam_param.cpp

- recommend_width の active/empty 分離モデルは、導出コメントを含めて妥当。timestamp_meta の互換更新も一貫している。
- **[軽微]** get_beam_width 内のローカル変数 `int beam_width` がメンバ `beam_width` を隠しており読みにくい。
- **[軽微]** get_beam_width は `time_sum / beam_width_sum` を計算する。base 版では幅が常に1以上なので0除算は起きないが、防御はない。
- base 版の動的幅が設定値を超えうる点、turn 版は上限になる点はドキュメントに明記されており仕様どおり。

## beam_log.cpp

- 問題なし。width_trace のダウンサンプルと sparkline も正しい。
- **[軽微]** tag_info/tag_ok/tag_warn 等がすべて tag_bs の別名で、区別が機能していない。

## beam_history.cpp

- **[注意] 空ファイル名でも書き込みを試みる**。`dump_history_json` は `ofstream ofs(filename)` の失敗を確認しない。record_history=true で history_file を省略(既定 "")すると、失敗ストリームへ全ノードの JSON 整形を行う無駄が生じる。beam_search_turn.cpp 内の同名関数は `if(!ofs) return;` があるので合わせるべき。
- **[軽微]** action_str と state_info を JSON エスケープせず埋め込む。`"` や `\` を含むと不正な JSON になる。

## naive_beam_search.cpp

- 内部 Candidates は push のたびに segtree を更新する方式で、重複置換判定の `seg[idx+s].first` 参照も有効。正しい。
- 候補が空でも found_finished があれば正常終了する分岐があり、base 版より丁寧。
- State を候補ごとにコピーする設計は「愚直版」として意図どおり。
- **[軽微]** callback 形式の enumerate_actions のみ対応で、base 版が持つ vector 形式へのフォールバックがない。ドキュメントにも naive 版の記載がない。
- **[軽微]** history_nodes が全ターン分蓄積され O(ターン数×幅) のメモリを使う。経路復元の簡便さとのトレードオフで妥当。

## beam_search_turn.cpp(可変深さ版)

- update_tree の一本道接頭辞の確定、PRE/POST の対消滅、subtree_end と部分木最小 target_turn の維持、pre_stack による再計算フラグの伝播を確認した。整合している。
- get_next_beam の部分木スキップ(target_turn > turn)と、展開時の apply→enumerate→rollback の対称性も正しい。
- get_next_beam で `Action action = act(node.aid)` と値コピーしているのは、enumerate 中の arena_put_reserve が action_pool を再確保しうるための正しい防御。参照にすると dangling になる。この理由は旧版にはコメントがあったが現行にはないので、書き戻すとよい。
- 候補が空になった場合は部分解を静かに返す。ドキュメントの差分表に明記されており仕様どおり。
- Candidates::reset の HashDict 事前確保はこのファイルだけ正しい(横断指摘参照)。
- **[軽微]** record_history 時、aid が free_slots 経由で再利用されると aid_to_node_id が旧ノードを上書きし、親リンクが別ノードを指しうる。可視化ログのみの影響。
- **[軽微]** seen_hash の登録は「ビームに採用されたときだけ」で、枝刈りとの整合は取れている。確認済み。

## beam_search_compose.cpp

- compose_pass の状態整合を確認した。ghost 化する親が直前に apply された葉(trace[turn])である場合に限り rollback→compose→(失敗時 re-apply)を行う設計で、他の親は状態に乗っていないため整合する。正しい。
- ghost slot を apply/rollback/materialize/result_prefix すべてで skip しており、論理深さ(trace 添字)と物理状態の対応が保たれている。
- snapshot_leaf_actions の reverse 順の詰め方は、parent_leaf の振り方(逆順 DFS)と一致している。コメントどおり。
- **[軽微] CandIdx::action_count のコメントが実装と不一致**。「composed 子=gen-1」とあるが、finalize_generation は常に gen を入れ、compose_pass は action_count を更新しない。実装は「常に gen、ghost は no-op」で一貫しており正しいので、コメント側を直すべき。結果として eff_depth は世代内で全要素同値になり、可変深さ対応の copy_tour_path(ratchet 版)は現状 base 版と同じ動きをする。
- **[軽微]** compose 版は `Action::compose(child)` を要求するが、how_to_use_beam_search.md に compose 版の記載自体がない(後述)。

## beam_search_state.cpp / beam_search_state_turn.cpp(雛形)

- どちらも対応するエンジンのインターフェースと一致している。try_op が pre_*/nxt_* を書く規約、submit.threshold の形、target_turn の扱いのコメントも正確。
- **[軽微]** beam_search_state.cpp に `#pragma once` がない(state_turn にはある)。
- **[軽微]** `titan23::Random brnd` と gen_param が非 inline のグローバル定義で、複数 TU では重複定義になる。単一 TU 前提なら実害なし。
- **[軽微]** HashType が state 版は `uint64_t`、state_turn 版は `unsigned long long` と表記が揺れている。

## how_to_use_beam_search.md

- 実装と突き合わせた。ライフサイクル、try_op の契約(const・action への書き込み・INF 打ち切り)、threshold の意味、finished の扱い、base/turn の差分表(引数の違い、候補空時の挙動、動的幅の上限有無、hash_window の有効範囲)まで、いずれも実装と一致しており正確。
- **[軽微] compose 版と naive 版の記載がない**。beam_search_compose.cpp は Action::compose という追加要件を持つため、使うなら差分節が必要。使い分け(naive はデバッグ・正当性確認用、compose は実験版など)の一言があると迷わない。
- **[軽微]** 「候補が空になると assert 停止」の記述に、NDEBUG では停止しない旨の注意があるとより安全。
