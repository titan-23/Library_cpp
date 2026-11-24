# titan_cpplib/ahc/beam_search レビュー

## 全ファイルの確認結果 (2026-09-03)

現行版、親番号版、圧縮版、基数ソート版、最適化版、`beam_result.cpp`、旧版、テスト、補助ツール、入力データまで再読しました。旧本文は 11 件だけを対象とし、旧版やテストなどを明示的に除外していたため、今回の確認範囲を判断する根拠には使用していません。実行はしていません。

### 現行版の確定事項

- `beam_search_turn.cpp:695-701` で自動的に設定する `seen` の容量 `beam_width*max_turn*2` は `int` で桁あふれします。最初から 64 ビットで計算し、上限をチェックするようにします。
- `beam_search_turn.cpp:672-686,713-717` と最適化版 `:704-720,749-754` は、`max_turn+1` の桁あふれに加えて、目標ターンごとの巨大な添字配列と閾値配列を保持します。連続しない目標ターンを許可する場合は、対応表または圧縮した添字に変更します。
- 木とすべての候補領域が空でも、通常版 `:726-735,816-849` と最適化版 `:763-772,858-891` は `max_turn` まで空の反復を続け、`turns_done` を `max_turn` にします。空になった時点で `NoCandidates` を返すようにします。
- `beam_param.cpp:24-26,103-137` の候補領域、幅、ターン数の累積値は `int` で桁あふれします。`:154-200` は NaN、無限大、範囲外の `double` をチェックの前に `int` へ変換し、未定義動作になり得ます。
- `beam_search_compose.cpp:57-66,170-190` は世代内の番号をチェックせずに 24 ビットへ格納し、`2^24` 以上の場合は世代を表す部分まで不正な値になります。
- `naive_beam_search.cpp:175-188,209-247` だけは、終了した候補と未完了の候補が同じターンに現れても後続のターンを探索し、基本版とは異なる深さの解に置き換える場合があります。
- `beam_history.cpp:30-43` とターン指定版の 2 実装にある同等の箇所は、`Action` の文字列を JSON 向けにエスケープしません。`state_info` は JSON の断片を直接渡す仕様であるため、文字列のエスケープの対象ではありません。
- ビームサーチのクラスは `HashType` をテンプレート引数にしていますが、`candidates.cpp:20-25,62-67,103-108,211-216,255-260`、ターン指定通常版 `:138-142,181-186,254`、最適化版 `:162-166,211-216,285` などでハッシュ値を `uint64` へ変換しており、`ds/hash_dict.cpp` のキーも 64 ビットです。128 ビットの値の上位だけが異なる状態を同一視します。64 ビットに限定することを `static_assert` で示すか、辞書を全ビットに対応させます。
- `beam_search_state.cpp:129-132`, `beam_search_state_turn.cpp:133-136` は `history_file` を公開していますが、内部では常に `record_history=false` として実体化するため、ファイルを作成しません。
- `beam_history.cpp:30-33` はファイルのオープンに失敗した後も JSON 全体を整形します。先に出力先の状態を確認するようにします。
- ターン指定版の通常版 `:742-755` と最適化版 `:779-792` は、遠い目標ターンへ進んだ場合も終了時の結果を `turns_done=t+1` とします。論理上到達したターンと外側の反復回数を区別する必要があります。
- ターン指定版の履歴は、通常版 `:779-793` と最適化版 `:816-830` で木の更新と現在のターンの候補領域の解放より前に記録されます。そのため、消費済みの親が次のターンの活動中ノードとして履歴に残ります。

履歴の仕様には確認が必要な事項があります。通常版 `beam_search_turn.cpp:364-369` と最適化版 `:410-415` は `try_op` の後に子の `HistoryNode` を作成しますが、`try_op` は `const` であり `State` へ遷移を適用しません。この時点の `state_info` は親の状態です。子の状態を記録する仕様であれば誤りであり、「遷移前の付加情報」を記録する仕様であれば公開している説明に明記する必要があります。

`how_to_use_beam_search.md:281-286` の「大きい `max_turn` でもメモリ増加はほぼない」という説明は、ターンごとの密な配列と矛盾します。メモリ使用量が O(`max_turn`) であることを明記するか、実際に現れるターンだけを保持する実装に変更します。`ahc/README.md:103-112` の排他的な読み込みの一覧も基本版、合成版、ターン指定版だけを挙げており、同名のクラスを定義する親番号版、圧縮版、ターン指定最適化版が漏れています。

