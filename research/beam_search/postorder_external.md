# 帰りがけ順 tour の後継構造に関する独立設計レビュー

## 目的

通常の `beam_search.cpp` は、完全な Euler tour ではなく、帰りがけ順を調整した `tour` と `leaf` を使う。
この文書では、この高速化後の実装を基準にして、さらに定数倍を削れる探索木表現を独立に設計する。

公開実装の有無や他問題の速度比は結論に使わない。対象条件は次に固定する。

- 深さが1ずつ進む固定深さビームサーチ
- 生存候補数は各世代で高々 `W`
- `State` は1個だけ持つ
- 状態移動は辺単位の `apply_op()` と `rollback()` だけで行う
- 候補の評価順、重複排除、同点順、終了判定を変えない
- 任意の `State` と `Action` を受け取る汎用ライブラリである

Action 合成、State snapshot、複数 State、探索順変更は別の契約になるため、主比較から外す。

## 結論の要約

- 現行はState操作回数についてopen DFSの下限に近く、一般LCA追加では `apply` と `rollback` を減らせない
- 木metadataは下限ではなく、未参照prefix削除と64 bit IDのslot-only化は低リスクである
- direct parent + adjacent LCPはstream writeを `O(M_all)` から `O(W_c)` へ変える本命比較になる
- LCP既知ならtarget suffix長 `r` に対するparent loadは `r-1` で、兄弟遷移は0回になる
- windowed decodeはparent load総数を変えず、複数葉chainのmissを重ねるpolicyとして価値がある
- unaryとEFはparent metadataを縮めるが、select decodeと `G>>E` が反例になる
- survivor-lateは開始trace保存なしでは不成立であり、snapshot量 `J` をcostへ入れる必要がある
- 片方向anchor baseはsnakeの向き反転で不成立で、使うならforwardとreverseの2本が必要になる
- path-chunkは長いunary chainと `G>>E` に強いが、毎段分岐ではlength 1へ退化する
- 絶対的な最速構造は未確定で、同一幅列による複数backendの実測が必要になる

## 最初に訂正する点

現行方式は「LCA構造」ではない。隣接葉間の共通接頭辞長を `leaf` と `tour` の境界から復元し、
同時に LCA まで rollback、次の葉まで apply している。

従って、次に比較すべきものは binary lifting や RMQ ではない。
比較対象は、同じ葉間移動を実現するための履歴表現とメモリ配置である。

また、現行 `beam_search.cpp` は Action 本体を `tour` にコピーしていない。
Action は世代別 `gblock` に1回だけ置き、`tour`、`next_tour`、`trace` は64 bitの `ActionId` を持つ。
残る構造コストは主に ActionId のコピー、`tour` の再構築、経路復元である。

## 記号

| 記号 | 意味 |
|---|---|
| `W_p` | 前世代の葉数 |
| `W_c` | 次世代へ選ばれた子の数 |
| `P` | 選択された子が参照するdistinct parent数 |
| `K` | `P` 親へ走査終了endpointを加えた葉数 |
| `D` | 確定接頭辞より下の最大深さ |
| `H` | 世代開始時のtraceにだけある未確定endpoint path長 |
| `E` | 現在の生存葉への経路の和集合に含まれる Action 辺数 |
| `E'` | 次世代の生存部分木に含まれる Action 辺数 |
| `E_all` | 全 `W_p` 葉とendpointが張る部分木の辺数 |
| `E_K` | survivor parentとendpointが張る誘導部分木の辺数 |
| `M_all` | 全 `W_p` 葉をeager tourへ出力する有効ancestor slot数 |
| `M_K` | 次世代のsurvivor parent巡回で実際に復元するancestor slot数 |
| `Z` | 現行が `leaf[0]` より前へ書く未参照tour prefix長 |
| `J` | 遅延再構築用に退避する開始trace suffix長 `0 <= J <= H` |
| `G` | 未解放の世代 block に確保済みの Action slot 総数 |
| `U` | 1世代の葉巡回で行う rollback 回数 |
| `F` | 1世代の葉巡回で行う apply 回数 |
| `X` | `U + F` |
| `T` | target suffix長が正の葉遷移数 |
| `Y` | LCP既知時のparent decode数 `F - T` |
| `S_A` | inline の `sizeof(Action)` byte |
| `q` | slot番号のbyte数。通常は4、幅制限時は2 |
| `l` | LCP深さのbyte数。通常は2または4 |
| `R` | patchまたは再利用spanの個数 |
| `B` | immutable chunkに入れる参照数 |
| `B_w` | windowed ancestry decodeで同時に進める葉chain数 |
| `N_e` | backendを固定するepochの世代数 |
| `B_o` | anchor overlayをflattenする最大世代間隔 |
| `M_A^+`, `M_A^-` | anchor frontierのforward、reverse stream有効slot数 |
| `M_A` | `(M_A^+ + M_A^-)/2` |
| `Y_o` | 1世代のanchor overlay parent decode数 |
| `M_jump` | anchor間移動でbase streamから読むsuffix slot数 |
| `C_live` | live path-chunk数 |
| `C_new` | 1世代に新規確保するpath-chunk数 |
| `C_path` | 1回のfrontier巡回で跨ぐchunk parent境界数 |
| `S_H` | path-chunk headerのbyte数 |
| `V` | 固定長chunk内の未使用Action slot数 |

`E` は live な辺数、`G` は過去に確保して世代ごと残っている slot 数であり、同じではない。
枝が大きく入れ替わると `G >> E` になり得る。

`M_all` は、展開した全 `W_p` 葉を次世代のparent候補として残すeager streamの有効slot数である。
`M_K` は、選択後に必要と分かった `P` parentと走査endpointだけへcompactした場合の有効slot数である。

各target葉の末尾Actionは `cand` から直接得るため数えず、target parentまでのancestor suffixだけを数える。
次世代で選択済みtargetを実際に巡回するとき、compact streamのancestor slot数は次になる。

```text
M_K = F - T = Y
```

DFS順の端から端へ移るなら、各辺を下向きに使う回数は高々1回なので次を満たす。

```text
M_K <= E_K <= E_all
M_all <= E_all
```

以下のbyte数は論理的なload/store量である。L1 cache上のcopyがそのままDRAM trafficになるわけではない。
それでも、表現間の相対的な帯域と依存関係を比べる尺度にはなる。

## 変えられない下限

### State操作回数

確定接頭辞より下の生存部分木を、葉 `s` から開始し、すべての葉を訪れて葉 `t` で終了する。
`s`--`t` 経路にない辺は、部分木へ入るときと出るときの2回通る必要がある。
`s`--`t` 経路上の辺も最低1回通る。

従って、辺単位の状態操作しか許さない場合、次が下限になる。

```text
X >= 2E - dist(s, t)
```

open DFS はこの下限を達成できる。帰りがけ順方式は、DFS順の端から反対端へ葉を処理するため、
状態操作回数について既にこの形に近い。

LCAを `O(1)` で返しても、この `X` は減らない。構造変更で削れるのは、LCAを知るためのメタデータ処理、
経路Actionの復元、次世代表現の構築である。

### Action payload

Action が任意のbit列で、後から再計算できないなら、live な各辺について Action を最低1個保持する必要がある。
従って live payload の下限は概ね `E * S_A` byteである。

現行の世代blockは、既に子孫を失った Action も block解放まで保持するため、実際には `G * S_A` byteを使う。
探索木表現を変えても、世代blockをそのまま使うならこの差は残る。

### 親関係の情報量

次世代の子を親順に並べると、親番号は単調非減少になる。
各親の子数を `c_0, c_1, ..., c_(W_p-1)` とすると、次を満たす。

```text
c_0 + c_1 + ... + c_(W_p-1) = W_c
```

この親関係の総数は弱合成の個数である。

```text
C(W_p + W_c - 1, W_c)
```

従って必要bit数の下限は次になる。

```text
H_parent = ceil(log2 C(W_p + W_c - 1, W_c))
```

`W_p = W_c = W` なら、これは漸近的に約 `2W` bitである。
つまり、親関係そのものは子1個あたり約2 bitまで圧縮できる。

`uint32_t parent[W]` の `32W` bitは高速な直接参照のための冗長表現であり、情報下限ではない。
同様に、live treeの各辺へ64 bit ActionIdを再度並べる `tour` も、情報下限ではない。

### 1世代に必ず書くもの

次世代へ `W_c` 個を選ぶなら、少なくとも次を書く必要がある。

- 新しい Action payload: `W_c * S_A` byte
- 親関係: `H_parent` bit
- score、hash、履歴など、候補契約が要求する情報

過去の生存部分木全体に比例する `M_all` 個の参照を書き直す必要は、情報理論上はない。
従って、現行の `next_tour` に対する `8(M_all+Z)` byteの永続書き込みは、構造的に削減可能である。

## 基準実装のコスト分解

現行postorder方式の主要な常駐領域は次になる。

| 領域 | 代表的な量 |
|---|---:|
| Action世代block | `G * S_A` |
| current/next `tour` の論理size | 約 `8(M_all + Z)` ずつ |
| current/next `leaf` | 約 `4(W_p + W_c)` |
| `trace` | `8D` |

1世代の木管理では、少なくとも次の論理trafficがある。

- `next_tour` の永続書き込み: `8(M_all + Z)`
- `trace` から `next_tour` への参照読出し: `8(M_all + Z)`
- 次世代の復元で読むsurvivorに必要な有効 `tour`: `8M_K`
- ancestor suffixとtarget末尾Actionを `trace` へ書く量: `8F`
- rollback/apply時の `trace` 参照: `8X`
- `leaf` 境界の走査: `O(W_p)` 個の32 bit値
- Action payloadの参照: 少なくとも `X` 回

### 先頭の未参照prefix

