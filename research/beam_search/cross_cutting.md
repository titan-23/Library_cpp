# ビームサーチ横断性能監査

## 目的と範囲

この文書は、次の横断事項を汎用ライブラリの性能という観点から監査した記録である。

- `naive_beam_search.cpp` の State 複製、Action 複製、経路保持
- `BeamParam`、`Timer`、ログ、計測カウンタの固定費
- ActionId、ノード ID、葉 ID の幅と上限
- 候補、探索木、Action arena のデータ配置
- API とコンパイル時ポリシー
- 可変ターン版の空ターンとターン別作業領域

探索ロジックは変更していない。記載する効果は、明記したものを除いてコード読解からの仮説であり、未計測である。
候補ハッシュ、selector、探索木そのものの詳細は、同じディレクトリの他の監査文書を参照する。

以下では `W` を生存候補数、`D` を探索深さ、`P` を生存候補が参照する異なる親数、`F` を同時に存在する
遷移先ターン数とする。

## 結論

以下は横断項目内での実装段階である。ライブラリ全体の期待効果は `performance_audit.md` を正本とし、active hash と
可変ターン版の pending Action を先に評価する。

| 段階 | 案 | 主な効果 | 意味論上の扱い |
|---|---|---|---|
| 先行 | naive の Action を採用時だけ保存する | 棄却候補の Action copy を除く | 値型契約下で同値 |
| 先行 | naive の2本の beam vector を事前確保する | 大きな State の再配置を除く | 値型契約下で同値 |
| 先行 | `get_best()` を score-only または const reference にする | verbose 時の Action copy を除く | 同値 |
| 先行 | 世代 Action block を move-construct する | 既定構築と move-assign を除く | 値型契約下で同値 |
| 次段 | 固定幅・統計不要経路から世代内 clock と計測書込みを除く | 軽い State の固定費を削る | 明示オプションが必要 |
| 次段 | `BeamParam` を設定と実行時統計へ分離する | fast path と再利用時の意味論を明確化 | API 移行が必要 |
| 次段 | 履歴なし候補から `node_id` を除き、hot/cold 配列を分ける | 候補走査の帯域を削る | 同値 |
| 条件付 | 可変ターン版で空ターンを次イベントまで飛ばす | 長い jump の `O(delta)` を除く | 固定幅なら同値可 |
| 条件付 | 固定深さ版の ActionId を32 bit slotへする | `tour` 系の帯域を削る | 不変条件と境界検査が必要 |
| 条件付 | naive の経路 Action を安定 arena で共有する | Action 二重保持と履歴再配置を減らす | 参照寿命に注意 |
| 条件付 | naive で親ごとの最後の子へ State を move する | 親 State copy を減らす | move の契約が必要 |
| 別backend | 可変ターン表を sparse/paged calendar にする | 初期化・メモリの `O(max_turn)` を削る | State API 変更が必要 |

naive は単に遅い参照実装ではない。State が小さく安価にコピーでき、`apply_op()` / `rollback()` の往復が高価な
場合には、木版より有利になり得る。明示的な backend として残し、低リスクな複製削減を先に行うのがよい。

## naive 版

### 現行の計算とメモリ

現行 `Node` は `State`、score、履歴 ID、直前 Action を持つ
（`naive_beam_search.cpp:134-139`）。各世代で最終生存候補ごとに次を行う
（`naive_beam_search.cpp:234-242`）。

1. 親の `State` を値コピーする。
2. 候補 Action をコピーした State へ適用する。
3. Action を `history_nodes` へコピーする。
4. 同じ Action を次世代 `Node::last_action` へもう一度コピーする。

したがって State 複製は概ね世代当たり `W` 回である。通常の生存候補に由来する履歴 Action は最悪 `O(WD)` になる。
さらに完成 score が改善するたび履歴を追加して古い改善を解放しないため、保持量は `O(WD + I_finished)` である。
`I_finished` は完成 best の更新回数で、全候補イベントに比例し得る。現在葉の `last_action` も `O(W)` 個を重複保持する。

候補登録関数は Action を値で受ける（`naive_beam_search.cpp:58`）。呼出し側の
`candidates.push(..., action)` では、閾値やハッシュで棄却される候補も関数へ入る前に Action をコピーする
（`naive_beam_search.cpp:175-187`）。最終生存候補には、少なくとも候補登録、履歴、`last_action` の3回の
Action copy が発生する。vector 再確保時の move/copy はこの数に含めていない。

