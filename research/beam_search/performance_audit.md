# ビームサーチ・ライブラリ性能監査

## 目的

特定問題のテストを速くするのではなく、次の実装を汎用ライブラリとして高速化する余地を調べる。

- `beam_search.cpp`: 固定深さの標準版
- `beam_search_compose.cpp`: 単一子の Action を合成する固定深さ版
- `beam_search_radix.cpp`: 明示的な圧縮木版
- `beam_search_turn.cpp`: 遷移先ターンが可変の版
- `candidates.cpp`, `beam_param.cpp`, `hash_dict.cpp`: 共通の候補管理と補助構造

この文書では、測定済みの事実とコードからの推論を区別する。本文のコード監査を先に固定し、その後の実装と
実測を追記する。

## 実装後の状態

- 通常版を保つ定数倍改善は `beam_search_optimized.cpp` へ実装済み
- direct parent oracleは `beam_search_parent.cpp` へ実装済み
- frontierをcandから導出するdirect parent版は `beam_search_parent_compact.cpp` へ実装済み
- 4 backendの差分試験、sanitizer、構造modelを実行済み
- 最終実測ではoptimized版が15ケースすべてで基準版以下、parent版は一律には勝たなかった
- 詳細値と測定限界は `benchmark_results.md`、個別kernelは `constant_factor_microbench_results.md` を正本とする

## 結論概要

最も優先度が高いのは、探索木を別方式へ全面置換することではなく、候補パイプラインで最終幅 `W` から
漏れている処理量とメモリを止めることである。

1. 現世代の重複表を、一時採用された異なる hash 数ではなく `O(W)` に制限する。
2. 可変ターン版の一時候補と Action を、採用イベント数 `A` ではなく最終 dirty slot 数に比例させる。
3. 標準版と Compose 版で、採用前の Action コピーと候補領域から世代 block への二重保存を除く。
4. 可変ターン版の target-turn pool が少数候補にも約 `16W` bucket を確保する設計を改める。
5. 共通セグメント木の不要な全初期化、統計、64 bit ID、比較ソートなどの帯域と固定費を順に測る。

標準版は、traP の記事にある帰りがけ順の `tour / leaf / trace` を使う高速化後の方式である。
この確認は現行方式の最速性を意味しない。単一 State と辺単位 API では状態遷移回数が下限に近い一方、
毎世代 `next_tour` へ ActionId を書き直す処理は情報下限ではない。

32 bit slot-onlyと隣接LCP付きparent-mapは比較実装まで完了した。選択後のsurvivor-parentだけを使う遅延構築は、
開始trace snapshotが必要な条件付き案として残る。一般 LCA、binary lifting、RMQ の追加だけでは
必須の `apply_op()` / `rollback()` を減らせないため、後継案にはしない。

外部資料の速度比は問題固有なので、このライブラリでの予測値としては使わない。改善案は
「候補と順序まで同じ」「上位集合だけ同じ」「探索方針が変わる」の三段階に分けて検証する。

## 詳細文書

- [候補・ハッシュ・Action・メモリ](./candidate_pipeline.md)
- [帰りがけ順版を基準にした後継構造](./postorder_successor.md)
- [後継構造の独立設計レビュー](./postorder_external.md)
- [探索木、LCA、parent-slot、可変ターン疎密案](./structural_options.md)
- [naive、計測、ID、データ配置、可変ターン jump](./cross_cutting.md)
- [並列化、BatchState、GPU、multi-start](./parallel_and_batch.md)
- [外部一次資料](./literature.md)
- [汎用合成ベンチマーク計画](./benchmark_plan.md)
- [固定深さ4 backendの最終実測](./benchmark_results.md)
- [定数倍変更の個別kernel](./constant_factor_microbench_results.md)
- [postorderとparentのtopology kernel](./topology_microbench_results.md)
- [direct parent実装監査](./parent_implementation_audit.md)
- [cand導出parent版の実装記録](./parent_compact_implementation.md)

## 記号

