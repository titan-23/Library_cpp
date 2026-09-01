# 通常版 beam search の定数倍実装ゲート

## 対象と結論

対象は `titan_cpplib/ahc/beam_search/beam_search.cpp` のみとする

`beam_search_state.cpp`、`beam_search_state_turn.cpp`、`old/` は対象外とする

ここでの store 数と byte 数はコードから求めた設計時の値である

このgateに基づくcombined版は `beam_search_optimized.cpp` へ実装済みで、個別kernelとend-to-end結果は
`constant_factor_microbench_results.md` と `benchmark_results.md` に記録した

第一の optimized backend へ入れる項目は次とする

1. 未参照の先頭 prefix を書かない
2. ActionId を深さ別の `uint32_t` slot に置き換える
3. entry を除いた prefix 確定と `C=1` の確定を入れる
4. 世代 block の Action を move construction する
5. callback と vector fallback を `push_lazy()` へ移す
6. 世代開始時の重複 timer read と未使用 RNG を除く
7. 確定prefixの Action を元blockから `result_prefix` へmoveする

1から7はcombined backendへ入れたため、end-to-end値だけから各寄与を分離しない

次は第一実装から分離する

- `CandIdx` と Action slot の完全な並び替え
- segtree reset の遅延化
- active hash と履歴 hash の分離
- selector のデータ構造変更

分離理由は正しさへの疑いではなく、効果と退化条件を単独で測るためである

## 現行コードで使う記号

- `t`: 外側 loop の `turn` で、`cand` の Action 深さ
- `C`: その loop で展開する `cand.size()`
- `e`: 世代開始 endpoint から最初の `cand` の親までの `lca_dist`
- `a_i`: 2個目以降の `cand` について計算する内部 `lca_dist`
- `W`: その世代の beam 幅
- `S`: segtree が過去に到達した葉数の2冪
- `s`: 現在幅に対する `bit_ceil(W)`
- `A`: その世代で一時的に selector へ採用された回数
- `U`: その世代で表へ入った異なる hash 数
- `H`: 世代をまたいで予約する survivor hash 数

`cand` の Action は深さ `t` にあり、`parent_leaf` が表す親は深さ `t-1` にある

この1段の差を落とすと prefix 確定位置を1段誤る

## 1 未参照の先頭 prefix

### 正しさ

各 loop の最初の `next_tour.insert()` は `e+1` 個の ID を `[0, next_leaf[0])` へ書く

次世代の `copy_tour_path()` が使う最小の source offset は `leaf[0]` である

LCA 計算も `leaf[k+1]-leaf[k]` とその累積差だけを使う

従って `[0, leaf[0])` は次のどの経路からも読まれない

- 次世代の親 path 復元
- max turn の path 復元
- finished path の構築
- `confirm_and_free()`

最初の insert を省き、最初の `next_leaf` を0にすれば後続境界の差は変わらない

State の entry 移動に必要な rollback、`copy_tour_path()`、apply は省いてはいけない

### 実際に消える処理

世代ごとに次を除ける

- `e+1` 個の trace ID load
- `e+1` 個の `next_tour` ID store
- その要素数が vector の高水位を越える場合の再確保

slot 幅を `q` byte とすると、直接の write 削減は `q(e+1)` byte である

`e=0` でも子 Action 1個分を省ける

`W=1` では使用中の `tour` が空になるが、これは正しい

### 退化条件と相互作用

entry が短い workload では削減は1 IDだけで、分岐判定の固定費が相対的に残る

entry を prefix 最大値から外す変更は、この未参照領域の削除後に行うのがよい

先頭領域を残したまま generation block を早く解放すると、未参照領域に dangling slot が残る

実行上は読まれないが、全 ID の有効性を検査する debug invariant と衝突する

### 判定

第一実装へ入れる

`W=1`、`e=0`、`e=t-1` を専用 test にする

## 2 深さ別の 32 bit slot

### 正しさ

現在の64 bit IDは上位に generation、下位24 bitに slotを持つ

通常版では Action を参照する時点の論理深さを常に別に知ることができる

- `trace[d]` は `gblock[d]` を参照する
- 現在の `cand` は loop の `gblock[t]` を参照する
- `tour` は直接 Action へ変換されず、既知深さの `trace` 範囲へコピーされる
- 最終 path の iterator offset からも深さを決められる