`beam` と `next_beam` は reserve せずに構築される（`naive_beam_search.cpp:203-206`）。最初の2世代では、swap で
小容量 vector が交互に出力先となり、大きな State を含む `Node` の段階的な再配置が起こり得る。

### 低リスク: Action の遅延保存

共通 Candidates にある `push_lazy()` と同じ形を naive 内部 Candidates に導入する。callback 内では
`[&]() -> Action { return action; }` を渡し、閾値、過去ターン重複、同一 hash の score 比較を通った場合だけ
Action をコピーする。

これは候補の採否、同点規則、親番号、列挙順を変えない。Action が大きく、分岐数が大きく、棄却率が高いほど
効く。Action が小さな整数なら効果は小さい。

### 低リスク: beam vector の事前確保

`beam` と `next_beam` を、少なくとも初期の設定幅まで reserve する。reserve は要素を構築しないので、State の
既定構築数は増えない。動的幅が設定幅を超え得る現在のモデルを維持する場合、超過時の再確保は残る。

これは完全同値であり、State の move が `noexcept` でないため vector が copy を選ぶ型で特に重要である。

### 条件付き: 親 State を1回だけ move する

現在の `W` 回の親 State copy は、候補ごとの親を調べて次のように減らせる。

1. 各親について、候補列中の最後の子スロットを求める。
2. 最後以外の子は従来どおり親 State をコピーする。
3. 最後の子だけ、以後参照されない親 State を move して Action を適用する。

候補スロット順を保って出力すれば、親 State から一時 `next_state` を作る copy は `W` 回から `W-P` 回になり、
代わりに `P` 回 move する。すべての親が子を1つだけ残す形では、この copy をすべて move へ置き換えられる。
一時 State から次世代 `Node` への move は別途残り、兄弟が多い形では copy の削減率も下がる。

注意点は次のとおりである。

- 「最後」は候補処理順について求め、move 後に同じ親を参照してはならない。
- State の copy/move が外部副作用を持つことは、通常の値型契約では認めないと明記する。
- 候補順、Action 適用順、同点時の選択は変えない。
- 例外安全性を保証する API にするなら、途中の move と `apply_op()` 失敗時の扱いを別途定義する。

### 条件付き: 経路 Action の一元化

`Node::last_action` を単純に `history_nodes[history_id].action` の参照へ変えるだけでは安全でない。列挙中に完成解が
見つかると `history_nodes.push_back()` が走り、vector 再確保で `enumerate_actions()` に渡した参照が無効になり得る
（`naive_beam_search.cpp:178-184`）。

安全な案は、固定長チャンクまたは世代 slab の安定 arena に Action を1回だけ置き、次を ID で参照する方式である。

- 現在の `Node` は last ActionId を持つ。
- 経路ノードは parent ID と ActionId を持つ。
- `enumerate_actions()` へ渡す Action のアドレスは、その呼出し中に候補追加されても変化しない。

これにより現在葉の Action 二重所有を除ける。さらに参照数を持って死んだ履歴枝を再帰解放すれば、保持量を
「これまでの全生存候補」から「現在葉へ到達する live trie」へ近づけられる。ただし参照数更新とランダムアクセスが
増えるため、まず安定 slab による一元化だけを比較すべきである。

### 完成解を見つけた後の意味論

naive 版は完成候補を見つけても、未完了候補が1件以上あれば探索を続ける。完成解発見後に同じ世代で返る標準版、
Compose 版、Radix 版、可変ターン版とは停止条件が一致しない（`naive_beam_search.cpp:219-247`）。

naive を比較基準に使う前に、次のどちらを契約とするか決める必要がある。

- `stop_on_finished_generation`: 完成解を含む世代の列挙後に返す。
- `continue_after_finished`: 未完了 frontier が尽きるか最大ターンに達するまで、より良い完成解を探す。

前者は速いが、現行 naive からは探索意味論の変更である。性能修正に混ぜず、Action、score、`turns_done`、公開統計を
含む明示ポリシーとして扱う。

### naive 固有の API 整理

- `record_history` テンプレート引数は処理を分岐していない。経路復元用履歴は常時必要であり、可視化履歴とは別物である。
- `history_file` は受け取るが使用しない。履歴対応と誤解させるうえ、既定引数から一時 `std::string` が作られる。
- `init_bs(const BeamParam&)` の引数は未使用である。
- callback 形式の `enumerate_actions()` だけを受け、他の木版にある vector 形式の fallback はない。