- `W`: ビーム幅
- `B`: 1状態から列挙する候補数
- `D`: 探索深さ
- `K`: そのターンで実際に展開する葉数
- `L`: 現在の葉数
- `P`: 生存候補が参照する異なる親数
- `F`: 同時に存在する target-turn 候補プール数
- `Q`: 可変ターン版の平坦木に含まれる PRE/POST/leaf token 総数
- `R`: そのターンにスキップせず実際に読む token 数
- `N`: 生存探索木のノード数
- `A`: 一時的に上位 `W` 件へ入った候補の総数
- `U`: 現世代の表へ挿入した異なる hash 数
- `H`: ターンをまたぐ履歴表に保持する異なる hash 数

一般に `try_op()` が十分重い問題では候補評価が支配的になる。一方、差分更新が軽い問題では、ハッシュ、
上位 `W` 件の管理、Action のコピー、木の走査と再構築、時刻計測が支配的になり得る。

## 現行構造の確認

### 固定深さの標準版

標準版は、明示的な全探索木を毎ターン走査していない。前世代の葉を `cand`、葉間の差分経路を `tour`、
各区間の終端を `leaf` に保持し、`cand` を親葉順に逆走査する。`li` は単調に減るため、LCA 距離の計算と
`copy_tour_path()` が調べる `leaf` の範囲は、同一世代全体で概ね一方向に進む。

これは、質問で示された「完全な Euler tour ではなく帰りがけ順だけを保持する」高速化後の構造そのものである。
ここで LCA は葉間移動の説明にすぎず、一般 LCA data structure を持つという意味ではない。
残るコストは次のとおり。

- 葉間を移動するために必要な `apply_op()` / `rollback()`
- 次世代用 `tour` への ActionId のコピー
- `cand` の `(parent_leaf, score)` ソート
- 候補管理と Action の一時保存

世代別 parent-map と隣接 LCP へ置き換えると `tour` の再構築を除けるが、下り経路で依存 load が増える。
また選択後の生存親だけで `tour` を遅延構築すれば、連続表現を維持したまま不要な親経路の書込みを省ける。
固定深さ版では現行を基準実装として残し、この2方向を別 backend で比較する価値がある。

### Compose 版

Compose 版は、単一子の親 Action を子へ合成し、親を ghost として `apply_op()` / `rollback()` から除外する。
ただし論理深さは常に世代番号であり、現在の `CandIdx::action_count` と `eff_depth` は可変にならない。
このため `eff_depth`、`next_eff_depth` と可変深さ版の `copy_tour_path()` は、標準版と同じ論理へ簡約できる
可能性が高い。

また、次の計測は `verbose=false` でも毎回更新される。

- `cnt_apply`, `cnt_rollback`
- ghost の省略回数
- `cnt_compose_align`
- `tour` と候補数の累積

計測をテンプレートまたは別ポリシーに分ければ、軽い `State` での固定費を除ける。

### Radix 版

Radix 版は明示ノードを持ち、単一子の内部ノードを `Action::compose()` で縮約する。根直下の単一子は
確定接頭辞へ移すため、一本道が長い場合の状態更新回数と木の深さを減らせる。

一方、`Node` が `Action` と木メタデータを同じ構造体に持つ。Action が大きい場合、DFS が必要とする
`parent`, `first_child`, `next_sibling`, `score` を読むだけでも大きなキャッシュラインを消費する。
ノードメタデータと Action を分離する SoA または別 arena は有力な候補である。

### 可変ターン版

可変ターン版は探索木を PRE/POST/leaf の連続列に平坦化する。`get_next_beam()` は部分木の最小
`target_turn` を使って未来の部分木をスキップできるが、各メタターンで木列を走査する。
`update_tree()` は生存木を `nxt_tree` へ再構築する。

