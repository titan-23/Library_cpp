# ビームサーチ高速化のベンチマーク計画

## 目的

改善案を特定問題の枝分かれや `State` 実装へ過適合させず、ライブラリの固定費と適用条件を測る。単一の
実行時間だけでなく、候補数、重複率、Action サイズ、差分更新コスト、木の形を独立に変える。

この計画は性能実装と測定の基準である。固定深さ4 backendのend-to-end driverは
`backend_benchmark.cpp`、実行scriptは`run_backend_benchmark.sh`、初回結果は`benchmark_results.md`に保存した。
候補selector、可変ターン、perf、RSSを含む残りのmatrixは引き続きこの計画に従って追加する。

主な記号は次のとおりである。

- `W`: 最終ビーム幅
- `B`: 1状態の平均分岐数
- `C`: 1世代で列挙した全候補数
- `A`: 一度でも上位 `W` 件へ入った候補数
- `U`: 現世代の候補表へ挿入した異なる hash 数
- `H`: ターンをまたぐ exact 履歴表が保持する異なる hash 数
- `P`: 生存候補が参照する異なる親数
- `L`, `K`: 可変ターン版の総葉数と、そのターンで展開する葉数
- `N`: 生存探索木の node 数
- `Q`: 可変ターン版の平坦木が保持する PRE/POST/leaf token の総数
- `R`: 可変ターン版で、そのターンに実際に読んだ token 数
- `F`: 同時に存在する target-turn 候補プール数
- `D`: 探索深さ
- `G`: 未解放の世代 block に確保された Action slot 数
- `E_live`: current frontierとendpointが必要とする未確定Action辺数
- `M_tour`: 未参照の先頭segmentを除いた帰りがけ順streamのslot数
- `L_parent`: direct parentが1世代で行うdepth方向の依存load数
- `S_A`: `sizeof(Action)`。Action が所有する外部領域は別に記録する
- `T`: 並列 worker 数

このディレクトリでは同じ記号を同じ意味で使う。特に `P` は親種類数、`F` は active target pool 数、`Q` は
平坦木全体、`R` は実際の読取量である。各詳細文書の局所的な補助記号は、結果ファイルでは説明的な列名へ展開する。

## 意味論を先に分類する

比較結果を混ぜないため、変更案を次の三群に分ける。

1. **完全同値**: 候補の採否、同点順、`try_op()` を含む State 呼び出し順が同じ
2. **集合同値**: 各世代の上位候補集合は同じだが、同点順や列挙順が変わり得る
3. **探索方針変更**: 近似閾値、親ごとの上限、多様性、best-first、multi-start など、残る候補自体が変わり得る

完全同値は、State と Action の copy、move、代入、破棄の回数や時刻に外部副作用がない通常の値型契約を前提とする。
固定幅では、active hash の `O(W)` 化、Action 遅延保存、セグメント木の遅延初期化、容量だけの変更を完全同値に
実装できる。ただし exact な cross-turn dedup は active table と別に `O(H)` の履歴表を保持しなければならない。
死んでいる `HashDict(8W)` の条件だけを直すと過剰確保になるため、単独の改善案にはしない。

`nth_element()` は同じ候補 multiset と全順序の tie key から exact select する場合に限って集合同値にできる。
並列版は canonical rank と、同一 key の rank 最小要素を選ぶ selective reducer を定義し、local dedup、
local top-`W`、exact global merge の順に処理する。初期比較は固定幅、`clear_hash_every_turn=true`、threshold が安全な
枝刈りにだけ使われる State に限る。score だけの threshold は同 score の良い tie key を落とさないよう、`>` だけを
枝刈りするか、rank 全体を表す必要がある。
さらに terminal 判定は通常候補の cutoff より先に確定するか、cutoff で潜在的な terminal を落とさない契約を要求する。
緩い threshold、親内の処理順変更、local quota、近似圧縮、多様性制御は候補集合を変え得るため別結果にする。

動的幅では、完全同値な内部変更でも速度差が後続幅を変える。固定幅の同値性と、同じ時間予算での品質分布を分ける。
浮動小数 Score は NaN を禁止するか total order を定義し、CPU、並列度、GPU FMA が変わっても比較規則を固定する。

## 合成 State の軸

### 候補列挙