これらは個々の実行時間には小さいが、backend 共通 API と型要件を定義するときに解消すべきである。

## 時刻計測、BeamParam、ログ

### verbose=false でも残る仕事

標準版、Compose 版、naive 版は、定常世代で少なくとも次の3回 `elapsed()` を呼ぶ。

1. 世代開始時刻の取得
2. `get_beam_width()` へ残り時間を渡す直前の再取得
3. `timestamp()` 用の世代終了時刻の取得

2回目は1回目の `now_time` を使えば除ける。固定幅で統計も不要なら、1回目と3回目も探索結果の最終
`elapsed_ms` には不要である。Radix 版は開始と終了の概ね2回、可変ターン版は展開と木更新を分けるため、active
meta-turn ごとに概ね4回呼ぶ。

可変ターン版の動的幅では、未確保の遷移先ターンプールを初めて作るたび `compute_req_w()` が `elapsed()` を呼ぶ
（`beam_search_turn.cpp:260-286`）。校正後に1 meta-turnから `F` 個の新しい target turn が出ると、clock 呼出しも
`O(F)` になり得る。同一 meta-turn内の推奨幅を1回だけ計算して cache する案を比較する価値がある。後から作る
poolほど狭くする現在の時刻依存挙動は変わるため、動的幅の意味論として明記する。

`Timer` は `high_resolution_clock` を使用する（`timer.cpp:15-35`）。標準規格上、この clock は単調増加とは限らない。
時間制御には `steady_clock` が適している。clock 種別変更の目的は主に正しさと移植性であり、速さは実装ごとに測る。
また、Timer構築時に開始時刻を読み、各searchの `init_bs()` ですぐ `reset()` する。beam objectを構築して1回だけ
searchする典型利用では、探索開始前の最初のclock読取りは使われない。Timerを未開始で構築するか、beam側で
search開始時だけ開始する設計なら、この1回も除ける。ただし世代内の重複読取りより優先度は低い。

### BeamParam の死んだ計測と責務混在

`BeamParam::timestamp()` / `timestamp_meta()` は固定幅でも毎回、累積値と `width_hist` を更新する
（`beam_param.cpp:102-137`）。現行コード全体を検索すると、次は探索判断に使われていない。

- `pool_size_sum`: 加算するが読まれない。
- `ema_active_rate`: 更新するが、`recommend_width()` は累積比 `count_active / turn_sum` を使う。
- `exp_count`: 引数を受けるが明示的に捨てる。
- `now_pool_size`: `get_beam_width()` が受けるが使わない。

`min_target_in_tree > turn` でスキップする空ターンは `timestamp_meta()` より前に `continue` するため、
`count_active / turn_sum` と `dt_empty` のモデルへ標本が入らない。`ema_active_rate` は探索判断に未使用であるだけでなく、
空ターン率も観測していない。

`width_hist` は最終ログ専用である。Compose 版の apply/rollback/ghost/tour カウンタと、標準版・Compose 版・
Radix 版の `explored_per_turn` も、出力しない実行で hot path から更新される。

一方、`BeamParam` は設定と実行中の観測値を同じ public object に持ち、search は非 const reference を受ける。
同じ `BeamParam` を再利用すると、通常版の `turn_sum`、`time_sum`、`prev_beam_width`、`width_hist` などは前回の値を
引き継ぐ。可変ターン版の `target_step_sum/count` だけは search 開始時にリセットされる。この部分的な再利用は、
意図した warm start か実行間汚染かを API から判断できない。通常版で使った Param を turn 版へ渡すと、別 backend の
`turn_sum/time_sum` が turn 版の active-rate 推定へ混ざる。

### 推奨する分離

次の三層へ分けると、固定幅 fast path と動的幅の両方を明確にできる。

1. `BeamConfig`: 最大ターン、最大幅、hash 方針、時間予算などの不変設定
2. `BeamRuntime`: その search 内だけの幅調整モデルと時刻標本
3. `BeamTelemetry`: 任意の詳細カウンタ、幅履歴、ログ sink

fast path の条件は `adjust_width=false && telemetry=none` である。この場合、各世代では clock を読まず、累積値も
書かない。最後に結果用の経過時間を1回だけ読む。動的幅では必要な標本だけを `BeamRuntime` に記録する。

