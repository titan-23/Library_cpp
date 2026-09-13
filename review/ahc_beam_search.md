# titan_cpplib/ahc/beam_search レビュー

## 全ファイルの確認結果（2026-09-03）

現行版、親番号版、圧縮版、基数整列版、最適化版、`beam_result.cpp`、旧版、テスト、補助ツール、入力データまで再読した。旧本文は11件だけを対象とし、旧版やテストなどを明示的に除外していたため、今回の確認範囲を判断する根拠には使っていない。実行はしていない。

### 現行版の確定事項

- `beam_search_turn.cpp:695-701` の `seen` 自動容量 `beam_width*max_turn*2` は `int` で桁あふれする。最初から64ビットで計算し、上限を検査する。
- `beam_search_turn.cpp:672-686,713-717` と最適化版 `:704-720,749-754` は、`max_turn+1` の桁あふれに加えて、目標ターンごとの巨大な添字配列と閾値配列を持つ。飛び飛びの目標ターンを許すなら、対応表または圧縮した添字へ変える。
- 木と全候補領域が空でも、通常版 `:726-735,816-849` と最適化版 `:763-772,858-891` は `max_turn` まで空の反復を続け、`turns_done` を `max_turn` にする。空になった時点で `NoCandidates` を返す。
- `beam_param.cpp:24-26,103-137` の候補領域、幅、ターン数の累積値は `int` で桁あふれする。`:154-200` はNaN、無限大、範囲外の `double` を検査前に `int` へ変換し、未定義動作になり得る。
- `beam_search_compose.cpp:57-66,170-190` は世代内番号を無検査で24ビットへ詰め、`2^24` 以上で世代を表す部分まで壊す。
- `naive_beam_search.cpp:175-188,209-247` だけは、終了候補と未完了候補が同じターンに出ても後続ターンを探索し、基本版とは異なる深さの解へ置き換え得る。
- `beam_history.cpp:30-43` とターン指定2実装の同等箇所は、`Action` の文字列をJSON向けにエスケープしない。`state_info` はJSON断片を直接渡す契約なので、文字列のエスケープ対象ではない。
- ビームサーチのクラスは `HashType` をテンプレート引数にするが、`candidates.cpp:20-25,62-67,103-108,211-216,255-260`、ターン指定通常版 `:138-142,181-186,254`、最適化版 `:162-166,211-216,285` などでハッシュ値を `uint64` へ変換し、`ds/hash_dict.cpp` のキーも64ビットである。128ビット値の上位だけが異なる状態を同一視する。64ビット限定を `static_assert` で示すか、辞書を全ビット対応にする。
- `beam_search_state.cpp:129-132`, `beam_search_state_turn.cpp:133-136` は `history_file` を公開しながら、内部では常に `record_history=false` として実体化するため、ファイルを作らない。
- `beam_history.cpp:30-33` はファイルを開けなかった後もJSON全体を整形する。先に出力先の状態を確認する。
- ターン指定版の終了結果は、遠い目標ターンへ進んだ場合も通常版 `:742-755` と最適化版 `:779-792` で `turns_done=t+1` とする。論理上到達したターンと外側の反復回数を分ける必要がある。
- ターン指定版の履歴は、通常版 `:779-793` と最適化版 `:816-830` で木の更新と現在ターンの候補領域解放より前に記録される。そのため、消費済みの親を次ターンの活動中ノードとして残す。

履歴の仕様確認事項として、通常版 `beam_search_turn.cpp:364-369` と最適化版 `:410-415` は `try_op` 後に子の `HistoryNode` を作るが、`try_op` は `const` であり `State` へ遷移を適用しない。その場の `state_info` は親状態である。子状態を記録する仕様なら誤りであり、「遷移前の付加情報」を記録する仕様なら公開説明へ明記する。

`how_to_use_beam_search.md:281-286` の「大きい `max_turn` でもメモリ増加はほぼない」という説明は、ターンごとの密な配列と矛盾する。記憶量がO(`max_turn`)であることを明記するか、飛び飛びのターンだけを保持する実装へ変える。`ahc/README.md:103-112` の排他的な読み込み一覧も基本版、合成版、ターン指定版だけを挙げ、同名クラスを定義する親番号版、圧縮版、ターン指定最適化版を漏らす。

### 旧版・テスト・補助ツールの確定事項

- `old/beam_search_recursion.cpp:59,64,154-161,181-189` は局所変数 `now_turn` が同名のメンバー変数を隠すため、メンバー側は初期値0のまま全ての `get_actions` へ渡る。再帰へ渡す局所式 `turn-now_turn` も常に0になる。一本道を圧縮した `cnt>0` の場合は深さが負になり、葉の展開へ到達しない。再利用したノードの子も `:210-214` で消去せず、`:178-179` で確保した `State` を解放せず、`:145-151` は `verbose=false` でも領域の大きさを出力する。
- `old/beam_search.cpp:423-462`, `old/beam_fast_old.cpp:269-310` は候補が全滅した場合を `assert` だけで扱い、`NDEBUG` 指定時は空範囲や存在しない最良ノードを使う。再帰版も同様である。
- 旧版の3実装は `clear_hash_every_turn=false` で同じオブジェクトの探索を繰り返すと、候補辞書に過去のハッシュ値が残り、2回目へ影響する。`old/beam_search_turn_old.cpp:256-272,302-318` は負または現在以下の `target_turn` を拒否せず、範囲外参照や進捗停止になる。
- `test/ahc/beam_search_turn_differential.cpp:276-358,685-701` は128ビットのハッシュ値を切り捨てた誤った探索結果を期待値にする。本体修正と同時に、全ビットを使った結果へ直す。
- `beam_search/test/a.cpp:73,301-303`, `a_radix.cpp:73,301-303`, `count_ab.cpp:88,320-322` は、既定構築した `Action` の方向文字などを初期化しないまま、ダミー経路または根の経路で読む。
- `beam_search/test/ahc_settings.py:58-64` の `map(x / sum(W) for x in W)` は `TypeError` になる。有効な得点が0件の場合はゼロ除算も起こす。さらに `ahc064.cpp:457-464` は `argc` と `argv` を読まず、最適化側が付加する値を問題側へ反映しない。
- `beam_search/test/beam_search_turn_microbench.cpp:314-336` は返却経路を破棄する前に計測対象を解除するため、その `Action` のデストラクタを回数へ含めない。