現行は各世代の最初の `next_tour.insert()` を行ってから `next_leaf[0]` を記録する。
`copy_tour_path()` が読む最小indexは `leaf[0]` なので、`[0, leaf[0])` の `Z` 個は以後参照されない。

最初のinsertだけを省き `next_leaf[0]=0` とすれば、全境界から同じ `Z` が引かれる。
境界差、LCA距離、Action順は変わらないため、この `8Z` byte writeは無条件に削減できる。

ただしこのprefixには、世代開始時のtrace suffixが含まれ得る。
通常走査には不要でも、後述の遅延再構築で開始endpoint pathを復元する情報として再利用する余地はある。

`tour` と `trace` は連続配列なので、依存loadが少なく、hardware prefetchと広いcopy命令を使いやすい。
ここが、参照数だけを減らした構造が必ずしも勝たない理由になる。

## 案A: 世代別 parent-slot

### 表現

深さ `d` の node は、深さ `d-1` の親を1個だけ持つ。

```text
action[d][slot]
parent_slot[d][slot]
```

深さは配列indexから分かるため、永続node IDに世代番号を入れない。
深さ `d` の現在葉は、その世代blockのslotそのもので表せる。

次世代へ選ばれた子を現行と同じ親順、score順に並べ、子slot `j` に次を保存する。

```text
action[d + 1][j] = selected[j].action
parent_slot[d + 1][j] = selected[j].parent_leaf_slot
```

### 葉間移動

現在の状態に対応する経路を `trace_slot[depth]` に保持する。
次の対象葉を `v` とすると、次のように親を上る。

1. `v` のslotと `trace_slot[d]` を比較する
2. 異なる間、`v = parent_slot[d][v]` として `d--` する
3. 上る途中のslotを `trace_slot` の対象区間へ逆向きに書く
4. 一致した深さがLCAなので、現在の `trace_slot` のsuffixをrollbackする
5. 復元した対象suffixを順にapplyする

現在側の祖先は `trace_slot` に既にあるため、両方の親chainを上る必要はない。
LCPを使わない単純版の親loadは対象側のdownward suffix、すなわち `F` 回になる。
隣接LCPが既知なら各suffixの最後を読まずに止められるため、`Y=F-T` 回になる。

### byteと依存関係

この節の式はfrontierを `cand` から導出するcompact版を表す。実装済みoracleは独立 `frontier_slot` も書くため、
1世代のwriteとcurrent/nextの常駐量へそれぞれ `qW_c` と `2qW_c` を加える。

| 項目 | 量 |
|---|---:|
| 親とLCPの永続書き込み | `qW_c + l(W_c-1)` |
| oracleのfrontier書き込み | `qW_c` |
| 親の読出し | LCPなしは約 `qF`、LCPありは `qY` |
| `trace_slot` の書込み | 約 `qF` |
| `trace_slot` の状態操作用読出し | 約 `qX` |
| 常駐する親配列 | `qG` |
| current/next tour | 不要 |

`q=4` なら、親表現の永続書き込みを `8(M_all+Z)` から `4W_c` へ減らせる。
`M_all/W_c` が大きいほどbyte差が大きい。

一方、`parent_slot[d][v]` を読まないと次の世代のslotが分からない。
1本のtarget pathに沿った最大 `D` 回のloadは直列依存になり、prefetchしにくい。

### allocationとreclamation

Actionとparentを世代別slabに連続確保できるため、nodeごとのallocationは不要である。
確定接頭辞が進んだら、Action blockとparent blockを同時にslab poolへ戻せる。

ただし、子孫を失った古いslotのparentも `G` に残る。
現行の `tour` はliveな `E` だけを持つため、`G >> E` の探索では `qG` が不利になる。

### 反証

次の形では、parent-slotが一律に速いという主張は崩れる。

- 深い位置で葉が分岐し、各葉間移動で長いparent chainを辿る
- `tour` がLLCに収まり、連続copyが非常に速い
- State操作が軽く、parentの依存load latencyが支配的
- 枝の入れ替わりが多く `G >> E` になる

従って、parent-slotは有力な比較案だが、無条件の置換ではない。

## 案B: adjacent-leaf LCP front coding

### 表現

世代開始endpointのpathは `trace_slot` に保持し、現在の葉を現行と同じopen DFS順に並べる。
endpointから各target葉へ移るたびに次を保存する。

```text
lcp[i]    : 直前の葉とtarget葉の共通接頭辞深さ
suffix[i] : 深さ lcp[i]+1 からtargetの親までのancestor slot列
leaf[i]   : target末尾のAction slotは既存candから得る
```

最初のfull pathは既に `trace_slot` にあるため、streamへ重ねて保存しない。
現行の `leaf[0]` より前にある未参照prefix `Z` も出力しない。

全 `W_p` 葉をeagerに残すancestor suffixのslot数を `M_all` とすると、常駐量は次になる。

```text
qM_all + l(W_p - 1) + qD
```

深さから世代が分かるため、suffixには64 bit ActionIdではなく、その世代内のslotだけを置ける。
`q=2` は全世代のslot数が65535以下と検証できる場合だけ使い、それ以外は `q=4` とする。

### 葉間移動

葉 `i-1` から `i` へ移るときは、次を行う。

1. 現在深さから `lcp[i]` まで `trace_slot` をrollbackする
2. `suffix[i]` を順に読み、`trace_slot` へ書きながらapplyする
3. `cand[i]` の末尾Actionを `trace_slot` へ書いてapplyする

LCA計算も親chain loadも不要であり、すべて連続streamになる。

### byteと依存関係

| 項目 | 量 |
|---|---:|
| eager streamの永続書き込み | `qM_all + l(W_p - 1)` |
| survivor巡回時のsuffix読出し | `qM_K + lT` |
| `trace_slot` の書込み | `qF` |
| current/next stream | 各世代の `qM + l(W-1)` |
| 直列parent load | なし |

`q=4, l=2` なら、現行の64 bit tourと32 bit leaf境界よりほぼ半分の参照帯域にできる。
`max_turn <= 65535` が保証できない場合は `l=4` にする。

### eager構築

現行と同じく全 `W_p` 葉を展開しながら、直前の葉のdownward suffixを次のstreamへ出力する。
最初の葉より前は将来参照されないため、LCP境界0だけを置いてsuffixを出力しない。

以後は現行の `leaf` 境界差からLCPとsuffix長を得られる。
候補選択を待たないため、開始traceが上書きされる前に必要情報を保存でき、案Cの情報欠落を起こさない。

### allocationとreclamation

current/nextの2本のbyte streamをdouble bufferにし、容量を再利用する。
Actionの世代blockは現行と同じであり、確定接頭辞まで進んだblockだけを解放する。

### 反証

この方式は `M_all` 個のslotを毎世代書く点を変えていない。
従って、`M_all/W_c` が大きくstream構築が支配項なら、parent-slotより構造的に弱い。

また、これは現行postorderの情報を別形式にfront codingしたものであり、状態操作 `X` は減らない。
主な利益はID幅、LCP幅、decoder単純化である。

一方、連続アクセスを維持するため、最初に試す低リスク案としては強い。

## 案C: 条件付きsurvivor-late front coding

### 狙い

現行は `W_p` 葉を展開している最中に、全葉のpathを `next_tour` へ出力する。
選択された `W_c` 個の子が使うdistinct parentは `P` 個だけである。

```text
P <= min(W_p, W_c)
```

選択後にsurvivor parentと走査終了endpointだけを残せれば、有効slot数を `M_all` から `M_K` へ減らせる。
ただし旧 `tour`、`leaf`、`cand` だけから常に再構築できるという前提は成立しない。

### 情報が失われる反例

open postorderは、世代開始時にStateがいるendpoint pathを `trace` にだけ保持し、`tour` から省く。
走査中に `trace` は次の葉のpathで上書きされる。

```text
root直下の葉: A, B
世代開始位置: B
走査終了位置: A
survivor parent: B
```

開始時のB pathが旧 `tour` に存在せず、終了時の `trace` はAを表すなら、終了後の3構造だけではBを復元できない。
深い木では、`cand` が末尾Actionを持っていても、その上の開始endpoint固有suffixが同様に失われる。

終了endpointを一時葉として追加する処理は、次世代最初の移動には必要である。
しかし、既に失われた開始endpointの情報は復元しない。

### 成立させる追加情報

次のいずれかが必要になる。

1. 世代開始時に未確定 `trace` の `H` slotをsnapshotする
2. 各trace slotを初めて上書きする直前に、失われるsuffixだけをlazy保存する
3. 全nodeのparent metadataを別に保持する

snapshotは確実だが毎世代 `qH` byteを書く。
lazy版の保存量を `J` とすれば `0 <= J <= H` だが、選択結果は未知なので失われるslotを先に保存する必要がある。

parent metadataを持つ第3案は、実質的に案DまたはEとの併用になる。
現行の未参照prefix `Z` に開始traceの一部は入るが、一般に全 `H` slotの代用になるとは限らない。

### 条件を満たす手順

1. 開始traceの必要suffixをsnapshotまたはlazy保存する
2. 現在葉へ処理順ordinal `0..W_p-1` を割り当てる
3. 選択後にdistinct survivor parentを昇順で取り出す
4. 走査終了endpointを加えて `K=P` または `P+1` 葉にする
5. 退避suffixと旧streamから `M_K` 個のslotと隣接LCPを再構築する
6. 旧parent ordinalをdense ordinalへ単調にremapする
7. 現行と同じ順でActionを次世代blockへ移す

走査終了endpointがsurvivorなら `K=P`、そうでなければ `K=P+1` になる。
endpointには子候補を対応させず、次の再構築で自然に消す。

