# ビームサーチ候補パイプライン性能監査

## 目的と範囲

候補の上位 `W` 件管理、重複排除、`Action` の保管、メモリ配置、計測などの定数倍を、特定問題ではなく
汎用ライブラリとして検討する。対象は次のファイルで、性能を変えるソース修正やベンチマークはまだ行っていない。

- `titan_cpplib/ahc/beam_search/candidates.cpp`
- `titan_cpplib/ahc/beam_search/beam_search.cpp`
- `titan_cpplib/ahc/beam_search/beam_search_compose.cpp`
- `titan_cpplib/ahc/beam_search/beam_search_radix.cpp`
- `titan_cpplib/ahc/beam_search/beam_search_turn.cpp`
- `titan_cpplib/ahc/beam_search/beam_param.cpp`
- `titan_cpplib/ds/hash_dict.cpp`
- `review/ahc_beam_search.md`
- `review/ahc_beam_search_constant_factor_optimization.md`

`old/` は対象外とする。

以下の記号を使う。

- `W`: ある候補集合のビーム幅
- `P`: 1 世代またはメタターンの候補に現れる異なる親の数
- `F`: 可変ターン版で同時に active な `target_turn` 別候補プール数
- `A`: 1 世代またはメタターン中の一時採用イベント数。同じ slot の再置換も数える
- `U`: ハッシュ表へ挿入された異なるキー数
- `H`: ターンをまたぐ重複排除で保持する異なるキー数
- `C`: 1世代で列挙した全候補数
- `N`: 生存探索木のノード数
- `G`: 未解放の世代 block に確保済みの Action slot 総数
- `Q`: PRE/POST/leaf 形式の平坦木の全 token 数
- `R`: スキップ後に実際に読んだ平坦木 token 数
- `D`: 探索深さ
- `S_A`: `sizeof(Action)` byte。Action 自身が所有するヒープ領域は含まない

`try_op()` が重い場合は State 側が支配的になるが、軽い State、高分岐、大きい `W`、大きい Action では、
この文書で扱う候補パイプラインが支配的になり得る。

## 現行実装の事実

### 共通 Candidates はオンライン exact top-W である

`candidates.cpp:19-190` の `Candidates` は、次の性質を持つ。

- `score >= current_worst` をハッシュ探索前に落とす (`candidates.cpp:62-65`, `103-106`)。
- 同一ハッシュは、より小さい score のときだけ同じ候補スロットを更新する (`71-79`, `112-123`)。
- 幅へ達した時点で tournament tree 相当のセグメント木を一度構築する (`42-49`, `86-89`)。
- 以後は最悪候補のスロットを置き換え、根を常に正確な threshold として返す (`92-98`)。
- score 更新は悪化せず、すべて decrease-key である。

したがって、列挙途中の `submit.threshold()` を常に正確に保つ点では現行方式は強い。単純に全候補を貯めて
`nth_element()` する方式へ置き換えるべきではない。

一方、セグメント木の `s` は過去最大幅に対して単調増加し (`candidates.cpp:155-157`)、毎 reset で `2*s`
要素を全初期化する (`158-161`)。動的幅が小さくなっても過去最大幅分を埋め続ける。可変ターン版の内部
Candidates は build 時に未使用葉だけを初期化しており (`beam_search_turn.cpp:159-167`)、この改善は共通版へ
まだ反映されていない。

### 候補管理実装が三重化している

ほぼ同じ top-W とハッシュ管理が次の三か所にある。

- Action を直接持つ `Candidates` (`candidates.cpp:19-190`)
- ActionId を持つが現在未使用の `CandidatesFlat` (`candidates.cpp:210-334`)
- 可変ターン版の内部 `Candidates` (`beam_search_turn.cpp:138-245`)

`CandidatesFlat` は現行エンジンから参照されていない。共通版と Flat 版には同じ容量判定と全初期化が複製され、
可変ターン版だけに一部改善が入っている。性能修正を三実装へ別々に入れるより、score/hash/slot の採否だけを行う
`CandidateSelector` と payload 保管を分け、既存クラスを薄い互換ラッパーにする方が安全である。

また、共通 `Candidates` のテンプレート引数 `State` は実装内で使われていない。異なる State ごとに不要な別
インスタンスを生成するため、実行時よりもコンパイル時間・バイナリサイズ・I-cache の観点で除去候補となる。

### 共通 HashDict は active W ではなく distinct key 数 U まで増える

共通 Candidates は最悪候補を追い出すと、古いキーの value を `-1` にするだけで control byte と key を残す
(`candidates.cpp:92-97`, `139-147`)。後で同じ hash が来れば再利用できるが、異なる hash の置換が続くと、
`HashDict::size` は最終生存 `W` ではなく distinct key 数 `U` に近づく。1世代では `U <= A`
なので、一時採用が多いと `W` を大きく超える。

`HashDict` は load factor が 1/2 を超えると倍増する (`hash_dict.cpp:164-175`)。したがって容量 `cap` は通常
`2U <= cap < 4U` 程度となり、共通 Candidates の表は `O(U)` である。毎世代 clear する場合は
`U <= A`、clear 間隔内で蓄積する場合は概ね `U <= W + sum_t A_t` であり、現在の1世代の `A` だけでは
上限を表せない。`clear()` は control byte を埋めるだけで容量を縮めない
(`hash_dict.cpp:230-234`)。一度の大きな高水位後も
常駐容量は残る。大きな control 配列の fill を毎世代払うのは `clear_hash_every_turn=true` のときであり、
periodic window では定期破棄ターンだけである。`false` かつ window 0 では fill はない代わりに、一時採用キーも
含む clear 以来の distinct key 数へ表が増え続ける。