木の最小 `target_turn` が現在値より大きい場合、木走査は省く一方、外側の `for (turn++)` は空ターンを
1ずつ通る。また `thresholds` と `turn_to_pool_idx` は `max_turn + 1` 要素を確保する。非常に疎で大きな
ターン番号を許すライブラリ用途では、イベント時刻へ直接進むループと、paged/sparse なターン表を別 backend として
検討する価値がある。現在の dense 配列はターン範囲が小さい場合の O(1) lookup に優れるため、置換はしない。

展開対象が木の大半を占める場合、連続列はキャッシュ効率がよい。反対に `K << L` で未来ターンの葉が多い
場合、対象葉のカレンダーバケットと親ポインタ木を使い、対象葉間だけを LCA で移動する方式に改善余地がある。
これは標準版より可変ターン版で効果が見込める構造変更である。

### backend の選択目安

単一の実装を全用途の最速にすることは難しい。次の表は自動判定規則ではなく、利用者が比較を始めるための目安である。

| workload / 契約 | 最初に比較する backend | 理由 |
|---|---|---|
| State が小さく copy が安い | naive と標準版 | 連続 State 配列が差分往復より速い場合がある |
| 固定深さ、compose 契約なし | 標準版 | 連続 tour の局所性と小さい保持量を両立する |
| 一本道が多く compose が安定して成功 | Radix と Compose | 合成で apply/rollback 回数を減らせる |
| Action が大きく metadata 走査が支配 | 標準版、または SoA 化した Radix | 現行 Radix の AoS は大きな stride になり得る |
| target turn が可変で範囲が密 | 現行 turn 版 | dense 配列と平坦木の O(1) lookup・連続走査を使える |
| target turn が極端に疎で `K << L` | calendar＋親鎖の試作 | 空ターンと対象外部分木の走査を避けられる可能性がある |
| `W*B` が大きく State を worker 数だけ複製可能 | exact CPU 並列版の試作 | 独立 State と local top-W の global merge が成立する |

`sizeof(State)` や `sizeof(Action)` だけでは copy、差分更新、合成、木形状の費用を推定できない。backend は明示選択を
基本とし、合成ベンチマークで損益分岐を提示する。短い warm-up による自動選択は、探索順と動的幅への影響を別途
仕様化できる場合だけ検討する。

## 既に確認できた優先候補

以下は候補パイプライン全体に残る仮説である。optimized版へ実装済みの項目は個別に明記する。

### P0: 候補ハッシュ表の増大を `O(W)` に抑える

`Candidates::push()` は追い出したハッシュの値を `-1` にするが、キーを表から削除しない。そのターンに
一時採用された異なる hash 数 `U`（`U <= A`）に比例してハッシュ表が成長し、次ターンに `clear()` しても
容量は縮まない。
分岐数が大きい一度のターンで、その後の全探索が大きな表を持ち続ける可能性がある。

候補管理専用表として次のいずれかを比較する。

1. `DELETED` 制御バイトを持つ開番地表と、tombstone が増えたときの生存 `W` 件からの再構築
2. 各候補が表内位置を保持し、位置指定で無効化したうえで定期再構築
3. 世代タグ付きの固定容量表

表の容量、tombstone 数、probe group 数、再構築回数を計測する必要がある。

なお、`Candidates::reset()` と `CandidatesFlat::reset()` の `func.inner_len() == 1` は、既定容量が16なので
成立しない。初回事前確保の意図が実現されていない。

### P0: 可変ターン版の一時候補を木更新前に圧縮する

可変ターン版は一度採用された候補を `new_candidates` に追加し、後から追い出されてもソートと木更新まで
保持する。したがって処理量と一時メモリが最終生存数ではなく `A` に比例する。

最小変更は `is_survived_node` で安定 compaction してからソートする方法である。ただし、これは列挙中に
Action arena が一度 `A` まで増える問題を直さない。最終案では候補スロットに dirty stamp と pending Action を
持たせ、同じスロットの再置換では Action を上書きする。列挙後は dirty slot の最終内容だけを収集する。

### P0: Action の遅延保存を標準版と Compose 版でも使う