`used_parent[W_p]` をepoch stampでmarkし、ordinalを昇順に走査すれば単調な `old_to_dense` を作れる。
元の `next_beam` 順のままparentだけを書き換えれば、候補本体の順序を先に変える必要はない。

### exact semantics

旧ordinalからdense ordinalへの写像を単調増加にする。

```text
old parent:   2, 2, 8, 11, 11
dense parent: 0, 0, 1,  2,  2
```

現行と同じsort入力、比較関数、tie処理を使えば、親group順とgroup内順を保てる。
終了endpointをordered frontierの同じ位置へ入れれば、次世代最初のrollbackとapplyも変わらない。

開始trace保存はmetadata copyだけに限定する。
State操作やAction accessを先行させなければ、候補評価順、threshold更新、終了判定は変わらない。

### 論理traffic

現行の構築writeは次になる。

```text
8(M_all + Z) + 4W_p
```

slot-only LCP streamをlazy保存 `J` と組み合わせた概算writeは次になる。

```text
qJ + qM_K + l(K - 1) + O(4(P + W_c))
```

最後の項はepoch mark、dense remap、候補parent書換えである。
array全体をclearすると追加で `O(4W_p)` writeになるため、epoch stampが必要になる。

再構築readは少なくとも次を含む。

```text
old stream slots: 8M_K または元表現のslot幅分
saved start suffix: qJ
leaf boundary: O(4W_p) worst case
selected candidates: O(W_c)
```

ordinalが単調なので境界scanは重複させずに済むが、選択後のsecond pass自体は消えない。
完成後の巡回は連続streamなのでdependent parent loadを追加せず、State操作 `X` も現行と同じになる。

### 常駐量とpeak

steady topologyは概ね `qM_K + l(K-1)` byteになる。
Action payloadの `G*S_A` byteは現行の世代blockを使う限り変わらない。

変換中は旧stream、新stream、開始suffix、mark/remap配列を同時に持つ。

```text
peak topology ~= old_stream + qM_K + lK + qJ + O(4W_p)
```

flat bufferを再利用してもcapacityは過去の `M_all` peakを保持し得る。
論理size削減とallocatorへ返る物理memoryは分けて測る必要がある。

### reclamation

終了endpointとsurvivor parentの共通接頭辞より上へ、次世代で戻ることはない。
このLCPまでprefixを確定できるが、endpointから最初のsurvivorへ動くための辺は先に解放できない。

開始trace snapshotは再構築完了後に捨てられる。
Action世代blockは現行と同じprefix単位で解放され、dead Actionの個別回収は行わない。

### worst case

- 開始traceを保存しない実装は上のA/B反例で不正になる
- `J=H` が毎世代続くとsnapshot writeが深さに比例する
- `P ~= W_p` かつ `M_K ~= M_all` ではsecond passとremapだけが増える
- 生存部分木が浅いと削れるstream writeが少ない
- 旧stream再読がLLC missになると、eagerなL1/L2 copyへ負けやすい
- 終了endpointが長い固有枝を持つと `M_K` が期待より減らない
- `used_parent` の全clearを行うと、`P` が小さくても `O(W_p)` writeが残る

従ってこの案は、追加情報なしでは不成立であり、無条件の第1候補にはできない。
`J`、`M_K/M_all`、second-pass missを測り、保存cost込みで利益が残る場合だけ有効になる。

## 案D: parent-slot + 世代別SoA + LCP補助

案Aを実装用に具体化し、metadataを完全なSoAへ分離する。

```text
GenerationBlock {
    Action actions[W_d]
    Slot parent[W_d]
}

FrontierBlock {
    Lcp adj_lcp[W_d]
}
```

score、hash、history ID、候補の一時情報をこのblockへ混ぜない。
探索木を移動するときに触るのは `actions`、`parent`、現在frontierの `adj_lcp` だけにする。

過去世代のLCPはparent chain復元に不要である。
currentとnextのFrontierBlockだけをdouble bufferにすれば、LCP常駐量は `lG` ではなく `O(lW)` になる。

### LCP補助の役割

parent chainを辿ればLCAは分かるため、LCPは正しさには不要である。
それでも、隣接葉のLCPを `uint16_t` または `uint32_t` で持つと次が可能になる。

- rollback回数を先に確定する
- 現在側とtarget側のslot比較branchを消す
- 全葉の共通接頭辞を端またはLCP最小値から求める
- instrumentationで葉間距離を直接集計する

既知のLCP深さを `h`、target suffix長を `r` とする。
target leafから深さ `h+1` のslotまで復元すれば、深さ `h` は現在の `trace_slot[h]` と同じだと分かる。

従ってparent loadは `r` 回ではなく `r-1` 回で済む。
兄弟間の遷移は `r=1` なのでparent loadが0回になり、LCPなしの場合より遷移ごとに終端の1 loadを削れる。

LCPは内部の `r-1` 個の直列依存loadまでは消さず、追加の `lW` byteを使う。

### windowed ancestry decode

次の `B_w` 葉についてLCP停止深さを先に読み、target suffixをState操作前にround-robinで復元できる。
各chainは1段前のparent結果へ依存するが、異なる葉のchainは独立している。

```text
for each window of B_w target leaves
    while unfinished chain exists
        issue one parent load for each ready chain
    visit the B_w leaves in the original order
```

総parent load数は `Y=F-T` のままである。
同時に未完了なchain数が十分あれば、露出するLLC miss latencyを最大 `B_w` 本へ分散できる。
実際の上限はCPUのload buffer、MSHR、memory-level parallelismで決まる。

復元したslotを短命scratchへ保存し、後で元順に読む。
既存traceと領域を共有できない部分について、追加trafficは最大で概ね次になる。

```text
scratch write + read <= 2qF
scratch peak <= qB_wD
```

候補は既に確定したcurrent frontierから読むだけであり、先読み中にStateやActionへ触れない。
その後のrollback、apply、`enumerate_actions()` を元順に実行すれば探索意味論は変わらない。

比較するwindowは `B_w=1,4,8,16` とする。
`B_w=1` は通常decodeで、値を大きくすると最初の葉までの待ち時間とscratch footprintが増える。

次の条件では負ける。

- 兄弟が集中して `r=1` が多く、そもそもparent loadが0回
- suffixが短く、各windowで同時に未完了なchainがほとんどない
- parent配列がL1またはL2に収まり、scratch trafficだけが増える
- `B_wD` が大きく、scratchがL1を圧迫する
- succinct mapでselect decodeがALU律速になり、隠すcache missがない

これはparent-mapを置き換える永続構造ではない。
direct parentとsuccinct mapの両方へ適用できる、epochal hybridのdecode policyとして独立に評価する。

### slab layout

世代blockを次の領域に分ける。

1. trivial metadata用のbyte slab
2. `Action` 用の適切にalignしたuninitialized slab
3. 構築済みAction数

候補確定時にActionをplacement moveし、block解放時に構築済みActionだけをdestroyする。
世代ごとの `vector` objectとcapacity metadataを減らし、固定数のslabをring再利用する。

### reclamation

全世代block解放は現行と同じ共通接頭辞単位で行う。
さらに各nodeへ `child_count` を追加すれば、子を1個も残せなかった葉から親へ0参照を伝播できる。

```text
parent[slot]      : q byte
child_count[slot] : 2または4 byte
```

これにより死んだActionを直ちにdestroyでき、Actionがheap payloadを所有するときは有効である。
ただしinline slabのcapacityとslot番号は縮まらない。物理compactには子孫のparent slot更新が必要になる。

### 反証

- 小さなtrivial Actionでは、SoAとslab管理の分岐が節約量を上回り得る
- LCPを追加してもsuffix内部のparent依存loadは消えない
- child countを入れるとnodeあたり2--4 byte増え、全nodeへの更新が必要になる
- inline Actionでは個別destroyしてもRSSやcache footprintを縮められない

従って、最小構成は `Action[] + uint32_t parent[]` とし、LCPとchild countはpolicyで切り替えるべきである。

## 案E: frontier順配置とsuccinct parent map

### frontierとAction slot

前世代frontierを `0..W_p-1` の親ordinal順に置く。
選択済みの子も、現行と同じparent、score、tieの順に並べて `0..W_c-1` のordinalを振る。

```text
action[d][j] = frontier d の子 j を作った Action
```

これによりnode IDは `(d, j)` だけになり、Action slotとfrontier ordinalが一致する。
既存と同じsort入力と比較関数でdescriptorを並べてからActionを移せば、同点を含む順序も再現できる。

### unary child-count bitvector

親 `i` の選択された子数を `c_i` とし、世代間写像を次のbitvector `B_d` で表す。

```text
B_d = 1^c_0 0 1^c_1 0 ... 1^c_(W_p-1) 0
```

長さは厳密に `W_p + W_c` bitになる。
ゼロ子の親も1個の `0` を置くため、親ordinalの穴は失われない。

`select1(B_d, j)` を0-originでj番目の `1` のbit位置とする。
その `1` より前には `j` 個の `1` と、親ordinal個の `0` があるため、写像は次で復元できる。

```text
parent(j) = select1(B_d, j) - j
```

`W_p = W_c = W` なら親写像はちょうど `2W` bit、すなわち `W/4` byteになる。
`uint32_t parent[W]` の `32W` bitと比べると16分の1である。

### sparse世代のElias--Fano

`parent[0..W_c)` 自体が `[0, W_p)` 上の単調非減少列なので、Elias--Fanoでもexactに保存できる。
`W_p >= W_c` の疎な場合、概算bit数は次になる。

```text
W_c * (floor(log2(W_p / W_c)) + 2)
```

unaryの `W_p + W_c` bitと違い、子を持たない親の長い範囲を1 bitずつ保存しない。
high部分のselectとlow部分のloadから `parent(j)` を復元でき、逆向き逐次cursorも構成できる。