以下の旧本文にある基本版・合成版の空候補時の未定義動作、通常版で `push_lazy` を使わない問題などは現行で修正済みである。現在の判定は上記を優先する。

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

- `push` の3経路（幅未満・同じハッシュ値の置換・最悪候補の追い出し）、セグメント木の遅延構築（`is_built`）、ハッシュ表の印（-1=削除済み、-2=前ターンの生存候補）の処理を確認した。正しい。
- 追い出し時の `func.set(hashidx[i], -1)` は既存キーへの set で rebuild が起きないため、先に取得した `get_pos(hash)` の位置は `inner_set` まで有効。確認済み。
- **[軽微]** Candidates(非 Flat)の reset の `hashidx.size() < w` が符号なし比較で警告が出る。Flat 版はキャストしており不統一。
- **[軽微] CandidatesFlat は現状どのエンジンからも使われていない**。beam_search_turn.cpp は内部に別実装の Candidates を持つ。残すなら用途をコメントに書く、使わないなら削除を検討。

## beam_param.cpp

- `recommend_width` で活動中領域と空き領域を分ける方法は、導出コメントを含めて妥当。`timestamp_meta` の互換更新も一貫している。
- **[軽微]** get_beam_width 内のローカル変数 `int beam_width` がメンバ `beam_width` を隠しており読みにくい。
- **[軽微]** get_beam_width は `time_sum / beam_width_sum` を計算する。base 版では幅が常に1以上なので0除算は起きないが、防御はない。
- base 版の動的幅が設定値を超えうる点、turn 版は上限になる点はドキュメントに明記されており仕様どおり。

## beam_log.cpp

- 問題なし。width_trace のダウンサンプルと sparkline も正しい。
- **[軽微]** tag_info/tag_ok/tag_warn 等がすべて tag_bs の別名で、区別が機能していない。

## beam_history.cpp

- **[注意] 空ファイル名でも書き込みを試みる**。`dump_history_json` は `ofstream ofs(filename)` の失敗を確認しない。`record_history=true` で `history_file` を省略（既定 `""`）すると、失敗した出力先に対して全ノードのJSON整形を行う無駄が生じる。`beam_search_turn.cpp` 内の同名関数には `if(!ofs) return;` があるので合わせるべき。
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
- **[軽微]** `record_history=true` のとき、`aid` が `free_slots` 経由で再利用されると `aid_to_node_id` が旧ノードを上書きし、親リンクが別ノードを指しうる。可視化用の記録だけに影響する。
- **[軽微]** seen_hash の登録は「ビームに採用されたときだけ」で、枝刈りとの整合は取れている。確認済み。

## beam_search_compose.cpp

- compose_pass の状態整合を確認した。ghost 化する親が直前に apply された葉(trace[turn])である場合に限り rollback→compose→(失敗時 re-apply)を行う設計で、他の親は状態に乗っていないため整合する。正しい。
- ghost slot を apply/rollback/materialize/result_prefix すべてで skip しており、論理深さ(trace 添字)と物理状態の対応が保たれている。
- snapshot_leaf_actions の reverse 順の詰め方は、parent_leaf の振り方(逆順 DFS)と一致している。コメントどおり。
- **[軽微] CandIdx::action_count のコメントが実装と不一致**。「composed 子=gen-1」とあるが、finalize_generation は常に gen を入れ、compose_pass は action_count を更新しない。実装は「常に gen、ghost は no-op」で一貫しており正しいので、コメント側を直すべき。結果として eff_depth は世代内で全要素同値になり、可変深さ対応の copy_tour_path(ratchet 版)は現状 base 版と同じ動きをする。
- **[軽微]** compose 版は `Action::compose(child)` を要求するが、how_to_use_beam_search.md に compose 版の記載自体がない(後述)。

## beam_search_state.cpp / beam_search_state_turn.cpp(雛形)

- どちらも対応するエンジンのインターフェースと一致している。try_op が pre_*/nxt_* を書く規約、submit.threshold の形、target_turn の扱いのコメントも正確。
- **[軽微]** `titan23::Random brnd` と gen_param が非 inline のグローバル定義で、複数 TU では重複定義になる。単一 TU 前提なら実害なし。
- **[軽微]** HashType が state 版は `uint64_t`、state_turn 版は `unsigned long long` と表記が揺れている。

## how_to_use_beam_search.md

- 実装と突き合わせた。ライフサイクル、try_op の契約(const・action への書き込み・INF 打ち切り)、threshold の意味、finished の扱い、base/turn の差分表(引数の違い、候補空時の挙動、動的幅の上限有無、hash_window の有効範囲)まで、いずれも実装と一致しており正確。
- **[軽微] compose 版と naive 版の記載がない**。beam_search_compose.cpp は Action::compose という追加要件を持つため、使うなら差分節が必要。使い分け(naive はデバッグ・正当性確認用、compose は実験版など)の一言があると迷わない。
- **[軽微]** 「候補が空になると assert 停止」の記述に、NDEBUG では停止しない旨の注意があるとより安全。