コールバック形式では、標準版と Compose 版が `Candidates::push()` に Action を値渡しする。このコピーは
関数内の閾値判定やハッシュ重複判定より前に起こる。Radix 版は既に `push_lazy()` を使っている。

標準optimized版は採用時だけActionを保存する経路へ変更済みで、独立kernelでも棄却率が高い領域の効果を確認した。
Compose版への適用と、候補から世代blockへの二段ownership除去は未実装である。

### P0: 可変ターン版の候補プール容量を実 occupancy に合わせる

可変ターン版は target turn ごとに `HashDict(8W)` を作るが、constructor は想定要素数の2倍以上の bucket を
確保するため、実際は最低約 `16W` bucket になる。64 bit hash と 8 byte Score の代表的な配置では、候補表、
セグメント木、候補列を合わせて1プール最低約 `272W` byteとなる。少数候補が多数の未来ターンへ散る場合、
保持量は `O(FW)` になる。

小容量から開始して実 entry 数に応じて grow する方式、または `(target_turn, hash)` の共有表を比較する。
`clear_hash_every_turn=false` の global seen も、現在の自動ヒントでは初期確保だけで最低およそ
`100 * W * max_turn` byteになり得る。全期間の exact 履歴は本質的に履歴 key 数 `H` のメモリが必要なので、
初期容量、window、上限超過時の意味論を別々に指定する。

### P1: pre-hashed key と通常 key を契約で分ける

標準版は hash を再混合するが、可変ターン版は `HashDict<..., false>` で常に identity を使う。Zobrist hash の
ように全 bit が十分拡散済みなら再混合を省ける一方、連番や低 bit が偏った key では probe が集中する。
安全な mixed hash を既定とし、`IdentityPrehashed` を明示 opt-in にする。identity 版では現在も実行される
未使用の `random_device` による seed 生成も compile-out する。

### P1: セグメント木の全初期化を避ける

共通 `Candidates::reset()` は毎ターン `2*s` 要素を `-INF` で埋める。その後、幅まで候補が入ると
`build_segtree()` が使用葉と全内部ノードを上書きする。幅と `s` が変わらない通常ケースでは、未使用葉だけを
幅変更時に初期化すればよい。可変ターン版の内部 Candidates は既に build 時に未使用葉だけを埋めている。

### P1: 上位 `W` 件構造をポリシー化する

現在のセグメント木は、列挙中に常に正確な worst 値を返せるため、`submit.threshold()` による早期枝刈りと
相性がよい。代替案は次の条件で分ける。

- exact threshold が必要: indexed max-heap または tournament tree
- 列挙中の threshold が不要: バッファ後に `nth_element()` / radix select
- Score が小さな整数範囲: bucket / histogram

一律置換ではなく `CandidateSelector` ポリシーにする方がライブラリとして安全である。

### P1: 親番号の全体比較ソートを減らす

標準版と Compose 版の `parent_leaf` は密な整数である。親ごとの count、累積和、scatter で候補を分け、
親内だけ score 順にすれば、比較回数を `O(W log W)` から親内ソートの合計へ減らせる。Radix 版には既に
同じ構造がある。同点順まで維持する場合は enumeration ordinal を比較キーとして仕様化する。

現行の標準版と Compose 版は配列を逆走するため、同じ親の中では悪い score の葉を先に展開する。
親区間内だけ良い葉を先にすれば状態移動回数を増やさず threshold を早く下げられる可能性があるが、
乱数消費順と同点候補を変え得るため opt-in の探索順ポリシーとする。

### P1: score の単調性があるときだけ親展開を省く

子の score が親より改善しない契約があれば、次世代の exact threshold 以上の親は、子を列挙しても採用されない。
Radix 版の `monotone_skip` はこの性質を部分木へ使っている。標準版、Compose 版、単純コピー版でも
`score_is_monotone` または安全な optimistic bound を opt-in trait として使える。契約がない問題へ適用すると
候補を誤って落とすため、既定にはしない。