ただし1回のdirect parent loadを、select、shift、mask、low loadへ置き換える。
圧縮でcache missが減らない範囲では、bit数が少なくても速度は遅くなり得る。

世代ごとに次の表現byteを計算し、最小候補を選ぶhybridは可能である。

- 16 bit direct parent
- 32 bit direct parent
- unary child-count
- Elias--Fano

速度優先ならbyte最小を即採用せず、幅比とCPU別benchmarkから決めたpolicyを使う。

### adjacent LCPとの併用

現在frontierについて、隣接する葉とのLCP深さだけを `adj_lcp[j]` に保存する。
同じ親の連続する子ならLCPは `d-1` である。

親が変わる境界では、前世代の `adj_lcp` の対応区間の最小値が新しいLCPになる。
子の親ordinalは単調なので、前世代の境界を一方向に走査すれば全LCPを `O(W_p + W_c)` で作れる。

LCPから次の葉へ移るrollback深さは直ちに分かる。
LCP深さを `h`、target suffix長を `r` とすると、深さ `h+1` のslotまでを `r-1` 回のparent decodeで復元する。
深さ `h` は現在のtraceと同一なので、`h+1` から `h` への最後のdecodeは不要になる。

兄弟遷移は `r=1` なのでdecodeが0回になる。
一方、長いsuffix内部の `r-1` 回は直列のまま残る。

currentとnextだけにLCPを置けば常駐量は概ね `l(W_p + W_c)` byteであり、`lG` byteにはならない。

### 逆向き走査

論理frontierは親ordinalの昇順に置き、現行と同じく展開は末尾から先頭へ行う。
`parent(j)` は単調非減少なので、子ordinalを降順に問い合わせると親ordinalも単調非増加になる。

この性質は世代を合成しても保たれる。
各世代へ逆向きcursorを1個置けば、通常の葉巡回ではbitvectorを後方へstream decodeできる。
同じ親が続く区間は直前のdecode結果を再利用する。

ただし問い合わせるordinalの間にdead slotが多いと、cursorはそのgapも通過する。
1回のfrontier巡回で全active世代を最大約 `2G` bit走査し得るため、`G >> E` ではcheckpointからjumpする必要がある。

survivor parentだけへdense remapする案Cでも、remapを単調増加にすれば向きは変わらない。
選択されないendpointを残す場合も、ゼロ子親として対応位置に `0` を置けばexactなordinalを保てる。

### random selectの実装候補

最終経路復元、debug、順不同アクセスにはcursor以外の `select1` が必要になる。
例えば256個の `1` ごとにbit位置を保存する。

| checkpoint幅 | `W_c` 個の子に対する概算overhead |
|---|---:|
| 32 bit | `4W_c/256 = W_c/64` byte |
| 64 bit | `8W_c/256 = W_c/32` byte |

checkpointから64 bit wordをpopcountし、対象の `1` を含むwordまで進む。
word内は小さいrankなら `x &= x - 1` とctz、x86 BMI2ならpdepとctzを候補にできる。

pdepのlatencyとthroughputはCPU依存であり、常に高速とは限らない。
runtime dispatchと対象CPUでのbenchmarkが必要で、非BMI2環境にはbroadwordまたはtable fallbackを残す。

`1` だけを256個ごとにsampleしても、長いゼロ列を跨ぐword数はworst caseで制限できない。
random accessを保証するなら、wordごとのrank directoryや `0` 側のsampleも必要になる。
通常走査はcursor、例外経路だけindexed selectとする分離が最も自然である。

### byteと依存load

| 項目 | 量 |
|---|---:|
| 1世代の親写像write | `(W_p + W_c) / 8` byte |
| 幅が同程度のactive世代 | 約 `2G` bit、すなわち `G/4` byte |
| direct parentとの比較 | 約 `G/4` byte 対 `4G` byte |
| Action pathの復元 | 深さ方向に直列のselectまたはcursor decode |

bitvectorがcacheへ収まりやすくなる利点はあるが、深さ `d` のparent結果が深さ `d-1` の問い合わせになる。
この世代間依存は残り、selectのALU命令とbranchがdirect `uint32_t` loadへ追加される。
逆向きcursorはrandom selectを減らすが、この深さ方向の依存chainは減らさない。

### allocationとreclamation

世代ごとに64 bit word列へappendし、Action slabと別のring blockで再利用する。
確定接頭辞より古い世代のmapはblockごと解放でき、nodeごとのallocationやdestructorは不要である。

途中の `1` を個別に消すと、それ以後のchild ordinal、Action slot、子孫参照がすべて変わる。
従ってdead childだけをcompactせず、reclamation単位は世代全体に固定する。

### exactness

次を守れば探索意味論を変えない。

- 候補のsort入力、比較関数、tieの扱いを現行と同じにする
- sort後のordinalをAction slotとして使う
- ゼロ子親を省略しない
- dense remapを単調増加にする
- 既存と同じ逆向きordinal順で展開する
- `adj_lcp` は移動量の算出だけに使い、候補順を変えない

bitvectorは単調なparent配列の別符号化にすぎず、これらの条件下ではAction列とState遷移をexactに保てる。

### 反証

- `W_c << W_p` ではゼロ子親の `W_p/8` byteが支配する
- 32 bit direct parentが小さい条件は `4W_c < (W_p+W_c)/8`、すなわち概ね `W_p > 31W_c` になる
- sparse世代では16/32 bit direct、unary、Elias--Fanoを世代ごとに選ぶ方がよい
- Elias--Fanoはゼロ子親の空間を避けるが、selectとlow decodeがdirect loadより遅くなり得る
- target leafをランダム順に問い合わせるとcursorを使えず、select indexとdecode costが増える
- `W_c <= 65535` なら16 bit parentが使え、圧縮率は8分の1まで下がる
- State操作が重い場合、metadataの数byteを削っても全体速度への効果は小さい
- `G >> M_all` ではdead slotを含むmapが約 `G/4` byte残り、live `tour` の約 `8M_all` byteを上回り得る
- 特に `G > 32M_all` ではparent map単体が1本の `8M_all` byte tourより大きい

Action payloadも `G * S_A` byte残るため、succinct parent mapだけでは `G >> E` の根本原因を解消しない。
この方式はmetadata帯域が律速するbackend候補であり、direct parentとの実測比較なしに既定方式とは決められない。

## 案F: threaded ancestry

### 設計1: jump parent

各nodeへ `2^k` 個上の祖先を指すjump pointerを追加し、LCAまたは共通接頭辞を速く求める。

これは採用しない。LCAが分かっても、その間のActionを1個ずつrollback/applyする必要がある。
jump pointerは `O(G log D)` の書込みと常駐量を追加し、状態操作を減らさない。

### 設計2: B段 ancestry packet

各nodeへ直近 `B` 段のslot列を持たせ、target pathをpacket単位で読む。
依存load深さを約 `1/B` にできるが、分岐する子ごとに親packetをpath copyする必要がある。

平均すると子1個につき `O(Bq)` byteを書き、`B` を大きくすると通常の履歴copyへ戻る。

### 設計3: leaf transition thread

各隣接葉について、次を直接持つ。

```text
up_count
down_suffix_offset
down_suffix_length
```

down suffixを連続配列へ置けば依存parent loadは消える。
しかし全down suffixに現れるslot総数は `E` であり、案BのLCP front codingと同じになる。

### 反証

threadが減らせるのは祖先探索であり、辺単位Actionの読出しと状態操作ではない。
祖先情報を事前展開して依存loadを消すほど、書込み量はpostorder/front codingへ近づく。

従ってthreaded ancestryは独立した最良点を作らず、次の両端の中間になる。

- 少ない書込みと依存loadを選ぶparent-slot
- `O(E)` 書込みと連続readを選ぶLCP front coding

## 案G: persistent/chunked postorder

### 表現

postorder streamを `B` 個のslot参照を持つimmutable chunkへ分ける。
次世代streamは、変化しないchunkを参照し、部分的に変わるchunkだけをcopy-on-writeする。

```text
Chunk {
    Slot refs[B]
    RefCount refs
}

Tour {
    ChunkRef chunks[]
}
```

最大retained spanを直接参照するropeまたはpiece tableにすれば、chunk境界に揃わない範囲も共有できる。

### byte

完全に再利用できるchunk数を `C_reuse`、copyが必要な参照数を `E_copy` とする。

```text
write ~= qE_copy + sizeof(ChunkRef) * number_of_output_chunks
```

lineageが安定し、長い内部pathが世代間でそのまま残るなら `E_copy << E'` になり得る。

### allocationとreclamation

chunkはarenaから確保し、世代またはrefcountで解放する。
refcount更新を避けるなら、epochごとのarenaを持ち、どのpatchからも参照されなくなったepochをまとめて捨てる。

古いchunkを直接参照する限り、そのepoch全体を解放できない。
小さなspanが1個残っただけで大きなchunkまたはarenaが保持されるretentionが起きる。

### 反証

次のadversarial patternでは共有が崩れる。

- 生存葉が1個おきに入れ替わり、各chunkが部分的にしか残らない
- 新しい子Actionが短い間隔でstreamへ挿入される
- 最大共有spanが小さく、descriptor数 `R` が `Theta(E')` になる

このとき参照descriptor、pointer chasing、refcount、partial chunk copyが純増し、平坦配列より遅い。

固定chunkは `B` が小さいとdescriptor過多、大きいとpartial copyとretentionが増える。
汎用の既定構造ではなく、lineage安定度を測定して有効化するpolicyになる。

## 案H: 差分patch付きtour

### 表現

次世代tourを完全にmaterializeせず、前世代tourに対するedit scriptとして持つ。

```text
Copy(base, begin, length)
Insert(new_slot_sequence)
Skip(length)
```