既存利用者が search 後に `param.report()` や public field を読む可能性があるため、現行 API のまま暗黙に統計を
止めると観測可能な挙動が変わる。互換 wrapper は従来どおり収集し、新 API で `telemetry=none` を明示させるのが安全である。

累積候補数は `int` では `W * D` が約21億を超えると overflow する。統計を残す場合、累積値は64 bitにする。
これは候補ごとのデータ幅には影響しない。

### runtime verbose の型コスト

`verbose` は runtime bool なので、`verbose=false` でもログ branch はコンパイルされる。その結果、ログを使わない
利用者にも ScoreType の `ostream << score` が要求される。Action は他の候補登録や経路構築でも既に copy を要求され、
`get_best()` が新しい型要件を加えるわけではないが、verbose 時の余分な copy は残る。

`record_history` は `if constexpr` でこの問題を避けている。ログも外側で一度 dispatch し、
`search_impl<LogEnabled>()` とすれば、候補ごとの branch を増やさずログ固有コードを除去できる。

テンプレート組合せを増やし過ぎないため、少なくとも次の二系統で十分である。

- `Telemetry::None`: per-action/per-edge counter、世代履歴、ログ型要件なし
- `Telemetry::Detailed`: 現在の verbose ログと詳細カウンタあり

動的幅用の時刻標本は Telemetry ではなく `AdjustWidth` 側の要件とする。

### `get_best()` の Action copy

共通 Candidates と naive 内部 Candidates の `get_best()` は BeamCandidate を値で返す
（`candidates.cpp:183-189`, `naive_beam_search.cpp:121-126`）。標準版、Compose 版、Radix 版、naive 版は
verbose のターン表示で score だけを使うが、Action 全体まで毎ターンコピーする。

`best_score()` を追加して ScoreType だけを返すか、`const BeamCandidate&` を返せばよい。候補はログ呼出しが終わるまで
変更されないため、const reference の寿命は足りる。best を常時追跡すると非 verbose の hot write が増えるので、
ログ時の `O(W)` scan はそのままにし、値 copy だけを除く方がよい。未使用の `CandidatesFlat::get_best()` も同様である。

### ログ I/O

通常のターンログは `if (verbose)` の内側にあり、非 verbose の実行では文字列を構築しない。ただし標準版、
Compose 版、Radix 版、naive 版の no-candidate ログには非 verbose でも `cerr` へ出す経路がある。例外的な終了なので
平均 hot path ではないが、ライブラリとしては `LogSink::None` で完全に無出力にすべきである。

`width_trace()` は終了時に履歴全体をコピーして sort するため `O(D log D)` である。verbose 専用なので通常探索を
遅くしないが、非常に長い探索では `nth_element()` による中央値、または online summary を選べる。開始ログが
ostream の `fixed` / precision を復元しない点も、共有 stream を受ける API では直した方がよい。

### 小さいが同値に除ける固定費

優先候補ではないが、構造変更と混ぜずに計測できる項目もある。

- 標準版と Compose 版の `rnd` member は参照されず、`init_bs()` で毎探索再構築される。member と再構築を除ける。
- 固定深さ版の `next_tour`、`next_leaf` と一時 Action 列は `clear()` で容量を保つが、増加局面では再確保する。
  直前世代の実測量を基に `reserve()` する案は、深い探索での再配置回数を減らし得る。
- 標準版と Compose 版は最終結果で `result_prefix` を一度コピーしてから `reserve()` する。空 vector を必要量まで
  reserve してから prefix と suffix を挿入すれば、終了時の再確保と Action の再移動を避けられる。
- 確定接頭辞は、直後に世代 block または Radix node を解放する。値型 Action の契約下では、適用を済ませた後に
  結果列へ move できる。ただし moved-from Action を rollback や履歴出力が参照しない順序を backend ごとに確認する。

いずれも探索結果は変えずに実装できるが、通常は探索終了処理または初回成長だけの費用である。`A/W` に比例する
候補表・Action 問題より後に扱う。

## ID 幅と上限

### 固定深さ版と Compose 版

両実装の ActionId は64 bitで、上位を世代、下位24 bitを世代内 slot とする
（`beam_search.cpp:39-47`, `beam_search_compose.cpp:58-67`）。この形には次の特徴がある。