### 旧版、テスト、補助ツールの確定事項

- `old/beam_search_recursion.cpp:59,64,154-161,181-189` は局所変数 `now_turn` が同名のメンバー変数を隠蔽するため、メンバー側は初期値 0 のまますべての `get_actions` へ渡されます。再帰へ渡す局所変数の式 `turn-now_turn` も常に 0 になります。一本道を圧縮した `cnt>0` の場合は深さが負になり、葉の展開に到達しません。さらに、`:210-214` では再利用したノードの子を消去せず、`:178-179` で確保した `State` も解放しません。`:145-151` は `verbose=false` でも領域の大きさを出力します。
- `old/beam_search.cpp:423-462`, `old/beam_fast_old.cpp:269-310` は候補がすべてなくなった場合を `assert` だけで扱い、`NDEBUG` の指定時は空の範囲や存在しない最良ノードを使用します。再帰版も同様です。
- 旧版の 3 実装は `clear_hash_every_turn=false` で同じオブジェクトの探索を繰り返すと、候補の辞書に過去のハッシュ値が残り、2 回目の探索に影響します。`old/beam_search_turn_old.cpp:256-272,302-318` は負または現在のターン以下の `target_turn` を拒否せず、範囲外参照や進捗の停止が発生します。
- `test/ahc/beam_search_turn_differential.cpp:276-358,685-701` は 128 ビットのハッシュ値を切り捨てた正しくない探索結果を期待値にしています。本体の修正と同時に、全ビットを使用した結果を期待値にします。
- `beam_search/test/a.cpp:73,301-303`, `a_radix.cpp:73,301-303`, `count_ab.cpp:88,320-322` は、デフォルト構築した `Action` の方向文字などを初期化しないまま、ダミーまたは根を扱う処理で参照します。
- `beam_search/test/ahc_settings.py:58-64` の `map(x / sum(W) for x in W)` は `TypeError` になります。有効な得点が 0 件の場合はゼロ除算も発生します。さらに `ahc064.cpp:457-464` は `argc` と `argv` を参照せず、最適化側が付加する値を問題側へ反映しません。
- `beam_search/test/beam_search_turn_microbench.cpp:314-336` は返却された経路を破棄する前に計測対象を解除するため、その `Action` のデストラクタを回数に含めません。

以下の旧本文にある、基本版と合成版で候補が空の場合の未定義動作や、通常版で `push_lazy` を使用しない問題などは、現行版で修正済みです。現在の判定は上記を優先します。

対象は以下の 11 ファイルです。テストは実行せず、コードを精査しました。
old/、test/、ahclib_results、ビルド成果物は対象外です。

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

重要度は次の 3 段階で付けました。

- **[バグ]** 誤動作、UB、コンパイル不能につながります。
- **[注意]** 特定の条件で問題になります。仕様として明記すれば許容できます。
- **[軽微]** 動作に影響しない指摘です。

次の項目をコードの精査によって検証しました。誤りは見つかりませんでした。

- tour/leaf/trace のオイラーツアー木
- 深さの整合 (rollback と apply の対応)
- 最終パスの再構成の長さ
- Candidates の segtree と hash マーカー
- compose の ghost の整合
- turn 版の update_tree の PRE/POST の整合

## ディレクトリ横断の指摘

- **[注意] 同名クラスの 3 重定義**：`flying_squirrel::BeamSearchWithTree` が beam_search.cpp、beam_search_turn.cpp、beam_search_compose.cpp の 3 ファイルで定義されています。差し替えを前提とした設計ですが、同一の TU に 2 つ include すると再定義エラーになります。expander が別の include を経由して両方を取り込む問題も発生し得ます。クラス名をファイルごとに異なるものにするか、少なくともファイルの冒頭に「排他 include」と明記すべきです。
- **[注意] 実行されない HashDict の事前確保**：candidates.cpp の Candidates/CandidatesFlat と naive_beam_search.cpp は、`if (func.inner_len() == 1)` で初回に `HashDict(beam_width*8)` を確保する意図ですが、HashDict::inner_len() は cap を返し、その初期値は 16 であるため、この条件は常に偽です。事前確保が行われず、初回のターンに rebuild が繰り返し実行されます。beam_search_turn.cpp 内の Candidates は `inner_len() < beam_width * 8` で判定しており、正しく動作します。同じ形に修正すべきです。
- **[注意] 候補が空になった場合の NDEBUG 時の UB**：beam_search.cpp と compose 版は候補が空の場合に `assert(candidates.size() > 0)` で停止します。この挙動自体はドキュメントに明記されていますが、NDEBUG のビルドでは assert が無効になり、そのまま処理が進むと `best_idx == -1` で `cand[-1]` にアクセスして UB になります。assert でなく明示的なエラー処理にする方が安全です。
- **[軽微]** `DAMMY_ACTION` (DUMMY の typo)、int と size_t の比較の警告 (`i < history.size()` など)、`<bits/stdc++.h>` の使用が各ファイルにあります。