現行の `next_tour` 構築時に行う各path copyを、実copyではなく `Copy` descriptorとして記録する。
新しいAction slotだけを `Insert` する。

### byte

retained range数を `R` とすると、descriptorを16 byteに収めた場合の永続書込みは概ね次になる。

```text
16R + qW_c
```

`R << E'/2` なら64 bit flat tourの `8E'` より小さい。

### traversal

patchを順に解釈し、base tourのspanとinsert spanを読み分ける。
patchを何世代も重ねると、1個の参照を読むために複数世代のpatchを再帰的に辿る可能性がある。

対策は次のいずれかになる。

- `K` 世代ごとにflattenする
- patch DAGをB-treeまたはropeへ入れる
- patch depthが閾値を超えたrangeだけmaterializeする

### allocationとreclamation

patchがbaseを参照する間、base bufferを解放できない。
世代blockのActionだけでなく、古いtour bufferもpatch chainの寿命まで残る。

flattenは古いbufferをまとめて解放できるが、その世代で `qE` のcopy spikeを発生させる。

### 反証

- `R = Theta(E')` ならdescriptorがflat slotより大きい
- patch depthが増えるとbranchとpointer chasingが支配的になる
- 古いbaseのretentionでpeak memoryが現行double bufferを超え得る
- periodic flattenにより、平均帯域が想定ほど下がらない場合がある

差分patchは、親系統が非常に安定し、長いrangeをそのまま共有できると測定できた場合だけ有望である。

## 案I: live nodeだけを持つarena tree

parent、最初の子、次の兄弟、子数を持つ明示treeを、SoA arenaへ置く。
枯れた葉から親へ参照数を減らし、live nodeだけをfree listで再利用する。

```text
action[node]
parent[node]
first_child[node]
next_sibling[node]
child_count[node]
```

### 利点

- payloadを `G` ではなくlive `E` に近づけられる
- `next_tour` の再構築が不要
- 枯れたActionを直ちにdestroyできる

### 不利

- metadataだけでnodeあたり最低16 byte程度増える
- DFSがparent/child/siblingの依存loadになる
- free listとtree surgeryが必要になる
- ActionをSoA分離しても、複数配列を同時に触る

### 反証

Actionが小さく、`G` と `E` の差も小さい場合、reclamation利益がない。
その場合は平坦streamの局所性に対し、metadataと依存loadが純増する。

この案は大きなheap-owning Action、深い未確定履歴、激しい枝の入替えでだけ再評価する。

## 案J: uncomposed path-chunk radix rope

### 表現

探索木の最大unary chainを、Actionを合成せずに連続列として保持する。

```text
PathChunk {
    parent_chunk
    start_depth
    length
    capacity
    refcount
    Action actions[length]
}
```

各frontier leafはtail chunkとchunk内の終端offsetを指す。
隣接葉のLCP深さは別のflat配列に保持する。

この構造はAction列を持つradix treeであり、`apply_op()` と `rollback()` は従来どおり辺ごとに呼ぶ。
Action合成やState snapshotを要求しない。

### 次世代更新

選択後の各parentについて子数を確定してから更新する。

- 子0個ならleaf参照を外し、refcountが0になったchunkを親方向へ解放する
- 子1個でtailがuniqueかつ容量内なら、新Actionをtailへappendする
- 子1個でも容量不足なら、新しい継続chunkを1個作る
- 子2個以上ならparent tailをimmutableにし、各子へ新しいchild chunkを作る

Action payloadの必須writeは `W_c*S_A` byteだけである。
topology writeは新しいchunk headerとrefcount更新になる。

ただし走査終了endpointには、Stateが次世代最初の葉へ移るまで一時的な `state_ref` を残す。
子0個だからと即時解放すると、最初のrollbackに必要なActionを失う。

```text
topology write ~= C_new*S_H + refcount_updates
```

長い1子連鎖では `C_new << W_c` にできる。
毎段分岐では各新Actionがlength 1 chunkになり `C_new ~= W_c` まで退化する。

### traversal

target leafとadjacent LCPから停止深さ `h` は既知になる。
tail chunkから、深さ `h+1` を含むchunkまでparentを辿り、chunk列だけを短命scratchへ逆順に積む。

その後は各chunk内のActionをforwardに連続readしてapplyする。
rollback側は現在pathのchunk stackを使い、chunk内をreverseに読む。

辺単位のState操作 `X` は変わらない。
依存parent loadは辺数 `Y` ではなく、跨いだchunk境界の総数 `C_path` になる。

```text
C_path <= Y
```

平均live chunk長 `E/C_live` が大きいほど差が出る。
chunk境界chainにもwindowed decodeを使えるが、境界数が少ない場合はwindow overheadが勝つ。

### payloadとtopology memory

dead branchをrefcountで直ちに解放できれば、Action payloadは `G*S_A` ではなく次へ近づく。

```text
live payload ~= (E + V) * S_A
topology ~= C_live*S_H + (q+l)W_p
```

`V` は固定長chunkの未使用slot数である。
分岐で半端なtailをsealすると、1分岐あたり最大でchunk容量未満のslackが残る。

可変長chunkは `V` を減らせるが、tail append時のreallocationと既存Actionのmoveを発生させる。
固定長arenaはreallocationを避ける代わりにslackとchunk境界を増やす。
各chunkを個別にheap allocationすると定数倍が大きいため、固定長またはsize-class別slabとfree listが前提になる。

### COWと後の再単一化

分岐時点のparent chunkは複数childから共有されるためimmutableにする。
child Actionは別chunkへ置き、共有chunk全体をpath copyしない。

後で兄弟が消えてrefcountが1へ戻っても、parentとchildの境界は自動では消えない。
そのままなら正しいが、過去の分岐点がchunk境界として蓄積する。

再結合すれば `C_path` を減らせるが、Action列のcopyまたはmove、allocator traffic、参照更新が必要になる。
非trivial Actionではdestructorと例外安全も必要になり、毎世代の再結合は避けるべきである。

再結合はepoch boundaryで、copy byteが将来の推定parent-load削減を下回る場合だけ行う。

### prefix切断とreclamation

single-threadの探索ならrefcountは非atomicでよい。
leaf参照、child chunk参照、走査終了endpointの `state_ref` を区別する。
次のState移動で不要になった参照を外し、0になったchunkを親へ再帰的に解放する。

確定prefixの深さがchunk境界なら、親chainを切って通常どおり解放できる。
chunk内部なら次のいずれかが必要になる。

- prefix Actionをresultへ移し、残りsuffixを新chunkへmoveする
- chunkにbegin offsetを持たせ、storageは参照が消えるまで保持する
- prefix確定をchunk境界まで遅らせる

1番目はcopy traffic、2番目はretention、3番目は早期解放の遅延になる。
最終State構築でprefix Actionをrollbackする契約があるため、確定Action自体はresult側に残す。

### exact semantics

physical chunk順とlogical frontier ordinalを分離する。
候補sort後のordinal、endpoint、adjacent LCPを現行と同じにし、chunk walkはAction pathの復元だけに使う。

chunkのappend、seal、free、再結合はgenerationまたはepoch boundaryで行う。
State操作中にAction storageを動かさなければ、rollback、apply、enumeration順をexactに保てる。

### worst case

- 毎段分岐すると全chunkがlength 1になり、parent-slotより大きいheaderとallocationが残る
- 固定長chunkを分岐直後にsealし続けると `V*S_A` がlive payloadを上回り得る
- 可変長tailのappendが幾何growthでないと、長いunary chainを二次量copyする
- 分岐後に再び1子へ戻るpatternでは、再結合しない限り境界が蓄積する
- 再結合を急ぐと、branch churnごとにAction列をcopyする
- prefixが毎回chunk内部へ進むと、split copyまたはretentionが毎回発生する
- 小さいtrivial Actionでは `S_H`、refcount、free listがpayload節約を上回る
- Action moveが高価または例外を投げる型では、COWと再結合の実装costが高い

### 他方式との差

postorder chunk案Gは、世代ごとの走査stream spanを共有する。
案Jはroot-to-leafのAction pathを共有し、死んだpayload自体を回収するため、共有軸が異なる。

parent-slotはnodeごとに1 parentを持ち、payloadを世代blockへ残す。
案Jはchunkごとに1 parentを持ち、長いunary chainの依存loadとdead payloadを同時に減らせる。

epochal hybridでは平均chunk長、branch churn、slack率、prefix split byteを使って案Jの継続可否を決める。
`E/C_live` が大きく `G/E` も大きい期間では独立価値がある。

一方、毎段分岐する汎用worst caseではparent-slotに支配される。
既定backendではなく、radix epochを長く維持できるworkload向け候補になる。

## 案K: bidirectional anchor stream + parent overlay

### 片方向baseが不成立になる理由

現行は `cand` をparent ordinal昇順へsortし、末尾から先頭へ展開する。
展開順に次世代parent ordinalを振るため、同じanchorから見たsnakeの向きは世代ごとに反転する。

```text
overlay depth 1: anchor ordinal 2 -> 1 -> 0
overlay depth 2: anchor ordinal 0 -> 1 -> 2
```

片方向front-coded streamは、直前葉から次葉へ必要なtarget suffixだけを持つ。
配列を逆から読むだけでは逆方向targetのsuffixにならないため、1本のbase streamを `B_o>1` 世代使い回せない。

full Euler event列を両方向にscanすれば木は辿れるが、skipped anchorのsubtreeまでStateで出入りする。
これはsimple pathよりrollbackとapplyを増やし、同一のState呼出し列を壊すため採用できない。

### 成立する二層表現

anchor深さ `a` でactive frontierをordinal `0..W_A-1` に並べ、次を保持する。