### P1: 計測なしの固定幅経路

標準版は固定幅でも1世代に複数回 `high_resolution_clock::now()` を呼び、`width_hist` と累積値を更新する。
可変ターン版と Compose 版はさらに多くの計測を行う。`adjust_width=false`, `verbose=false`,
`collect_stats=false` をコンパイル時または検索ポリシーで分離する。

### P1: 可変ターン版の Action arena を安定アドレス化する

可変ターン版は `action_pool` の再確保中に参照が無効になるため、展開対象 Action を毎回コピーしてから
`enumerate_actions()` を呼ぶ。固定長チャンク arena にすれば、参照を保ったまま候補追加できる。
32 bit のチャンクIDとオフセットを使えば、ActionId を維持しつつメタデータ帯域も抑えられる。

### P1: 可変ターンをイベント駆動にする

`min_target_in_tree > turn` の区間は、論理ターンを1ずつ進めず次のイベントへ直接飛べる可能性がある。
そのためには、途中の候補プールをいつ free list へ戻すか、`turns_done`、動的幅の統計、履歴のターン番号を
同じ意味に保つ必要がある。`max_turn` が大きくイベント数が少ない workload では、時間を
`O(max_turn)` からイベント数へ寄せられる。dense な `thresholds` も支配する場合は paged 配列か sparse map を使う。

## 構造案の比較で守る条件

- 同点候補の順序と乱数消費順を変えると探索結果は変わり得る。
- `State::apply_op()` と `rollback()` の呼び出し順を変えてよいかは、API契約として明示する必要がある。
- `Action` の参照安定性、move 後の状態、`compose()` の破壊性を考慮する。
- `clear_hash_every_turn=false` の全ターン重複排除は、ターンごとの重複排除と別構造にする。
- `record_history=false` では履歴用フィールドと分岐を hot path から除く。

## 測定計画

問題固有の1テストだけで結論を出さない。少なくとも次の軸を直交させた合成 State を用意する。

- Action サイズ: 8 / 32 / 128 byte
- `try_op()` コスト: 極小 / 中 / 大
- 分岐数 `B`: 2 / 8 / 64 以上
- ビーム幅 `W`: 小 / 中 / 大
- 重複率: 0% / 中 / 高
- 生存木の共有接頭辞: 長い / 短い
- 可変ターン版の展開密度 `K/L`: 低 / 中 / 高
- 可変ターン版の走査量 `R/K` と走査率 `R/Q`: 小 / 中 / 大
- Score: 32 bit 整数 / 64 bit 整数 / 浮動小数

収集する指標:

- 1秒当たりの `try_op()` 回数
- 候補1件当たりの CPU cycle
- Action の copy / move / destruction 回数
- `apply_op()` / `rollback()` 回数
- ハッシュの probe group 数、再構築回数、最大容量
- 一時採用数 `A` と最終生存数の比
- 木メタデータの走査量と更新量
- peak RSS と主要 vector の capacity
- branch miss、LLC miss、命令数

## 今後の実測と実装

通常optimized版とdirect parent版の初回実装・実測は完了した。次は
[汎用合成ベンチマーク計画](./benchmark_plan.md) に従い、候補集合と順序を保存する変更を一つずつ測る。特に
`A/W`、`K/L`、`R/K`、`R/Q`、Action size、State costを直交させ、単一の問題やtestに最適化しない。

実装順は「期待効果」と「導入リスク」を分ける。期待効果の第一群は active hash の `O(W)` 化、turn 版の dirty
slot/pending Action、Action の遅延保存、target-turn pool の実 occupancy 化である。導入は、計測、segtree の遅延初期化、
`push_lazy()`、pool 容量の順で小さく始め、その後に hash の再構築と Action 所有権を変更する。

一般 LCA、binary lifting、GPU top-k、無条件の `nth_element()`、無条件の identity hash は既定案にしない。
parent-slot、疎な可変ターン木、並列・BatchState、best-first 系は別 backend または opt-in policy として比較する。