`clear_hash_every_turn=false` では、最終生存候補だけを `-2` にして次世代以降の予約済みキーとする
(`candidates.cpp:166-175`)。同じ表に、現在の上位 W、今世代の脱落キー、過去世代 survivor の三種類を混在させる
ことが、容量を制御しにくい主因である。この場合の物理表は active `W` と履歴 `H` だけでなく、不活性の
一時採用キーも保持する。従って、分離前の容量上限を単純な `O(W + H)` とは表せない。

### HashDict の初期容量指定は直感より 2 倍以上大きい

`HashDict(n)` は `cap >= 2*n` となる 2 冪まで拡張する (`hash_dict.cpp:76-86`)。つまり `n` は bucket 数でなく
想定要素数である。

共通 Candidates の

```cpp
if (func.inner_len() == 1) func = HashDict<int>(beam_width * 8);
```

は、既定容量が 16 なので成立しない (`candidates.cpp:176-178`, `322-324`)。過去レビューの指摘は現行にも残る。
ただし、条件だけを直して `HashDict(8W)` を実行すると、容量は最低 `16W`、最大でほぼ `32W` になり、今度は
過剰確保になる。条件修正と容量設計は分けずに行う必要がある。

`HashDict<int>` の三配列は、概算で control 1 byte、key 8 byte、value 4 byte、合計 13 byte/bucket である。
したがって `HashDict(8W)` だけで最低約 `208W` byte、2 冪の丸め次第では約 `416W` byteを使う。

### 可変ターン版は候補プールごとに最低約 16W bucket を確保する

可変ターン版の内部 Candidates は、`inner_len() < 8W` なら `HashDict<int, false>(8W)` を構築する
(`beam_search_turn.cpp:225-241`)。これは意図どおり条件が成立するが、1 プール当たり最低約 `208W` byte の
ハッシュ表になる。さらに、概算で次を持つ。

- `next_beam`: ScoreType が 8 byte の場合、多くの ABI で約 `24W` byte
- `hashidx`: HashType が 64 bit なら `8W` byte
- segtree: `pair<ScoreType,int>` が 16 byte なら約 `32W` から `64W` byte

合計は Action arena を除いても、1 プール当たり最低およそ `272W` byte となる。`cands_pool` は検索開始時に
破棄されず、過去に作ったプールを free list へ戻して再利用する (`beam_search_turn.cpp:681-686`)。したがって高水位は
概ね `O(F_peak W)` のまま残る。`F_peak` は検索中の `F` の最大値である。多くの target_turn に
少数候補だけが飛ぶケースでは特に過剰である。

`clear_hash_every_turn=false` の global `seen_hash` は、初期ヒントを
`max(2^14, 2*W*max_turn)` とする (`beam_search_turn.cpp:697-704`)。HashDict ctor がさらに 2 倍以上にするため、
bucket 数は最低およそ `4*W*max_turn` である。value が `pair<ScoreType,int>` で 16 byte なら、1 bucket は概算
25 byte、最低約 `100*W*max_turn` byte になる。これは上限ではなく初期容量であり、異なる一時採用 hash が
さらに多ければ再構築する。

全期間の exact 重複排除を維持するなら、異なる履歴 hash 数 `H` に比例するメモリは理論上避けられない。
したがって「必ず O(W)」にできるのは現世代の active table であり、履歴表には window、明示的上限、近似化の
いずれかが必要になる。

### HashDict は新規挿入時に同じ hash を再計算する

`get_pos()` は splitmix 系の混合を一度行う (`hash_dict.cpp:28-35`, `89-119`)。新規キーの `inner_set()` は
control byte を求めるため、同じ key をもう一度混合する (`164-170`)。共通 Candidates で最悪候補を置き換える
経路は、概ね次の 3 回の混合・探索を行う。

1. 新 hash の `get_pos`
2. 旧 hash に `-1` を設定する `set`
3. 新 hash の `inner_set` 内で h2 を再計算

位置と h2 を返す locator、および候補スロットに保存した表内位置を使えば、通常経路は新 hash の 1 回だけに
できる。ただし HashDict が自動 rebuild すると保存位置が無効になるため、固定容量と一括再構築を同時に設計する
必要がある。

`USE_HASH_FUNC=false` でも constructor は `random_device`、`mt19937` を使って未使用の seed を作る
(`hash_dict.cpp:38-43`, `66-86`)。可変ターン版はプールごとにこのコストを払うため、`if constexpr` で省ける。

### Action の保存状況

標準版と Compose 版の callback 経路は Action を値渡しで `push()` する
(`beam_search.cpp:165-173`, `beam_search_compose.cpp:262-270`)。関数に入る前にコピーが生じるため、threshold や
重複で棄却される候補にもコピーが起きる。vector fallback も `move(action)` を引数へ作ってから採否判定する
(`beam_search.cpp:290-297`, `403-411`)。

採用後も、Candidates の Action を世代 block へもう一度 move する
(`beam_search.cpp:83-99`, `beam_search_compose.cpp:169-190`)。つまり Action は候補スロットと最終世代 block の
二段階に保存される。

Radix 版は `push_lazy()` を使い、採用時だけ Action をコピーする (`beam_search_radix.cpp:144-159`)。親ごとの
counting bucket と小区間 sort も実装済みである (`209-257`)。過去レビューのこの二提案は Radix 版では実装済み。