```text
AnchorBase {
    forward_suffix_stream
    reverse_suffix_stream
    adjacent_lcp
    forward_offsets
    reverse_offsets
    state_endpoint_ordinal
}

OverlayGeneration[d] {
    parent_ordinal
    Actionは既存generation block
}
```

forward streamはanchor ordinal増加方向、reverse streamは減少方向のtarget suffixを持つ。
隣接LCPは方向に依存しないため共有できる。

各streamはsuffix offsetと到達深さを持つ。
ordinal区間のLCP最小値を求め、区間内で初めて現れる各深さのslotだけを集めれば、遠いtargetのsuffixを復元できる。
これは現行 `copy_tour_path()` のrank更新を各方向へ用意するのと同じである。

各overlay frontierも親順にordinalを振る。
childからparentへの写像は単調であり、`k<=B_o` 世代合成した `anchor_of(child)` も単調になる。
世代ごとにorientation bitを持ち、その世代のanchor問い合わせが増加か減少かを選ぶ。

current frontierのadjacent LCPは案Eと同じ単調区間scanで作り、絶対depthを保存する。
これにより各遷移で `h>=a` か `h<a` かをparent decode前に判定できる。

### 葉間遷移

現在深さを `d=a+k`、targetとのLCP深さを `h` とする。

`h>=a` なら同じanchor nodeの子孫である。
現在pathを `h` までrollbackし、最大 `k` 段のoverlay parentをdecodeしてtarget suffixをapplyする。

`h<a` ならanchor nodeが異なる。

1. current overlay suffixを深さ `a` までrollbackする
2. target leafから最大 `k` parentを辿ってtarget anchor ordinalを得る
3. ordinal方向に対応するbase streamでcurrent anchorからtarget anchorへのsimple pathを復元する
4. base内を `h` までrollbackし、target anchorまでapplyする
5. target overlay suffixを深さ `d` までapplyする

base streamのrange queryは、間にあるanchorをStateで訪れず、必要なtarget suffixだけを復元する。
従って辺単位のState操作列は元のleaf間simple pathと一致する。

anchor問い合わせは1世代内で単調なので、skipped ordinalのboundary scan区間は重複しない。
Action slot readは必要suffixだけだが、indexを強化しない版では最大 `O(W_A)` のboundary metadataを読む。

overlayのdependent parent chainは最大 `B_o` 段になる。
base部分は連続stream readであり、parent依存loadを持たない。

### flattenと開始trace

overlay深さが `B_o` へ達したgeneration boundaryで、current frontierを新しいanchorにする。
旧bidirectional base、全overlay map、current Stateのtraceを保持したまま、新しい両方向streamを構築する。

current traceは現在endpointのfull pathを持つ。
targetがcurrent anchorより小さければreverse、大きければforwardの旧baseを使い、任意のtarget pathを復元できる。

この条件なら、世代開始traceを別snapshotとして保存する必要はない。
新base完成前に旧baseまたはcurrent traceを破棄する実装では、案Cと同じ情報欠落が起きる。

新anchor ordinalは現行のlogical frontier順を保つ。
forwardとreverseをそのordinalの両方向へ生成し、endpoint ordinalも新baseへ移す。

### writeとread

幅を概ね `W` とし、1epochの各世代幅を `W_i` とする。
overlayの永続writeとflatten writeは概ね次になる。

```text
overlay write = q * sum(i=1..B_o) W_i
              + l * sum(i=1..B_o) (W_i-1)

flatten write = q(M_A^+ + M_A^-) + l(W_A-1) + offset indexes
```

1世代平均では次になる。

```text
qW + lW + (q(M_A^+ + M_A^-) + index_bytes) / B_o
```

片方向を仮定した `qW + qM_A/B_o` ではなく、snake対応のためbase stream係数は約2になる。

flatten readは旧base、overlay parent、current traceからlive pathを復元する量になる。
worst caseは `Theta(qE + qB_oW)` であり、1epochに1回発生する。

通常世代のpath復元readは最大 `B_o` parent load、LCP、必要時の片方向base suffixになる。
同じanchorに留まる遷移ではbaseを読まない。
単純なrange decoderでは、これに最大 `O(4W_A)` のboundary scanが加わる。

### memoryとreclamation

steady topologyは概ね次になる。

```text
q(M_A^+ + M_A^-)
+ l(W_A-1)
+ two offset indexes
+ q * sum(active overlay widths)
+ current/next LCP
```

幅一定ならoverlay parentは約 `qB_oW` byteになる。
flatten中は旧baseと新baseを同時に持つため、幅が同程度ならbase部分の一時peakは約 `4qM_A` になる。

flatten完了後は旧baseと古いoverlay parent blockを一括解放できる。
Action payloadは既存generation blockに残るため `G*S_A` のままであり、`G>>E` のpayload問題は解消しない。

epoch途中で確定prefixが進んだら、baseへ `min_depth` を設定してそれより浅いslotをquery対象から外す。
staleな参照wordは次のflattenまで残せるが、対応Action blockは将来読まないことを確認して解放できる。

### exact semantics

- anchor ordinalは現行のleaf順から変えない
- orientation parityに応じてforwardまたはreverseだけを選ぶ
- skipped anchorのActionをStateへ適用しない
- flatten中はmetadataだけを読み、StateとAction methodを呼ばない
- old base、overlay、current traceはnew base完成まで保持する
- endpoint ordinalとprefix解放深さを変換前後で一致させる

この条件ならrollback、apply、enumeration、候補sort、tie処理を変えない。

### worst case

- 片方向baseだけの版は2世代目のorientation反転で不成立になる
- `B_o=1` では両方向baseを毎世代書き、eager片方向streamよりほぼ2倍になる
- `M_A~=W` の浅い木では毎世代 `qW` overlayを加える余地がない
- `B_o` を大きくするとdependent chain、`qB_oW` memory、flatten burstが増える
- ほぼ全遷移が異なるanchorへ移ると、overlay decodeとbase readの両方を毎回払う
- anchor gapが大きいと、State操作は増えなくても `O(W_A)` boundary scanが残る
- widthやtree形状がepoch内で急変すると、flatten時の `M_A` 予測が外れる
- old baseを先に解放すると開始endpoint固有pathを再構築できない
- `G>>E` でAction payloadが支配するとtopology削減が全体memoryへ効かない

### backend-switch hybridとの差

案Kはepoch内で表現を選び直す方式ではない。
常にflat anchorと短いparent overlayを同時に持ち、依存深さを `B_o` へ制限する二層backendである。

backend-switchは変換後にsource表現を捨てる。
案Kはepoch中ずっとbaseとoverlayの両方を必要とし、flatten周期そのものが主要parameterになる。

## 主要方式の統一比較

以下では新しいAction payload `W_c*S_A`、Action参照 `X*S_A`、候補選択costを共通項として外す。
同じ葉順と同じstart、endを使う限り、各方式のState操作回数はすべて同じ `U`、`F`、`X` になる。

logicalなAction byte数は共通でも、gblock、stream、path-chunkでcache localityは異なる。
ActionのL1、L2、LLC missは共通項と決めつけず、実測表では別列に残す。

```text
X >= 2E - dist(start, end)
```

windowed decodeもState操作を先読みせず、metadataだけを先に読むため、この回数と順序を変えない。

### 永続write

| 方式 | 1世代のtopology write |
|---|---:|
| current postorder | `8(M_all+Z) + 4W_p` |
| survivor-late front coding | `qJ + qM_K + l(K-1) + O(4(P+W_c))` |
| direct parent derived + adjacent LCP | `qW_c + l(W_c-1)` |
| direct parent oracle + adjacent LCP | `2qW_c + l(W_c-1)` |
| unary parent + adjacent LCP | `(W_p+W_c)/8 + l(W_c-1) + index` |
| EF parent + adjacent LCP | `b_EF/8 + l(W_c-1) + index` |
| uncomposed path-chunk | `C_new*S_H + l(W_c-1) + refcount更新` |
| bidirectional anchor overlay | `qW_c+l(W_c-1)+(2qM_A+index)/B_o` 償却 |
| backend-switch hybrid | 選択backendのwriteと変換writeの償却和 |

`W_p >= W_c` のとき、Elias--Fano本体の概算bit数は次になる。

```text
b_EF = W_c * (floor(log2(W_p/W_c)) + 2)
```

currentの `Z` は無条件に省け、64 bit IDも深さ既知なら `q` byte slotへ変えられる。
従って新構造との厳しい比較基準は、currentを `qM_all + l(W_p-1)` へ縮めたeager streamである。

### 1世代の論理read/write

Action payload以外の作業用traceまで含めた概算を同じ形で書く。
`R_enc` はsuccinct decoderが実際に読んだbitvector、index、low bitsの合計byteとする。
`parent_bytes` はchunk IDまたはpointerの幅とする。

| 方式 | logical read | logical write |
|---|---:|---:|
| current | `8(M_all+Z)+8M_K+8X+O(4W_p)` | `8(M_all+Z)+8F+4W_p` |
| survivor-late | `source_width*M_K+qJ+qM_K+qX+O(4W_p+W_c)` | topology式に `qF` を加える |
| direct parent derived | `qY+lT+qX+LCP構築read` | `qW_c+l(W_c-1)+qF` |
| direct parent oracle | derivedにfrontier readを加える | derivedに `qW_c` を加える |
| unaryまたはEF | `R_enc+lT+qX+LCP構築read` | `encoded_bytes+l(W_c-1)+qF` |
| path-chunk | `parent_bytes*C_path+lT+path stack read` | `C_new*S_H+l(W_c-1)+refcount+path stack write` |
| anchor overlay | `qY_o+qM_jump+lT+qX+boundary+flatten read/B_o` | `qW_c+l(W_c-1)+qF+flatten write/B_o` |
| backend-switch hybrid | 選択backend readと変換read | 選択backend writeと変換write |