従って ID には generation が不要で、`uint32_t slot` と既知の深さで参照できる

これは packed ID を単純に32 bitへ縮める案ではない

単純な packed 化なら generation が8 bitに狭まり、256世代で破綻する

現在の候補 slot は `int` で生成されるため、非負の有効 slot は `uint32_t` に収まる

それでも変換点に assert を置き、silent overflow を許さない

### 実際に消える処理

- `tour`、`next_tour`、`trace` の ID load/store byte が8から4になる
- `CandIdx` の ID field が8から4になる
- `act()` ごとの generation shift と slot mask が不要になる
- 現在の24 bit slot 上限がなくなる

`CandIdx` 全体の削減量は ABI、`ScoreType`、field 順、history の有無で変わる

ID field が4 byte減ることと、構造体が4 byte小さくなることは同義ではない

### 必須の実装条件

Action 参照関数は `act(depth, slot)` の形にし、slotだけを受ける overload を作らない

`materialize()` は iteratorだけでなく開始深さも受け取る形へ変える

`copy_tour_path()` は slotをコピーするだけに保ち、source generationを推定しない

freed depth以下の trace slotは danglingになり得るため、`freed_to` 以下を dereferenceしない

### 退化条件と相互作用

State と Action が重い場合は ID 帯域の差が隠れる

slotからActionへの2段 vector lookupは残る

Action slotを `cand` 順へ揃える案と組み合わせると localityをさらに改善できるが、別項目として測る

### 判定

第一実装へ入れる

64 bit baselineとの世代別 path digest比較を必須にする

## 3 entry除外と `C=1` の prefix確定

### 正しさ

`confirm_and_free(L)` は Action 深さ `d<L` を確定する

`e` は世代開始 State の endpoint と最初の展開親の間の距離である

これは現在展開する `cand` 同士の共通 prefix を制限しない

loop終了時の traceは最後に展開した現在葉の pathなので、確定 Actionの取得元として使える

`C>=2` のとき、展開した現在葉が共有する最後の深さは次になる

```text
h = t - 1 - max(a_i)
```

従って `confirm_and_free(t-max(a_i))` で深さ `h` まで確定できる

同じ親の兄弟間では `a_i=0` となり、親の深さ `t-1` まで確定する

`C=1` のときは現在葉そのものが全ての次候補の祖先なので、深さ `t` まで確定できる

この場合は `confirm_and_free(t+1)` とする

ここでの `C` は次世代 survivor 数ではなく、現在展開した `cand.size()` である

次世代 survivor の親だけを使う強い確定は別 policy である

### 実際に消える処理

この変更だけでは State の apply、rollback、tour read/write は減らない

次を早い世代へ移す

- dead generation block の Action destructor
- block を `slab_pool` へ戻す処理
- 確定 Action の `result_prefix` への保存

主効果は Action payload の live memory と slab 再利用時期である

大きい Action と長い共通 prefix では allocation と RSS を減らせる可能性がある

### 退化条件と相互作用

- 根付近で分岐し続ける場合は確定位置が進まない
- 小さい trivially-copyable Action では CPU 改善がほぼない場合がある
- 解放処理の回数総量は大きくは変わらず、実行時期だけが前へ移る
- 現在葉のうち survivor childを持たない葉も `a_i` に含むため、強い survivor版より保守的である
- finished と no-candidate は現行どおり prefix更新より前に returnする

### 判定

第一実装へ入れるが、未参照先頭 prefix の削除後に有効化する

`C=1`、全て同じ親、entryだけが深い、endpointの子が全脱落する場合を testする

## 4 世代 Action block の move construction

### 正しさ

現行の `finalize_generation()` は次を行う

1. `gblock[gen].resize(sz)` で `sz` 個の Action を default constructionする
2. 各要素へ候補 Action を move assignmentする

poolから得る vectorは size 0でcapacityだけを再利用できる

`clear()`、`reserve(sz)`、`emplace_back(std::move(action))` へ変えれば同じ slot順を保てる

Action は通常の値型として、move先が元と同じ操作内容を表すことを契約にする

### 実際に消える処理

survivor 1件ごとに次を置き換える

```text
旧: default construction 1 + move assignment 1
新: move construction 1
```

trivial Actionでも `resize()` の値初期化 storeを避けられる可能性がある

候補 slotに残る moved-from Action の lifetimeと、次世代の上書きは残る