可変ターン版は Action 本体の fill を採用後まで遅らせているが、ActionId の予約は採否前である
(`beam_search_turn.cpp:343-360`)。より重要なのは、一度採用後に追い出された候補も `new_candidates` と arena に
木更新まで残る点である。dead 判定と release は `update_tree()` 内 (`447-459`, `499-514`) で行われるが、sort は
その前 (`797-817`) にある。したがって sort と `new_candidates` のサイズは最終生存数でなく `A` に比例する。
arena では既存の live node `N` に今回の未反映の採用分が重なり、空き slot がなければ一時的に概ね
`N + A` slot まで必要になる。解放後も `action_pool` の size/capacity は縮まず、過去の高水位が残る。

### Action arena と参照安定性

可変ターン版は `vector<Action> action_pool` と free slot を使う (`beam_search_turn.cpp:56-79`)。候補追加による
再確保で参照が無効になるため、展開対象 Action を値コピーしてから enumerate する (`420-424`)。このコピーは
正当性のために必要であり、単純に参照へ変えてはいけない。

標準版と Compose 版は世代ごとの `vector<Action>` と slab 再利用を持つ (`beam_search.cpp:39-48`, `67-80`)。
これは世代単位の再利用として実装済みだが、ActionId は 64 bit で、候補の一時 Action との二重保存は残る。

Radix 版の `Node` は Action と parent/sibling/score を AoS で持つ (`beam_search_radix.cpp:23-35`)。Action が
小さい場合は下り・上りで Action も使うため局所性がよい。一方、surgery と部分木 score 再計算はメタデータだけを
走査する (`209-303`) ので、大きい Action では stride と cache line 消費が悪化する。

### 不要な hot-path 計測が残る

固定幅でも標準版は世代ごとに複数回 `elapsed()` を呼び (`beam_search.cpp:327-333`, `459`)、`timestamp()` が
統計と `width_hist` を更新する (`beam_param.cpp:102-108`)。可変ターン版は展開・更新を個別計測するため、active
meta-turn ごとにさらに多い (`beam_search_turn.cpp:739-742`, `797-822`, `830-831`)。

`explored_per_turn` は verbose=false でも候補ごとに increment される。Compose 版の apply/rollback/ghost/tour
カウンタも常時更新され (`beam_search_compose.cpp:39-55`, `238-241`, `476-490`, `578-589`)、出力するのは verbose
時だけである。軽い State では、この一候補・一辺ごとの increment は clock 呼び出しより重要になり得る。

## 過去提案の実装状況

`review/ahc_beam_search_constant_factor_optimization.md` の主な提案を現行と照合した。

| 過去提案 | 現行状況 | 主な根拠 |
|---|---|---|
| active hash 表を O(W) に制限 | 未実装 | 追い出しを value=-1 とし、U/A に比例して grow |
| 脱落 `new_candidates` を sort 前に除去 | 未実装 | sort が `update_tree()` より先 |
| Action を採用後だけ保存 | Radix は実装、turn は一部、base/compose は未実装 | `push_lazy` は Radix のみ |
| Action の候補→世代 block 二重保存を除去 | 未実装 | base/compose の `finalize_generation()` で move |
| 親番号 counting bucket | Radix は実装、turn は親連続区間 sort、base/compose は未実装 | Radix `surgery()` |
| 固定長 block Action arena | base/compose は世代 block、turn は未実装 | turn は単一 vector |
| record_history=false で node_id を除去 | 未実装 | `BeamCandidate`, `CandIdx` に常在 |
| ActionId の 32 bit 化 | turn は int、base/compose は uint64_t | `SLOT_BITS=24` |
| segtree 全初期化を除去 | turn 内部のみ実装 | 共通 reset は全 fill |
| 固定幅・統計なし専用経路 | 未実装 | clock と timestamp は常時実行 |
| hash 再混合の選択 | HashDict には template がある。turn は false 固定 | base は true、turn は false |
| 現世代表と全ターン表の分離 | turn のみ分離。base 系は混在 | `seen_hash` 対 `-2` marker |

## 優先順位付き改善案

### P0-1: active 候補 hash を O(W) に制限する

第一候補は、候補選択専用 table を次の構造にすることである。

1. 容量を 4W 前後の 2 冪に固定する。
2. 候補スロットごとに table 位置を保存し、追い出し時は再探索せず value/control を位置指定で無効化する。
3. 使用済み位置が約 2W へ達したら、最終 active W 件だけを連続して再挿入する。
4. locator は `{position, found, h2}` を返し、新規挿入時に hash を再混合しない。
5. `clear_hash_every_turn=false` の履歴 survivor は別 table へ移す。

単純に control byte を EMPTY にすると probe chain を途中で切るため正しくない。`DELETED` control を導入して
最初の tombstone を挿入先候補として記憶するか、value=-1 のまま定期再構築する。後者は現行 SIMD control の
形を保ちやすい。

- 期待効果が大きい条件: `A/W` が大きい、高分岐、世代数が多い、過去の一世代だけ A が突出する。
- 悪化条件: `A` がほぼ `W`、小さい W。位置配列と再構築判定が余分になる。
- 結果再現性: strict `<`、`>=`、`-2` の昇格時点を保てば固定幅では同じ。hash table の配置順は探索順に非公開。
- メモリ上限: active table はおよそ 4W bucket。履歴 table は別途 O(H)。window なしの exact H は上限なし。
- API 互換性: BeamSearch 内部は維持可能。汎用 HashDict へ危険な位置 API を出すより専用 selector 内に閉じる。
- 実装難度: 中から高。rebuild 後の位置更新と periodic clear の意味論を重点検証する。
- 検証指標: `A/W`, used/active/tombstone 数、平均/最大 probe group、rebuild 回数、clear byte 数、peak cap/RSS。