currentの最初の `8(M_all+Z)` readはtraceからstreamをemitするread、次の `8M_K` はsurvivor巡回時のreadである。
write側の `8F` はancestor suffixと各target末尾Actionをcurrent traceへ書く量になる。

survivor-lateのtopology式には `qJ` snapshot、`qM_K` stream、LCP、remapを含む。
表で追加した `qF` は完成streamと各cand末尾Actionからcurrent traceへ復元する作業writeである。

directとsuccinctの `qF` は復元したtarget suffixをtraceへ書く量である。
windowed decodeでは、さらに最大 `2qF` のscratch read/writeが加わる。

この表はlogical byteであり、同じcache lineを再読する場合も数える。
hardware counterのL1、L2、LLC byteと照合し、係数はbackendごとにfitする。

### readとdependent load

| 方式 | topologyの主なread | 深さ方向の依存chain |
|---|---:|---:|
| current postorder | build `8(M_all+Z)`、巡回 `8M_K+O(4W_p)` | なし |
| survivor-late | replay `source_width*M_K+O(4W_p)`、巡回 `qM_K+lT` | なし |
| direct parent | parent `qY`、LCP `lT` | suffixごとに `r-1` |
| unary parent | cursorまたはselectが読んだword、LCP `lT` | suffixごとに `r-1` decode |
| EF parent | high select、low bits、LCP `lT` | suffixごとに `r-1` decode |
| path-chunk | chunk parent `C_path`、LCP `lT` | suffixごとにchunk境界数 |
| anchor overlay | 最大 `B_o` parentと必要時のbase suffix | 最大 `B_o` decode |
| backend-switch hybrid | 選択backendと変換元のread | 選択backendに従う |

`source_width` は旧streamが現行なら8、slot-onlyなら `q` になる。
direct parentの総load数 `Y=F-T` は、LCPで各suffixの最後のparent loadを省いた値である。

unaryのreverse cursorは問い合わせordinalのgapも通過する。
全active世代で最大約 `2G` bitをscanし得るため、実read byteはparent問い合わせ数だけでは決まらない。
indexed selectはscanを減らす代わりにcheckpoint read、popcount、pdepまたはbroadword decodeを追加する。

EFはゼロ子親の長いrunを避けるが、high selectとlow loadが必要になる。
圧縮でLLC missが減らなければ、direct arrayより命令数だけ増える。

### 常駐量とpeak memory

current、front coding、direct、unary、EFには `G*S_A` の世代別Action payloadが共通で残る。
path-chunkはこれを `(E+V)*S_A` へ近づける代わりにchunk管理を追加する。

| 方式 | steady topology | 変換または構築時のpeak追加分 |
|---|---:|---:|
| current postorder | `8(M_all+Z)+4W_p+8D` | current/next bufferのcapacity和 |
| survivor-late | `qM_K+lK+qD` | source、destination、`qJ`、`O(4W_p)` |
| direct parent derived | `qG+l(W_p+W_c)+qD` | capacity slackと次世代block |
| direct parent oracle | derivedに `q(W_p+W_c)` を加える | capacity slackと次世代block |
| unary parent | `sum_active b_u/8+lW_p+qD+index` | 次世代bitvectorとindex |
| EF parent | `sum_active b_EF/8+lW_p+qD+index` | high、low、select index |
| path-chunk | `C_live*S_H+(q+l)W_p+current chunk stack` | candidate、split、再結合buffer |
| anchor overlay | `2qM_A+qB_oW+indexes+LCP` | old/new base同時保持で約 `4qM_A` |
| backend-switch hybrid | 選択backendのsteady量 | sourceとdestinationの同時保持 |

ここで `b_u=W_prev+W_cur` bitである。
幅がほぼ一定ならunary parent mapは約 `G/4` byte、direct 32 bit parentは `4G` byteになる。

`vector::clear()` はcapacityを返さないため、streamの論理sizeが縮んでもpeak RSSは直ちに下がらない。
parent mapはdead nodeを世代block解放まで保持するため、`G >> E` ではstreamより大きくなり得る。

### allocationとreclamation

currentとfront-coded streamはflat double bufferを再利用でき、nodeごとのallocationを必要としない。
確定prefixより古いAction世代blockだけをslab poolへ返す。

survivor-lateの開始snapshotは再構築直後に解放できる。
mark/remap配列は探索全体で再利用し、epoch stampで全clearを避ける。

direct、unary、EFは世代ごとのimmutable blockとして確保する。
prefix確定時にblock全体を解放できるが、途中のdead slotだけをcompactすると子孫ordinalが変わる。

path-chunkはrefcountが0になったdead chainを即時解放できる。
prefixがchunk内部にある場合だけ、split copy、begin-offset retention、確定遅延のいずれかが必要になる。

anchor overlayはflatten後に旧baseと古いoverlay mapを一括解放する。
Action blockは新baseから参照され続けるため、通常の確定prefixより先には解放できない。

backend-switch hybridの変換中はsourceをdestination完成まで保持する。
変換失敗やallocation例外でもsourceを残せるが、一時peakは単一backendより必ず大きい。

### exact semantics

全backendで次を共通不変条件にする。

- current frontierのlogical ordinalを1個だけ定義する
- 候補のsort入力、比較関数、tie処理を現行と同じにする
- 親ordinalのremapは単調増加にする
- Stateがいるendpointを同じordinal位置に維持する
- rollback、apply、`enumerate_actions()` の呼出し列を変えない
- metadata predecode中にStateとActionへ触れない
- path-chunk storageはState操作中にmoveまたはreallocateしない
- anchor overlayはorientationに合うbaseだけを使い、skipped anchorをStateで訪れない
- prefix解放は将来の最初のendpoint移動に必要な辺を残してから行う

unaryとEFは単調parent配列の可逆符号化なので、decodeが正しければ意味論を変えない。
survivor-lateだけは開始traceの情報保存が追加条件になり、`J` を保存しない版はexactではない。

動的ビーム幅がwall clockを入力に使う場合、高速化そのものが次の幅を変える。
exact比較では幅列をrecord/replayし、固定した幅列の下で候補、State、historyを比較する必要がある。

### worst-case反例

| 方式 | worst case |
|---|---|
| current postorder | `M_all/W_c` が大きく、同じ古いpathを毎世代copyする |
| survivor-late | `J=H`、`P~=W_p`、`M_K~=M_all` でsnapshotとsecond passだけが増える |
| direct parent | 長いsuffixのparentがLLC missし、`Y` 個の依存latencyが露出する |
| unary parent | `G>>E` または長いゼロrunでcursor scanがlive tree量を大きく超える |
| EF parent | mapがcache内でselectとlow decodeの命令overheadだけが残る |
| path-chunk | 毎段分岐でlength 1 chunkへ退化し、header、allocation、refcountが純増する |
| anchor overlay | `B_o=1` でbase writeが倍化し、`B_o` 増大時は依存chainとoverlay memoryが増える |
| backend-switch hybrid | workloadがepochより速く変化し、毎回逆のbackendへ変換したくなる |

兄弟集中では `r=1` なのでdirect parentの `Y` は0になり、streamの再出力より有利になりやすい。
逆に深い位置で大きく枝が入れ替わると、dependent missと `G/E` の両方が悪化する。

## break-evenモデル

実時間はcache階層で非線形になるため、byte比較だけで採用を決めない。
対象CPU上のmicrobenchmarkから次の係数をfitする。

| 係数 | 意味 |
|---|---|
| `c_sr` | contiguous stream readのcycle/byte |
| `c_sw` | contiguous stream writeのcycle/byte |
| `c_b` | leaf boundary 1個を処理するcycle |
| `L_p` | parent load 1個の平均露出latency |
| `c_dec` | succinct parent 1個の追加decode cycle |
| `c_map` | survivor mark/remap 1要素のcycle |
| `c_sc` | scratch readまたはwrite 1 byteあたりのcycle |
| `C_conv` | backend変換1回の実測cycle |

これらはDRAM帯域の理論値ではなく、代表workloadでのcache missを含む回帰係数にする。
State操作cost `C_state*X` は全方式の共通項なので、topology比較では相殺する。

### currentからslot-only eager stream

64 bit IDと未参照prefixを除くwrite削減は次になる。

```text
saved_write = 8(M_all+Z) - qM_all
```

深さはtraceのindexとsuffix長から既知なので、slotからAction世代を一意に決められる。
dependent loadを増やさないため、これは最初に測るべき低リスク差分になる。

LCPを `leaf` の代わりに持つ場合は `l(W_p-1)` byteを加え、LCP生成scanのcostも入れる。

### survivor-late対eager stream

writeだけの必要条件は次になる。

```text
q(M_all-M_K-J) + l(W_p-K) > O(4(P+W_c))
```

実時間では、左辺のwrite節約が次の追加costを上回る必要がある。

```text
c_sw * saved_bytes
>
c_sr * (source_width*M_K + qJ + boundary_bytes)
+ c_map * (P+W_c)
+ second_pass_fixed_cost
```

`P/W_p` だけでは判定できない。
`M_K/M_all`、`J/H`、旧streamのcache miss率を同時に使う。

### direct parent対eager stream

永続writeだけなら次が必要条件になる。

```text
qW_c + l(W_c-1) < qM_all + l(W_p-1)
```

時間の主条件は概ね次になる。

```text
saved_stream_cycle
>
L_p*Y + LCP_build_cycle + trace_update_delta
```

`Y=F-T` が小さい兄弟集中ではdirect parentが強い。
`M_all/W_c` が大きくてもparent miss率が高いと、連続streamが逆転し得る。

### windowed ancestry decode

`m_eff(B_w)` を実測した有効MLPとする。
windowが勝つ必要条件は概ね次になる。