Action の候補保存と世代 block保存という二段 ownershipも残る

### 退化条件と相互作用

- 小さい Actionでは差が小さい
- move constructionが高価な型では当然その費用は残る
- pool容量が足りない場合のallocationは残る
- Actionをsorted slotへ揃える変更とは分ける
- `result_prefix.push_back()` のcopyをmoveへ変える案も別に測る

### 判定

第一実装へ入れる

default、copy、move construct、move assign、destruct回数を計測する Action型で確認する

## 5 `push_lazy()`

### 正しさ

callback経路の `Action& a` は `Candidates::push()` の値引数へ入る前にcopyされる

vector fallbackの `std::move(action)` も関数本体のthreshold/hash判定より前にmove constructionされる

既存の `push_lazy()` はthreshold、履歴 hash、同一hash scoreを確認した後だけ `make()` を呼ぶ

callbackでは `a` をcopyする lambdaを使い、enumeratorが再利用する Actionをmoveしてはいけない

vector fallbackでは採用時だけ `std::move(action)` を返す lambdaを使える

`record_history=true` では `to_string()` をmove前に取得し、node idとstatusの時点を変えない

### 実際に消える処理

棄却候補1件ごとに次を除く

- callbackでは Action のcopy constructionと引数destruction
- vector fallbackでは Action のmove constructionと引数destruction

採用候補では candidate slotへのmove assignmentが残る

lambdaがinlineされなければcall overheadが残るため、release buildのassemblyとcycleを確認する

### 退化条件と相互作用

- 採用率がほぼ100%なら削減は小さい
- trivially-copyableな小 Actionでは差が小さい
- copyやmoveの副作用へ依存する Action型とは互換にしない
- move construction改善とは独立で、両方入れても候補からblockへの1回のmoveは残る

### 判定

第一実装へ入れる

callbackとvector fallback、history on/offを全て同値試験する

## 6 timer read と未使用 RNG

### timer

通常完走する1世代では現在おおむね3回 `elapsed()` を呼ぶ

1. loop先頭の `now_time`
2. `get_beam_width()` へ渡す残り時間
3. `timestamp()` へ渡す世代終了時間

第一段階では1と2を同じ `now_time` にし、3回を2回へ減らす

世代全体の時計読出しを1回にするという意味ではない

終了時の読出しは世代時間の更新に必要である

`is_adjusting=true` では width計算の観測時刻がvector clear後からloop開始時へ少し移る

動的幅は実行時間そのものに依存するためbit-identicalな幅列は元から保証できない

厳密に観測位置を維持する modeでは2番目のreadを残す

前世代の終了時刻を次世代の開始時刻として使う1境界1read方式は、統計区間も変えるため後段とする

### RNG

`rnd` は宣言と `init_bs()` の再初期化以外で読まれない

削除すると objectから16 byteと検索開始時の4個の32 bit代入を除ける

主効果は小さいが意味論上の依存がない

### 判定

開始時の重複 read と RNG は第一実装へ入れる

動的幅では recordした幅列をreplayする構造比較も併用する

## 7 `CandIdx` layout と Action slot整列

### 現状

`CandIdx` は次をAoSで持つ

```text
parent_leaf, score, 64 bit action_id, node_id
```

`record_history=false` でも `node_id` が残る

sortはparentとscoreだけを読み、展開はparent、Action ID、history時だけnode idを読む

selector slotの置換が多いと、sort後の `cand` 順と `gblock` slot順が一致しない

### 安全に分けられる変更

1. 32 bit slotへ縮める
2. `record_history=false` のnode idを空のfield policyで除く
3. alignmentを見てscore、parent、slotの順を決める

代表的な64 bit ScoreのABIでは、現行32 byteがslot-onlyで24 byteになり得る

historyなしのfield順まで整えると16 byteになり得るが、`sizeof` をstatic testで記録する

### Action slotをsort順へ揃える案

候補の軽いdescriptorを現行と同じ入力列とcomparatorでsortし、その順にActionを世代blockへmoveする

これにより次世代の逆走査で現在世代 Actionをほぼ連続して読める

slotが常に `cand` ordinalなら、最終的に `CandIdx::action_id` 自体も除ける

この段階では展開ごとのslot field loadも1回除ける

sort scanの概算metadata readは `sizeof(CandIdx)` と比較回数の積なので、padding削減も反復して効く