注意点として、base 系の履歴表は現行どおり最終 survivor だけを昇格させれば `H <= W*turns` となる。一方、turn
版の global seen は一度ローカル候補へ採用された hash を即時登録する (`beam_search_turn.cpp:344-353`)。現行の
意味論を完全に維持するなら H は一時採用 distinct hash 数に比例し、final survivor だけへ変更してはいけない。

### P0-2: 共通 Candidates の Action を遅延保存し、最終 block へ直接置く

短期には base/compose の callback 経路を既存 `push_lazy()` へ変える。vector fallback では採用時に
`std::move(action)` する lambda を使える。これだけで threshold/hash で棄却された Action のコピー・move を除ける。

次段階では、世代 Action block を列挙前に W slot 用意し、candidate slot と Action slot を一致させる。selector は
score/hash/parent/node_id の hot metadata だけを持ち、採用時に最終 block の該当 slotへ Action を直接構築する。
`finalize_generation()` は metadata を整えるだけとなり、W 件の二度目の move と一時 Action 配列を除ける。

- 期待効果が大きい条件: 大きい/non-trivial Action、棄却率が高い、Action move/代入が所有メモリを処理する。
- 悪化条件: 8 byte 程度の trivially copyable Action。lambda/invoke が inline されなければ差が消える。
- 結果再現性: Action が通常の値型なら同じ。copy/move の副作用に依存する型はライブラリ契約外と明記する。
- メモリ上限: Action 本体を W slot に限定。現行の候補 W + 世代 W の一時二重保持を W へ近づける。
- API 互換性: `Candidates::next_beam[i].action` の公開利用があると破壊的。既存 wrapper と accessor で移行する。
- 実装難度: push_lazy 化は低、直接 block 保存は中。
- 検証指標: Action の default/copy/move/assign/destruct 回数、候補当たり cycle、candidate block peak bytes。

### P0-3: turn 版は「一時採用イベント」でなく「dirty 最終 slot」を持つ

sort 前の stable compaction は最小変更として有効だが、Action slot は既に各採用イベントで予約済みである。
従って sort 対象の圧縮だけでは、今回の `A` と既存の live node `N` が重なる arena 高水位を解決しない。
より良い構造は、各 target-turn Candidates slot に current meta-turn の dirty stamp と pending ActionId を
持たせる方式である。

- その slot を今 meta-turn で初めて変更するときだけ pending Action slot を確保する。
- 同じ slot が再び置換されたら pending Action を上書きし、新しい ActionId を増やさない。
- 以前から tree にある ActionId を置換した場合だけ、旧 ID を tree 更新まで deferred-release に置く。
- 列挙後は dirty slot の最終内容だけを `new_candidates` として集める。

これにより、一時採用イベント数 A でなく、最終的に変更された slot 数に sort と新 Action 保管を制限できる。
単純な即時 release は危険である。旧 ID はまだ tree にあり、finished が見つかって `get_result()` が tree を読む
場合や、残りの DFS が参照する場合がある。

pending ID の寿命は「slot の初回 dirty 化から `update_tree()` の所有権確定まで」とする。最終生存なら
tree へ所有権を渡し、非生存なら tree、candidate pool、dirty list の全参照を消してから解放する。
history の node ID と再利用可能な ActionId は同じ寿命とみなさない。

- 期待効果が大きい条件: `A/final_survivors` が大きい、同じ worst slot が何度も更新される、大きい Action。
- 悪化条件: ほぼ全採用が最終生存。stamp/dirty list/deferred list が余分になる。
- 結果再現性: 最終候補集合は同じ。列挙 ordinal を保存し、親・score・ordinal の tie 規則を固定しないと順序は変わる。
- メモリ上限: 新 pending Action を概ね dirty slot 数へ制限。既存の live tree は別途 `O(N)`。
  構造上の粗い上限は `O((sum_t W_t)D)`、active pool が `F` 個で各 `W_t <= W` なら `O(FWD)`。
  arena の capacity はさらに過去の高水位を保持する。
- API 互換性: turn エンジン内部で完結。record_history=true は ActionId 再利用と履歴 node id を分離する。
- 実装難度: 高。tree 所有 ID、pending ID、履歴 ID の寿命を明示したテストが必要。
- 検証指標: A、dirty slot 数、deferred ID 数、action_pool size/capacity、sort 要素数、peak RSS。

### P0-4: turn 版の `HashDict(8W)` を止め、target pool の疎性へ適応する

条件だけ直す案ではなく、ctor の意味に合わせて少なくとも `HashDict(W)` 程度まで下げる。さらに多くの
target_turn に少数候補が散る場合は、小さく開始して実績 high-water まで grow する方がよい。候補 table を共有し、
key を `(target_turn, hash)` とする案は `F` 個分の最低容量をなくせるが、複合 key と削除管理が増えるため
比較対象とする。

- 期待効果が大きい条件: `F` が大きい、各 target pool の entry が W よりかなり小さい、メモリ制約が厳しい。
- 悪化条件: すべての pool が即 W まで埋まる場合。段階 grow の再hashが増える。
- 結果再現性: 容量・配置だけなら同じ。OOM/時間調整以外で候補順は変わらない。
- メモリ上限: 個別表なら各 active pool O(actual entries) から O(W)、共有表なら全 active slot 合計に比例。
- API 互換性: 内部変更。`seen_hash_capacity_hint` の単位を「想定要素数」と明記する。
- 実装難度: 容量縮小は低、共有表は高。
- 検証指標: `F`、pool ごとの max entry/cap、総 bucket 数、再hash時間、RSS。

### P1-1: selector をポリシー化し、現行 tournament tree を既定として残す

上位 W の方式は一律に置き換えず、条件に応じて選ぶ。