| 軸 | 値の例 | 見たいもの |
|---|---:|---|
| ビーム幅 `W` | 16, 256, 4096, 65536 | 小幅の固定費と大幅の帯域・容量 |
| 分岐数 `B` | 2, 8, 64, 512 | 候補選択が支配的になる境界 |
| 一時採用比 `A/W` | 約1, 2, 8, 64 | 追い出し履歴によるハッシュ・arena の増大 |
| 重複率 | 0%, 1%, 25%, 90% | 重複排除の損益分岐 |
| 重複の位置 | 連続、遠方、同一親、別親 | probe と同一ハッシュ更新の挙動 |
| cross-turn dedup | 毎ターン破棄、全期間 exact、periodic、sliding | active `U` と履歴 `H` を分離 |
| `H/W` | 0, 1, 16, 256 以上 | exact 履歴の容量と window の意味論 |
| Score 分布 | ランダム、昇順、降順、同点多数 | 置換頻度、閾値枝刈り、同点処理 |
| Hash 分布 | 一様、低bit偏り、高bit偏り | identity hasher の契約と耐性 |
| 列挙 API | callback、vector fallback | threshold 利用と Action 一括構築の差 |

`A/W` は重要である。最終的な生存候補が同じ `W` 件でも、良い候補が徐々に現れる列では `A` が大きくなり、
現行のハッシュ表と可変ターン版の一時候補が最悪挙動を示す。

### データ型と State

| 軸 | 値の例 | 実装方法 |
|---|---:|---|
| Action サイズ `S_A` | 8, 32, 128, 512 byte | 固定長 payload と copy/move カウンタ |
| Score | `int32_t`, `int64_t`, `double` | selector の型依存を分離 |
| `try_op()` | 約10, 100, 1000 cycle | 固定回数の整数演算で校正 |
| `apply_op()` / `rollback()` | 約5, 50, 500 cycle | 可逆な合成更新で校正 |
| State サイズ | 16 byte, 256 byte, 4 KiB | copy と差分木の損益分岐 |
| 履歴 | off / on | `record_history` のコンパイル時除去を確認 |
| telemetry | none / detailed | clock、counter、ログ型要件の固定費 |
| 幅調整 | fixed / dynamic | 同値性と時間予算内品質を分離 |
| 最終 State | materialize off / on | 最終 rollback、再適用、State move の費用 |
| 閾値利用 | off / 強い early-out | exact threshold の価値を確認 |
| 完成候補 | なし、1件、多数改善 | finished path の反復 materialize を測る |

ダミーループがコンパイラに消されないよう、状態と候補値にデータ依存を持たせる。ただし乱数生成そのものが
支配しないよう、候補列は試行前に生成する。

### 木の形

固定深さ版では、次の値を独立に制御する。

- 生存葉の親種類数 `P`: 1、`W/16`、`W`
- 共有接頭辞長: 深い、中間、浅い
- `M_tour/W`: 約1、2、4、16以上
- `G/E_live`: 約1、2、8、64以上
- 各親から残る子数: 1中心、均等、多数
- 探索深さ: 32、256、4096
- `G / tour_id_count` と、tour が読み書きする ActionId byte 数
- Action 合成の成功率: 0%、50%、100%
- Action 合成後の primitive 長、payload サイズ、`compose()` コスト
- ghost 数 / 論理 slot 数

可変ターン版ではさらに次を加える。

- 展開密度 `K/L`: 1%、10%、50%、100%
- 平坦木の走査密度: `R/Q`、`R/K`、`Q/K`
- `target_turn - turn`: 1固定、狭い分布、長い裾
- 同時に存在する遷移先ターンプール数 `F`
- 総葉数 `L`、生存木サイズ `N`、平坦木 token 数 `Q`
- `max_turn` と active target 数を独立に変えた空ターン比
- pool ごとの occupancy、最大 entry、capacity

これにより、連続 Euler 列、明示木、対象葉カレンダー＋親鎖の損益分岐を測れる。

## 個別マイクロベンチマーク

### 1. Candidates

State や木を通さず、事前生成した `(score, hash, parent, action)` を投入する。

比較対象:

- 現行セグメント木＋現行 HashDict
- セグメント木の reset 全初期化 / build 時だけの初期化
- active `O(W)` ハッシュ＋定期再構築＋別の exact 履歴表
- indexed max-heap / tournament tree
- `2W` バッファ＋`nth_element()`
- 小整数 Score の histogram
- 重複排除 off / mixed hash / identity hash
- Action 値渡し / `push_lazy()` / 最終世代 block への直接保存
- target-turn pool の `16W` 相当固定容量 / occupancy grow / 共有複合 key 表

測定値:

- 候補1件当たり cycle、instructions、branch miss、L1/LLC miss
- 最大ハッシュ容量、平均 probe group 数、tombstone 数、再構築回数
- active `U`、履歴 `H`、pool ごとの entry/capacity、全 active pool の総 bucket 数
- Action の copy / move / construct / destroy 回数
- peak allocation bytes
- exact threshold を利用した場合の `try_op()` 回避数