- `trace`、`tour`、`next_tour`、`CandIdx` の ActionId が8 byteになる。
- 1世代の slot は `2^24 - 1` までだが、`make_id()` に境界検査がない。
- 世代は実際には `int max_turn` の範囲なのに、40 bitを割り当てている。

現行 packing を維持する最低条件は、各 slot が `2^24` 未満であることを `finalize_generation()` で検査することである。
超過時はslotのbitが世代部へ混ざり、誤った Action を参照するため、単なる性能上の上限ではない。

固定深さ版では `trace[d]` の添字 `d` が世代を表す。`tour` から `trace` へ復元した後も Action を使う場所は論理深さを
知っている。この不変条件を確認できれば、ID 本体を32 bitの世代内 slotにし、`act(depth, slot)` で参照できる。
これにより `tour` 系の帯域を半減できる可能性がある。

Compose 版では ghost と Action 合成後の物理位置が絡むため、次を debug 検査してから変更する。

- `trace[d]` の非 ghost Action が常に対応世代 blockにある。
- 合成結果は子世代 slotへ移り、親 slotだけが ghostになる。
- `copy_tour_path()` が slotと論理深さの対応を崩さない。

現 API の `W` は正の `int` なので32 bitへ収まる。将来より広い幅型を受ける場合だけ明示検査を追加する。
16 bitは大幅の beam を扱えず、汎用 defaultにはしない。

### 可変ターン版

可変ターン版の ActionId は `int` で、再利用可能な `action_pool` slotを表す
（`beam_search_turn.cpp:27-79`）。32 bitはメタデータ帯域との釣合いがよい。符号付き `-1` sentinelの代わりに
`uint32_t` と `UINT32_MAX` を使えば正の範囲は広がるが、フィールド幅は変わらない。

`UINT16_MAX` を sentinel にする16 bit化は、live Action 数が最大65535個と保証できる opt-inだけにする。可変ターン木は
未来葉を長く保持し、
live slot数が単純な `W` では抑えられないため、自動選択は危険である。

### Radix 版と履歴 ID

Radix 版はノード ID を `int` にし、DFS stackで `(node << 1) | phase` と詰める
（`beam_search_radix.cpp:42-52`, `beam_search_radix.cpp:178-206`）。従って pool indexは実質
`INT_MAX / 2` 以下でなければ shiftが安全でない。new node時に上限を検査するか、phaseを別 fieldにする。

`record_history=true` の `node_id_counter` は列挙した全候補で増える。これは生存候補数 `WD` よりはるかに早く
32 bitを超え得る。履歴は cold/debug 機能なので64 bit IDを選ぶか、出力可能件数に明示上限を設ける方がよい。
履歴 IDを無条件に64 bit化して通常候補へ持たせると hot payloadが太るため、履歴なし型から完全に除去する。

## データ配置

### 候補 payload

共通 `BeamCandidate` は `parent_leaf`, score, Action, `node_id` の AoS である
（`candidates.cpp:11-17`）。`record_history=false` でも `node_id` が残る。scoreだけを走査する `get_best()` や
metadataを移す処理でも、大きな Actionと同じ strideを通る。

典型的な64 bit ABIで ScoreTypeとActionがともに8 byte alignmentを持つ場合、現行順の候補は32 byteになる。
`score, action, parent_leaf, node_id` の順なら24 byteに収まる場合がある。履歴なし特殊化でも24 byteになる。
ただし型ごとの alignmentで結果は変わるため、対象型ごとの `sizeof` をベンチ結果へ必ず記録する。

より汎用的なのは次の hot/cold 分離である。

- hot: score、parent slot、必要なら hash locator
- cold: Action arena、履歴 node ID

総 byte数が同じでも、threshold、sort、best scanが読む cache lineを減らせる。Actionを採用時だけ構築する設計とも
相性がよい。Action適用時には別配列へのloadが増えるため、小さな ActionではAoSが勝つ可能性も測る。

### Action の既定構築と世代 block

共通 Candidates と naive Candidates は、最大幅が増えたとき `next_beam.resize(w)` で Action を含む候補を `W` 個
既定構築する。これは最大幅到達時に原則1回だけだが、Actionが重い型では起動費と既定構築可能という型制約になる。
hot/cold分離でAction slotを必要時だけ構築すれば除けるが、置換時の破棄と例外安全性をarena側で扱う必要がある。