一方でfinalize時の候補 Action source readは非連続になり、temporary source indexが必要になる

高い置換率では展開localityの利益が期待でき、置換が少ない場合は現行もほぼ連続である

同score、同parentの順は現行 `std::sort` でも仕様化されていない

厳密比較では同じdescriptor列を一度sortしてからpayloadだけを並べる

### 判定

32 bit slot fieldは第一実装へ含める

history field除去、field再配置、Action slot整列、ID field除去は別 benchmarkへ分離する

## 8 segtree reset

### 正しさ

reset直後は `entry<W` なので `threshold()` はsegを読まず `INF` を返す

初めて `entry==W` になった時だけ、次を行えばよい

1. 使用葉 `W` 個へscoreを書く
2. 未使用葉 `s-W` 個へ `-INF` を書く
3. 内部 node `s-1` 個をbuildする

内部 nodeは全て上書きされるためreset時のfillは不要である

logical treeの `s` は毎世代 `bit_ceil(W)` に戻し、vector capacityだけを高水位のまま保持できる

valid scoreが `-INF` と同値になる場合、現行も未使用葉がworst indexになり得る

selector契約として採用可能scoreは `-INF` より大きいことを明示するか、別sentinel表現が必要である

### 実際に消える処理

pair 1個の大きさを `p` byteとする

候補がW件へ届かない世代では、現行の `2S*p` byteのreset storeを全て省ける

ただしseg vectorのcapacityが初めて増える世代では、`resize()` の新要素初期化storeが別に発生する

候補がW件へ届き、`S=s` の場合は次になる

```text
現行: reset 2s + 使用葉 W + 内部 s-1 = 3s+W-1 pair store
遅延: 全葉 s + 内部 s-1             = 2s-1 pair store
```

差は `s+W` pair storeである

過去幅により `S>s` ならlogical `s` を縮める効果も加わる

### 退化条件と相互作用

- 毎世代すぐW件に達し、Stateが重い場合は差が隠れる
- 幅が2冪なら未使用葉fillは0だが内部buildは残る
- lazy resetは現行tournamentのthresholdとeviction順を保てる
- heap、bucket、buffer selectorへの変更とは混ぜない
- 共通 `Candidates` を変更するとbaseline側も速くなり、backend比較を汚す

### 判定

正しさは低リスクだが第一 backendから分離する

専用 selectorまたは独立 commitで、reset store counterを取ってから共通化する

## 9 active hash と履歴 hash

### 現状の正確なコスト

追い出したhashはvalueを `-1` にするだけでkeyとcontrol byteを残す

従って `HashDict::size` はactive Wではなくclear以来の異なる一時採用hashへ増える

load factorが1/2を越えるたびにgrowし、clearしてもcapacityは縮まない

`clear_hash_every_turn=true` では大きくなったcontrol配列を以後の各resetでfillする

`false` では前世代の最終survivorだけを `-2` にして予約する

一時採用後に追い出されたhashは履歴予約されず、将来また採用できる

periodic clearでも直前世代survivorはclear後に `-2` として入れ直す

`func.inner_len()==1` は初期capacityが16なので成立せず、初期reserveの意図は実行されない

ただし条件だけ直して `HashDict(8W)` にすると最低16W bucketとなるため採用しない

worst置換の通常経路は概ね次を行う

- new hashの `get_pos()` で1回probe
- old hashの `set()` で1回probe
- new hashの `inner_set()` で混合を再計算してh2を得る

`inner_set()` の再計算は完全な2回目probeではない

### 正しさを保つ分離案

active tableは現在の候補だけを持ち、容量をWに比例させる

追い出し位置をcandidate slotに保存し、DELETEDまたは定期rebuildでprobe chainを保つ

tombstoneが閾値へ達したらactive W件だけから再構築し、全slotの位置を更新する

履歴tableは `clear_hash_every_turn=false` の最終survivorだけを保持する

resetではactiveを空にする前に最終survivorを履歴へ移す

periodic clearでは履歴を消した後、直前survivorを入れ直す

push時はworst閾値を先に判定し、その後に履歴予約、active同hash、同hash scoreを判定する

active tableの再構築を約W回の追い出しごとに行えば、Aに対して償却線形にできる

### 実際に消える処理