| 方式 | online exact threshold | 時間 | 追加メモリ | 向く条件 | 主な欠点 |
|---|---:|---:|---:|---|---|
| 現行 tournament tree | 可 | update O(log W) | 2s 個の score/index | 汎用、枝刈り threshold が重要 | pair の余白、全初期化 |
| indexed max-heap | 可 | decrease O(log W) | heap+position 約2W index | 大 W、segtree が cache を圧迫 | 1段で子を複数比較、tie 再現が必要 |
| 小 W の線形 scan | 可 | accepted update O(W) | ほぼなし | W が 8～16 程度、採用が少ない | W や採用率が上がると急激に悪化 |
| buffer + periodic select | flush 間は緩い | 償却 O(C) 期待 | W～cW | threshold 利用が弱い、batch 向き | 追加 try_op、順序・tie が変化 |
| `nth_element` | 不可 | 平均 O(C) | 全候補 C | 全候補を既に持つ、W/C が大きい | O(C) Action/metadata、再現性が弱い |
| `partial_sort`/heap select | 不可 | O(C log W) | 全候補 C | batch API との互換 | online heap より保存量が大きい |
| integer radix select | 通常不可 | O(passes*C) | C + work | 32/64 bit整数、非常に大きい C | generic Score 不可、threshold が遅れる |
| bounded-score bucket | 可にできる | 期待 O(1) | score range `B_score` | 小さい既知整数範囲 | `B_score` が広いと cache/memory 悪化 |
| monotone max radix heap | 可 | bit幅に対し償却 | W + stale | 整数score、大W、worst単調減少 | version/stale再構築、実装複雑 |

現行 tournament tree は worst 置換で各 level 1 比較で済み、CPU 時間では heap より有利な可能性が高い。
indexed heap の第一目的は、`pair<ScoreType,int>` を `2s` 個持つメモリを減らすことにある。4-ary heap は依存段数を
減らす一方で比較数を増やすため、binary/4-ary/tournament を実測する。

batch 方式は `submit.threshold()` が緩くなり、State 側の枝刈り量まで変える。最終 top-W が同じでも候補列挙数、
乱数消費、Action の書き込みが変わり得るため、既定にはしない。

共通の検証条件は次のとおり。

- 同点時に現行 tournament tree は score 比較で右側を選ぶ。方式変更時に evict 対象が変わり得る。
- 完全再現を求めるなら候補へ列挙 ordinal を付け、比較キーを仕様化する。
- 固定幅では同じ comparator なら再現可能。動的幅では高速化自体が時刻観測を変えるため bit-identical にはならない。
- 測るものは comparisons、dependent levels、writes、threshold reject率、accepted replacement率、L1/LLC miss。

### P1-2: 共通 segtree の初期化を build 時だけにする

reset では `entry=0, is_built=false` だけにし、幅へ達したときに `[s, s+entry)` を書き、残りの葉を `-INF` で
埋めて内部ノードを構築する。可変ターン版の方式を共通化する。また `s=bit_ceil(current_w)` と毎回設定し、
確保容量だけを過去最大のまま残せば、動的幅が縮んだときも current W 分だけ処理できる。

- 期待効果が大きい条件: W が大きい、候補不足で W へ達しない、動的幅が過去最大より小さくなる。
- 悪化条件: 毎世代必ず即 W へ達する場合でも同等。build 内 fill 位置が変わるだけ。
- 結果再現性: 未使用葉を必ず初期化すれば同じ。
- メモリ上限: 変わらない。処理範囲は current W へ縮む。
- API 互換性: 完全に内部。
- 実装難度: 低。
- 検証指標: reset/build の書込 byte 数、W 未達世代率、過去最大 W/current W。

### P1-3: 親 grouping を counting scatter にする

base/compose の `parent_leaf` は 0 から始まる密な整数である。Radix と同様、count、prefix sum、scatter の O(W)
grouping を使い、親区間だけ score sort する。子数 2 は一比較、少数は insertion sort、多数だけ `std::sort` とする。

turn 版は tree 順に enumerate するため `new_candidates` が既に親ごとの連続区間になり、現行の区間 sort
(`beam_search_turn.cpp:809-816`) は合理的である。まず dead compaction/dirty slot 化を優先する。

- 期待効果が大きい条件: W が大きく、親当たり子数が小さい。全体 `W log W` の比較が無駄になる場合。
- 悪化条件: 親数が非常に大きく大半が空、または一親に W 件集中。count 配列 zero-fill が増える。
- 結果再現性: score tie の ordinal を仕様化しない限り std::sort 変更で順序が変わり得る。
- メモリ上限: 親 ID を圧縮できれば count `O(P)`、items `O(W)`。密な ID 範囲を直接使うなら
  count 配列は `O(L)` になる。Radix は既にこの領域を持つ。
- API 互換性: 内部。
- 実装難度: 低から中。Radix 実装を共通 helper 化できる。
- 検証指標: 比較数、親当たり子数 histogram、sort時間、scatter書込量。

### P1-4: hot metadata と Action を SoA 化する

共通 candidate は `parent, score, Action, node_id` の AoS (`candidates.cpp:11-17`) である。score scan、hash 重複、
segtree build の際に Action は不要であり、Action が大きいほど stride が悪い。次へ分ける。

- hot: score、parent、active hash/table position
- optional: record_history=true の node_id
- cold: Action storage

`record_history=false` でも node_id は BeamCandidate と base/compose の CandIdx に残る。部分特殊化で完全に除く。
Compose の `action_count` は現在世代番号と同じなので、ActionId の generation から導ける限り重複保持しない。