## beam_search.cpp (base 版)

- DFS の深さの整合を確認しました。ターン t の終了時に状態は深さ t にあり、次のターンの最初の候補 (f=0) と 2 番目以降の候補 (f=1) のどちらでも、rollback の回数は apply と正確に対応します。
- 最終パスの再構成 (result_prefix + trace 復元 + 最終 action) の長さが max_turn に一致することを確認しました。confirm_and_free による接頭辞の確定と slab の再利用も整合しています。
- 世代ブロック (gblock) + ActionId 方式のコメントは設計の理由まで記載されており、適切です。
- **[軽微]** 時間による打ち切りはなく、time_limit は幅の調整だけに使用されます。ドキュメントに明記済みで仕様どおりですが、TLE の回避は max_turn と幅の設定に依存します。

## candidates.cpp

- `push` の 3 つの分岐 (幅未満、同じハッシュ値の置換、最悪候補の追い出し)、セグメント木の遅延構築 (`is_built`)、ハッシュ表のマーカー (-1=削除済み、-2=前のターンの生存候補) の処理が正しいことを確認しました。
- 追い出し時の `func.set(hashidx[i], -1)` は既存のキーへの set であり rebuild が発生しないため、先に取得した `get_pos(hash)` の位置は `inner_set` まで有効であることを確認しました。
- **[軽微]** Candidates (非 Flat) の reset の `hashidx.size() < w` は符号なし比較となり、警告が出力されます。Flat 版はキャストしており、統一されていません。
- **[軽微] CandidatesFlat は現状どのエンジンからも使用されていません**。beam_search_turn.cpp の内部には別実装の Candidates があります。残す場合は用途をコメントに記載し、使用しない場合は削除を検討します。

## beam_param.cpp

- `recommend_width` で活動中の領域と空き領域を区別する方法は、導出のコメントを含めて妥当です。`timestamp_meta` の互換更新も一貫しています。
- **[軽微]** get_beam_width 内のローカル変数 `int beam_width` がメンバー `beam_width` を隠蔽しており、読みにくくなっています。
- **[軽微]** get_beam_width は `time_sum / beam_width_sum` を計算します。base 版では幅が常に 1 以上であるため 0 除算は発生しませんが、防止する処理はありません。
- base 版の動的な幅が設定値を超え得る点と、turn 版では設定値が上限になる点は、ドキュメントに明記されており仕様どおりです。

## beam_log.cpp

- 問題はありません。width_trace のダウンサンプルと sparkline も正しく実装されています。
- **[軽微]** tag_info/tag_ok/tag_warn などがすべて tag_bs の別名であり、区別が機能していません。

## beam_history.cpp