より直接的な無駄は、標準版とCompose版の `finalize_generation()` にある。世代blockを `resize(sz)` してActionを
`sz` 個既定構築し、その直後に全slotへ候補Actionをmove-assignしている
（`beam_search.cpp:83-99`, `beam_search_compose.cpp:164-190`）。世代blockと再利用slabは空の状態で受け取るため、
`clear()`、`reserve(sz)`、`emplace_back(move(action))` ならslot順を保ったままmove-constructできる。

これは通常の値型契約では候補採否や探索順を変えない。default construction、move assignment、destruction の時刻や
回数へ外部副作用を持つ Action は同値性の対象外とする。DUMMY Action などが別に存在するため、これだけでは
ライブラリ全体の「Action は既定構築可能」という要件までは外れない。

固定深さ標準版の `CandIdx` は `parent_leaf`, score, 64 bit ActionId, `node_id` の順である。上記の典型型では
32 byteになり、scoreとActionIdを先に並べるだけで24 byteになる可能性がある。ActionIdを32 bit化し、履歴なし
payloadを特殊化すれば、さらに hot strideを縮められる。

Compose 版は `action_count` も持つため、単純な並べ替えだけでは32 byteのままになりやすい。現在の
`action_count` が常に世代番号なら、この field自体を暗黙化できる可能性があり、構造監査側の簡約案と合わせて評価する。

### 可変ターン木

可変ターン版 `TreeNode` は4個の32 bit fieldで16 byte、BeamCandidateは64 bit scoreなら一般に24 byteである
（`beam_search_turn.cpp:37-52`）。Actionは別 arenaなので、大きな Actionがtree走査のstrideを直接広げない点はよい。

TreeNodeの各走査で必要なfieldは異なる。

- `get_next_beam()`: marker、subtree end、ActionId、target turn
- `update_tree()`: ほぼ全field
- 結果復元: markerとActionId

SoA化するとskip判定だけの帯域は減るが、木更新では複数streamを読む。16 byte AoSは既に小さいため、まず
Action arenaの安定化とstale候補圧縮を優先し、TreeNode分割はprofile後にする。

### Radix Node

Radix `Node` はAction、5個のint link/count、scoreを同じobjectに持つ
（`beam_search_radix.cpp:23-30`）。Actionとscoreが各8 byteなら典型的に40 byte、Actionが128 byteなら
典型的に160 byteになる。surgeryと部分木最小値再計算は主にlinkとscoreを使うため、大きな Actionほどcache strideが
悪化する。

metadataとActionを別arenaにすれば総保持量は必ずしも減らないが、metadata-only phaseのstrideを小さくできる。
一方、DFS下降ではmetadataに続いてActionも必要なのでloadが2本になる。次の二型を分けて測る。

- small/trivial Action: 現行AoSが有利な可能性が高い。
- large/non-trivial Action: metadata + Action arenaが有力である。

### naive Node

naive `Node` のstrideはほぼ `sizeof(State) + sizeof(Action)` で決まる。NodeをSoAへ分けても各葉展開でStateと
last Actionの両方を使うため、まず効果が大きいのは次の二点である。

- vector reserveでNode再配置を止める。
- last Actionを安定arenaのIDへ置き換え、重複所有をなくす。

Stateが非常に大きい場合は、Naive backendではなく差分木またはState側のcopy-on-writeを選ぶべきである。
`sizeof(State)` だけではcopy costやapply/rollback costを推定できないため、自動backend選択はしない。

## 可変ターン版のイベント駆動化

### 現行の空ターンコスト

`beam_search_turn.cpp` は `for (turn++)` で全論理ターンを進める。`min_target_in_tree > turn` の間は探索本体を
実行しないが、1ターンずつ分岐、pool確認、`turns_done` 更新を行う
（`beam_search_turn.cpp:727-737`）。target turnが大きく飛ぶ場合、active meta-turnが少なくても時間は
`O(max_turn)` になる。

さらに初期化時に `turn_to_pool_idx` と `thresholds` を `max_turn + 1` 要素で埋める
（`beam_search_turn.cpp:681-689`）。ループを飛ばしても、この初期化時間とメモリは `O(max_turn)` のまま残る。

### 条件付き: 次の target turnへ直接進む

空ターンでは現在、次を行っていない。

- `get_next_beam()` と `update_tree()`
- `timestamp_meta()` と clock取得
- 履歴 snapshot
- `note_target_step()`