```text
L_p*Y*(1 - 1/m_eff(B_w))
>
2qF*c_sc + window_startup(B_w)
```

`B_w=1,4,8,16` を比較し、平均値だけでなくsuffix長分布ごとに集計する。
兄弟集中や短suffixでは `m_eff` が上がらず、`B_w=1` が最良になる。

### succinct parent対direct parent

1世代のmetadata write節約は次になる。

```text
unary: qW_c - (W_p+W_c)/8
EF:    qW_c - b_EF/8
```

unaryより32 bit directが小さいのは `W_p > 31W_c` の極端に疎な場合である。
疎な範囲ではEFがゼロrunを避けるが、速度条件は次になる。

```text
write_cycle_saved + cache_miss_cycle_saved
>
c_dec*Y + select_index_cycle + excess_cursor_scan_cycle
```

従って16 bit direct、32 bit direct、unary、EFのbyte最小だけでなく、decode後のcycle最小を選ぶ。

### path-chunk対direct parent

path-chunkのtopology writeがdirect parentより小さい必要条件は次になる。

```text
C_new*S_H + refcount_write + l(W_c-1)
<
qW_c + l(W_c-1)
```

`S_H` は `q` より大きいため、`C_new << W_c` となる長いtail appendが必要になる。
payload memoryの削減量は概ね次になる。

```text
(G-E-V)*S_A
```

実時間では次の利益とcostを比較する。

```text
L_p*(Y-C_path)
+ dead payloadのcache miss削減
+ 早期reclamation利益
>
chunk allocation
+ refcount更新
+ prefix split
+ 再結合のepoch償却
+ slack Vのcache cost
```

平均chunk長 `E/C_live`、`C_new/W_c`、`V/E`、`G/E` が主要な判定量になる。
毎段分岐では `C_new~=W_c` かつ `C_path~=Y` となるため、direct parentへ切り替える。

### bidirectional anchor overlay対eager stream

幅とstream sizeがepoch内でほぼ一定の `W`、`M` だとする。
LCP writeを両方式の共通項として外すと、writeだけの必要条件は次になる。

```text
qW + 2qM/B_o < qM
```

`M>W` の場合は次と同値になる。

```text
B_o > 2M/(M-W)
```

`M~=W` の浅い木では有限の小さい `B_o` で利益を出しにくい。
`M/W` が大きいほど、双方向baseの2倍writeを数世代で償却できる。

実時間では次も右辺へ加える。

```text
flatten_read_cycle/B_o
+ overlay_parent_exposed_latency
+ base_range_query_cycle
+ flatten_burst_penalty
```

`B_o` を増やすとflatten償却は改善するが、dependent chainと `qB_oW` resident memoryが増える。
`B_o=2,4,8,16` を比較し、write最小ではなくcycleとpeak制約を同時に満たす値を選ぶ。

direct parentの `qG` に対し、案Kのparent metadataは直近 `B_o` 世代の `qB_oW` に制限できる。
ただしbase `2qM_A` とflatten burstを払い、Action payload `G*S_A` は両方式とも残る。

### backend-switch epochal hybrid

epoch長を `N_e`、現在backendを `a`、候補backendを `b` とする。
切替条件は次になる。

```text
C_conv(a,b) + sum_epoch C_b
<
sum_epoch C_a
```

1世代平均では次になる。

```text
average(C_a-C_b) > C_conv(a,b)/N_e
```

変換はgeneration boundaryだけで行い、logical ordinal、endpoint、traceを保つ。
sourceからdestinationを作り終えてからpointerを切り替え、State操作は行わない。

streamからparent mapへ変換するには、active treeを走査して各深さの親関係を再構築する必要がある。
parent mapからstreamへ戻すには、全live pathをflattenする必要がある。
どちらも概ね `Theta(E)` read/writeとsourceとdestinationの同時保持を要求する。

path-chunkへ切り替えてpayloadもcompactする場合、live Actionを世代blockから移す必要がある。
move-only Actionではsourceを保ったtransactional変換が難しいため、metadataだけの切替より高い安全costを見込む。

短いepochでは償却できないため、探索開始時選択または長いepochに限定する。
切替にはhysteresisを入れ、推定利益が変換costの安全率付き上限を超えた場合だけ実行する。

## 採用順

1. current postorderの未参照prefix `Z` を省き、64 bit ActionIdを世代内slotへ縮める
2. eagerなslot-only LCP streamとdirect parent + adjacent LCPを独立backendで比較する
3. direct parentのmissが支配するときだけ `B_w=4,8,16` のwindow decodeを試す
4. `M/W` が大きいworkloadでbidirectional anchor overlayの `B_o=2,4,8,16` を比較する
5. parent metadataがLLCを圧迫するとき、unaryとEFをdirect 16/32 bitと比較する
6. `E/C_live` と `G/E` が共に大きいworkloadでpath-chunkを比較する
7. survivor-lateは開始trace保存を実装し、`J` を含むbreak-evenを満たす場合だけ試す
8. backend-switch hybridは変換costとdual peakを測定した後に限る

chunk、patch、threaded ancestry、live arenaは補助候補として残る。
いずれも主要方式より失敗条件が多く、最初の比較backendにはしない。

## 必要な計測

構造の優劣を特定testだけで決めないため、少なくとも次を記録する。

- `W_p`, `W_c`, `P`, `K`, `D`, `H`, `E`, `E_all`, `E_K`, `G`
- `M_all`, `M_K`, `Z`, `J`
- `U`, `F`, `T`, `Y`, `X`
- `M_all/W_c` と `F/W_c`
- `P/W_p`, `M_K/M_all`, `J/H`
- `next_tour` またはstreamへ書いたbyte
- 遅延再構築で再読したbyte
- 経路復元で読んだbyte
- parent chainのload数
- LCPで省略した終端parent load数
- unary cursorがskipしたdead slot数とindexed select回数
- direct、unary、Elias--Fanoそれぞれの親metadata byte
- parent loadのL1、L2、LLC miss
- `B_w=1,4,8,16` ごとの有効MLP、scratch byte、window startup cycle
- backend変換のread、write、cycle、一時peak、償却epoch長
- `C_live`, `C_new`, `C_path`, `E/C_live`, `V`, refcount更新数
- chunk allocation、prefix split、再結合、即時解放したAction byte
- `B_o`, `M_A^+`, `M_A^-`, `Y_o`, `M_jump`, same-anchor遷移率、base range query数
- forward/reverse stream byte、flatten read/write/cycle、old/new base同時peak
- branch miss
- Action payloadのinline byteと所有heap byte
- generation slabのpeak capacityとlive Action数
- prefix確定までの平均世代数
- retained span数 `R` と平均span長

workloadは次の軸を直交させる。

- `sizeof(Action)`: 4、16、64 byte、heap-owning
- State操作cost: 数命令、数十命令、cache missを伴う更新
- 深さ: 短い、長い
- 幅: 64、1024、65536程度
- 系譜: 強く収束、安定した複数枝、毎世代大きく入替え
- 親分布: 1親集中、均等、疎な親だけ使用
- survivor parent比: `P/W_p` が小さい、中程度、ほぼ1

## 実装前に守る不変条件

新構造でも次をテストする。

- 生成した候補列と呼出し順が現行と一致する
- dense parent remap後も親group順とgroup内順が一致する
- 各葉到着時のState hashが現行と一致する
- endpointがsurvivorでない世代でも、最初のrollback/apply列が一致する
- startがB、endがA、survivorがBとなる世代で開始traceを完全に復元できる
- window decodeの `B_w=1,4,8,16` でStateとActionの呼出し順が一致する
- backend変換の直前と直後でlogical pathとendpointが一致する
- anchor 3葉を2世代辿るorientation反転でforward/reverseの結果が一致する
- anchorを飛び越すrange queryでskipped葉のState操作が0回になる
- flatten完了前にold baseとcurrent traceが保持される
- 動的幅は同じ幅列をrecord/replayした比較でも一致する
- prefix確定直前と直後でStateが一致する
- 幅1、子0、1親集中、全親生存、同点候補で一致する
- 最終経路とmaterialized final Stateが一致する

構造比較用のoracleでは、Action列だけでなく各 `enumerate_actions()` 直前のState hashを比較する。

## 最終判断

帰りがけ順postorderが「構造的にこれ以上改善できない」という結論にはならない。

状態操作回数 `X` は既に下限に近く、一般LCAでは減らせない。
しかし、現行が毎世代行う `8(M_all+Z)` byteの `next_tour` 書込みは情報下限ではなく、削減余地がある。

確実性の異なる次の方向が存在する。

1. 未参照prefixを省き、slot-only LCP streamで連続性を保ったまま参照幅を縮める
2. parent-slot SoAでpostorder streamを捨て、`O(M_all)` writeを `O(W_c)` writeへ変える
3. unaryまたはEFでparent mapを圧縮し、decode命令とcache footprintを交換する
4. windowed decodeで複数parent chainのLLC missを重ねる
5. bidirectional anchor overlayでparent依存深さを制限し、stream writeをepoch償却する
6. path-chunkで長いunary chainを連続化し、dead Actionを早期回収する
7. 開始traceを保存した条件付きsurvivor-lateで、不要親のmaterializeを避ける
8. 十分長いepochだけbackendを切り替える

第1はdependent loadを増やさない確実な比較対象になる。
第2以降は連続copy、依存latency、decode、常駐量の交換であり、実測なしに一律の最速とは決められない。

survivor-lateは旧 `tour/leaf/cand` だけでは一般に不成立である。
開始trace保存量 `J` をcostへ入れない評価は採用判断に使えない。

従って「別の方法が存在しない」のではなく、「状態操作の下限はほぼ詰めているが、木metadataの最適点は未確定」
というのが、この制約下での結論である。