Radix Node は、`NodeMeta` と `Action edge[]` を分ける版を比較する。大きい Action で surgery/recompute が多い場合は
SoA が有利、小さい Action で DFS apply/rollback が支配する場合は AoS が有利なので policy または閾値が必要である。

- 期待効果が大きい条件: `S_A >= 32`、大 W/N、metadata-only scan が多い、record_history=false。
- 悪化条件: 小さい Action、DFS が常に meta と edge の両方を使う。二配列アクセスが増える。
- 結果再現性: 配置だけなら同じ。
- メモリ上限: padding と false-history field を削減。Action 所有メモリは変わらない。
- API 互換性: 公開 `next_beam` を維持する facade が必要。Radix Node は private なので影響なし。
- 実装難度: 中。
- 検証指標: `sizeof`、世代当たり metadata 読込 byte、L1/LLC miss、cycles/apply と cycles/surgery。

### P1-5: turn 版を安定アドレスの chunk arena にする

2 冪長の raw-storage block を個別確保し、ActionId を block/offset または再利用可能な 32 bit slot とする。block list
自体が再確保されても Action は移動しない。これにより `beam_search_turn.cpp:420` の防御コピーなしで、同じ Action
参照を apply、enumerate、rollback に渡せる。

`deque<Action>` は簡単な比較対象だが、block サイズや ID、free slot、配置構築を制御しにくい。汎用ライブラリでは
custom chunk arena の方が Action の非 trivial destructor と lifetime を明示しやすい。

- 期待効果が大きい条件: 大きい Action、展開葉が多い、leaf Action copy が目立つ、vector 再確保が多い。
- 悪化条件: 小 Action。act ごとに block pointer と offset の二段参照が増える。
- 結果再現性: 参照安定性以外の順序を変えなければ同じ。
- メモリ上限: 使用 slot は現行で概ね `N + A`、P0-3 後は `N + dirty_slots`。確保量はその過去最大に
  最大1 block の余りを加えた量で、live 数が下がっても自動では縮まない。
- API 互換性: ActionId は private。Action が通常の destructible/movable 型なら維持可能。
- 実装難度: 高。placement construction、例外、安全な再利用、検索終了時 destructor が必要。
- 検証指標: Action copy/move、block数、未使用末尾slot、act lookup cycle、peak RSS。

### P1-6: 固定幅・統計なしの search_impl を分ける

公開 `search(BeamParam&, verbose)` は維持し、入口で一度だけ次へ dispatch する。

- `search_impl<adjust_width, collect_stats, collect_counters, use_global_seen>`
- record_history と materialize_final_state は現行どおり compile-time
- Radix の `monotone_skip` も DFS 入口で template dispatch

固定幅・非 verbose では世代途中の clock、`width_hist.push_back`、`explored_per_turn`、Compose counters を除く。
Result の elapsed_ms のため、検索開始と終了の二回だけ計測する。現在 BeamParam の累積統計は検索後に利用できるため、
既定互換経路で残し、明示的 fast policy で無効化するか、固定幅で O(1) に合成できる統計だけ終了時に反映する。

- 期待効果が大きい条件: try_op/apply が非常に軽い、小 W、多ターン、Compose の ghost slot が多い。
- 悪化条件: State が重い場合は code size 増加だけが残る。
- 結果再現性: 固定幅なら順序は同じ。動的幅では計測を外せない。
- メモリ上限: width_hist を取らなければ O(D) を除去。
- API 互換性: policy の既定を現行互換にする。BeamParam の検索後統計が不要という opt-in が必要。
- 実装難度: 中。テンプレート膨張と命令 cache を測る。
- 検証指標: clock call数、increment数、instructions/candidate、binary text size、I-cache miss。

### P2-1: ActionId と tree metadata を狭くする

base/compose の ActionId は uint64_t で、tour、trace、CandIdx が 8 byte ID を持つ
(`beam_search.cpp:39-60`, `beam_search_compose.cpp:57-67`)。一般的な active Action 数が 2^32 未満なら、再利用 arena の
32 bit ID で十分であり、tour/trace の帯域を半減できる。現在の `gen << 24 | slot` を単純に uint32_t 化すると
generation が 8 bit に制限されるため、固定 bit 割当ではなく active slot arena、または制約を template parameter と
runtime assert で管理する。

turn の TreeNode は 4 個の int で通常16 byte (`beam_search_turn.cpp:37-43`)。kind は leaf/PRE/POST の3種類だけなので、
aid 上位 bit などへ pack できれば12 byteにできる。Radix の index は live node 数に応じて32/64 bitを選ぶ。

- 期待効果が大きい条件: 大 W/N、軽い State、tour/tree scan が帯域律速。
- 悪化条件: pack/unpack 命令、上限 check、2^32 slot を超える巨大探索。
- 結果再現性: 値が収まる限り同じ。overflow は silent にせず invalid parameter または64 bit fallback。
- メモリ上限: base の ID 配列は概ね半減、turn TreeNode は約25%削減候補。
- API 互換性: ID は private。制限を文書/API status に表す必要がある。
- 実装難度: 中。
- 検証指標: ID 配列 byte、tree sizeof、pack命令、cache miss、上限到達テスト。

### P2-2: hash policy と seed を整理する

State が Zobrist など十分拡散された64 bit hashを返す場合、HashDict の再混合は冗長である。可変ターン版は既に
`USE_HASH_FUNC=false`、base 系は true で固定されている。Beam policy に `RehashStateHash` を追加し、安全既定を true、
opt-in を false とする。

false 経路は seed 初期化を compile-out する。true 経路も map ごとの `random_device + mt19937` が必要かを再検討し、
プロセス単位 seed や軽い splitmix seed と比較する。