従って固定幅では、`turn = min_target_in_tree` へ直接進めても探索計算は同じにできる。実装前に守る条件は次のとおり。

| 項目 | 必要な不変条件 |
|---|---|
| pool | `[turn+1, next_target)` の allocated pool を昇順に解放し、next_target 自身は残す |
| `turns_done` | `min(next_target, max_turn)` まで論理的に進んだ値を設定する |
| target==max_turn | 未完了候補は展開せず、pool を残して最終候補として復元する |
| 履歴 | 現行も空ターンsnapshotを作らないため、新規snapshotを補わない |
| 終端 | `max_turn` は有効葉と空木 sentinel の両方なので、`get_result()` で区別する |

特に pool 条件は推測で省略しない。現行コードは空ターンごとに `turn_to_pool_idx[turn]` を検査するため、その確認を
飛ばすには次のいずれかが必要である。

1. 解放対象の半開区間に allocated pool がないことを debug assert とテストで保証する。
2. allocated target turn の min-heap または連結リストを持ち、区間内の pool だけを昇順に解放する。

固定幅では同点順やState操作順は変わらない。動的幅では高速化で残り実時間が増え、後続の推奨幅が変わり得る。
時間適応型探索では、どの性能改善でも起こる差である。

### 別 backend: sparse/paged turn calendar

極端に大きく疎な max_turnを汎用的に扱うには、イベントジャンプだけでなくdense配列も置き換える。
現 State API は `try_op()` に `const vector<ScoreType>& thresholds` を渡し、利用側が `.size()` と添字アクセスを行える。
従って `thresholds` の sparse 化には `ThresholdView` または accessor への API 変更が必要である。

候補は次の三方式で比較する。

- dense: 現行。target lookupが最速で、max_turnが小〜中なら基準実装。
- paged dense: target turnを固定長pageに分け、触れたpageだけ確保する。
- sparse map + min-heap: calendar保持量を `O(F)` にするが、候補ごとのthreshold lookupが高い。

自動切替は `max_turn` だけでなくtargetの局所性に依存する。明示 `TurnCalendarPolicy` または、dense上限を持つ
hybrid backendにする。thresholdは候補列挙中のhot lookupなので、疎構造のメモリ削減だけで採用しない。
また global seen の既定初期容量も `W * max_turn` に比例するため、`clear_hash_every_turn=false` で全体を `O(F)` に
するには closed-set capacity policy も同時に変える必要がある。

## API とコンパイル時ポリシー

### 現行で有効なコンパイル時除去

- `record_history` は `if constexpr` により、文字列化と履歴構築の大部分を除く。
- `materialize_final_state` は最終Stateの巻戻し・再適用を除ける。
- callback形式とvector形式の `enumerate_actions()` は `requires` でコンパイル時に選ばれる。
- Radixの `push_lazy()` は採用時だけActionをコピーする。

これらは維持する。特に callback APIは、Stateが現在のthresholdを参照しながら早期枝刈りでき、巨大なAction列の
一括構築を避けられる。

### 不完全な除去

- `record_history=false` でも `BeamCandidate::node_id` と各 `CandIdx::node_id` が残る。
- `verbose=false` はruntime分岐なので、counter更新、型要件、コードサイズが残る。
- `materialize_final_state=true` が既定で、Action列だけ欲しい利用者は明示的に `<false>` を指定する必要がある。
- `history_file = ""` は履歴なしでも一時 `std::string` を作り得る。
- 共通 `Candidates` の State template引数は内部で使われていない。

`search_actions()` のような最終Stateなしの名前付き入口を用意すると、速い利用法を発見しやすい。既定値をfalseへ
変えるのは返却内容が変わるため、互換性を考えた別入口の方が安全である。履歴ファイルは `std::string_view` または
履歴有効overloadへ分ける。

### 最小限の policy 軸

すべてをtemplate boolにするとコードサイズとコンパイル時間が組合せ爆発する。次の区分が妥当である。

| 軸 | compile-timeにする理由 | 推奨 |
|---|---|---|
| History | fieldと文字列化を完全除去したい | 現行どおり |
| FinalState | 大きな巻戻し・再適用を完全除去したい | 現行を維持し名前付き入口追加 |
| Telemetry | hot counterとログ型要件を除去したい | None / Detailedの2値 |
| Index | payload幅が型layoutを変える | 不変条件を確認した backend だけ32 bit、narrowは明示 |
| Selector | データ構造全体が変わる | 別policyまたはbackend |
| Hash | 再混合・重複排除の意味が変わる | 明示policy |