- **[注意] 空のファイル名でも出力を試みます**。`dump_history_json` は `ofstream ofs(filename)` の失敗を確認しません。`record_history=true` で `history_file` を省略 (既定値 `""`) すると、失敗した出力先に対して全ノードの JSON を整形する無駄な処理が発生します。`beam_search_turn.cpp` 内の同名の関数には `if(!ofs) return;` があるため、同じ処理にそろえるべきです。
- **[軽微]** action_str と state_info を JSON エスケープせずに埋め込みます。`"` や `\` を含む場合は不正な JSON になります。

## naive_beam_search.cpp

- 内部の Candidates は push のたびに segtree を更新する方式であり、重複時の置換判定での `seg[idx+s].first` の参照も有効です。正しく実装されています。
- 候補が空でも found_finished があれば正常終了する分岐があり、base 版より丁寧な実装です。
- State を候補ごとにコピーする設計は「愚直版」として意図どおりです。
- **[軽微]** callback 形式の enumerate_actions だけに対応しており、base 版にある vector 形式へのフォールバックがありません。ドキュメントにも naive 版の記載がありません。
- **[軽微]** history_nodes が全ターン分蓄積され、O(ターン数×幅) のメモリを使用します。経路復元の簡便さとのトレードオフであり、妥当です。

## beam_search_turn.cpp (可変深さ版)

- update_tree の一本道の接頭辞の確定、対になる PRE/POST の削除、subtree_end と部分木の最小 target_turn の維持、pre_stack による再計算フラグの伝播が整合していることを確認しました。
- get_next_beam の部分木のスキップ (target_turn > turn) と、展開時の apply→enumerate→rollback の対称性も正しく実装されています。
- get_next_beam で `Action action = act(node.aid)` と値をコピーしているのは、enumerate 中の arena_put_reserve が action_pool を再確保し得ることへの正しい対策です。参照にすると dangling になります。この理由のコメントは旧版にはありましたが現行版にはないため、再度記載することを推奨します。
- 候補が空になった場合は、エラーを出力せずに部分解を返します。ドキュメントの差分表に明記されており、仕様どおりです。
- Candidates::reset の HashDict の事前確保を正しく実装しているのは、このファイルだけです (ディレクトリ横断の指摘を参照)。
- **[軽微]** `record_history=true` の場合、`aid` が `free_slots` を経由して再利用されると `aid_to_node_id` で旧ノードが置き換えられ、親リンクが別のノードを指す可能性があります。可視化用の記録だけに影響します。
- **[軽微]** seen_hash の登録は「ビームに採用されたときだけ」であり、枝刈りと整合していることを確認しました。

## beam_search_compose.cpp

- compose_pass の状態の整合を確認しました。ghost 化する親が直前に apply された葉 (trace[turn]) である場合に限り rollback→compose→(失敗時 re-apply) を行う設計であり、他の親は状態に適用されていないため整合します。正しい設計です。
- ghost slot を apply/rollback/materialize/result_prefix のすべてで skip しており、論理的な深さ (trace の添字) と物理的な状態の対応が維持されています。
- snapshot_leaf_actions で reverse 順に格納する方法は、parent_leaf の割り当て方 (逆順 DFS) と一致しています。コメントの記載どおりです。
- **[軽微] CandIdx::action_count のコメントが実装と一致していません**。「composed 子=gen-1」とありますが、finalize_generation は常に gen を設定し、compose_pass は action_count を更新しません。実装は「常に gen、ghost は no-op」で一貫しており正しいため、コメント側を修正すべきです。結果として eff_depth は世代内の全要素で同じ値になり、可変深さに対応した copy_tour_path (ratchet 版) は現状では base 版と同じ動作になります。
- **[軽微]** compose 版には `Action::compose(child)` が必要ですが、how_to_use_beam_search.md には compose 版の記載自体がありません (後述)。

## beam_search_state.cpp / beam_search_state_turn.cpp (テンプレート)

- どちらも対応するエンジンのインターフェースと一致しています。try_op が pre_*/nxt_* を設定する規約、submit.threshold の形、target_turn の扱いについてのコメントも正確です。
- **[軽微]** `titan23::Random brnd` と gen_param が非 inline のグローバル定義であり、複数の TU では重複定義になります。単一 TU が前提であれば実害はありません。
- **[軽微]** HashType の表記が state 版は `uint64_t`、state_turn 版は `unsigned long long` であり、統一されていません。

## how_to_use_beam_search.md

- 次の記述を実装と照合しました。いずれも実装と一致しており正確です。
  - ライフサイクル
  - try_op の仕様 (const、action への値の設定、INF による打ち切り)
  - threshold の意味
  - finished の扱い
  - base/turn の差分表 (引数の違い、候補が空の場合の挙動、動的な幅の上限の有無、hash_window の有効範囲)
- **[軽微] compose 版と naive 版の記載がありません**。beam_search_compose.cpp には Action::compose という追加の要件があるため、使用する場合は差分を説明する節が必要です。naive はデバッグと正当性の確認用、compose は実験版といった使い分けの短い説明があると、選択に迷いにくくなります。
- **[軽微]** 「候補が空になると assert 停止」の記述に、NDEBUG では停止しないという注意があるとより安全です。