候補列を全方式へ同じ順序で与える。完全同値方式では全スロットの `(hash, score, parent, tie ordinal)`、採否列、
追い出した slot、各 push 後の threshold を比較する。cross-turn 版は active table と履歴表の内容も世代ごとに比較する。

### 2. 差分木走査

候補選択を固定済みの親・子列に置き換え、木の移動だけを測る。

比較対象:

- 現行64 bit `tour` / `leaf`
- 32 bit slot-only `tour` / `leaf`
- 独立 `frontier_slot` 付きの depth ごとの direct parent 鎖
- `cand` からfrontierを導出する direct parent 鎖
- Compose の ghost 付き `tour`
- Radix の明示木 AoS / metadata と Action を分けた SoA
- 可変ターン版の平坦 PRE/POST 列
- 平坦 PRE/POST 列＋空ターン event jump
- dense / paged-dense turn calendar
- 可変ターン版の対象葉カレンダー＋親鎖（試作後）

測定値:

- ActionId の読取・コピー件数
- `apply_op()` / `rollback()` 件数
- 木メタデータ走査件数
- `M_tour`, `L_parent`, suffixごとの最大dependent chain
- `Q`, `R`, 書き出した token 数、event jump で省いた論理ターン数
- 1葉当たり cycle と LLC miss
- 1世代の一時メモリと保持メモリ
- compose 成功/失敗、ghost 率、合成 primitive 長、`compose()` の時間と allocation
- live Action slot `G`、tour ID 数、両者の比
- `E_live` と `G/E_live`

現行の `tour` は同一世代内で葉区間を概ね一方向に走査するため、比較では「LCA問合せ回数」だけでなく、
実際のメモリアクセスと必須の状態遷移回数を測る。

### 3. 統計・時刻計測

空に近い State と `B=1` を使い、次を個別に on/off する。

- `Timer::elapsed()`
- `BeamParam::timestamp()` / `timestamp_meta()`
- `width_hist`
- Compose の各種カウンタ
- `verbose`
- `record_history`
- `materialize_final_state`
- fixed / dynamic width

固定幅・ログなし専用経路の改善量を、探索本体が重いケースで希釈せず測る。

### 4. Action の保存と arena

固定深さ版では、候補 slot と世代 block の二重保存に対して次を比較する。

- 現行 Candidates から世代 block への move
- `push_lazy()` 後に世代 block へ move
- selector metadata と最終世代 Action block を分離し、採用時に直接構築

可変ターン版では Action サイズと生存期間を変え、次を比較する。

- 現行 `vector<Action>`＋free list
- 固定長チャンク arena
- Action と木メタデータを分離した arena

default/copy/move/assign/destruct、再確保、展開前コピー、peak bytes、fragmentation を記録する。Action は trivial 型と
非 trivial 型、move が `noexcept` の型とそうでない型を分ける。move-only 対応は将来契約として compile test を分ける。

## 統合ベンチマーク

マイクロベンチだけで勝った案を採用しない。少なくとも次の代表クラスを用意する。

1. 小 State・安い差分・低重複: ライブラリ固定費が支配
2. 大 Action・高棄却: 遅延保存の上限効果を見る
3. 高重複: ハッシュの利益が大きい
4. 低重複・高価なハッシュ: no-dedup policy の利益を見る
5. 深い一本道: Compose / Radix の path contraction を見る
6. 浅い共有接頭辞: 連続 tour の帯域を見る
7. 可変ターン・疎な展開: `K << L` かつ `Q/K` または `R/K` が大きい構造案を見る
8. 可変ターン・密な展開: 現行平坦列が有利な対照群
9. State 全コピーが小さいケース: naive backend が差分木より速い境界を見る
10. 大きな `W*B` と高価な評価: exact CPU generation parallel の成立域を見る
11. 規則的な SoA State と大きな batch: sibling / full ParentBatch の成立域を見る
12. 複数 seed で品質分散があるケース: multi-start の総CPU時間と品質分布を見る
13. device resident な State: GPU batch の転送を含む損益分岐を見る

各ケースで探索品質に意味を持たせる必要はない。完全同値または集合同値を主張する方式は、比較可能な
deterministic モードを持つ。探索方針変更案は同一時間での最終スコア分布も別途測り、単純な nodes/sec と
混同しない。

naive 版は他の backend と完成解発見後の停止条件が異なる。比較ケースで完成候補を生成しないか、
`stop_on_finished_generation` / `continue_after_finished` を明示して揃える。Compose と Radix は Action の区切りが
変わるため、Action vector の直接一致だけでなく、primitive へ正規化した列または最終 State checksum で比較する。