runtime optionは外側で少数のtemplate実装へdispatchすればよい。候補ごとの `if (verbose)` は避ける。

### hash 方針の一貫性

標準版、Compose版、Radix版、naive版の共通 CandidatesはHashDictの再混合を使う。一方、可変ターン版は
`HashDict<..., false>` で入力hashをそのまま使う。後者はZobristなど十分混ざった64 bit hashには速いが、低bitに
偏りがあるHashTypeではprobeが悪化する。

汎用defaultはrobust mixer、`IdentityHash` は「入力が均一な64 bit hash」という契約付きopt-inにする。
重複排除自体を無効にするpolicyは高速になり得るが、生存候補集合が変わるため探索方針変更として扱う。

### backend 名と選択

標準版、Compose版、可変ターン版は同じ名前 `BeamSearchWithTree` を定義するため、同一translation unitで複数を
includeして比較・選択しにくい。backend固有名と互換aliasを分けると、型安全な明示選択と共通benchmarkが容易になる。

backendの自動選択を `sizeof(State)` や `sizeof(Action)` だけで行ってはいけない。copy、apply、rollback、composeの
実コストと木形状は型サイズから分からない。利用者が明示選択し、benchmarkで損益分岐を示す方がライブラリ向きである。

## 意味論リスク一覧

| 変更案 | 候補集合 | 同点順 | State呼出し順 | 主な注意 |
|---|---|---|---|---|
| Action遅延保存 | 同一 | 同一 | 同一 | lambdaの参照寿命はpush中だけ |
| vector reserve | 同一 | 同一 | 同一 | copy/move回数は観測対象外とする |
| score-only `get_best` | 同一 | 同一 | 同一 | verbose専用 |
| clock sample再利用 | 固定幅は同一 | 同一 | 同一 | 動的幅は時刻値が微小に変わる |
| telemetryなし | 固定幅は同一 | 同一 | 同一 | search後の統計値は変わる |
| 空ターンjump | 固定幅は同一 | 同一 | 同一 | pool解放とturns_doneを保証する |
| 親Stateの最後の子へmove | 同一 | 同一 | apply順は同一 | Stateの値型move契約が必要 |
| IDの32 bit化 | 同一 | 同一 | 同一 | overflow検査と世代不変条件が必要 |
| AoS/SoA変更 | 同一 | 同一 | 同一 | Action参照の安定性を維持する |
| 完成世代でnaiveを停止 | 変わり得る | 変わり得る | 短くなる | 明示探索policy |
| dedup無効化 | 変わる | 変わる | 変わる | 探索方針policy |

「完全同値」は、StateやActionの構築、copy/move、代入、破棄の回数や時刻に外部副作用がない通常の値型契約を
前提とする。乱数消費、列挙順、同点比較規則は維持する。

## 実装順

性能コードに着手する場合は、次の順で独立に測る。

1. `get_best()` のscore-only化、naive vector reserve、Action遅延保存、世代blockのmove構築
2. clock sampleの重複除去と、死んだ統計fieldの計測
3. `Telemetry::None` の固定幅経路
4. 可変ターンのイベントjumpとpool不変条件テスト
5. 履歴なしpayloadの特殊化とfield並べ替え
6. naiveの親State moveと安定Action arena
7. 固定深さ版32 bit slot ID
8. Radix metadata/Action分離、可変ターンpaged calendar

各段階で、候補採否列、同点順、最終Action列をdigest比較する。型軸として少なくとも次を含める。

- State: 16 byte、256 byte、4 KiB。copyとapply/rollbackのコストを別々に変える。
- Action: 4 byte、32 byte、256 byte。trivialとnon-trivialの両方。
- Score: 32 bit整数、64 bit整数、double。
- `W`: 16、256、4096、65536。
- naiveの `P/W`: 小、中、1。
- 可変ターンのjump幅: 1、16、1024、100万。
- `max_turn` とactive target数 `F` を独立に変える。
- telemetry、dynamic width、history、final Stateを個別にon/offする。

clock呼出し回数、State/Actionのcopy/move回数、各vectorの再確保回数、候補payloadの `sizeof`、peak RSS、
cycles、instructions、LLC missを保存する。単一の問題固有testの総時間だけで採否を決めない。