- 期待効果が大きい条件: 候補当たり State 計算が軽い、hash lookup が支配的、入力 hash が十分均一。
- 悪化条件: 連番・低bit偏り・敵対的 hash。probe が急増し、最悪では大幅に遅くなる。
- 結果再現性: table 配置だけなら同じ候補集合。動的時間幅は速度差で変わる。
- メモリ上限: 変わらない。
- API 互換性: default true の policy 追加で維持。
- 実装難度: 低。
- 検証指標: hash混合cycle、probe group histogram、入力hash低bit分布、collision/duplicate率。

### P2-3: branch hint より先に経路分離と PGO を使う

幅へ達した後は threshold reject が多い問題もあれば、改善候補が多い問題もある。duplicate 率、finished 率も問題で
逆転するため、固定の `[[likely]]` は汎用ライブラリでは危険である。

まず warmup/full selector、global-seen on/off、monotone-skip on/off、history on/off を入口で分ける。次に代表的な
合成 State 群で PGO を試す。`-march=native` と LTO も、header template と State callback の inline に有効である。

- 期待効果が大きい条件: 非常に軽い State、hot branch が多い、利用側も同じ最適化設定でビルドできる。
- 悪化条件: workload と profile が異なる、template variant による code bloat。
- 結果再現性: コンパイル配置だけなら同じ。fast-math は score 比較を変えるので別扱いとする。
- メモリ上限: runtime data は同じ、text size は増える。
- API 互換性: 維持可能。
- 実装難度: 低から中。
- 検証指標: branch miss、instructions、I-cache miss、text size、profile別cross-validation。

### P2-4: prefetch/SIMD は限定条件でだけ試す

HashDict は既に SSE2 で16 control byteを比較する (`hash_dict.cpp:94-117`)。load factor 1/2で通常1 groupで空きを
見つけるなら AVX2 32-byte化の利得は小さく、周波数低下や portability の損があり得る。長い probe が観測された場合
のみ AVX2/AVX-512/NEON backend を比較する。

`get_pos()` は index 算出直後に control を必要とするため、その場の `prefetch` は隠せる仕事がない。複数候補を先に
hash 化する batch pipeline は threshold の逐次更新を遅らせるため意味論・枝刈り量が変わる。既定には向かない。

Radix の sibling/node は pointer-chase に近いので、次の stack item や sibling metadata の prefetch は試す価値がある。
ただし State::apply_op が重いと hardware prefetch で十分であり、無駄な cache pollution になる。

- 期待効果が大きい条件: probe group > 1、NodeMeta が LLC に収まらない、State が軽い。
- 悪化条件: 通常1 probe、W/N が小さい、Action/State が cache を使う、非x86。
- 結果再現性: 同じ。
- メモリ上限: SIMD control の末尾 padding が16から32へ増える程度。
- API 互換性: compile-time backend と scalar fallback で維持。
- 実装難度: 中。
- 検証指標: probe groups、LLC miss、prefetch hit usefulness、cycles、CPU別結果。

### P2-5: finished path は最後に一度だけ materialize する

base/compose/radix は、より良い finished 候補が見つかるたびに深さ D の path をコピーする
(`beam_search.cpp:152-158`, `beam_search_compose.cpp:249-255`, `beam_search_radix.cpp:148-154`)。同一世代に多数の finished
改善があると O(number_of_improvements * D) の Action copy になる。turn 版は parent ActionId と最後の Action だけを
保存し、終了時に path を作るので、この点は既に良い。

base/compose は current parent の再構築情報と final Action だけ、Radix は parent node と final Action だけを保存し、
世代列挙終了時に最良 path を一度構築する。世代 block を先に解放しないよう寿命を調整する。

- 期待効果が大きい条件: 深い探索、finished 候補と改善回数が多い、大きい Action。
- 悪化条件: finished が最初の1件だけ。保持情報と寿命管理が余分。
- 結果再現性: 最良 score の tie 規則を保てば同じ。
- メモリ上限: 繰返し path vector assignmentを減らす。必要な block pin は O(D) ID。
- API 互換性: 内部。
- 実装難度: 中。
- 検証指標: finished件数/改善件数、path Action copy数、終了世代時間。

## 結果再現性についての共通注意

性能案は次の三段階で区別する。

1. **候補集合・順序とも維持**: hash 配置、seg lazy init、SoA、arena、clock除去など。
2. **最終 top-W 集合は維持し得るが順序が変わる**: heap、counting grouping、dirty slot収集。ordinal tieが必要。
3. **列挙量や top-W 自体が変わり得る**: batch threshold、良い候補優先、bounded history、近似 dedup。

同じ score の候補をどれから追い出すか、同じ親の子をどの順で展開するかは、State 内の乱数消費と次世代の
threshold 低下時点を変える。現行 `std::sort` 自体が同点順を保証しないため、将来の最適化前に
`(score, enumeration_ordinal)` などの規則を明文化するのが望ましい。

また `is_adjusting=true` は実測時間から幅を変える。意味論を完全に保つ内部変更でも実行速度が変われば幅と結果が
変わるため、「固定幅での再現性」と「動的幅での品質分布」を分けて検証する。

## メモリ上限の設計方針

ライブラリとして、各領域の上限または増加要因を API で説明できる形にする。