- old hashを探し直す1回のprobeを位置指定storeへ置き換えられる
- locatorがh2も返せばnew hashの2回目の混合を除ける
- active control/key/valueの高水位をUからWへ制限できる
- clear時のactive control fillをO(W)に制限できる

履歴を有効にした場合の `O(H)` memoryは消えない

履歴とactiveの2回lookup、tombstone管理、rebuild storeは新たに増える

### 退化条件と相互作用

- `A` がほぼWならposition配列とrebuild判定が純増し得る
- `clear_hash_every_turn=false` でHが大きい場合は履歴lookupが支配し得る
- 重複率が高い場合はactiveと履歴のlookup順で損益が変わる
- active slotを並べ替える変更は保存したtable位置の更新と結び付く
- HashDict共通APIへ生positionを公開するとrebuild後の無効参照を招く
- exact履歴にhard capを付けると探索意味論が変わる

### 判定

期待効果は大きいが第一実装から分離する

通常版専用 `CandidateSelector` 内に閉じ、現行selectorとの候補列逐次比較を行う

## 10 併せて見つかった通常版の固定費

### `explored_per_turn`

callbackは候補ごとにincrementし、vector fallbackは列挙数を毎世代加算する

値を読むのはverboseのturn logだけである

runtimeの `if (verbose)` を候補ごとに置くとbranchが増えるため、入口でlog有無をtemplate dispatchする必要がある

これは候補ごとのread-modify-writeを1回除けるが、code sizeとI-cacheを増やすので第一実装から分離する

### verbose時の `get_best()`

`Candidates::get_best()` は `BeamCandidate` を値で返す

turn logがscoreしか使わなくてもActionを1回copyする可能性がある

`best_score()` または `const BeamCandidate&` を返せば除ける

verbose専用なのでrelease探索の優先度は低い

### `result_prefix` への保存

`confirm_and_free()` はActionを `result_prefix` へcopyした直後に元blockをclearする

通常のmove契約ならmoveへ変えられ、確定Actionごとに1 copyを1 moveへ置き換えられる

prefix解放の差分試験とAction lifetime監査後に実装し、direct parent版と所有権を揃えた

### 全体sortから親bucketへの変更

親番号は密だが、score同点時の順序が現行でも仕様化されていない

count、prefix sum、scatterは親group比較を除く一方、count clearとW件のscratch writeを増やす

親当たり候補が少ない時に有利で、一親集中では全体sortとの差が小さい

Action slot整列と同じscratchを共有できるが、最初から統合せず比較回数とwrite数を別々に取る

## 相互作用の実装順

### 第一 optimized backend 内

1. 64 bitのまま未参照先頭 prefixを削除する
2. path digest一致後に深さ別32 bit slotへ変える
3. Action blockをmove constructionへ変える
4. callbackとvector fallbackをlazy化する
5. entry除外と `C=1` prefix確定を有効にする
6. timer開始readと未使用RNGを整理する
7. prefix Actionのmoveを有効にする

この順ならtour表現、ID表現、Action lifetime、prefix lifetimeを別々に検証できる

### 後続の独立比較

1. segtree lazy reset
2. historyなし `CandIdx` layout
3. sorted Action slot整列
4. active hashと履歴hashの分離
5. locatorによるold hash probeとnew hash再混合の除去

hashとslot整列はcandidate slot位置を共有するため、単独結果を得てから統合する

## 必須 counter と同値性ゲート

各世代で次を数える

- 先頭未参照ID数 `e+1`
- tour ID read/write数とbyte数
- Action default、copy、move construct、move assign、destruct数
- timer read数
- seg reset、leaf build、internal buildのpair store数
- hashのmix、probe group、old-key lookup、used、active、reserved、rebuild数
- `sizeof(CandIdx)` と世代ごとのsorted slot stride

次をbaselineと比較する

- candidateの `(score, hash, parent_leaf)` の順序
- try、apply、rollback、enumerateの呼出し列
- 各展開葉でのState digest
- `tour` の各境界差
- prefix Action列と最終Action列
- status、score、turns、history survivor集合

動的幅は同じ幅列をrecord/replayした比較と、同じ時間制限の実運用比較を分ける

第一実装は全edge caseの差分試験、Action lifetime、sanitizer、合成benchmarkを通過した

個別寄与を確定したのは `push_lazy()`、世代blockのmove construction、timer readの独立kernelである