## 実行条件

- release build、`-O3 -DNDEBUG -march=native` を基準とし、portable build も別系列にする
- GCC と Clang を最低1版ずつ測る
- CPU governor、SMT、コア固定、NUMA node を記録する
- warm-up 後に複数回実行し、中央値、p10、p90を保存する
- wall time に加えて `perf stat` の cycles、instructions、branches、branch-misses、cache-misses を保存する
- peak RSS は別プロセス実行で取得し、allocator と標準ライブラリを記録する
- 並列版は1, 2, 4, ... threads と幅を同時に振り、速度向上率だけでなく効率とメモリ増加を出す
- 固定幅の同値試験と、動的幅の同一時間予算試験を別系列にする
- seed、同点規則、NaN 方針、停止 policy、final State の有無を固定して記録する
- nested parallel は既定で無効にし、multi-start 数と run 内 thread 数の積を記録する
- GPU は host/device resident の境界を固定し、転送込みと kernel-only を分ける

短い測定はノイズを受けやすいため、各ケースを最低数百 ms になる反復数へ自動調整する。ベンチマーク中に
ファイルI/O、ログ、乱数シード生成を含めない。

## 採用判定

完全同値の低リスク案は、代表ケースのどこかだけでなく、軽い State 群の中央値で改善し、重大な退行と
メモリ増加がないことを条件にする。バックエンドやポリシーとして追加する案は、勝つ領域を設定値または
型特性で利用者が説明可能に選べることを条件にする。

期待効果の全体優先順位は `performance_audit.md` を正本とする。最上位群は active hash の `O(W)` 化、turn 版の
dirty slot、Action 遅延・直接保存、target-turn pool の occupancy 対応である。次は効果順ではなく、依存関係と
実装リスクが低い順に一つずつ測る。

1. 現行値と計測器を固定
2. セグメント木の全初期化を build 時へ移す
3. 標準版、Compose 版、naive 版を `push_lazy()` 相当へ移す
4. target-turn pool を実 occupancy に応じた容量へ変える
5. hash locator の二重混合を除き、active 表と exact 履歴表を分離して `O(W)` 化する
6. turn 版を dead stable compaction、dirty slot、pending Action の順に変更する
7. 標準版と Compose 版の Action を最終世代 block へ直接保存し、hot metadata を分ける
8. 親 grouping、固定幅 telemetry-none、naive の低リスク複製削減を個別に測る
9. 32 bit slot-only tour を試し、その後に parent-slot、Radix SoA、event jump を比較する
10. selector policy と dense / paged / sparse turn calendar を条件別 backend として比較する
11. exact CPU parallel、BatchState、multi-start を別 entry point として比較する

複数案をまとめて入れず、各段階で結果とコミットを対応させる。

## 結果の保存形式

各実験は次を1行にした CSV/JSON と、人間向け Markdown の両方を残す。列挙しきれない backend 固有値は、
`config_json` と `metrics_json` に安定した key で保存する。

```text
commit, compiler, compiler_version, flags, cpu, numa, allocator, backend, selector, hash_policy,
calendar_policy, seed, W, B, C, A_over_W, U, H, P, F, D, N, Q, R, M_tour, L_parent,
E_live, G_over_E_live, duplicate_rate,
action_bytes, state_bytes, try_cycles, apply_cycles, K_over_L, Q_over_K, R_over_Q, R_over_K,
threads, runs, adjust_width, telemetry, history, materialize_final_state, stop_policy,
elapsed_ns, cpu_ns, cycles, instructions, branch_misses, cache_misses, peak_rss,
score, status, turns_done, result_digest, state_digest, trace_digest, config_json, metrics_json
```

`result_digest` は候補採否列、各世代の `(score, hash, parent ordinal, enumeration ordinal)`、threshold 列、最終
Action 列を意味論レベルに応じてハッシュ化する。`state_digest` は各葉と最終 State、`trace_digest` は try、apply、
rollback、enumerate、RNG 消費順を検証する。Compose/Radix は primitive 正規化列も残す。

`metrics_json` には少なくとも hash capacity/probe/rebuild、Action/State copy/move、apply/rollback、allocation、
compose/ghost、pool occupancy を入れる。並列版では speedup、parallel efficiency、barrier、追加 `try_op()`、
worker-local memory、false sharing、NUMA remote access を加える。multi-start は各 run の seed、score、総CPU時間を、
GPU は H2D/D2H byte、kernel/launch時間、device memory と occupancy を記録する。