| 領域 | 望ましい増加要因 | 備考 |
|---|---|---|
| current candidate metadata | `O(sum_t W_t)` | 各 pool が `W_t <= W` なら `O(FW)` |
| current candidate hash | `O(sum_t W_t)` | active 表を `A` に比例させない |
| historical exact hash | `O(H)` | 無制限履歴なら hard cap 不可 |
| pending Action | `O(dirty_slots * S_A)` | 現行 turn 版は `O(A * S_A)` |
| live tree Action | `O(N * S_A)` | turn 版の粗い上限は `O(FWD * S_A)` |
| retained generation block | `O(G * S_A)` | base/compose は死んだ slot も世代確定まで保持 |
| turn Action arena capacity | 過去の `N + A` 高水位 | P0-3 後は過去の `N + dirty_slots` 高水位 |
| segtree/heap | `O(W)` | pair padding を含む byte 上限も記録 |
| history/log | record_history 時のみ | false なら node_id も hot struct から除く |

`G` は live node 数 `N` ではなく、未解放 block に実際に確保した slot 数である。base/compose では
死んだ候補の slot も残るため、`G` は `N` を大きく超え得る。世代ごとの幅上限を `W_max` とすると、
粗い最悪上限は `G = O(W_max D)` である。各 `vector` の capacity は block 再利用後も過去の高水位を保持する。

履歴 hash のメモリ上限が必要な利用者には次を明示的 policy として用意する。

- periodic window clear: 現行 base の意味論に近い
- sliding K generations: 厳密だが世代別 key list が必要
- max entries到達時 clear/LRU: 結果が変わる
- fingerprint/Bloom only: 近似であり false positive が候補を落とす

Bloom filter は exact historical map の前段に置き、negative 時だけ lookup を省く用途なら結果を変えない。H が大きく、
新規 hash が多い場合の二段 lookup を軽くできるが、filter 自体の更新・memoryを測る。

## 検証計画

単一 test の wall time だけで採用しない。少なくとも次の軸を直交させた合成 State と複数の実問題で比較する。

- W: 8 / 64 / 1K / 16K 以上
- 分岐数: 2 / 8 / 64 / 数百
- `A/W`: 1前後 / 4 / 32以上
- 重複率: 0 / 中 / 高、同一hashの改善頻度
- Action: trivial 8 byte / 32 byte / 128 byte / heap所有型
- Score: int32 / int64 / double / 小さい bounded integer
- State cost: 数命令 / 中 / 重い
- `clear_hash_every_turn`: true / false / window
- 固定幅 / 動的幅
- turn版の `F` と各pool occupancy、`target_turn` の疎密
- tree共有接頭辞とRadixの縮約成功率

収集する指標は次のとおり。

- throughput: `try_op/s`, cycle/列挙候補, cycle/採用候補
- selector: score比較数、更新level、threshold reject率、heap/tree書込数
- hash: lookup数、hash混合回数、probe group histogram、used/active/tombstone、rebuild、clear byte
- Action: default/copy/move/assign/destruct回数、pending/live/free slot、高水位
- sorting: 入力 A、compaction後、dirty slot数、比較数、親子数histogram
- memory: 各vector size/capacity、各HashDict cap、arena block、peak RSS
- hardware: instructions、branch miss、L1/LLC miss、cycles
- quality/reproduction: 固定seedで候補ID列・最終score・Action列、動的幅では複数seedのscore分布

計測 instrumentation 自体が hot path を歪めるため、counter build と release build を分け、copy/move は専用の計測
Action 型で数える。CPUごとに最低でも x86-64 SSE2 baseline と `-march=native` を分ける。

## 低リスクから進める実装順

期待効果の優先順位では、`A` が漏れる active hash と turn 版の pending Action が最上位である。ただし、まず計測基盤と
結果を変えにくい小変更で仮説を確認し、その後に所有権を変える修正へ進む。以下は効果順ではなく実装リスク順である。

1. 共通 Candidates の segtree 全初期化を build 時へ移す。`s` は current width から再計算する。
2. base/compose を `push_lazy()` へ移す。Action copy/move counter で確認する。
3. `HashDict(8W)` の意味を修正し、turn pool の容量を実 occupancy へ適応させる。false時seed生成も除く。
4. selector locator で hash の二重混合を除き、旧hash位置を直接無効化する。
5. active table を約4Wへ固定し、A-W件ごとの survivor rebuildを入れる。履歴表を分離する。
6. turn の dead stable compactionを先に入れ、次にdirty slot/pending Action再利用へ進む。
7. base/compose の Action を最終世代blockへ直接保存し、hot metadataをSoA化する。
8. base/composeへ親counting groupingを導入し、同点ordinalを仕様化する。
9. 固定幅・統計なし経路とCompose/Radix counterなし経路を分ける。
10. turn のstable chunk arenaと32bit ActionId、TreeNode packingを実装候補として比較する。
11. selector policyとしてsmall-W linear、indexed heap、bounded-score bucketを比較する。tournamentを既定に残す。
12. PGO、prefetch、AVX2/NEONは上の構造改善後のprofileに基づいて判断する。

## 結論

最優先は別の top-W アルゴリズムではなく、現行の exact online threshold を保ちながら `A` が漏れている二箇所を
止めることである。

1. 共通 Candidates の hash tableを active W と履歴 H に分離し、現世代表を O(W) にする。
2. 可変ターン版の `new_candidates` と追加 pending Action を、一時採用イベント `A` でなく
   最終 dirty slot 数に比例させる。既存の live Action `N` と過去の arena capacity は別途残る。

その次が、base/compose の Action 遅延・直接保存、turn pool の16W bucket過剰確保、seg全初期化である。これらは
探索アルゴリズムや State 契約を変えず、固定幅なら結果を維持しやすい。

top-Wについては現行 tournament tree が汎用既定として妥当である。heapは主にメモリ削減、`nth_element`/radixは
batch専用、bucketはbounded integer score専用として policy 化する。prefetch/SIMD/branch hintは profile に依存し、
構造的な A/W 問題を直す前に優先すべきではない。
