# 帰りがけ順 beam search の次に検討する構造

## 先に結論

`beam_search.cpp` は、明示木を持つ従来版ではなく、traP の記事で説明されている帰りがけ順の
`tour`、`leaf`、`trace` を使う高速化後の方式である。

本稿の設計後、`beam_search_optimized.cpp`、`beam_search_parent.cpp`、
`beam_search_parent_compact.cpp` を別backendとして実装した。実測は `benchmark_results.md`、
実装監査は `parent_implementation_audit.md` と `research_synthesis_audit.md` を参照する。

- [差分更新ビームサーチライブラリの実装](https://eijirou-kyopro.hatenablog.com/entry/2024/02/01/115639)
- [木上のビームサーチ：高速化編](https://trap.jp/post/2920/)

したがって、比較すべき問いは「明示木に LCA を追加するか」ではない。

> 帰りがけ順の compact tour より、固定深さの汎用 beam search を速くできる履歴表現はあるか

この問いへの回答は次のとおりである。

1. 帰りがけ順方式が CPU 上で常に最速だと証明されているわけではない。
2. 単一の mutable `State` を辺単位の `apply_op()` / `rollback()` だけで動かす契約では、
   帰りがけ順方式は非空generationの固定された始点、target順、終点に対する状態遷移下限を厳密に達成する。
3. current tour の `[0, leaf[0])` は全復元処理から到達不能で、optimized版では`leaf[0]=0`として省いた。
4. optimized版はentry jumpをprefix境界から外した。survivor parentとendpointによる追加確定も成立する。
5. optimized版はtourを32 bit slot-onlyにした。tiny trivialかつStateから変更されないActionなら
   gblock自体を消すinline版も成立する。
6. deferred selected-parent tourはstart trace snapshotなしでは不成立で、snapshot込みの条件付き案になる。
7. tourを消すordered frontier + adjacent LCP + parent mapは、oracle版とcand導出版を実装済みである。
8. parent mapは32 bit slotのほか、親順の単調性を使う約`Wprev+Wcur` bitのunary表現にできる。
9. parent方式は次世代tourの書き直しを消せるが、連続コピーをtarget親鎖の依存load / selectへ交換するため、
   常に速いとは限らない。
10. `B_p`世代path-blockはparent鎖の依存段数を約`B_p`分の1にできるが、共有prefixの重複とboundary transposeを増やす。
    direct parentの低memory版ではなく、依存loadが実測で支配した後の第二段案である。
11. tourとparentの両極をつなぐ一案として、forward / reverseの2本のanchor streamを持ち、
    数世代のparent overlayを定期的に
    flattenするhybridがある。このhybridの片方向epochal tour版は走査方向の反転により不成立である。
12. `apply_op()` / `rollback()` の回数そのものを大きく減らすには、State copy、checkpoint、Action compose、
   path 一括適用など、現在より強い契約が必要である。
13. 生存親とLCAだけのvirtual treeは、Action composeがなければprimitive Action列をtour、parent chain、spanの
    いずれかで別途供給する必要があり、State遷移回数`X`を減らさない。
14. refcount 付き parent-only arena は dead Action を即時解放できるが、arena load と参照数更新が増えるため、
   主に `G/E_live` や Action payload が大きい場合の memory-oriented 変種である。
15. そのarenaへordered child / siblingを足すopen treeも成立するが、同じ`E_live+1`個のlogical nodeに
    より大きいmetadataを足す。`G >> E_live`対策の第一実装ではなく、下りDFSのlocalityを比べる第二案である。

つまり「これ以上の構造は存在しない」ではない。ただし、現行契約のまま現行方式を一方的に支配する構造も
現時点では見つからない。workload によって勝者が変わるため、汎用ライブラリでは backend を分けるべきである。

## 誤解しやすい用語の整理

### 明示木版

eijirou の記事の最初の実装は、各 node に親、子、左右の兄弟を持たせ、object pool 上の木を DFS する。
これは木構造の更新に pointer/index load が多く、node metadata も大きい。

### 完全な Euler tour 配列版

辺を下る event と戻る event の両方を平坦配列へ持つ方式では、基本的に1辺あたり2 token を読む。
明示木より連続アクセスしやすい一方、更新時に配列を再構築する。

### 帰りがけ順版

traP の記事は、固定深さで葉を DFS 順に処理する場合、完全な enter/leave 列を保持しなくてもよいことを利用する。

- `tour`: 生存経路を復元するための帰りがけ順 Action 列
- `leaf`: 隣接葉間の LCA 距離を符号化する境界
- `trace`: 現在 `State` が乗っている root-to-leaf path
- `cand`: 次に展開する候補と、その親葉番号

`beam_search.cpp` の `copy_tour_path()`、`lca_dist`、`next_tour` の構築は、この方式そのものである。
この文書ではこれを current postorder と呼ぶ。

ここで `leaf` が LCA 距離を符号化していることと、binary lifting や RMQ の LCA data structure を持つことは別である。
current postorder は前者であり、後者を追加する案を検討しているわけではない。

## 評価の前提

記号を次のように置く。

| 記号 | 意味 |
|---|---|
| `W` | 次世代に残る候補数 |
| `L` | 今回展開する葉数。通常は `W` と同程度 |
| `D` | 確定済みprefixからcurrent depthまでの未確定path長 `current_depth-freed_to` |
| `E_live` | live frontierと現在cursorの保持に必要な未確定Action辺の和集合サイズ |
| `E_walk` | 世代開始cursorと、その世代で展開する全nodeを結ぶ最小部分木の辺数 |
| `T` | 到達不能な先頭segmentを除いたcompact tourのAction token数。external `trace`上のendpoint pathは含まない |
| `Z` | current実装が`tour[0..leaf[0])`へ書く到達不能な先頭token数。省略後は0 |
| `X` | 葉の巡回で実行する `apply_op()` と `rollback()` の合計回数 |
| `G` | 未解放 generation block の論理 Action slot 数 |
| `G_cap` | live blockとslab poolを含む確保済み Action capacityの合計 |
| `P` | 次世代候補に現れる異なる親葉数 |
| `S_A` | `sizeof(Action)`。Action が所有する外部領域は含まない |
| `U` | deferred版でsnapshotする世代開始endpointの未確定path長 |

評価対象は次の契約である。

- frontier は固定深さ
- 探索結果を変える枝刈りは追加しない
- 展開には親が表す正確な `State` が必要
- `State` は基本的に1個だけ mutable にする
- `apply_op(a)` は親から子へ1辺進む
- `rollback(a)` はその1辺を戻る
- 任意の祖先へ直接 jump する API はない
- 複数 Action を1回で適用する API はない

State copy、checkpoint、Action compose は、この基本契約を拡張した別 case として評価する。

### `tour`が保持する集合と記号の関係

深さが同じordered frontier葉を`v[0]..v[L-1]`とする。先頭segment省略後は、`trace`が終端葉
`v[L-1]`の未確定pathを持ち、`tour[leaf[i]..leaf[i+1])`が`v[i+1]`から`v[i]`へ入るときの
target suffixを持つ。遠い葉へは複数segmentのrecord-high部分を使い、一度にpathを復元する。

各木辺を含むordered leaf区間は連続するため、reachable tourはfrontier誘導部分木のうち
終端`trace`path上にない各辺を1回だけ持つ。従ってgeneration開始時にcursorがその終端葉にいる通常形では、
未確定endpoint path長が`D`なら`E_live = T + D`である。現在のソースが持つ物理tour sizeは
`Z + T`であり、先頭省略後に初めて`T`と一致する。選択直後にdead cursorを別にpinする方式では、
`E_live`の対象集合が変わるためこの等式をそのまま使わない。

`E_live`、`E_walk`、`T`は同じ量ではない。例えば1葉で長い未確定pathを持っていても、その葉へStateが既にいるなら
`E_live=Theta(D)`に対して次の展開walkは1辺、先頭segment省略後のtourは空になり得る。memory、State操作、schedule帯域を
それぞれ`E_live`、`E_walk`、`T`で分けて比較する。

この文書では`T`をreachable tour token数に限定し、`M`は後述の1方向anchor stream長にだけ使う。
`postorder_external.md`の`M_all` / `M_K`はこの文書の`T_all` / `T_keep`に対応するが、同文書の`T`は
suffix長が正の遷移数であり、ここの`T`とは別物である。また同文書の生存部分木`E`は`E_live`に、
walk下限の部分木辺数は`E_walk`に対応させる。

時間連動のdynamic widthでは、同じロジックでも高速化によって`elapsed()`が変われば次のwidthと探索結果が変わる。
構造の正当性確認は固定width、または記録済みwidth列のreplayで行い、その後に時間制限下のend-to-end結果を別に比較する。

## 状態遷移回数の下限

### 木上の walk としての下限

世代開始cursor `s` とその世代の全target nodeを含む最小部分木を `H` とし、その辺数を `E_walk` とする。
単一 `State` は探索履歴木の1頂点だけを表すため、全葉を展開する処理は `H` 上の walk になる。

walk の始点を `s`、終点を `t` と固定する。`s` と `t` を結ぶ path 上にない辺は、その辺の先にある葉を訪れて
戻るために最低2回通る。`s` と `t` の path 上の辺だけは1回でよい。従って必要な辺遷移数は最低でも

```text
2E_walk - dist(s, t)
```

となる。始点と終点も自由なら、最良値は

```text
2E_walk - diameter(H)
```

である。DFS 順で部分木を連続して訪れ、最後だけ root へ戻らない open traversal はこの値を達成できる。

固定深さの frontier では、異なる root child にある最初と最後の葉は直径の両端になりやすい。
current postorder は親ordinalを端から単調に処理し、同一親の子を連続して処理する。
定深さordered frontierで、ある木辺の子側にあるtarget ordinalは常に連続区間になる。従って一度その辺を
離れた後に再進入することはない。

最後に処理するtargetを`t`とする。currentは`s`から始まり、`s`と`t`のpath上にない各辺を正確に2回、
そのpath上の各辺を正確に1回通る。従って通常loopの非空generationでは

```text
X = 2E_walk - dist(s, t)
```

が等号で成立する。これは固定された開始cursor、candidate処理順、終了cursorに対する厳密な下限達成である。
始点と終点を自由に変えた`2E_walk-diameter(H)`と一致するとは限らないが、そのための並べ替えは
candidate列挙順、tie、RNG消費と次世代の開始点を変え得る。

### この下限が意味すること

履歴の表現を変えるだけでは、通常 `X = Theta(E_walk)` を `o(E_walk)` にできない。

- LCA を `O(1)` で返しても、LCA までの各 Action を rollback する必要がある
- 次の葉までの path が分かっても、各 Action を apply する必要がある
- virtual edge を1本に見せても、中の Action を順番に実行するなら仕事量は同じ

従って current postorder より速い同契約の構造が狙えるのは、主に次の固定費である。

- `tour` と `next_tour` の ID read/write
- `trace` への path 復元
- `leaf` の境界走査
- Action ID の decode
- 明示木なら parent/child pointer の load
- cache miss、branch、allocation

この下限は「current postorder が CPU cycle でも最速」という証明ではない。State 操作が極端に軽い場合、
上記の metadata cost が支配的になり、別表現が勝つ余地がある。

## current postorder の強い点と残る費用

### 強い点

- 完全 Euler tour のように各辺の enter/leave token を2個持たない
- 明示木の parent、child、sibling metadata を hot path で読まない
- Action ID を連続配列から読む
- 親葉順に処理するため、同じ部分木の葉が連続する
- `li` が一方向に動くため、`leaf` の LCA 境界走査は世代合計 `O(L)`
- `next_tour`は前generationで選択され、今回展開した全active candidateのpath suffixを生成する
- 今回の選択で子を失った枝は、このnext schedule構築後に初めて不要と分かるため1 generationは含まれる
- 全葉の共通 prefix を確定して古い generation block を再利用する
- leaf-to-leaf の最短 path を直接たどり、毎回 root へ戻らない

### 残る費用

概念的には、1世代で次の metadata work がある。

```text
old tour の path fragment を読む
trace へ Action ID を復元する
next_tour へ今回の全active leafのpath suffix IDを書く
leaf / next_leaf の境界を読む・書く
```

State 操作とは別に、reachable scheduleに対し `O(T)` word、現在のソースそのままなら
`O(T+Z)` word程度を扱う。現在の `ActionId` は64 bitなので、reallocationを除く論理sizeは新旧合計で
おおよそ `8 * (T_old + Z_old + T_new + Z_new)` byteになる。実際のreserved byteはそれぞれのvector capacityで決まる。

一方、Action 本体は generation block にあり、死んだ slot も世代が解放されるまで残る。
論理保持量は概ね `G * S_A`、実allocationは`G_cap * S_A`であり、tourの表現だけを変えても消えない。

## `beam_search.cpp` の行単位再監査

以下の行番号は監査時点の `titan_cpplib/ahc/beam_search/beam_search.cpp` を指す。
探索結果を変えない定数倍改善と、順序 policy を変える案を混同しないため、まず各範囲の役割を固定する。

| 行 | hot work / lifetime | 再監査結果 |
|---|---|---|
| 24--26 | RNG、timer、dummy Action | `rnd` は現在の class 内で使われず、reset も固定費だけを払う |
| 39--48 | 64 bit ActionId と `gblock` decode | path depth が既知なのに generation を ID から毎回復号している |
| 50--65 | `cand`、2本の tour、2本の leaf | `CandIdx` は score、ID、履歴用IDを traversal 中も運ぶ |
| 67--100 | prefix 解放、Action slab 再利用、generation 確定 | `resize` 後の move assignment は Action を二段階で構築する |
| 102--131 | finished / final state の path materialize | 通常の世代loop外だが、深い解で Action copy が増える |
| 133--176 | streaming submit | State側の本質的な候補評価で、tour改善からは独立している |
| 178--202 | search workspace 初期化 | tour類はcapacityを保つが、`gblock`と`slab_pool`はsearch間でcapacityを失う |
| 204--217 | history survivor記録 | `record_history=false`でも一部node ID fieldはstruct内に残る |
| 219--234 | `copy_tour_path` | leaf境界を再走査し、record-highのrankが増えた分だけIDをcopyする |
| 244--326 | root展開とgeneration 1確定 | generation 1だけは`cand`をscore順にsortしない |
| 328--343 | 世代開始 | clockを2回読み、両next bufferをclearし、最初のprefix用に`trace[turn]`を書く |
| 345--370 | 葉間遷移 | LCA scan、rollback、tour append、path copy、applyが最重要hot pathになる |
| 372--415 | 子候補列挙 | current Actionをapply時とenumerate時に少なくとも2回decodeする |
| 416--460 | 世代commit | leaf write、prefix free、buffer swap、Action移送、`O(W log W)` sortを行う |
| 464--490 | max-turn path復元 | 解放済みprefixを含む長さの一時ID列を作ってから未解放部分だけmaterializeする |

ここで「未使用memberを消す」「final pathの一時vectorを短くする」などは固定費改善であり、
State transitionやtour構築が支配する大きな探索では優先度が低い。

### 1世代の正確なhot-path count

深さ`d`で展開するcurrent candidate数を`C`とする。処理順のcandidate `i`について、直前のparent leafから
target parent leafまでの`lca_dist`を`q_i`、その区間に含まれる旧leaf境界数を`k_i`と置く。
最初の移動も`i=0`へ含め、`R=sum(q_i)`、`K=sum(k_i)`とする。

親leaf indexは単調に減るので区間は重ならず、`K <= L-1`である。現在のloopが行う主要workは次になる。

```text
rollback calls       = R + C - 1
apply_op calls       = R + C
next_tour ID writes  = R + C
trace source reads   = R + C
tour -> trace copies = R IDs
candidate trace writes = C IDs + loop前の冗長1 ID
next_leaf writes     = C integers
old leaf scans       = LCA用 K境界 + copy用 K境界
```

従ってState呼出し回数は`2R+2C-1`である。これは上述の固定`s,t`に対する
open DFS walk下限`2E_walk-dist(s,t)`と一致し、metadataだけでは減らせない。
一方、64 bit ID版のschedule構築とpath復元に伴う明示的なID trafficは、再allocationを除いて次になる。

```text
trace source read          8 * (R + C) byte
next_tour write            8 * (R + C) byte
old tour read              8 * R byte
ancestor trace write       8 * R byte
candidate trace write      8 * C byte
redundant initial write    8 byte
next_leaf write            4 * C byte
```

これとは別に、State呼出し用の`act(trace[k])`がtrace IDを`2R+2C-1`回読む。多くはL1 hitだが、decodeと
`gblock` lookupの入力になる。さらにcandidate IDはtrace代入と`enumerate_actions()`で少なくとも2回読む。
先頭segmentを省くと、最初の`q_0+1`個についてtrace source readとnext writeが消え、冗長initial writeも消える。

`copy_tour_path()`は`k_i`個の境界を全部見るが、ID copy長はrecord-high rankの増分がtelescopingするため
正確に`q_i`個である。この区別は、leaf二重scanを減らす案とtour ID幅を減らす案の損益分岐に必要になる。

### `CandIdx` の実サイズ

典型的な64 bit ABIでは、現在の`CandIdx { int, Score, uint64_t, int }`は次の大きさになる。

| Score | 現在の64 bit ID | 32 bit slot | 32 bit slotかつ履歴IDなし |
|---|---:|---:|---:|
| 32 bit / align 4 | 24 byte | 16 byte | 12 byte |
| 64 bit / align 8 | 32 byte | 24 byte | 24 byte |

64 bit Scoreでは末尾paddingへ`node_id`が収まるため、node IDだけを消してもslot版は小さくならない。
実際の値は`ScoreType`のalignmentに依存するため`static_assert`で固定してはならない。

さらに`Candidates::next_beam`にも`parent_leaf`、score、Action、`node_id`があり、
`finalize_generation()`で同じmetadataを`cand`へ再度書く。Actionをmoveした後もvectorのobject storageは残るので、
peak byteを比較するときはcandidate selectorのcapacityを別項として数える必要がある。

## current tour の未使用な先頭segment

### 読み出し範囲

`copy_tour_path(parent_leaf, leaf_end, dst_end)` が `tour` から読む範囲は、各 `k` について

```text
[leaf[k], leaf[k+1])
```

の一部である。`k` の最小値は0なので、どの呼出しでも `tour[0..leaf[0])` は読まれない。

最終解の復元も同じ `copy_tour_path()` を使い、LCA距離の計算は `leaf` の差だけを見る。
finished pathはcurrent `trace`から作る。従って、現在の実装で各generationの最初の
`next_tour.insert(...)` が書く `[0, next_leaf[0])` はpath復元に使われていない。

### なぜ省けるか

postorder表現が必要なのは、current Stateが乗っているleaf列末尾endpointの`trace`から、別の葉pathを復元するためである。
このlast endpoint pathは既に`trace`にある。ordered leaf間の各transition suffixだけを保持すれば、その`trace`と
`L-1`個のsegmentから他の全leafを逆向きに復元できる。

つまり、葉が`L`個なら必要なtour segmentは隣接葉間の`L-1`個であり、最初の葉より前のsegmentは不要である。
現行の先頭segmentは、世代開始cursorの祖先suffixとfirst candidate Actionが混ざる場合があり、単独のendpoint pathと
解釈する必要もない。正当性の根拠は、全readerの開始offsetが`leaf[0]`以上であることにある。

最初の`lca_dist`を`q_0`とすると、このsegment長は正確に`q_0+1`である。現在の64 bit ID版では、
1世代あたり`8 * (q_0+1)` byteのappendを消せる。32 bit slot版なら`4 * (q_0+1)` byteになる。
最初のcandidateを出す旧endpointが次世代parentとして死んでいると`q_0`が深くなり、この節約量も大きくなる。

### 変更後の不変条件

最初のcandidateではtourへ何もappendせず、展開後に

```text
next_leaf.push_back(0)
```

とする。2個目以降はcurrentと同じsuffixをappendする。元の先頭segment長を`b = old_next_leaf[0]`とすると、

```text
new_next_leaf[i] = old_next_leaf[i] - b
new_next_tour[x] = old_next_tour[x + b]
```

である。従って全ての隣接差

```text
next_leaf[i+1] - next_leaf[i]
```

は完全に同じで、LCA距離も`copy_tour_path()`が見るsegment内容も変わらない。

実装上は`f == 0`、または`next_leaf.empty()`のときだけ`next_tour.insert()`をskipする。
`f`は最初のrollback範囲にも使われるため、insertをskipしても従来どおり1へ更新する。

この変更後は、loop前の`trace[turn] = cand.back().action_id`も不要になる。最初のrollbackは`f=0`なので
`trace[turn]`を読まず、最初のtarget Actionは従来どおりpath復元後に代入すればよい。

### entry jump

generation開始時のState endpointから、最初のcandidate parentまで距離がある場合、最初の`lca_dist`は0とは限らない。
currentは最初に`trace`をrollbackし、旧`tour`からtarget parent pathを復元してapplyする。

省くのは、その移動前後に新`next_tour`へ書いていた先頭segmentだけである。State移動、旧tourの読込、
`trace`更新は省かない。最初のtargetへ入った後の`trace`が、次の葉とのsegmentを作る基準になる。

従って「最初のparentが現在endpointと同じ」という仮定は不要である。

### `C=1`

今回展開したcurrent leafが`C=1`個なら、変更後の`tour`は空、`leaf`は`{0}`になる。

- 次generationの`copy_tour_path(0, 0, ...)`は何も読まない
- Stateと唯一pathは`trace`が保持する
- 隣接LCAは存在しない
- 唯一pathはその葉の深さまでprefix確定できる
- 最終復元もparent leafと`leaf_end`が同じなのでtourを読まない

従ってempty tourは正常状態であり、`tour.empty()`をno-candidate判定に使ってはならない。

### 最終復元

max-turn時の復元は、current endpointの`trace`を初期pathにし、best candidateのparentから`leaf_end`まで
`copy_tour_path()`する。best parentがendpointならrangeは空、他のleafなら`leaf[parent]`以降のsegmentだけを読む。

`leaf[0]=0`になっても、parent 0からの復元は新tourのindex 0から始まり、これは旧tourのindex `b`に対応する。
未使用だった旧prefixを参照するcaseはない。

### prefix解放

`max_lca_dist`は隣接する`leaf`値の差から計算される。全`leaf`から同じ`b`を引いても差は変わらないため、
`confirm_and_free(turn - max_lca_dist)`の境界も同じである。

Action IDの参照がtour先頭から消えても、current endpoint pathは`trace`が参照し、他leafに必要なActionは後続segmentから
復元できる。generation blockの寿命規則を変える必要はない。

### telemetry

`tour.size()`は短くなる。現在はverbose logと`BeamParam::timestamp()`の`pool_size_sum`へ渡される。
`pool_size_sum`は通常版の幅計算には現在使われないが、外部から統計値を読む場合は観測値が変わる。

`std::vector::clear()`はcapacityを縮めないため、既に大きくなった2本のbufferの予約byteが直ちに減るわけではない。
確実に減るのはappend writeと論理sizeであり、peak allocationが減るのは省略後の必要sizeが過去capacityを超えない場合である。

論理live edge数を統計として維持したいなら、physical `tour.size()`と分けてcounterを持つ。
動的幅や探索結果をphysical tour sizeへ依存させてはならない。

### 判定

先頭segment省略は成立する。探索順、State操作、隣接LCA、最終path、prefix解放を変えず、各generationで
`next_leaf[0]`個のAction ID writeと保持を消せる。

これはparent-slotなどの別backendより小さく、current postorderに直接入れられる低risk候補である。
特に`C=1`やentry jumpが長いgenerationで効果が大きい。実装時はpreprocess後token一致ではなく、世代ごとの
survivor digest、State checksum、最終path一致で検証する必要がある。

## prefix解放境界の再監査

### 最初のentry jumpを最大値へ含める必要はない

現在は全candidateについて`max_lca_dist`を更新し、世代末に

```text
confirm_and_free(turn - max_lca_dist)
```

を呼ぶ。ここには最初の`State endpoint -> first current leaf`の移動距離も含まれる。
しかし世代の走査が終わると開始endpointはcursorでもfrontier leafでもなく、以後そのpathへ戻らない。
最初の移動は次frontierの2葉間transitionではないため、共通prefixを制限する理由にならない。

深さ`d=turn`のcurrent leafが`C>=2`個あり、それらの間のparent-level `lca_dist`最大値を`m`とする。
隣接current leaf間のsegment長は`m+1`なので、全current leafのLCP depthは

```text
h = d - (m + 1)
```

である。深さ`h`までを確定する正しい呼出しは

```text
confirm_and_free(h + 1) = confirm_and_free(d - m)
```

となる。式の形はcurrentと同じだが、`m`へ最初のentry jumpを入れない点が違う。

### 1葉だけのoff-by-one

`C=1`では隣接transitionがなく、唯一の葉自身まで共通なので`h=d`である。従って

```text
confirm_and_free(d + 1)
```

が正しい。`m=0`を上の複数葉用の式へ代入して`confirm_and_free(d)`とすると、depth `d`のActionを
1世代余分に保持する。正当性は壊さないが、`C=1`で最も欲しい早期解放を逃す。

### survivor parentと終了endpointだけへ絞れる

さらに、次generationで必要なのは全current leafではない。

- 選択後candidateの`parent_leaf`に現れるdistinct survivor parent
- 走査終了時にmutable Stateがいるendpoint leaf

の和集合だけでよい。endpointは次generationの最初のentryでtarget parentへ移るまで必要なので、
survivorでない場合も外せない。

current leaf ordinalは処理順に増え、終了endpointは`e = next_leaf.size()-1`である。survivor parentの最小ordinalを
`p`とすると、集合の最小葉は`p`、最大葉は常に`e`になる。ordered treeでは集合全体のLCPは両端葉のLCAなので、

```text
r = max(next_leaf[k+1] - next_leaf[k]) for k in [p, e)
h = (p == e ? d : d - r)
confirm_and_free(h + 1)
```

まで進められる。`p==e`は集合が1葉だけなので`h=d`とする。

### dead segmentにdangling IDが残る反例の監査

この早期解放後も、物理tourにはordinal `p`より前のdead leaf segmentや未使用先頭segmentが残り得る。
それだけでは不正にならない。次generationの`copy_tour_path(target, li, ...)`では

- 初期`li=e`
- 全target parentは`p以上`
- 以後`li`もtargetへ単調に減少する

ため、`[p,e)`より外を読まない。範囲内の最大rankは`r=d-h`以下なので、depth `h+1`より浅い位置へ
Action IDを復元しない。従ってdepth `h`以下のblockを解放しても、読まれるIDはdanglingにならない。

解放直後から`finalize_generation()`まで旧`cand.action_id`もdanglingになり得るが、この区間では旧candを参照せず、
新candidateから同じvectorを再構築する。future rollback、apply、final materializeはいずれも`freed_to`より深いIDだけを使う。

反例になるのはendpointを集合から外す実装である。endpointが遠いdead subtreeにあり、最初のsurvivor parentが
別subtreeにあると、次entryのrollbackに必要なActionを先に解放してしまう。endpointを含めるか、世代末にStateを
survivor endpointへrelocateしてから解放する必要がある。

### costと優先度

全current leafだけを使う版は、既存loopで`f==1`のときだけ最大値を更新すれば追加scanなしで実装できる。
survivor版はcandidateから最小parentを求める`O(W)` scanと、`next_leaf[p..e]`のsuffix max scanを追加する。

Actionが所有resourceを持つ場合や未確定深さが長い場合は、早いdestructor実行とslab再利用がscan費を上回り得る。
一方、tiny ActionかつState操作が非常に軽い場合は4 byte境界scanが純増になる。従って次の二段階が妥当である。

1. entry jumpを除外し、`C=1`を直す追加scanなし版
2. `G/E_live`やAction byteが大きいbackendだけでsurvivor + endpoint版を有効にする

なお`slab_pool`はcapacityを保持するため、早期解放が常にRSS低下になるわけではない。Action destructor、
live generation数、再allocation回避には直ちに効くが、余ったslabをpoolへ保持し続けるpolicyは別途調整が必要になる。

## 選択後に distinct parent だけで tour を作る案

### 現行実装が tour を作る時点

深さ `d` の `cand` を展開するとき、current は各葉を訪れながら `next_tour` と `next_leaf` を作る。
この時点では、そこで生成した深さ `d+1` の候補のうち、最終的に上位 `W` 件へ残るものはまだ分からない。

従って current の `next_tour` は、深さ `d` で展開した全 active leaf を含む。選択後の深さ `d+1` 候補が参照する
distinct parent leaf 数を `P` とすると、`P` は active leaf 数より大幅に小さい場合がある。

次の generation では `cand.parent_leaf` に現れない葉を実際には訪れない。current はその generation の走査中に
不要葉を飛ばしながら、さらに次の `next_tour` から自然に除去する。つまり不要枝は1 generation 遅れて消える。

### 重要な区別

選択 parent だけへ圧縮する方法は2種類ある。

1. 全 active leaf の eager tour を従来どおり作り、選択後にもう一度 filter / compact する
2. 展開中は tour を作らず、選択後に旧 tour と current candidate descriptor から selected-parent tour だけ作る

1は正当だが、初回の full tour write を消せない。速度案として本当に検討すべきなのは2である。
以下では2を deferred selected-parent tour と呼ぶ。

### full next tour は現在の展開には不要だが、後から完全には再生できない

深さ `d` の current leaf は次で一意に表せる。

```text
current_leaf = old frontier の parent leaf + depth d の candidate Action
```

深さ `d` の全 `cand` を巡回するために必要なのは、深さ `d-1` の旧 `tour` / `leaf` と各 `cand.action_id` である。
構築中の `next_tour` は現在の State 移動には使われず、次 generation のためだけに書かれている。

また、生成した深さ `d+1` 候補の `parent_leaf` は、current leaf の訪問 ordinal でよい。
従って展開中は integer counter だけを進め、次の対応を残せばよい。

```text
expanded_ordinal -> {old_parent_leaf, current_action_id}
```

current は `cand` を逆順に全件処理するため、別 vector を持たずに元の `cand` index から ordinal を逆算することもできる。
ただし `finalize_generation()` が `cand` を上書きする前に、後で必要な selected descriptor を退避する必要がある。

ここまでから「選択後に旧tourだけでnext tourを再構築できる」と結論してはいけない。current postorderは
旧frontierの終了endpoint pathをtour内に持たず、世代開始時の`trace`だけに持つ。走査終了時の`trace`は別endpointへ
上書きされているため、その情報はnext tourを書かなければ失われる。

最小の情報欠落は、2つのsubtreeを`A`、`B`とし、世代開始時のendpointが`B`、走査終了時が`A`、
次候補のsurvivor parentが`B`だけになる形で見える。走査中に書くpostorder segmentを全て省くと、終了時に残るのは
`A`のtraceであり、開始時`B`のpath suffixは旧tourのendpoint省略部分にも存在しない。

root直下1辺だけならterminal Actionをcandidate descriptorから拾える場合がある。しかし同じ形を深さ2以上へ伸ばすと、
descriptorが持つのはcurrent leafの最後の1 Actionだけであり、開始endpoint側の祖先suffixは復元できない。
従ってこの反例はdescriptorを残すだけでは解消しない。

成立させるには少なくとも次のどれかが必要である。

1. 世代開始時の未確定`trace`をsnapshotする
2. 走査中に、後でsurvivorになり得るsuffixをlazyに保存する
3. generation parent-slotなど、endpoint以外から祖先pathを復元できるmetadataを持つ

2はsurvivorが選択終了まで分からないため、任意入力に対するworst caseではeager full tourと同量を保持する。
3を採るとdeferred postorder単体ではなくparent backendとのhybridになる。純粋なdeferred案の公平な比較対象は1である。

### start trace snapshot込みのdeferred構築手順

概念的な手順は次になる。

1. 未確定prefixの深さを`U=d-1-freed_to`とし、世代開始時`trace`の`U` IDsをsnapshotする
2. 旧 `tour` / `leaf` とsnapshotを使って深さ `d` の全 `cand` を展開する
3. 各 generated candidate へ current leaf の `expanded_ordinal` を保存する
4. finished または no-candidate なら、next tourを作らず終了する
5. 最終 survivor を元の `parent_leaf`、score の順でcurrentと同じように整列する
6. survivor に現れる distinct old ordinal `p[0] < ... < p[P-1]` を集める
7. 走査終了endpoint `e=C-1`がsurvivorでなければ、cursor用の一時leafとしてkeep集合へ加える
8. snapshotを開始endpoint pathとして、keep leaf descriptorを旧tourからordinal昇順に復元する
9. `K_keep=P`または`P+1`葉のcompact postorder `next_tour` / `next_leaf`を作る
10. candidate の親 ordinal を`old p[k] -> compact k`へ単調remapする
11. endpointをcompact葉列の末尾に置き、実際のState / trace / `li`と一致させてswapする

`U`は「選択後に最初のkeep葉へ入る長さ」ではない。選択結果が分かる前に消える世代開始endpointの
未確定path全体を保存するworst-case長である。選択後に実際に再構築するtarget suffix長が`T_entry`であり、
`U`と`T_entry`を一つの項へ縮約してはならない。事前に必要suffixだけsnapshotするには将来のsurvivorが必要で、
lazy保存で代用しても任意入力のworst caseは`U`に戻る。

選択 current leaf の path は、旧 parent leaf の path に current Action を1個加えたものである。
選択 ordinal が current DFS 順なら、その old parent ordinal も単調になる。同じ old parent の兄弟は連続する。
従って旧 `leaf` 境界は一方向に走査でき、一般 LCA 構造は不要である。

### compact tour の構築量

次を区別する。

| 記号 | 意味 |
|---|---|
| `T_all` | 全active leaf版で物理的に書くpostorder token数 |
| `T_keep` | survivor parentと終了endpointをkeepする版で物理的に書くpostorder token数 |
| `T_entry` | 世代開始endpointから最初のkeep葉まで、post-passで再構築するtarget suffix長 |
| `U` | external endpoint traceとして別に必要な未確定path長 |

先頭unused segmentを省いたpostorderでは、endpoint pathは`trace`にありtourへ含まれない。
従ってpath和集合辺数`E_live`をそのままtour write量とみなすと、長い共通幹を二重計上する。

隣接leafのLCP depthを`h_i`とすると、物理token数は

```text
T = sum(i=1..leaf_count-1, d - h_i)
```

である。current eagerは展開中に`T_all`個、deferredは選択後に`T_keep`個のIDを書く。
現行の未使用先頭segmentを残す場合だけ、最初のentry長を別途加える。

deferred の追加 work は概ね次である。

- 世代開始時の未確定path `U` IDsのsnapshot write
- final candidate `W_next` 件の parent group scan と remap
- 旧 leaf 境界の単調走査。最悪 `O(L_old)`
- 最初のkeep葉への`T_entry`個と、keep葉間の`T_keep`個のpath fragment再読込および`trace`復元
- `T_keep` 個の compact tour write
- `K_keep=P`または`P+1`個の compact leaf boundary write

従ってpure deferred版の最小safe bufferを`O(T_entry)`とするのは誤りである。毎世代`O(U)`の
snapshot storageと`U` tokenのwrite trafficを先に払い、選択後に`T_entry+T_keep`の復元readを払う。

重要なのは、selected path の復元が追加 passになることである。eager は State traversal 中に既に hot な `trace` を
そのまま `next_tour` へ書く。deferred はその機会を捨て、選択後に最初のkeep葉へ入り直し、selected pathを旧tourから
もう一度復元する。`T_keep`は先頭segmentを含まないため、この入り直しを`T_keep`へ含めてはいけない。

帯域だけの粗い比較では次になる。

```text
eager     : write T_all IDs while trace is hot
deferred  : snapshot U IDs + reread/reconstruct (T_entry + T_keep) IDs + write T_keep IDs + remap
```

従って`P < W`だけでは不十分で、`T_keep`が`T_all`より十分小さく、snapshot `U`と`T_entry`も短い必要がある。
64 bit IDならsnapshot bufferは`8U` byte、毎世代のcopyも`8U` byteになる。32 bit slotならそれぞれ`4U` byteである。
snapshot capacity自体は再利用できるが、write trafficは毎世代必要になる。

### `T_keep` は materialize 前に求められる

keep葉が全て同じ深さ`d`で、隣接keep葉のLCP depthを`h_i`とすると、
先頭segment省略後の物理tour token数は次で求められる。

```text
T_keep = sum(i=1..K_keep-1, d - h_i)
```

leaf列最後のpathはexternal endpoint traceへ置き、tourにはそれより前の`K_keep-1`葉のsuffixだけを置く式である。
`h_i` は old parent が同じなら `d-1`、異なるなら旧 leaf 境界の区間 LCP から求められる。

同様にwould-be `T_all`は展開中のLCA距離からIDを書かずにcountできる。
従って`T_keep/T_all`、`P/L`、`U`、推定copy bytesは計測できる。

ただし選択結果が出た時点では、eager build の機会は既に過ぎている。1回の generation 内で cost を見て完全に
最適な方へ切り替えることはできない。compile-time policy、直近 generation の比率による予測、または常に deferred の
いずれかになる。eager と deferred を両方実行して良い方だけ使う設計は、節約したい work を二重に行う。

### parent ordinal remap

generated candidate の `parent_leaf` は current leaf の訪問 ordinal であり、元の `cand` index とは限らない。
current は sorted `cand` を逆向きに処理するため、両者を混同すると別の親 path を参照する。

安全な remap は次のどちらかである。

- `expanded_descriptor[ordinal]` を展開時に明示保存する
- 全件を逆順に処理する不変条件を固定し、`ordinal -> cand[size-1-ordinal]` を使う

final candidate を旧 ordinal でsortした後なら、同じ親の run ごとに compact ordinalを付け、その run の
`parent_leaf` を直接書き換えられる。old ordinal から new ordinal への size `L` 配列は必須ではない。

remap は単調変換なので、親 group の順序は変わらない。親内 score 順も、remap 前に current と同じ comparator で
sortすれば維持できる。完全同点の順序まで一致させるなら、元の enumeration ordinal を明示 tie-break にする必要がある。

### endpoint の問題

展開終了時の mutable State は、最後に訪れた active leaf にいる。その葉が final survivor の parent でなければ、
selected-parent tour には State cursor の位置が存在しない。

このまま full tour を捨て、次 generation で `li = compact_leaf.size()-1` とすると、State と `trace` が表す葉、
`li` が表す葉が一致せず壊れる。これは最も単純な反例である。

修正方法は3つある。

1. 選択後、State を次 generation で最初に使う selected parent endpoint へ直ちに移す
2. compact tour に cursor 用の非 survivor leaf を1個だけ残す
3. external cursor と entry LCP/path を別管理し、次 generation の冒頭で compact tree へ入る

探索結果とState操作の時点を最も忠実に保つのは2である。current leaf ordinalの最大値は終了endpoint `e=C-1`なので、
survivorをordinal昇順へcompactし、必要なら最後に`e`を追加すれば、compact葉列の末尾と実State endpointが一致する。
次generationの最初の移動だけが一時endpointから最大survivor parentへ入り、その後は従来どおり単調に進む。

1はeager方式が次generation冒頭で行うparent間移動を前generation末尾へ移せる場合がある。ただし次generationが
finished/no-candidateになる場合やwidthが時刻依存で変わる場合まで含め、完全に同じworkになるとは限らない。
厳密同順序を要求するbackendでは一時endpoint leafを残し、relocationを行わない方が安全である。

### prefix 確定

keep集合へ絞ると、全active leafを基準にしたcurrentより共通prefixが深くなる場合がある。

- `K_keep >= 2`: keep leaf間の最小LCP depthまで確定可能
- `K_keep == 1`: 唯一のkeep leafの深さ`d`まで確定可能
- `P == 0`: no-candidate 終了

endpointをkeep集合へ含めれば、その枝から次entryでrollbackするActionもLCP計算へ入る。
endpointを外してselected parentだけのLCPまで解放するには、先にStateをselected endpointへrelocateする必要がある。

次 generation の survivor が1 candidateだけなら、その child Actionまで共通である。ただし parent-only tour構築と
State relocation の段階ではまだ childを適用していない。そこまで即時確定するなら、first child apply と
`last_action` の扱いも同じ phaseへ移す必要がある。まずは parent depthまでの確定に留める方が安全である。

### 順序と探索結果

次を守れば、candidate の展開順は eager と同じにできる。

- current leaf の訪問 ordinalをそのまま元の親順とする
- selected parent をその ordinalの単調順で圧縮する
- remap 前に親、score、必要なら enumeration ordinalでsortする
- compact tree の走査方向を current と同じにする
- endpointを一時leafとしてcompact列末尾へ残す

この構成ならState relocationを世代境界越しに移さず、次generationでもcandidate展開順とState操作順をcurrentに合わせられる。
同点のordinalだけはsort実装依存なので、完全一致には明示tie-breakが必要になる。

### `P << W` でも勝たない反例

#### 長い共通幹

世代開始cursorだけが浅いLCAで分かれるdead subtree `B`にあり、今回のcurrent leafは別subtree `A`内で
深さ`D-1`まで共通、最後の1辺だけ`W`個に分岐する木を考える。前generationの精密prefix解放でも、
survivor側`A`とcursor側`B`を両方含めるため未確定pathは長く残る。

```text
T_all ~= W - 1
T_keep ~= K_keep - 1
snapshot U ~= D - 1
T_entry ~= D
```

`P << W`でも`D >> W`なら、減らせるtour writeよりsnapshotとentry reconstructionの方が大きい。
deferredは`B`から`A`へのpath情報を保存して入り直すため、eagerの短い隣接suffix writeより遅くなりやすい。

#### 全親が生存

各 active leaf から1個ずつ次候補が残り `P=W` なら、compact tour はfull tourと同じである。
deferred は parent remap と post-selection path reconstructionだけを追加するため、明確に不利である。

#### 少数親だが木全体に散る

`K_keep`個のkeep leafが互いに浅いLCAしか持たず、各pathが長い場合、`T_keep`は`K_keep*D`に近い。
`P` が数値上小さくても、working set がcacheに収まらなければ再読込費が大きい。

#### endpoint が全滅

最後に訪れた active leaf の子が全て脱落し、離れた subtree の候補だけが残る場合、metadataだけ圧縮して
State relocationも一時endpoint leafも省く実装は直ちに壊れる。

#### 開始endpointだけにあるpathが生存

開始endpoint`B`から走査を始め、最後は別leaf`A`に到達し、`B`だけが次候補のparentとして生き残る場合を考える。
full next tourを一度も書かず、start trace snapshotも持たなければ、終了時の`A` traceと旧tourから`B`の祖先suffixを
復元できない。`P=1`かつ`T_keep`が小さい、最もdeferredが勝ちそうな形でも正当性が壊れる反例である。

### 勝ち得る条件

- active path が浅い位置で分岐し、`T_all` が大きい
- survivor parent が少数の近い subtree に集中し、`T_keep/T_all` が小さい
- State traversal に比べ、full next-tour write が profile へ現れている
- `next_leaf` の `L` 件 write と次 generation の `L` 境界 scan が支配的
- selected-only prefix 確定により古い Action block を早く解放できる
- finished / no-candidate が多く、不要な next tour build 自体を省ける
- 未確定path `U` と最初のkeep葉への`T_entry`が短い

最終generationでは次のtraversal用tourは不要である。start trace snapshotを持つdeferred版なら、best candidateの
parent path 1本だけを旧tourとdescriptorから`T_entry`相当のworkで復元でき、`P` parent tourは作らずにmaterializeできる。
snapshotを省いた版は、best parentが開始endpoint側だけにある同じ反例で壊れる。

### eager build 後の post-compaction の判定

既に`T_all`のfull tourを書いた後でselected parentだけへ圧縮する方式は、次generationの論理treeを小さくできる。
しかし current の `copy_tour_path()` は、unselected subtree の全 Action IDを読むわけではない。主に leaf境界を単調走査し、
必要な path fragmentだけを読む。従って post-compactionが次 generationで節約する readは見た目より小さい。

一方、post-compactionは`O(L + T_keep)`の追加read/writeを必ず払う。vector capacityも通常はpeakを保持するため、
RSSが直ちに下がるとは限らない。

従って eager後のpost-compactionは、速度目的の既定値にはしない。次の場合だけmemory policyとして検討する。

- Action block の早期 prefix freeが大きい
- full tour working setがcache / memory上限を超える
- `P/L` と `T_keep/T_all` が極端に小さい
- compaction後に大容量bufferを別searchへ再利用できる

### 判定

snapshotなしのdeferred selected-parent tourは不成立である。旧tourは開始endpoint pathを意図的に省いており、
走査後のtraceだけでは情報が足りない。

start trace snapshotを加えれば正しく実装できるが、これは毎世代`O(U)`のcopyとbufferを増やす。
判断には`T_keep/T_all`だけでなく、`U`、`T_entry`、追加path reconstruction、endpoint relocation、早期prefix freeを含める。
現時点ではslot-only currentより優先しない。`T_keep << T_all`かつ`U + T_entry`が短いworkload向けの条件付きpolicy、または
parent metadataを既に持つbackend上のcompactionとして比較する位置づけになる。

## 案1: 32 bit slot-only tour

### 構造

固定深さでは、`trace[d]` の Action が属する block は必ず `gblock[d]` である。
従って hot path に保持する ID は `(generation, slot)` でなく `slot` だけでよい。

```text
trace[d]        = uint32_t slot
act(d, slot)    = gblock[d][slot]
tour element    = uint32_t slot
cand.action     = uint32_t slot in current generation
```

`tour` 中の要素を単独で解釈することはできないが、current code でも tour fragment は既知の深さ範囲へ
`trace` として復元される。復元先の index から generation が分かる。

### 期待できる効果

- `tour`、`next_tour`、`trace` の ID traffic をほぼ半減
- shift、mask、64 bit index decode を削除
- `CandIdx` の配置が小さくなる可能性
- current postorder の連続アクセスと探索順を完全に維持
- State / Action の公開契約を変更しない

### 条件と危険点

- generation 内の Action 数が `uint32_t` に収まること
- `trace[d]` と `gblock[d]` の対応を全 path 復元箇所で維持すること
- `confirm_and_free()` 後も、確定境界より先の slot の generation が一意に分かること
- 最終 path の materialize では、iterator だけでなく開始深さも渡すこと

現在の slot は `int` で扱われ、64 bit ID 内では24 bitしか予約されていないため、32 bit slot の方が範囲も広い。

現在は`slot >= 2^24`を検査せず`make_id()`へ渡すため、巨大widthではslot上位bitがgeneration fieldへ混ざる。
これは性能ではなくcorrectness上の上限である。64 bit packed IDを残す場合でも、少なくともgeneration確定時に
上限を検査する必要がある。

### generationを省けることの証明

slot-only化で最も危険なのは、tour token単体からgenerationが分からなくなる点である。
しかしcurrentの全読出しにはdepthが付いている。

- `trace[d]`は常に`gblock[d]`のslot
- `cand`を処理するloopではcandidate generationが`turn`
- `copy_tour_path()`はtour fragmentを既知の`trace`深さ範囲へ書く
- `next_tour`は既知の`trace`連続範囲から作るため、fragment内の相対depthを保存する
- final materializeは開始depthと要素数を知っている

従って`act(id)`を`act(depth, slot)`へ変えれば情報は失われない。prefix解放後も、読んでよい最浅depthを
`freed_to+1`で止める限り同じである。

### byteとdecode

前節の記号では、1世代の明示的ID trafficが概ね次のように変わる。

```text
64 bit : source read 8(R+C) + next write 8(R+C) + old read 8R + ancestor write 8R + candidate write 8C
32 bit : source read 4(R+C) + next write 4(R+C) + old read 4R + ancestor write 4R + candidate write 4C
```

さらに`trace` capacityは`8D -> 4D`、2本のtour capacityは合計`8(C_tour+C_next) -> 4(...)`になる。
`CandIdx`は典型的な32 bit Scoreで`24 -> 16 byte`、64 bit Scoreで`32 -> 24 byte`になる。

State呼出し用のtrace read `2R+2C-1`回とcandidate fieldのID readも8 byteから4 byteになるが、通常はL1 trafficなので
streaming tour writeと分けて計測する。

時間面ではshiftとmaskそのものより、既知depthを捨てて`gblock[decoded_generation]`を引き直す依存loadを消す効果が重要である。
ただしslot版も`gblock[depth].data()`のpointer loadは必要で、Action本体がcache missする場合は差が埋もれる。

### 判定

これは新しい探索構造ではなく、current postorder の表現改善である。しかし、リスクと期待値の比が最もよい。
最初に比較実装する価値が高い。

## current postorderを保つAction storage案

### `gblock.resize()`の二段階構築

`finalize_generation()`は再利用slabを取得した後、次を行う。

```text
gblock[gen].resize(sz)
gblock[gen][i] = move(candidate.action)
```

これは各Actionをdefault constructionしてからmove assignmentする。default member initializerや所有resourceを持つActionでは、
最終的に上書きする初期化と解放を`W`回余分に行う。

slabを`clear()`したsize 0のvectorとして再利用し、必要capacityを`reserve()`した後に
`emplace_back(move(candidate.action))`すれば、1回のmove constructionだけにできる。byte capacityと探索順は同じなので
探索semanticsのriskは低いが、後述のAction型要件は変わり得る。

反例は、Actionのmove constructorが高価だがmove assignmentだけ特別に安い独自型である。また`reserve(sz)`を毎回exactに行うと、
動的widthが少しずつ増えるcaseでgeometric growthを失う。既存capacityが不足するときだけ通常のvector growthへ任せる方がよい。

現在はdefault constructible + move assignableだがmove constructorを削除したActionでも通る余地がある。
`emplace_back(move(...))`はmove constructibleを追加要求するため、traitでfallbackするか公開conceptを明文化しないと
source compatibilityを壊す。この点は単なるloop書換えより実装riskが高い。

### slab poolのallocated byte

generation blockの論理sizeだけを`G`と数えるとpeak memoryを過小評価する。実際のallocated slot数は

```text
sum(live gblock capacity) + sum(slab_pool capacity)
```

である。prefixが一度に複数depth進むとpoolへ複数slabが入り、次generationが再利用するのは1本だけなので、
残りcapacityはsearch終了まで保持され得る。Action destructorは`clear()`時に走るが、RSSは下がらない場合がある。

動的widthで巨大slabを小generationへ割り当てるとcapacityを長くpinする。best-fit poolや
`capacity > alpha * requested_size`なら保持しないpolicyはmemoryを改善するが、pool searchとdeallocationを増やす。
固定widthでは現在のLIFOが直近と同程度のcapacityを再利用しやすく、通常はこちらが速い。

### search間の再利用

tour、leaf、cand、fallback actionsは`clear()`なので同じBeamSearch objectの次回searchへcapacityを持ち越す。
一方、`gblock.clear()`と`slab_pool.clear()`は内側vectorを破棄し、Action slab allocationを持ち越さない。

1 objectで多数回searchするlibrary利用なら、search終了時に全slabをcross-search poolへ移す余地がある。
通常のAHCで1回しかsearchしないなら効果はなく、巨大widthだった前回のcapacityを保持する反例もあるため既定にはしない。

## tiny inline Actionなら`gblock`を除けるか

### direct-Action postorder

`Action`が小さくtrivially copyableなら、tourとtraceのtokenをActionそのものにできる。
ただしcurrent candidate Actionは次のselectorをresetする前に別storageへ退避する必要があるため、実用的な構成は次になる。

```text
current_actions[slot] : 今世代candidateのActionだけを保持
cand.action_slot      : current_actionsへの32 bit index
tour / next_tour      : 過去pathのActionをinline保持
trace                 : current pathのActionをinline保持
```

current leafを離れるとき、そのActionはsource `trace`から`next_tour`へcopyされる。世代末に`current_actions`を破棄しても、
生存pathのActionは新tourと終了endpointのtraceへ移っている。これにより全未解放generationの`gblock`を持つ必要がなくなる。

### memory break-even

2本のtourとtraceのcapacity合計を`Q=C_tour+C_next+D`、current Action数を`W_cur`とする。
candidate selector自身が持つAction bufferは両方式に共通なので除外する。

```text
slot-only + gblock : G*S_A + 4Q
direct tiny Action : W_cur*S_A + Q*S_A
```

従ってdirect版が小さい粗い条件は

```text
(G - W_cur) * S_A > Q * (S_A - 4)
```

である。

- `S_A < 4`ならschedule byteが減るためlogical memoryでstrictに有利
- `S_A == 4`なら`G>W_cur`でstrictに有利、`G==W_cur`ではlogical memoryは同量
- `S_A == 8`ならscheduleがslot版の2倍なので、`G-W_cur > Q/2`程度が目安
- 大きいActionではtour再構築のcopy byteが急増し、通常は不利

この式はlogical sizeであり、vectorのcapacity roundingとslab poolを含めて実測する必要がある。

### timeと正当性

direct版はAction ID decodeとAction本体への追加indirectionを消せる。`S_A`が1 wordなら、IDをcopyして後でActionをloadする代わりに、
必要なAction bitを最初からstreaming copyできる。

一方、non-trivial Actionでは同じ論理edgeをtour更新のたびにcopyし、constructor / destructorも増える。
`std::string`、`std::vector`、参照count付きhandleなどはtinyに見えてもtrivialではなく、deep copyやatomic更新の反例になる。

現行呼出しは`state.apply_op(act(...))`のようにnon-const lvalueを渡すため、template上は`Action&` overloadがActionを
変更することを禁止していない。gblock版は同じedgeのcanonical Action objectを再利用するが、direct版はtourごとのcopyを
別々に変更するため、Action mutationがあると意味が変わり得る。

従ってdirect backendは、`apply_op`、`rollback`、`enumerate_actions`が渡されたActionを変更しないsemantic promiseを要求するか、
`const Action&`だけを渡すAPI conceptへ分ける必要がある。その上でstorage traitを少なくとも次に限定する。

```text
is_trivially_copyable_v<Action>
is_trivially_destructible_v<Action>
sizeof(Action) <= compile-time threshold
Action is semantically immutable during State calls
```

汎用class内部で暗黙に切り替えるより、`InlineActionStorage` policyまたは別backendにする方がcode sizeと検証範囲を管理しやすい。

### 判定

`S_A<=4`のtrivialかつimmutableなActionには構造的に強い。`S_A=4`かつ`G=W_cur`ではlogical memoryは同量だが、decodeを消せる。
これは単なるID幅短縮ではなく、dead generation Actionを保持する`gblock`自体を消し、scheduleをself-containedにする。
`S_A=8`は`G/Q`依存、それより大きい型は原則slot-only gblockを優先する。

## candidate sortとgeneration ordinal

### 現在の順序は単なるgroupingではない

generation 2以降の`cand`は`(parent_leaf, score)`昇順に`std::sort`され、loopでは末尾から処理される。
従って実際の順序はparent降順、同一parent内はscore降順である。scoreを最小化しているため、兄弟内では悪いcandidateから先に展開する。

generation 1だけはsortされない。root展開後に`finalize_generation(1)`を呼ぶだけなので、selector slotの逆順になる。
全generationを同じsortへ変える案は、この既存の列挙順を変える。

親groupだけを維持しscore sortを消してもState transition回数は変わらないが、次を変え得る。

- beam境界の同点採否
- 同じhashで残るpath
- `threshold()`が厳しくなる時点
- thresholdを使うState側enumerationの省略量
- user codeにglobal RNGや副作用がある場合の観測順

従ってcounting sortでparentだけgroup化する案は純粋な定数倍置換ではなく、別ordering policyである。

### parent bucket + group-local sort

現在と同じkey順を保ったまま比較数を減らすには、parentでbucket化した後、各groupだけscore sortできる。

```text
O(L + W + sum(m_parent * log(m_parent)))
```

各親から残る子が少なければほぼ`O(L+W)`、1親へ全候補が集中すれば`O(W log W)`へ戻る。
追加memoryはcount `4(L+1)` byteとoutput `W*sizeof(CandIdx)`程度になる。

動的widthが急減し、少数candidateのparent ordinalが巨大rangeへ散る場合、`O(L)`のcount clearがcurrent sortより遅い。
touched-parent listとstampでclearを避けても、parent key自体のsortに`O(P log P)`が必要になる。

完全同点keyの順序は`std::sort`自身もstableではない。数学的なkey順は保てるが、現在のlibstdc++が偶然生成する同点順まで
一致させたい場合は既存sortを残すしかない。将来の再現性を重視するならenumeration ordinalを明示tie-breakへ追加する方がよいが、
既存結果を一度変えるmigrationになる。

### integral Score向けradix sort

`ScoreType`が固定幅整数なら、signed-order変換したscoreとparent ordinalをsmall keyへ詰め、LSD radix sortまたは
parent counting + group-local radixで比較sortを消せる。key順を同じにすれば親groupとscore順は維持できる。

ただし各passでkey/index bufferを往復し、histogram clearも増える。small `W`、64 bit score、多数の小parent groupでは
`std::sort`のin-place localityが勝つ反例がある。floatや比較だけを提供するuser-defined Scoreへは一般化できない。
完全同点の順序も既存`std::sort`とは一致しないため、exact replayが必要なら元selector ordinalを両実装のtie-breakへ入れる
migrationが先に必要になる。従ってgeneric defaultではなく、integral Score用policyとしてbenchmarkする案である。

### Action slotをsorted ordinalへ一致させる

現在はcandidate selector順で`gblock[gen][i]`へActionを置き、`CandIdx`を後からsortするため、
sorted位置とは別に64 bit `action_id`を各candidateへ持つ。

代わりにselector indexのpermutationだけをsortし、その順でActionを`gblock[gen]`へgatherすれば、

```text
Action slot == sorted cand ordinal
```

を世代全体の不変条件にできる。loop index`i`がそのままAction slotなので、`CandIdx.action_id`を消せる。
scoreもsort完了後のtraversalでは使わない。generationごとのbest score / ordinalを別に保存すれば、compact `cand`は
`parent_leaf`と、history有効時の`node_id`だけにできる。

典型的な32 bit Score、history無効では、currentの24 byte `CandIdx`を4 byte parentへ縮められる。
代わりにsort permutation `4W` byteを一時保持するので、traversal中のworking setとsort move byteは大幅に減る。

反対にpermutation comparatorは`Candidates::next_beam[index]`へindirect loadする。keyを直接持つcurrent sortよりcache missが増え得る。
中間案は`{parent, score, source_index}`だけのcompact keyをsortし、Actionとhistory IDを最後にgatherする方法である。

この案はpostorder構造を維持し、非同点keyでは候補key順とState呼出し順も維持できる。完全同点まで固定するには、
current側と新backendの両方へ共通のselector / enumeration ordinal tie-breakを入れるmigrationが必要になる。
generation 1だけidentity permutationにすること、max-turnの「同点最小scoreで最初のcandidate」をsorted ordinal上で
保存することも必要になる。

### best-first sibling policy

comparatorを`parent昇順、score降順`へ変え、逆loopでscore昇順に展開すると、良い兄弟が先にthresholdを厳しくできる。
State側がthresholdで高価な評価を打ち切れる場合は大きく効き得る。

ただし列挙順を変えるため、これはstorage最適化ではない。同点、hash dedup、heuristic thresholdの結果を変え得る
opt-in policyとして、current-compatible backendとは分ける。

## buffer、reserve、in-place更新

### 現在のping-pong bufferは基本的に正しい

`tour` / `next_tour`と`leaf` / `next_leaf`はswapされ、次にbuild側になったvectorを`clear()`する。
capacityは保持されるため、固定widthで形状が安定した後はallocationしない。

旧tourを読みながら同じvector末尾へnext tourをappendするin-place案は、peakで旧+新の領域が必要な点を変えない。
世代末に新領域を先頭へcompactする追加copyが必要になる。旧領域を前から上書きすると、後で低いoffsetのsegmentを読む
`copy_tour_path()`と衝突する反例がある。二本ping-pongの方が単純で速い。

leafも旧indexを末尾から読み、新boundaryを先頭から書くため、同一buffer上ではfrontが交差し得る。
4 byte/leafしかないので、複雑なin-place cycleを導入する価値は低い。

### `next_leaf`のcapacity

次のleaf数は展開するcurrent candidate数`C`と正確に一致する。従って`C`要素までの領域が必要になる。
固定widthならsearch開始時に既知上限を2本へreserveすることで、初期世代のreallocationを避けられる。

一方、毎世代`reserve(C)`をexactに呼ぶと、dynamic widthが少しずつ増えるcaseで標準vectorのgeometric growthを失い、
交互bufferが何度も再allocationする可能性がある。dynamic版ではcurrent capacityの2倍以上を確保する通常growthへ任せるか、
観測済み最大widthを余裕付きでreserveする。leafは4 byteなので、最適化優先度自体も低い。

### next tour sizeの上界

parent intervalは単調で互いに重ならず、各`q_i`は区間内の正のadjacent rankの最大値なので

```text
sum(q_i) <= sum(all scanned adjacent ranks) <= tour.size()
next_tour.size() = C + sum(q_i) <= C + tour.size()
```

となる。先頭unused segmentを省く版では`q_0+1`をさらに引ける。

これは安全なreserve上界だが、無条件の`reserve(tour.size()+C)`が速いとは限らない。dynamic widthが縮み、
selected parentが狭いsubtreeへ集中するとactual next sizeは上界より大幅に小さい。過大な仮想address / allocator負荷と
後続searchまで保持されるcapacityが、自然なgeometric growthより高くなる反例になる。

実用上は次の順で比較する。

1. 固定widthなら2本のleaf bufferへ既知上限を一度だけreserveする
2. next tour reallocation回数を計測する
3. 支配的ならLCA距離を事前passで求めactual sizeを算出するか、過去比率から控えめにreserveする

exact sizeを毎世代`reserve()`すると、sizeが少しずつ増えるたびにcapacityをexact growthして再allocationする実装もある。
`required > capacity`のときはgeometric余裕を残す必要がある。

### append APIの差

`next_tour.insert(end, trace_first, trace_last)`は連続range appendで、最適化後は単純copyになる。
`resize(old+n)`して`copy_n()`する手書き版が必ず速いとは限らない。差が出るのは主にcapacity checkと
trivial/non-trivial tokenのdispatchであり、compilerと標準libraryごとにbenchmarkする。

slot-onlyのtrivial tokenでは`memcpy`相当へ落ちる可能性が高い。iterator abstractionを消すためだけの手書きloopは、
vector internalsより悪いcodegenになる反例がある。

### final pathだけの一時領域

max-turn時は`trace[1..max_turn-1]`全体を`ridx`へcopyした後、`freed_to`より後ろだけをmaterializeする。
一時vectorをunconfirmed suffix長だけにすれば、`8*freed_to` byteの最終copyを避けられる。

`materialize_final_state=false`ならsearch終了後のlive `trace`をtarget pathへ上書きし、`ridx`自体を消す余地もある。
trueの場合はcurrent Stateをrollbackするため元のtraceが必要なので、順序を組み替えない限り別bufferが要る。
これはsearch全体で1回の最適化なので優先度は低い。

## Action lookupとID decode

### known depthを使う

rollback / apply loopはdepth `k`を既に持つ。それでも現在の`act(trace[k])`はIDをshift/maskし、
`gblock[decoded_gen]`からvector data pointerをloadする。

32 bit slot版の

```text
act(k, trace[k]) = gblock[k][trace[k]]
```

ならgeneration decodeを消し、slot rangeも24 bitから32 bitへ広げられる。外側`gblock[k]`のpointer loadは残るが、
depth loopが連続するためvector headerはcacheへ乗りやすい。

### current Actionの二重lookup

各leafでcurrent Actionは最後の`apply_op()`と直後の`enumerate_actions()`の両方へ渡される。
現在はそれぞれ`act(...)`を呼ぶ。Action blockはこの間resizeもmoveもしないため、

```text
Action& current_action = act(current_id)
```

を一度bindして両方へ使える。1 leafにつき1回のdecodeとouter block lookupを消す。

祖先apply loopの最後だけを分けるbranch / loop形が必要になるため、compilerが既にcommon subexpressionとして
保持できるかをassemblyで確認する。関数境界のalias解析により再loadされるbuildでは低riskな改善になる。

### pointer token

64 bit IDを`Action*`へ置き換えるとtour byte数を変えずにdecodeとouter vector lookupを消せる。
inner `vector<Action>`はgeneration確定後にresizeされず、outer `vector<vector<Action>>`のreallocationやvector moveは
inner allocationのAction addressを通常維持する。prefix free後にdangling pointerを読まない不変条件もID版と同じである。

ただしpointer版は32 bit slot版の2倍のtour byteを使い、allocator / lifetimeへの依存を強める。
Action load latencyよりtour帯域が支配的なら負ける。比較するなら次の三者になる。

1. 64 bit packed ID
2. 64 bit direct pointer
3. 32 bit generation-local slot

汎用既定としてはbyteと範囲の両方が改善するslot-onlyを先に試す。

### flat absolute arena

32 bit absolute indexを単一`vector<Action>`へ入れれば、depth引数なしで1 base + index lookupにできる。
absolute indexなので、通常のvector再allocationでもindex自体のremapは不要である。ただし全live Actionがmoveされ、
一時的にold/new allocationが重なる。大きい`G`やnon-trivial Actionでは、この低頻度burstがgeneration blockの小さな
新規allocationよりはるかに高くなり得る。

prefix freeされたgenerationは連続intervalとしてfree listへ返し、best-fitで新generationへ再利用できる。難点は次になる。

- width変動でfree intervalが断片化し、末尾capacityが増え続ける
- arena全体のcapacityが`2^32` slot未満でなければ32 bit absolute indexを使えない
- freed slotのdestructorと再利用時placement constructionをcustom storageで管理する必要がある
- vector growthで全live Actionをmoveするか、最大peakを先にreserveする必要がある
- page/chunk化でgrowthを避けると、Action lookupへpage decodeと追加loadが戻る
- monotone appendだけなら総探索候補数分を保持する

従ってこれは誤りではなく、Action lookupを1 base + indexへする別の比較候補である。固定最大capacityを安全にreserveできる場合や、
Actionがtiny trivialでarenaが安定する場合は勝ち得る。一方、汎用既定ではgeneration-local slotがgeneration単位reclaim、
安定したinner allocation、32 bit tokenを同時に得るため先に試す。

## leaf境界の二重scan

### なぜ単純な融合は壊れるか

現在は同じparent intervalを二度走査する。

1. `li-1 -> parent_leaf`で最大rank `lca_dist`を求める
2. rollback後、`parent_leaf -> li-1`でrecord-high rankを見つけ、target suffixを`trace`へcopyする

2のscanは終了時`prog=lca_dist`になるので、一見1だけを消せそうに見える。しかし2をrollback前に実行すると、
source Stateを戻すための`trace`をtarget IDで上書きする。source suffixを別scratchへ全copyすれば正しくなるが、
leaf scanを減らす代わりに`q_i` IDsの追加write/readを払う。

### record-high scratchでrollbackと融合する

source `trace`を上書きせず、1回目のscan方向をtarget側からの昇順へ変えると、二回目を短くできる。

1. `f==1`ならscan前にsource childの`trace[d]`を必ず1回rollbackする
2. `prog=0`で`k=parent_leaf..li-1`を走査する
3. `rank=leaf[k+1]-leaf[k]`が`prog`を超えたら`k`をscratchへ記録する
4. 増分`rank-prog`に対応するsource parent suffixを`trace[d-1-prog]`から浅い向きへrollbackする
5. scan終了後、source `trace`が未変更のままなので必要suffixを`next_tour`へappendする
6. `trace[d]`へtarget candidate Actionを代入する
7. scratchに残したrecord-high位置だけを再生し、currentと同じancestor chunkをtourからtraceへcopyする
8. target pathをapplyする

rollback callの順序は従来と同じく深いdepthから浅いdepthである。metadata scanの間に分割されるだけで、
`apply_op()`との相対順序、候補列挙順、回数は変わらない。
特に同一親の兄弟では`q_i=0`でrecord-highが1件もないが、step 1により前candidateのActionを1回rollbackする。
さらにstep 6でtarget Actionへ必ず置き換える。この`f`処理やterminal Actionをrecord増分へ混ぜるとoff-by-oneになる。

record-high数を`J_i`とすると、rankは正整数でstrictに増えるため`J_i <= q_i`である。
現在の二回目は`k_i`境界を全て見るが、この案のreplayは`J_i`件だけを見る。

```text
current : first scan k_i + second scan k_i + q_i ID copy
fused   : one scan k_i + J_i scratch/replay + q_i ID copy
```

start prefixを省く最初のcandidateではsource suffixをnext tourへ書かないため、rollbackしながらtarget traceへ
直接copyするさらに単純なspecial caseも作れる。

### 性能反例

scratchは`trace`深さまでの`uint32_t` index配列で足りるが、push、branch、後のreplayが増える。
rankが左からstrictに増えるintervalでは`J_i=k_i`となり、currentの単純な連続leaf readより遅くなり得る。

勝ちやすいのは、dynamic width縮小などで多くのdead parent leafを飛ばす一方、LCA距離`q_i`が浅いcaseである。
固定widthでほぼ隣接parentを処理するなら`k_i`自体が小さく、State Action lookupの方が支配的になる。

従って無条件置換ではなく、少なくとも次を計測して判断する。

- `sum(K)`と`sum(R)`
- record-high数`sum(J)`
- scratch write byte
- branch miss
- current `copy_tour_path()`のcycle

### RMQとnext-greater table

range maximumだけをRMQで`O(1)`にしても、target pathを構成するrecord-high segment位置が必要である。
next-greater linkを各leaf境界へ持てばrecord chainをjumpできるが、毎世代`4L` byteのbuild/writeを増やす。
currentの全LCA intervalは重ならず合計`O(L)`なので、RMQ build自体が消したscanと同じorderになる。

急激なwidth縮小で同じ旧frontierを多数回queryするわけではない現在のloopでは、persistent RMQより
per-transition record-high scratchの方が小さい。それでもtypical `J/K`が大きければ現行二重scanが最速になり得る。

### 判定

二重scanは正当性のため絶対に必要なのではない。record-high scratchで1 full scanへ減らす構成は成立する。
ただしleafの連続readをscratch write / branchへ交換するため、slot-onlyと先頭prefix省略より優先度は低い。
利益は削れる境界read `K-J` がscratch write / branch / replay overheadを上回るかで決まる。
`J<=R`なので`K>>R`は安全寄りの試作gateになるが、必要条件ではない。`K-J`を直接計測して判断する。

## その他のcurrent実装内固定費

### timer read

世代冒頭で`now_time = elapsed()`を取得した直後、dynamic widthの残り時間用にもう一度`elapsed()`を呼ぶ。
同じ`now_time`を使えばclock readを1回/世代減らせる。dynamic widthが時間境界ぎりぎりの場合は入力値が数十ns程度変わり、
丸め後widthが変わる可能性があるため、探索結果のbitwise一致ではなくpolicy同値の変更として扱う。

### unused RNG

class memberの`rnd`は現在の`beam_search.cpp`内で参照されず、`init_bs()`で再初期化されるだけである。
memberとresetを消せるが、1 searchあたり1回の固定費なのでhot path最適化ではない。

### history無効時のfield

`record_history=false`でも`CandIdx.node_id`と`BeamCandidate.node_id`はobject layoutへ残る。
conditional field / specialized candidate型で消せるが、64 bit Score + 32 bit slotの`CandIdx`では末尾paddingへ収まり
sizeofが変わらないcaseもある。`sizeof`実測なしにtemplate分岐を増やす価値はない。

### improving finished pathの反復copy

同じgenerationでfinished candidateが何度も改善すると、そのたびに`result_prefix`と未確定traceの全Actionを
`best_finished_path`へcopyする。深さ`D`、改善回数`F`なら`O(F*D*S_A)`になり得る。

best finishedのcurrent leaf ordinalと最後のActionだけを保持し、generation終了後に`next_tour`から1回復元すれば減らせる。
ただし先頭prefixを省いた表現、finished直後のreturn、start endpoint情報を含めたpath復元を揃える必要があり、
通常`F`が小さいなら複雑さが勝つ。finished改善回数を測ってから扱う。

### candidate selectorとの境界

Stateがno-opに近いbenchmarkでは、tourより`Candidates::reset()`のsegment tree全fill、hash lookup、accepted candidateの
tree updateが支配する可能性がある。これはpostorder構造とは独立である。

tour案を比較するときは`cycle/search`だけでなく、`cycle/State transition`と候補selector時間を分離する。
selectorを同時に変更するとslot-onlyやparent backendの差を帰属できない。

## 案2: leaf + adjacent LCP + generation parent-slot

### 構造

各 generation の Action slot に、1世代前の親 slot を追加する。

```text
action[d][s]        : 深さ d の Action
parent_slot[d][s]   : 深さ d-1 の親 slot
leaf_slot[i]        : DFS 順で i 番目の frontier leaf
```

候補は既に `parent_leaf` を持つため、次世代 slot を確定するときに

```text
parent_slot[d + 1][child_slot] = leaf_slot[parent_leaf]
```

と書ける。新しい ancestry metadata の構築は1候補につき1 wordであり、`next_tour` 全体を書き直さない。

### 葉間遷移

同じ深さの現在葉 `u` から次葉 `v` へ移る最小実装は次のようになる。

1. `u` と `v` が一致するまで両方の parent slot を1段ずつたどる
2. `u` 側の Action はその場で rollback する
3. `v` 側の `(depth, slot)` は小さい scratch stack へ積む
4. LCA で一致したら scratch stack を逆順に apply する

親鎖を上る1回は、必要な rollback または後の apply に対応する。LCA だけを先に求める table は不要である。

上の両側 parent walk は最小構成ではあるが、比較実装の本命ではない。固定深さと DFS 順をさらに使えば、
`leaf_handle + adjacent_lcp_depth + parent_slot` にできる。

### leaf handle + adjacent LCP + parent-slot

frontier を次の3種類の配列で表す。

```text
leaf_handle[i]          : DFS 順で i 番目の葉の current-generation slot
adjacent_lcp_depth[i]   : leaf i と leaf i+1 の LCA depth
parent_slot[d][s]       : depth d, slot s の親 slot
```

現在の root-to-leaf path は `trace[d]` に保持する。隣の葉へ移るときは LCA depth `h` が既知なので、
source と target の parent chain を照合する必要がない。

1. `trace[d], trace[d-1], ... trace[h+1]` を使って rollback する
2. target の `leaf_handle` から parent-slot を `h+1` までたどり、target suffix を `trace` へ逆向きに埋める
3. `trace[h+1], ... trace[d]` を apply する

これにより、source 側の parent load と「同じ node に着いたか」の比較を消せる。target 側の dependent load は残る。
current の `copy_tour_path()` による連続 ID copy を、この target parent chain で置き換える構造である。

### 次世代 adjacent LCP の生成

旧 frontier の深さを `d` とし、新候補を親葉 index で group 化する。

- 同じ親の連続する子同士は、LCA depth が `d`
- 異なる親 `p < q` の子同士は、旧葉 `p` と `q` の LCA depth
- ordered leaf 列では、`LCP(p, q)` は区間 `p..q-1` の `adjacent_lcp_depth` の最小値

候補の親 index は単調なので、group 境界ごとの区間 minimum は旧 LCP 配列を一方向に走査して求められる。
同じ旧境界を何度も読む必要はない。

固定幅で `L` と `W` が同程度なら世代合計 `O(W)` である。動的幅で旧 `L` が新 `W` より大きい場合は、正確には
`O(L + W)`、または最初と最後の選択親の間を読んだ長さになる。`W` 件しか残らないから常に `O(W)` とするのは
正しくないが、current の単調な `leaf` 走査と同じ上限である。

この生成法は新しい候補の Action 内容を見ない。Action が同じでも別 node として採用された兄弟なら LCP は親深さ `d`
であり、hash dedup で同一 state を統合するかどうかは candidate selector 側の問題である。

### traversal orientation と世代境界

adjacent LCP は左右どちらへ走査しても使えるが、current と同じ候補評価順にするには orientation を固定する必要がある。
`beam_search.cpp` は `(parent_leaf, score)` で整列した `cand` を逆向きに処理する。hybrid でも次のどちらかに統一する。

- `leaf_handle` を格納順で持ち、毎世代末尾から先頭へ処理する
- 実際の処理順に反転して格納し、先頭から処理する

重要なのは、generation 終了時の State がどちらの endpoint にいるかと、次 generation の最初の親を一致させることである。
終了 endpoint の葉から生存子が出なければ、次の最初の候補親まで旧 frontier 上を移動してから子 Action を apply する。
この最初の移動にも旧 `adjacent_lcp_depth` の区間 minimum と target parent chain を使える。

current の `f` は、最初の候補ではまだ current-generation Action を rollback しないための境界処理である。
hybrid でも「State は旧葉にいるが、次に展開する node はその子」という世代境界を明示しないと off-by-one になる。

### 同一親の兄弟

同じ親の子 `a`, `b` の LCP depth は親の深さ `d` なので、必要な移動は

```text
rollback(a)
apply(b)
```

だけである。target suffix は1 nodeなので parent chain を読む必要もない。多数の生存候補が少数親に集中する場合、
この hybrid の dependent load は少なくなる。

### `L=1`

現在走査する葉が1個なら adjacent LCP 配列は空である。走査後のState cursorもその葉にいるため、
その葉のdepthまでは`result_prefix`へ確定でき、葉間traversalは不要である。

ただし「選択後の新frontierが1候補」というだけでは、そのchild Actionまで即時確定できない。
世代境界ではStateがまだ親endpointにいるため、親が別なら先にrelocateし、childをapplyしてtraceへ入れる必要がある。

現在の葉集合について実装する場合、一般 case の `min(adjacent_lcp_depth)` を呼んではならない。共通prefix depthは

```text
L == 1 ? current_depth : min(adjacent_lcp_depth)
```

とする必要がある。唯一の候補が消えて frontier が空になる case は別の終了処理である。

### prefix 確定

`L >= 2`なら現在のfrontierだけの共通prefix depthはadjacent LCPの最小値、`L=1`なら葉depthである。
しかし世代commit時に解放してよい集合は、新frontierのchild葉そのものではなく、次generationで参照するdistinct parent葉と
現在のState cursor endpointの和集合である。cursorをrelocateしない限り、frontierだけのLCPを使うとdead endpointからの
次entryに必要なActionを先に解放する反例がある。

ordered parent葉では、survivor parentの両端とcursor endpointのLCPをadjacent LCPの区間minimumから求められる。
代表となるcursorの`trace`から新しく確定したActionを`result_prefix`へ移す。child depthまで進めるのは、Stateをそのchildへ
移動してActionをtraceへ入れた後に限る。

generation block を深さ単位で解放する場合、深さ `prefix+1` の node が持つ `parent_slot` は解放 block を指したままにできる。
以後の LCA depth は必ず `prefix` 以上なので、target suffix 復元は depth `prefix+1` の Action を取得したところで止まり、
その dangling parent 値を読まない。この停止条件を debug build で検査する必要がある。

### この hybrid は反証できるか

正当性上の反例は、次の条件を守る限り見つからない。

- 新 frontier が旧 DFS leaf orderを保つ親 group 順である
- group 間 LCP を旧 adjacent LCP の区間 minimum で作る
- 同一親の兄弟 LCP を旧葉深さにする
- generation endpoint と最初の親の移動を処理する
- `L=1` と prefix 確定を特別扱いする
- prefix LCPの集合へState cursorを含めるか、解放前にcursorをrelocateする
- parent slot の寿命を descendant より短くしない

性能上は反証できる。current の target suffix 復元は連続した ID を copy するのに対し、この hybrid は target ごとに
依存 parent load を行う。従って tour write が支配的でない workload では、hybrid の方が遅くなる可能性が高い。
それでも pure parent-slot を試すなら、両側 parent walk よりこの hybrid を優先するべきである。

### current postorder から消えるもの

- `tour`
- `next_tour`
- `copy_tour_path()`
- Action ID の generation field
- live tree 全体を毎世代 flat schedule へ書き直す処理

### 代わりに増えるもの

- 未解放 slot ごとの `uint32_t parent_slot`、概ね `4G` byte
- 親鎖をたどる dependent load
- target suffixを`trace`へ深い側から埋めるdependent writeと、浅い側からのapply read
- generation 解放境界の re-root 管理
- 最終 path 復元時の parent walk

### 時間の比較

先頭`Z`を除いたcurrent postorder は連続した `O(T)` ID read/write を行う。parent-slot は ancestry 構築を `O(W)` write にできるが、
状態巡回中に `O(X)` 回程度の親 index load が加わる。

両者の主要項はどちらも `O(X)` である。

```text
current  : edge-wise State work + sequential ID copy/read
parent   : edge-wise State work + dependent parent load + trace backfill
```

hybrid本命では別scratchを確保せず、rollback済みの`trace` suffixへtarget slotを深い側からbackfillする。

`std::copy` 相当の連続処理は非常に安い。parent load は次の address が直前の load 結果に依存するため、
cache miss を memory-level parallelism で隠しにくい。従って metadata の書込量が減っても遅くなる場合がある。

### メモリの比較

`K_trace=capacity(trace)` とする。固定幅でcurrent/next capacityが論理幅へ近い定常概算は次になる。

```text
current 64 bit tour : 8 * (capacity(tour) + capacity(next_tour)) + 8K_trace + O(4L)
slot-only tour      : 4 * (capacity(tour) + capacity(next_tour)) + 4K_trace + O(4L)
parent oracle       : 4G + 16W + 4K_trace
parent compact      : 4G +  8W + 4K_trace
```

oracleは独立frontier 2本とLCP 2本、compactはLCP 2本を持つ。`4G`はlogical parent sizeであり、
実allocationはparent slab capacityに従う。
両方式ともAction側の`G_cap*S_A`、vector header、allocator metadataを別に加える。

`G` は live edge 数ではない。generation block のうち、既に子孫を失った Action slot も含む。
`G >> E_live` なら parent-slot は metadata memory でも不利になり得る。

### 勝ちやすい条件

- State 操作が軽く、tour の ID bytes が profile 上で支配的
- 未確定深さが長く、毎世代ほぼ同じ古い prefix ID を next tour へ書いている
- `G/E_live` が小さい
- parent arrays が LLC に収まる
- Action が小さく、current の metadata 比率が大きい

### 負けやすい条件

- `apply_op()` / `rollback()` が重く、tree metadata が誤差
- `G/E_live` が大きい
- parent chain が LLC を外れ、依存 miss が多い
- current tour が L1/L2 に収まり、copy が帯域上ほぼ無料
- Action が大きく、Action 本体の load が支配的

### 判定

current postorder と本当に比較すべき、構造的に異なる第一候補である。
ただし「parent-slot の方が速い」と事前には言えない。別 backend として同一探索結果を確認しながら測るべきである。

## 案2b: refcount 付き parent-only arena

### generation block の弱点を除く変種

generation 配列では、ある深さの少数 node だけが live でも block 全体を保持する。parent metadata は `4G`、
Action 本体も `G * S_A` 残る。

代わりに、各 live node を stable arena slot に置く。

```text
action[id]      : 親から node id への Action
parent[id]      : 親 node id
refcount[id]    : live child、frontier handle、State cursor からの参照数
leaf_handle[i]  : current frontier node id
adjacent_lcp[i] : 隣接 frontier の LCA depth
```

child / sibling pointer は不要である。DFS 順は `leaf_handle`、分岐深さは `adjacent_lcp`、下り path は parent chain から得る。

### 更新と即時解放

1. 最終選択された子 node だけを arena に作り、その親 refcount を増やす
2. 全ての新 leaf handle を作った後、旧 frontier handle を release する
3. refcount が0になった node を破棄し、その parent を再帰的に release する
4. 解放 slot は free list で再利用する

各 node は生成時と解放時に1回ずつ処理されるため、探索全体では allocation / release node 数に対して償却線形である。
live frontierとcursorへのpath上にないActionを即時破棄でき、理想的にはnode数を`E_live+1`程度に保てる。

### State cursor の pin

generation 終了時の mutable State は、選択後に死ぬ旧葉の上にいる場合がある。その葉を直ちに解放すると、
次の生存親へ移動する rollback Action が失われる。

安全な方法は State cursor を arena node への追加参照として扱うことである。

- State が現在乗る node を cursor ref で pin する
- frontier ref を外しても cursor path は残る
- State が rollback して node を離れた時点で cursor ref を親へ移し、離れた node を release する
- 生存 subtree に入ったら通常の child ref が寿命を保証する

別案として、全旧 frontier ref を外す前に State を最初の生存親へ移動する方法もある。処理順の変更が大きいため、
cursor pin の方が current の search loop と対応させやすい。

### prefix 確定と re-root

新frontierと、pinしたState cursorの全てに共通する祖先`r`まで確定したら、旧rootから`r`までのActionを
`result_prefix`へ移す。その後`r`を新しいroot sentinelとみなし、`r.parent`を切って祖先chainをreleaseできる。
frontierだけのLCAがcursorとのLCAより深い場合は、cursorをlive subtreeへ移すまで深い方へre-rootしてはならない。

`r` 自身の親辺 Action は既に result に移したため、arena 上では破棄するか dummy にする。全ての boundary child を
付け替える必要はなく、`r` だけを re-root すればよい。`W=1`でもcursorが同じpath上にあるか、先にrelocateした場合は
同じ処理で唯一の葉をrootに進められる。

### memory

Action と metadata を SoA にし、logical live node数を`N`、arenaの確保済みslot capacityを`N_cap`とする。
logical live量は

```text
N * (S_A + sizeof(parent) + sizeof(refcount)) + free-list capacity + O(W)
```

であり、`N`は通常`E_live+1`と同程度になる。一方、inline slot storageを保持する通常のfree-list arenaのallocated量は

```text
N_cap * (S_A + sizeof(parent) + sizeof(refcount)) + index / free-list capacity + O(W)
```

になる。free時にdestructorを実行すればActionが所有するheap payloadは直ちに返せるが、大きなinline `Action` byteは
arenaのpeak capacityに残る。page単位で空きをOSへ返す設計ならallocated量を下げられるが、fragmentationとlookupが増える。

一方、arena vector が peak slot 数まで capacity を保持するだけでは、Action payload は自動では解放されない。
free 時に destructor を実行し、再利用時に placement construction する slot storage が必要である。
単に `free_ids` へ入れるだけでは「即時 Action 解放」という利点が成立しない。

### speed risk

- parent NodeId が generation-local slot より散らばりやすい
- refcount の read-modify-write が node ごとに増える
- node 解放の cascade が特定 generation に集中する
- Action destructor が重いと hot path へ解放費が出る
- free slot 再利用で Action の placement construction が必要
- stable NodeId の reuse 前に candidate、cursor、history の全参照を消す必要がある
- debug 用 generation tag を handle に持たせると ID が64 bitになり得る

### 判定

これはlogical memoryとowned heap payloadについてparent-slot generation配列より明確に強い領域がある。
特に`G/E_live`が大きい場合と、Actionが大きな所有payloadを持つ場合である。inline RSSでも勝つには、
`N_cap`がgeneration slab全体の`G_cap`より小さいこと、または空pageを返せることが必要になる。

速度については current postorderを無条件に上回らない。小さい Action では refcount と arena pointer chasing が
tour copy より高くつきやすい。parent-slot 試作で generation block の保持量が問題になった場合に比較する第二変種とする。

## 案2b-2: live ordered tree + open DFS

### `G >> E_live`に対する成立性

refcount arenaのnodeにordered child / sibling linkを追加し、flat tourもadjacent LCPも作らず、
構造木をそのままopen DFSする案は正しく構成できる。世代境界で残すnodeを次に限る。

- 確定済みprefix直後のroot sentinel
- root sentinelから新frontierへの全未確定path
- root sentinelからmutable State cursorへの未確定path

この木の未確定Action辺数を`E_live`とすれば、root sentinelを含むlogical live node数は正確に

```text
N_live = E_live + 1
```

である。旧frontier refを外したとき、child、frontier handle、cursorからの参照が全て0になったnodeを
親方向へ連鎖解放すれば、generation blockのdead slotを残さない。ただし更新中は旧木と新しい
`W_new`個の子辺を一時的に両方持つため、logical peakは概ね`E_old + W_new`まで増える。

### endpoint zombieとcursor pin

open DFS後のStateは最後に展開した葉にいる。その葉から子が1個も生きなくても、次generationのentryで
生存pathへrollbackするまでそのActionは破棄できない。従って次の順序が必要になる。

1. cursor nodeへ外部refを1個保持し、世代間で切らさない
2. selected childを親順、必要な兄弟順で接続する
3. 旧frontier refを外し、dead subtreeを連鎖解放する
4. 次generationの先頭でcursorから最初のactive leafへ移動する
5. 上向き辺は`rollback(edge)`後にcursor refを親へ移し、離れたnodeをreleaseする
6. 下向き辺は`apply_op(edge)`後にcursor refを子へ移す

cursor refだけが保持し、frontier refを持たない構造上の葉をendpoint zombieとする。
`first_child == NIL`だけで展開葉と判定するとこのzombieを誤って展開するため、active frontier flagまたは
frontier handle集合を別に持つ。prefix確定のLCP対象にcursorも必ず含め、relocate前にfrontierだけへ
re-rootしてrollback Actionを失ってはならない。

### open DFSとState遷移下限

child orderとsibling directionを通常版のparent ordinal単調順に合わせる。世代開始cursorを`s`、最後のtargetを`t`、
全targetと`s`の最小部分木を`H`とすれば、ordered tree上のopen DFSはcurrentと同じく

```text
X = 2|E(H)| - dist(s, t)
```

を厳密に達成できる。完全Euler列やclosed DFSのように毎回rootへ戻すのではなく、末尾pathの帰還を省く。

ただしこれはcurrentが既に達成している同じ辺遷移下限である。ordered treeが減らせる可能性があるのは

- `tour` / `next_tour`とadjacent LCPの構築
- target parent chainの逆向きbackfill
- Action IDのdepth / generation decode

である。下りはchild linkを浅い側から直接たどれる。代わりに次が増える。

- parent / child / siblingのdependent load
- leaf / internal / zombie判定のbranch
- intrusive sibling listの散在writeとunlink
- refcount release cascade
- DFS path stackまたはnext-node探索のbranch
- free-list slot再利用によるlocality悪化

従ってCPU上の勝敗は、currentの連続stream copyとordered nodeのpointer / index chaseの交換である。

### logical memoryとallocated memory

32 bit indexのordered nodeは概ね次を必要とする。

```text
parent
first_child
prev_sibling
next_sibling
refcount / active flags
```

非循環listで反対側の端も直接得るなら`last_child`も必要である。循環sibling listへ統合するかにより、
Actionとalignmentを除くordered metadata `m_ordered`は概ね20--24 byte / nodeになる。

```text
ordered live tree    : N_live * (S_A + m_ordered) + O(W + D)
parent-only refcount : N_live * (S_A + 8)         + O(W + D)
generation parent    : G      * (S_A + 4)         + O(W + D)
```

`O(W+D)`の係数はcurrent / next handleが重なるgeneration boundaryか、stackless traversalか明示stackかで変わる。
parent-onlyはsteadyでleaf handleに加えadjacent LCPを約`4W` byte持つ。ordered treeはLCPを消せるが、木の辺数は
固定深さfrontierの葉数以上なので通常`E_live >= W`であり、nodeごとの追加link 12--16 byteを
`4W`のLCP削減だけで相殺できない。generation parentに対する粗いmemory損益条件は

```text
G / N_live > (S_A + m_ordered) / (S_A + 4)
```

である。`S_A=4`なら右辺は約3--3.5、parent-onlyの同じ条件は`(S_A+8)/(S_A+4)=1.5`に過ぎない。
従って`G >> E_live`対策のmemory break-evenはparent-onlyの方が早い。

通常の`vector<Node> + free_ids`で確保するbyteは

```text
N_cap * (S_A + m_ordered) + free-list capacity
```

であり、`N_cap`は過去の最大同時live数を保持する。従って保証はinstantaneous `O(E_live)` allocationではなく、
`O(max_t E_live(t))`である。現在量に合わせallocationを戻すには、ID remap付きcompactionまたは空pageを返すpaged arenaが
必要になる。paged arenaも1 pageに1 nodeずつ残るfragmentationでworst case保証を失う。

Actionが所有するheap payloadをrelease時に返すには、destructorを呼び、再利用時にplacement constructionする
manual slot lifetimeが必要である。inline Action storage自体はarena capacityに残る。`result_prefix`とcandidate selectorの
storageもこの`N_live`式には含まない。

### `beam_search_radix.cpp`との違い

既存`beam_search_radix.cpp`は最も近い明示木実装だが、composeを無効にするだけでこの案にはならない。

- `Node`は`parent / child_cnt / first_child / prev / next / Score`を持つAoSである
- `free_node()`はActionをdestroyせずそのままfree listへ入れるため、owned payloadの即時解放にならない
- 単一子pathで`Action::compose()`を呼ぶため、通常版より強いAction契約を要求する
- 毎世代rootから始めてrootへ戻るclosed DFSで、open traversalの`dist(s,t)`辺の省略を使わない
- surgery時にStateがrootにいることを前提にするため、cursor pinとendpoint zombieの概念がない
- root直下のprefix確定時にStateへActionをapplyするが、open版のStateは既にdescendantにいるため同じ処理は二重applyになる
- 兄弟を良いscore順に展開する一方、通常版は現在、同親内でscore降順となるため、順序policyも異なる

従って比較するにはopen traversal、cursor lifetime、active leaf判定、prefix re-root、兄弟順を全て変える必要がある。
composeが真にprimitive workを減らす型では、既存Radixを別契約backendとしてそのまま比較する。

### 判定

live ordered treeは構造として成立し、generation別storageの`G >> E_live`問題をlogical memory上は解消できる。
しかし同じ`N_live=E_live+1`はより小さいrefcount parent-only arenaでも達成でき、State辺遷移回数も
currentから減らない。従ってdead Action保持を解消する第一実装にはしない。

まずparent-only arenaを作り、`G/E_live`、`N_cap/E_live`、parent-chain backfill、LCP処理のprofileを取る。
logical memoryよりmetadata latencyが支配し、自然な下りDFSがparent鎖復元より速いと見込める場合にだけ、
ordered treeを第二のCPU trade-offとして比較する。

## 案2c: monotone parent map の unary bitvector

### parent-slot の値列には単調性がある

新 frontier を親 ordinal 順に並べる。同じ親の子を連続させると、child ordinal `j` から parent ordinal `p_j` への写像は
単調非減少になる。各旧 parent `i` の生存 child 数を `c_i` とすると、この写像は count 列だけで決まる。

次の bitvector を generation ごとに作る。

```text
B = 1^c_0 0 1^c_1 0 ... 1^c_(Wprev-1) 0
```

長さは `Wcur + Wprev` bit、1の数は `Wcur` である。0-indexed の child `j` に対応する1の位置を `select1(j)` とすると、

```text
parent(j) = select1(j) - j
```

となる。右辺は、その1より前にあるseparator 0の数である。子が0個のparentは連続する0として自然に表現される。

### tour-free traversal への利用

各 generation の Action slot を、その frontier ordinal と一致させる。

```text
action[d][j]       : depth d の frontier ordinal j の Action
parent_map[d]      : depth d の ordinalを depth d-1 へ写す unary bitvector
adjacent_lcp       : current frontier の隣接 LCP depthだけを保持
```

target leafからLCA直下までpathを復元するとき、各depthで `parent_map[d].select1(slot)-slot` を計算する。
source側はcurrent `trace` でrollbackできるので、案2のleaf + adjacent LCP traversalをそのまま使える。

過去全 generation の adjacent LCPを残す必要はない。次 frontier のLCPを生成したら旧配列を捨てられるため、
LCP memoryは `O(W)` である。一方、parent bitvectorは未確定pathを復元するため generation ごとに残す。

### action slot と exact order

現行 `finalize_generation()` はcandidate selector内のslot順でActionをgeneration blockへ移し、その後small `CandIdx`だけを
親、score順にsortする。action slotをfrontier ordinalと一致させるには、順序を逆にする必要がある。

1. final candidate のsmall index列を親、score、必要ならenumeration ordinalでsortする
2. sorted index順にActionをgeneration blockへ1回だけmoveする
3. sorted位置を新しいaction slot / frontier ordinalにする
4. 親groupのcountからunary bitvectorを作る

Actionのmove回数は1候補1回のままにできる。ただし大きなAction自体を`std::sort`してはならず、small indexをsortする。

currentの完全同点順は`std::sort`だけでは規定されない。探索結果のexact一致を要求するなら、元selector slotまたは
enumeration ordinalをtie-breakへ含め、current側も同じ順序へ固定する必要がある。

### 向き

currentは、前generationでStateが到着したendpointから次の走査を始めるため、見かけ上のleaf orientationが反転し得る。
単調写像を保つ方法は2つある。

- frontierの物理順を常に同じDFS canonical orderにし、走査方向だけ左右交互にする
- frontierを実走査順に格納し、parent groupを前generationの逆順で列挙して単調非増加mapとして符号化する

前者ならbitvectorのparent ordinalは常に物理ordinalそのもので、generationごとにtraversal direction bitだけ持てばよい。
後者ならunaryが返すparent rankを `Wprev-1-rank` へ変換するorientation bitが必要になる。

どちらでも成立するが、candidateの`parent_leaf`、Action slot、adjacent LCP index、State endpointの4つで同じ向きを使う必要がある。
単にcurrentの`now_leaf_idx`をsorted indexへ置換すると、次generation開始時のState endpointが合わない。

### raw memory

固定幅 `Wprev ~= Wcur ~= W` なら、1 generationのraw parent mapは約 `2W` bitである。
未解放generation全体では概ね

```text
sum(Wprev + Wcur) bit ~= 2G bit = G/4 byte
```

となる。`uint32_t parent_slot[G]` の `4G` byteと比べ、raw bitだけなら約16分の1である。

ただし実用的な `select1` には補助indexが必要である。例えば64個の1ごとに32 bitのbit位置をsampleすると、
平均幅が固定なら約 `G/16` byteが加わる。generation offset、width、orientationも `O(D)` 必要になる。
それでも典型的には32 bit parent arrayより1桁小さい。

### 情報量の下限

zero-childを許し、`Wcur` 個のordered childを `Wprev` 個のordered parentへ単調に割り当てるmap数は、weak compositionの

```text
C(Wcur + Wprev - 1, Wprev - 1)
```

である。必要bit数はこの2進対数以上になる。`Wprev = Wcur = W` では約 `2W - O(log W)` bitなので、
長さ約`2W`のunary bitvectorは加法 `O(log W)` を除いて情報量下限に近い。

つまり、親写像を全てexactに保存するという条件ではraw表現を大幅に小さくする余地は少ない。
残る問題はselect補助indexとCPU costである。

### select1 の実装cost

x86に汎用の単一 `select1(bitvector, k)` 命令はない。queryは少なくとも次を必要とする。

1. k番目の1に近いsample positionを読む
2. sampleからtargetを含む64 bit wordまで進む
3. wordのpopcountで残りrankを減らす
4. word内のk番目の1のbit位置を求める

BMI2の`PDEP`と`TZCNT`でword内selectを作れるCPUもあるが、`PDEP`のthroughput / latencyはmicroarchitecture依存である。
portable実装ではpopcount、countr_zero、byte tableなどを組み合わせる。

parent chainでは、あるdepthのselect結果が次depthのquery indexになる。従ってgenerationをまたぐqueryは依存列であり、
複数cache missを並列に隠しにくい。32 bit parent arrayなら1 dependent loadで済むところを、unary版はsample、bit word、
popcount/selectの命令列へ置き換える。

### sample index と長いzero run

「64個の1ごとにpositionをsampleし、そこから最大64個の1を数える」だけでは、bit position上のworst-caseは小さくない。
sample間にzero-child parentの長いrunがあると、多数の0 wordを横断し得る。

例として、子を持つparentが先頭と末尾だけで、中間のほぼ全parentが0 childなら、2つの1-runの間に
`Theta(Wprev)` 個のseparator 0が並ぶ。target 1のrank差が小さくてもbit距離は大きい。

対策は次になる。

- select sampleを細かくし、large gapだけ例外tableへ置く
- bit positionのsuperblockとone-rankの二段indexを持つ
- nonempty word bitsetを追加する
- standard succinct selectのsuperblock / subblock構造を実装する
- `P` が小さいgenerationだけpositive-parent run listへ切り替える

補助indexを増やすほどraw `2W` bitの利点は減る。平均densityが約1/2なら単純sampleでも速い可能性があるが、
汎用ライブラリではlong zero runをbenchmarkへ必ず含める。

### dynamic width の反例

`Wcur` が `Wprev` より極端に小さい場合、unaryは0-child parentのseparatorだけで `Wprev` bitを使う。
32 bit parent arrayは`32Wcur` bitなので、

```text
Wprev > 31 * Wcur
```

ならraw sizeだけでもunaryの方が大きくなり得る。動的幅が急減するgenerationや、selected parentだけを残す
deferred tourと組み合わせてparent universeが大きいままの場合が反例になる。

この場合は次を選ぶ。

- `uint16_t` / `uint32_t` direct parent array
- positive parent ordinalとchild countだけのrun encoding
- Elias-Fano系のmonotone sequence
- parent universeをselected parentへremapしてからunary化

### 1葉とprefix解放

現在走査する葉とState cursorが同じ1本だけなら、bitvectorとselect indexを作るより、その親pathを即時prefix確定する方がよい。
選択後の新frontierが1候補でもcursorがdead endpointにいる場合は、その候補pathだけのLCPで解放してはならない。
cursorとのLCPまでに留めるか、先にcursorを唯一の生存pathへrelocateする必要がある。未適用のchild Actionを確定する場合は、
applyしてtraceへ入れる処理も先に行う。

prefix depthより古いgenerationを解放するとき、target suffix復元はdepth `prefix+1` のActionを得た時点で止まる。
そのnodeからprefix depthへのparent queryは不要なので、境界のparent bitvectorも読まない設計にできる。
bitvector slabはgeneration単位で再利用する。

### 判定

この方式は正しく成立する。`Wprev ~= Wcur`のdense / fixed-width regimeではparent mappingのraw memoryがほぼ情報量下限で、
32 bit parent-slotより明確に小さい。一方、`Wprev > 31Wcur`のような急縮小ではraw unary自体がdirect parentより大きい。

一方、speed版とは限らない。parent-chainの各edgeでrank/selectを行うため、parent arrayがcacheに収まる条件では
direct loadに負けやすい。unary working setだけがLLCに収まり、32 bit parent arrayがmemory bandwidth / cache missを
支配する深い大幅beamで勝つ可能性がある。

実装順は `uint32_t parent` を正解oracle兼speed基準にし、その後unaryを同じleaf + LCP traversalへ差し替える。
固定の1形式にせず、width ratioとzero-run統計でdirect / unary / sparse-runを選ぶgeneration-local encodingが妥当である。

## 案2d: B世代 path-block

### 狙いと表現

direct parentはtarget suffixを`r`辺復元するとき、ほぼ`r-1`個のparent slotを直列に読む。
path-blockはこの深さ方向の依存列だけを短くする。anchor周期を`B_p`、boundary深さを
`a=kB_p`とし、深さ`a`のordered active nodeごとに次を持つ。

```text
path_slot[a][r]      : 深さ a-B_p+1..a の B_p 個の Action slot
previous_anchor[a][r]: 深さ a-B_p の祖先anchor ordinal
```

`path_slot` は浅い側から深い側へleaf-majorに連続配置する。`previous_anchor`はAction slotではなく、
1つ前のboundary recordを引くordinalと明示する。この区別をせず、generation-local Action slotで古い
anchor blockを直接indexする実装は、slotとfrontier ordinalが異なると壊れる。
以下のbyte式は、sorted / gatherにより全generationの`Action slot == logical frontier ordinal`を保つ場合の式である。
この不変条件を使わない版は、boundaryごとのslot-to-anchor-ordinal inverseまたは明示record handleが必要で、
少なくとも追加`qW_a` byteとlookup costをmemory / construction式へ足す。

block末尾の深さ`a`のslotはrecord ordinalから復元できる。従って`B_p-1`個の中間slotと
`previous_anchor`1個、合計`B_p` wordに
省略できる。slotとordinalが一致しない場合は末尾のleaf handleを別に1 word持つため、
このmemory省略は成立しない。

### decodeとState操作

adjacent LCP depthを`h`とする。current partial epochは従来のparent slotでたどり、それより古いtarget pathは
`path_slot` をtraceの対応深さへ連続copyする。次のblockだけ`previous_anchor`で選ぶ。`h`を含む
blockでは`h+1`より深いsuffixだけ読み、そのblockの`previous_anchor`を読まず止まる。

suffixが長い場合のmetadata依存段数は概ね次になる。

```text
direct parent : r - 1
path-block    : current partial epoch深さ + ceil(older suffix深さ / B_p)
```

block内のslotは全て読むが、address同士は依存せず連続する。ただしAction payloadは依然として
`action[depth][slot]`から読み、`apply_op()`は浅い側から辺ごとに実行する。`rollback()`も辺ごとである。
従って減るのはslot decodeの依存段数であり、State操作回数`X`は減らない。

`path_slot`は各targetの絶対的な深さchunkなので、葉ordinalの増加、減少のどちら向きにも使える。
後述のfront-coded anchor streamと違い、snakeの向き反転のために2本持つ必要はない。

### 構築cost

epoch内の各世代で親のpartial pathをcopyして子へappendする素朴版は、幅`W`一定なら1 epochで

```text
W * B_p * (B_p + 1) / 2 slots
```

を書く。1世代平均`Theta(WB_p)`になり、依存段数を減らすほど書込みを増やすため棄却する。

成立性のある構築法はboundary transposeである。epoch中は通常のdirect parentを書き、boundaryで
深さ`a`の各active nodeから`B_p`段たどってleaf-major blockへ転置する。幅一定ならboundaryで
約`B_p W` slotを読み、full版は`(B_p+1)W`、末尾slot省略版は`B_p W` wordを書く。

slot byteを`q`とすると、通常のparent writeも含む1世代平均は次になる。

```text
direct parent                  : qW
full path-block               : qW + q(B_p+1)W/B_p = qW(2+1/B_p)
implicit-top-slot path-block  : qW + qB_pW/B_p     = 2qW
```

adjacent LCPの`lW`とAction payload writeは共通項である。`B_p=1`は最適化すればdirect parent配列を
そのまま1-word blockとしてaliasできるが、依存段数も減らない。別bufferへtransposeする実装なら
単にcopyが増える。

### memoryと幅変動

unresolved深さのAction slot数を`G`、各boundaryのactive幅を`W_a`とする。完成blockのword数は

```text
full             : sum_a (B_p + 1) W_a
implicit top slot: sum_a B_p W_a
```

である。全世代の幅が安定する近似では、full版が約`qG(1+1/B_p)`、末尾slot省略版が
約`qG`である。さらにcurrent epochのdirect parentが最大`qB_pW`重なる。Action payload `G*S_A`はどちらにも残る。

幅変動時は`sum_a B_pW_a` と `sum_d W_d`が一致しない。boundaryだけ幅が大きいとpath-blockが大きく悪化し、
boundaryだけ幅が小さい場合は小さくなり得る。同じ古いanchorの多数子孫がboundaryで生きると、
共有祖先slotを葉ごとのblockへ重複書込す。これはper-leafの連続pathを得るための交換条件である。

### endpointとprefix寿命

path-blockは下りtarget suffixの供給形式だけを変える。generation開始のState cursorと最初のparentの移動、
`f=0`相当のchild apply、終了endpointの保持はleaf + LCP + direct parentと同じである。prefixを確定するLCP集合には
survivor parentだけでなく現在cursorを含めるか、先にcursorを生存pathへrelocateする。

`h`を含むblockの`previous_anchor`を読まない停止条件を守れば、確定prefix上のstale slotがblock内に
残っても対応Actionをdereferenceしない。全要素が`freed_to`以下になったblockは解放できる。

### 反例と判定

- 兄弟集中でsuffix長が1ならdirect parent loadは元々0個で、blockに利益がない
- prefixが`B_p`世代より速く進むと、transposeしたblockをほぼ使わず解放する
- transposeは各葉に`B_p`段のparent readとblock writeを払い、幅ピークでburstする
- State操作またはAction payload missが支配すると、slot decode依存の短縮は表に出ない
- `G/E_live`が大きい場合、generation Action blockとdead boundary recordを持つ問題は解消しない

従ってpath-blockは成立するが、direct parentのmemory圧縮ではない。slot総数を同程度以上に保ったまま、
依存parent loadを連続block readへ交換するlocality / latency案である。まずwindowed direct decodeで複数target chainを
並行させる方法と比べ、長いsuffixのparent missが支配すると確認できた場合にだけ`B_p=2,4,8,16`を試す。

### bidirectional anchor overlayとの違い

path-blockは深さ方向のabsolute chunkをboundary葉ごとに重複して持ち、依存段数を約`B_p`分の1にする。
bidirectional anchor overlayはanchor frontierの葉間suffixをfront codingして木辺を共有し、epoch内だけparentを持つ。
後者はshared edgeの重複を避ける代わりにsnake対応の2方向baseが必要である。どちらも`X`は減らないが、
path-blockはdecode latency、anchor overlayはschedule writeの償却を主に狙う別の交換である。

## 下り path を供給する方法の三択

ordered frontier の隣接 LCP depth が全て分かっていても、LCA から次葉までの Action sequence は別途必要である。
同一 subtree の葉を連続して処理するとき、各 target suffix を順に連結した列が compact postorder tour に対応する。

現行契約で下り Action を State へ渡す方法は、実質的に次へ分かれる。

1. target suffix を事前に flat ID 列へ materialize する
2. target leaf の parent chain から実行時に復元する
3. suffix の primitive sequence を実行しなくてよい強い表現を使う

1が current postorder、2が leaf + LCP + parent-slot である。明示木は2に child traversal metadata を加えた形、
path-blockは2の深さ方向decodeをchunk化した形、span ropeとbidirectional anchor overlayは1と2の中間である。
3はAction compose、State snapshot、State copy、
path batch APIに当たる。

従って adjacent LCP だけで `tour` と parent chain の両方を消すことはできない。LCP は「どこまで戻るか」を与えるが、
「そこから何を適用するか」は与えない。この三択が、同契約で current を無条件に支配する第4の表現を見つけにくい理由である。

## 案3: 生存親の virtual tree / Patricia 圧縮

### virtual tree が小さくなる部分

次世代候補に現れる異なる親を DFS 順に並べ、その隣接 LCA を追加すると、分岐骨格は高々 `2P-1` node になる。
これは生存親が少ない場合に魅力的に見える。

ただし virtual edge は元の履歴木の複数 Action を表す。現行契約では、その中の Action をすべて順番に
apply/rollback しなければならない。

### virtual edge の中身をどう持つか

| 表現 | 実際に起きること |
|---|---|
| Action ID の配列 | current tour と同じく primitive edge ごとに1 ID必要 |
| 親ポインタ鎖 | parent-slot 方式になる |
| old tour の span 列 | span の断片化と old buffer の寿命管理が必要 |
| 合成 Action | Radix / composed Patricia になる |
| State snapshot | checkpoint backend になる |

`gblock[d]` は generation ごとの配列なので、1本の root-to-leaf path の Action 本体は連続領域にない。
従って virtual edge を単純な `{pointer, length}` だけで表すこともできない。

### current postorder との関係

current の `next_tour` は、今回展開した全active leafに必要なpath suffixを出力する。
今回の子候補選択で不要と分かる枝も構築時点では含まれ、次generationのscheduleから落ちる。
`leaf` は分岐位置を持ち、`tour` は各 primitive edge の Action ID を compact に並べる。
これは virtual tree を node object として構築しない、暗黙の succinct 表現とみなせる。

明示 virtual tree は branch metadata を `O(P)` にできるが、保持すべきprimitive Action sequenceは`O(E_live)`のままである。
current は元々 node ごとの parent/child/sibling を持たないため、削れる branch metadata も少ない。

### 判定

Action compose なしの virtual tree / Patricia は、独立 backend としての優先度が低い。
実装すると parent-slot、span rope、または current tour のどれかへ収束する。

## 案4: Action compose を伴う Patricia / Radix

### 何が初めて下限を変えるか

長さ `k` の virtual edge を1個の Action に合成し、その apply/rollback cost が primitive `k` 回より小さいなら、
状態操作回数 `X` を実際に減らせる。

```text
cost(apply(composed k actions)) << sum cost(apply(each action))
```

この条件が成立すると、分岐点と葉だけを残す Patricia tree は有力である。
全ての単一子 chain を縮約できるなら、葉 `L` に対する live node 数は概ね `2L-1` 以下になる。

### `beam_search_radix.cpp` との関係

この構造は既に `beam_search_radix.cpp` が実装している。

- 明示 parent / child / sibling tree
- 枯れ枝の削除
- 単一子 node の `Action::compose()`
- root の単一子 prefix の確定
- 親ごとの candidate bucket

新しい virtual tree backend を追加する前に、Radix の compose 成功率、live node 数、合成長、Action 所有メモリを
測るべきである。

### compose が見かけだけの場合

合成 Action が内部に primitive vector を持ち、apply のたびに全 primitive を loop するだけなら、
関数呼出し数は減っても本質的な仕事量は変わらない。次の費用で遅くなる可能性もある。

- vector の allocation と連結
- 合成 payload の copy / move
- rollback 情報の保持
- 大きな Action による cache footprint
- compose 失敗時の分岐と古い node の保持

### 判定

真に安い集約 compose を提供できる workload では、current postorder より構造的に速くなり得る本命である。
ただし汎用 `Action` にその性質は仮定できず、既定 backend にはできない。

## 案5: 小さい State の直接 copy

### current の下限を回避する方法

frontier 葉ごとに `State` を持てば、葉間を木上で移動する必要がない。
各候補は親 State を copy し、Action を1回適用するだけでよい。

```text
tree differential : O(X) edge operations + one State
state copy         : O(W * State copy) + selected transition
```

State が小さく trivially copyable なら、連続配列の copy は parent chain や tour より速いことがある。
これは例外ではなく、汎用ライブラリで必要な主要 backend である。

### 現在の実装との関係

`naive_beam_search.cpp` がこの領域を担当する。名前は naive だが、small State workload では合理的な最適化版である。
標準 tour を無条件に置き換える案ではなく、State cost による backend 選択である。

### 判定

構造的に current postorder より速くなり得る。特に `sizeof(State)` だけでなく、copy bandwidth、constructor、
owned allocation、Action 適用費を測る必要がある。

## 案6: 部分 checkpoint

### full State copy との中間

全 frontier に State を持つ代わりに、選んだ分岐点だけ State snapshot を持つ。
ある分岐点から複数 subtree を巡回するとき、先の subtree から辺を rollback して分岐点へ戻る代わりに、
snapshot を restore できる。

分岐点から subtree 末端まで戻る辺数を `h`、snapshot 作成費を `C_save`、復元費を `C_restore`、
同じ checkpoint を使う帰還回数を `r` とすると、rollback 側だけを置換する単純な損益条件は概ね

```text
C_save + r * C_restore < r * h * C_rollback
```

である。次 subtree へ下る apply は通常残る。

### 実装形

- root child ごとの checkpoint
- 深さを一定間隔で区切る checkpoint
- 子数と subtree 高さから選ぶ adaptive checkpoint
- user が提供する cheap `snapshot()` / `restore()`

DFS stack 上だけに snapshot を置けば、最悪 `W` 個でなく `D` 個以下に抑えられる。
ただし nested branch で何個作るかを決める policy が必要である。

### 難点

- non-copyable State では使えない
- copy constructor と restore の意味を新たに規定する必要がある
- State 内 pointer が self-relative だと単純 copy できない
- snapshot の作成自体に現在 path への移動が必要
- State が大きい典型 workload では、差分 rollback の方が圧倒的に安い
- current open traversal が既に一部の帰還を省いているため、比較基準を full root DFS にしてはいけない

### 判定

中程度の State、長い path、非常に安い snapshot という狭い領域では有力である。
汎用既定にはしない。full copy と current postorder の間に測定上の空白がある場合だけ試作する。

## 案7: traversal schedule の差分保持

### current が既に行っている差分生成

current は完全な旧木を明示的に更新してから Euler tour を作るわけではない。
葉を巡回しながら、今回展開した全active leafのpath suffixを `next_tour` へ書く。
今回の選択で不要と分かる枝は1 generation遅れて落ちる。
従って「次世代 schedule を差分で作る」という一般論のかなりの部分は実装済みである。

残っている重複は、次世代にも残る古い Action ID を新しい flat buffer へ再度書くことである。

### persistent span / rope

old tour の範囲を immutable span として参照し、新しい Action だけ小さな chunk に追加すれば、古い ID の copy を
減らせる可能性がある。piece table、rope、chunked postorder などが候補になる。

しかし、世代を重ねると次の問題が出る。

- span が span を参照して断片化する
- old tour buffer を子孫が参照する間は解放できない
- 葉削除で chunk の途中を切る
- traversal 中に chunk 境界の pointer load と branch が増える
- path 復元が複数 span をまたぐ
- 定期 flatten をしないと1辺あたりの間接参照が増える

参照深さを常に1にするなら、結局どこかでstream長`O(T)`のflattenが必要である。
細粒度を極限まで小さくすると parent-slot と同じになる。

### 判定

汎用 backend としては複雑さが高く、直接 rope を最初に試す価値は低い。
次のbidirectional anchor hybridの方が、断片化をepoch内へ制限しやすい。

## 案8: bidirectional anchor stream + bounded parent overlay

### 片方向epochal tourは不成立

currentは`cand`をparent ordinal昇順へsortし、末尾から先頭へ展開する。展開順に次frontier ordinalを振るため、
同じanchor frontierから見た走査方向はoverlay世代ごとに反転する。

```text
overlay depth 1: anchor ordinal 2 -> 1 -> 0
overlay depth 2: anchor ordinal 0 -> 1 -> 2
```

片方向front-coded streamが持つのは、その向きで直前葉からtarget葉へ入るsuffixである。
配列を逆から読むだけでは逆向きtarget suffixにならない。従って旧節の「片方向base tourを`B_o>1`世代再利用する」案は
情報不足であり、そのままでは成立しない。

full Euler enter/leave列なら両方向へ走査できるが、離れたactive anchorへjumpするときに間のdead subtreeまで
Stateで出入りする。simple pathより`rollback()` / `apply_op()`を増やすため、currentと同じ`X`を保つ代替にならない。

### 成立する二層表現

anchor深さのordered frontierについて次を持つ。

```text
AnchorBase {
    forward_suffix_stream
    reverse_suffix_stream
    forward_offsets
    reverse_offsets
    adjacent_lcp
    endpoint_ordinal
}

OverlayGeneration {
    parent_ordinal
    Actionは既存generation block
}
```

forwardはanchor ordinal増加方向、reverseは減少方向のtarget suffixを持つ。
adjacent LCPは方向に依存しないため共有できる。各overlay frontierも親順にordinalを振り、世代ごとのorientation bitで
どちらのbase streamを使うか選ぶ。

ここでは各generationの`Action slot == logical frontier ordinal`を不変条件にする。最終candidateのsmall permutationを
親順へsortし、その順でActionをgeneration blockへgatherする。これを行わない場合は、各overlayに別の`leaf_handle`または
ordinal-to-slot配列が`qW` byte必要になり、以下のwrite / memory式へその分を追加しなければならない。

childからparentへの写像は単調なので、最大`B_o`世代parentをたどった`anchor_of(child)`も単調になる。
current frontierのadjacent LCPを持てば、遷移のLCA depthがanchorより深いか浅いかを先に判定できる。

### traversal

generation冒頭は同深度leaf間遷移ではない。Stateはdepth `d-1`の前frontier endpointにいて、最初に展開するnodeは
そのfrontier上のtarget parentにActionを1個足したdepth `d`のchildである。従って外部cursorのoverlay ordinalを保持し、
まずtarget parentまで前frontierのbase / overlayを使って移動し、その後にtarget child Actionをちょうど1回applyする。
このentryではまだsource childをrollbackしない。これはcurrentの`f=0`に対応する。

LCAがanchor depth以上なら同じanchor内なので、current overlay suffixをrollbackし、最大`B_o`段のparent chainから
target suffixを復元してapplyする。

LCAがanchorより浅ければ次の順になる。

1. current overlay suffixをanchorまでrollbackする
2. target leafから最大`B_o` parentをたどってtarget anchor ordinalを得て、base内LCA depth `h`を決める
3. current `trace`のsource anchor suffixをanchor depthから`h+1`までrollbackする
4. ordinal方向に合うbase streamからtarget anchor suffix `h+1..anchor depth`を`trace`へ復元する
5. target anchor suffixを浅い側からapplyする
6. target overlay suffixを復元してcurrent depthまでapplyする

base offsetとLCPの検索だけはstep 3より前に行えるが、target suffixで`trace`を上書きするのはsource suffixのrollback後でなければ
ならない。順序を逆にすると、leaf境界scanの単純融合と同じくrollback用source IDを失う。

base range queryは間のanchor leafをStateで訪れず、simple pathに必要なActionだけを復元する。
1世代内のanchor queryは単調なので、強いindexを持たない版でもboundary scan区間は重複しない。

### flatten

overlay depthが`B_o`へ達したgeneration boundaryで、今まさに走査し終えたdepth `d`のactive frontierを新anchorへ平坦化する。
このphaseではState cursorがそのfrontierの記録済みendpointにいる。selection後の未適用childを含むdepth `d+1` frontierを
直接anchorにする案では、cursor用temporary leafを残すか、Stateを新frontierへrelocateしてchildをapplyする処理が別途必要になる。
新しいforward / reverse streamが完成するまで、旧2方向base、全overlay parent map、current endpoint traceを保持する。

current traceが一方向endpoint pathを供給し、旧baseが両向きtarget suffixを供給するため、別のstart trace snapshotなしで
全新anchor pathを再構築できる。旧baseを先に破棄するとdeferred案と同じendpoint固有path欠落が起きる。

新anchor ordinalはcurrentのlogical frontier順を保ち、endpoint ordinalも同じ向きで変換して明示保持する。
flatten中はmetadataだけを操作し、State methodを呼ばなければ候補列挙順を変えない。

### write cost

次を置く。

| 記号 | 意味 |
|---|---|
| `q` | Action slotまたはparent slot 1個のbyte数、通常4 |
| `l` | adjacent LCP 1個のbyte数、通常4 |
| `W` | epoch中のおおよそのfrontier幅 |
| `M^+`, `M^-` | forward、reverseのanchor stream有効slot数 |
| `M` | `(M^+ + M^-)/2`。定常近似の1方向平均stream長 |
| `B_o` | flatten間隔、overlay最大深さ |

毎世代は新parent slotとadjacent LCPを書き、`B_o`世代ごとにforward / reverse baseを1回ずつ書く。
epoch内の幅とstream長を平均化した形は

```text
qW + lW + (q(M^+ + M^-) + index_bytes) / B_o
```

である。ここではepoch間で`W`、`M^+`、`M^-`が大きく変わらない定常近似を使う。
offset indexを除いた簡略形は

```text
qW + lW + (2qM) / B_o
```

となる。片方向を仮定した`qW + lW + qM/B_o`ではなく、snakeの方向反転によりbase係数が2になる。

current slot-only eagerの実際のreachable tour長を`T_eager`とする。共通の`lW`を相殺した正確なwrite必要条件は

```text
qW + q(M^+ + M^-)/B_o + index_bytes/B_o < qT_eager
```

である。同じ定常frontierに対する1方向current stream長を`T_eager ~= M`と近似し、indexを除くと

```text
qW + 2qM/B_o < qM
```

になる。`M/W`が大きく、`B_o`が2より十分大きい領域でなければ償却できない。

これはwrite byteだけの下限寄り比較である。flattenでは旧baseを読み、最大`B_o`段のparent overlayをたどって
新streamを両方向へ作るため、概ね`O(M_new^+ + M_new^- + B_oW)`のmetadata read / dependent lookup burstも生じる。
このCPU costも`B_o`世代へ償却し、write条件を満たすだけで採用を決めてはならない。

### memoryとflatten peak

offset、LCP、Action payloadを除くsteady topology memoryは概ね

```text
q(M^+ + M^-) + qB_oW ~= 2qM + qB_oW
```

である。前半はforward / reverse base、後半は最大`B_o`世代のparent overlayになる。
Action本体は既存generation blockの`G*S_A`が別に残る。

flatten中は旧2方向baseと新2方向baseを同時に保持するため、base部分は

```text
q * (M_old^+ + M_old^- + M_new^+ + M_new^-)
```

になる。各epochの1方向平均を`M_old`、`M_new`と書けば`2q(M_old+M_new)`であり、
`M_old ~= M_new ~= M`の定常時だけ約`4qM`になる。
さらに旧overlay、新overlay構築buffer、offset、LCPが重なる。peak RSSをsteady式だけで評価してはならない。

```text
flatten peak >= q(M_old^+ + M_old^- + M_new^+ + M_new^-)
              + qB_oW + offset / LCP / build scratch
```

`B_o`を大きくするとflatten writeは薄まるが、dependent parent chain、`qB_oW` memory、1回のflatten burstが増える。
幅や木形状がepoch中に急変すると、旧`M/W`から選んだ`B_o`も外れる。

### 反例

- 片方向baseだけでは2世代目のorientation反転を復元できない
- generation冒頭を同深度遷移として扱うと、未適用source childをrollbackするoff-by-oneになる
- `B_o=1`ではbase writeだけでcurrent片方向streamの約2倍になり、parent / LCP writeがさらに加わる
- `M`が`W`と同程度の浅い木では、base copyを減らす前に`qW+lW` overlay writeを追加する
- ほぼ全遷移が別anchorなら、overlay parent decodeとbase suffix readを両方払う
- anchor gapが大きいとState操作は増えなくてもboundary metadataを`O(W)`読む
- full Eulerで双方向性を代用すると、skipped dead subtreeを余分にwalkして`X`を増やす
- flatten中のold/new base同時保持がmemory limitを超える
- `G >> E_live`でAction payloadが支配すると、topology削減が全体memoryへ効かない

### 判定

ここで成立を確認したfront-coded anchor案は、片方向tourではなく
bidirectional base front coding + adjacent LCP + bounded parent overlayである。
pure parent-slotの依存chainを最大`B_o`へ制限し、currentの古いstream書換えを`B_o`世代へ償却できる点には研究価値がある。

ただしsteady memoryに`q(M^++M^-)=2qM`、平均writeに`q(M^++M^-)/B_o=2qM/B_o`、flatten時に
`q(M_old^++M_old^-+M_new^++M_new^-)=2q(M_old+M_new)`のbase peakを必ず数える。
slot-only currentとdirect parentのprofileで両者の中間が確認でき、上の不等式を複数epoch継続して満たす場合だけ試作する。

## traversal 順序を変える案

### 状態遷移だけを見る場合

各 subtree の葉を連続して訪れる DFS 順なら、兄弟の並べ方を変えても多くの辺は2回ずつ通る。
始点と終点を直径端へ置くことで最大 `diameter(H)` 辺の帰還を省ける。currentのopen DFSは固定された
`s,t`に対し`2E_walk-dist(s,t)`を厳密達成するが、`s,t`自体を自由に選んだ直径端とは限らない。

従って、score の大域順に葉を並べると高速になるとは限らない。異なる subtree を往復し、同じ辺を何度も通るため、
最悪 `O(WD)` の余分な State 操作を生む。

### search の早期 cutoff を見る場合

同じ親の兄弟を良い score 順に展開すると、次世代候補の threshold が早く厳しくなり、`try_op()` の内部計算を
省ける可能性がある。兄弟内だけなら State 移動距離は変わらない。

ただし次を変え得る。

- 同点候補の採否
- 同じ hash のどの path を残すか
- RNG 消費順
- threshold を利用した問題側の列挙順

これは履歴構造の純粋な高速化ではなく、順序 policy として分ける。

## 比較表

| 案 | `X` を減らすか | schedule 構築 | 主な memory | locality | 現実性 |
|---|---|---|---|---|---|
| current postorder | 固定`s,t`で厳密下限 | `O(T+Z)` sequential | 2 tour + `G*S_A` | 高い | 実装済みの基準 |
| 先頭segment省略 | 同じ | `Z`を削除し`O(T)` | unreachable segmentを削減 | 高い | optimizedで実装済み |
| prefix境界精密化 | 同じ | 追加なし | Action slabを早く解放 | 高い | optimizedで一部実装 |
| 32 bit slot tour | 同じ | `O(T)` sequential | tour ID半減 | 高い | optimizedで実装済み |
| sorted ordinal + compact cand | 同じ | indirect sort + gather | candを4--8 byte中心へ縮小 | 高い | 比較価値あり |
| tiny inline Action | 同じ | `O(T*S_A)` copy | gblockなし + direct tour | 非常に高い | `S_A<=4`かつimmutable |
| flat absolute Action arena | 同じ | free interval割当 | peak + fragmentation | 1 base lookup | trivial / fixed-cap向け |
| record-high fused scan | 同じ | `O(K+J+T)` | `O(D)` scratch | 高い | `K-J`が大きい場合 |
| deferred parent tour | 同じ | snapshot + `O(T_entry+T_keep)` post-pass | `O(U)` snapshot + selected tour | 高い | 条件が狭い |
| parent oracle | 同じ | `12W byte` | `4G+16W+4K_trace+G_cap*S_A` | dependent load | 実装済み |
| parent compact | 同じ | `8W byte` | `4G+8W+4K_trace+G_cap*S_A` | dependent load | 実装済み |
| unary parent map | 同じ | `O(Wprev+Wcur)` bit build | dense時raw約`G/4 + G*S_A` byte | select依存 | 急縮小時はdirect/runへ切替 |
| B世代path-block | 同じ | parent + transpose | `qG`--`qG(1+1/B_p)` + temp | block内連続 | latency支配時の第二案 |
| refcount parent arena | 同じ | node add/release | live `E_live`、allocated `N_cap` | arena load | owned payload向け |
| live ordered tree | 同じ | link / unlink | `(E_live+1)*(S_A+20..24)` | link load | parent-only後のCPU比較 |
| virtual tree、非 compose | 同じ | skeleton `O(P)`、Action列 `O(E_live)` | span 次第 | 中程度 | current と重複 |
| composed Patricia / Radix | 減らせる | tree surgery | composed node | pointer load | 契約適合時の本命 |
| full State copy | tree walk 不要 | contiguous frontier | `W*sizeof(State)` | 非常に高い | small State の本命 |
| partial checkpoint | rollback を一部削減 | checkpoint policy | snapshot stack | State 次第 | 条件が狭い |
| persistent rope | 同じ | edit / splice | chunk + old buffer | block 内は高い | 複雑、後回し |
| bidirectional anchor | 同じ | `qW+lW+2qM/B_o` | steady `2qM+qB_oW` | 調整可能 | 条件付き |

## 何が「構造的により速い」と言えるか

### 現行契約を一切変えない場合

確実に言えるのは次だけである。

- current postorder は通常loopの固定された開始cursor、target順、終了cursorに対しState辺遷移回数を厳密最小にする
- current postorderは参照記事のparent / child / sibling付き明示node木よりmetadataが小さく、完全Euler列よりtokenが少ない
- currentの未使用な先頭segmentは探索結果を変えずに除去できる
- entry jumpをprefix LCPへ含めないことと、1葉の`h=d`処理でAction blockを早く解放できる
- survivor parentと終了endpointだけの両端LCAを使えば、追加scanと引換えにさらに解放を進められる
- 32 bit slot-only は同じ構造のまま ID bytes を減らせる
- tiny trivialかつimmutableなActionではdirect postorderによりgeneration gblock自体を消せる
- Action slotをsorted ordinalへ合わせれば、traversal中のcandidate metadataを大幅に縮められる
- record-high scratchによりleaf境界の2 full scanを1 full scan + `J`件replayへ変えられる
- deferred selected-parent tourはstart trace snapshotが必須で、`T_keep << T_all`かつ`U+T_entry`が短い場合だけ有望
- leaf + LCP + parent-slot は `O(T)` のschedule writeを `O(W)` のancestry writeへ変えられる
- denseなmonotone parent mapはほぼ情報量下限のunary bitvectorへ圧縮できる
- parent load / select の依存列があるため、tour-free方式が常に速いとは言えない
- path-blockはparent依存段数を短くできるが、slot数の圧縮ではなくboundary transposeを追加する
- live ordered treeはlogical node数を`E_live+1`にできるが、parent-only arenaよりnode metadataが大きい

この条件で current を漸近的に上回る表現は見つからない。parent-slot の schedule 構築だけは漸近的に軽くなるが、
必要な State walk が `Theta(E_walk)` のままなので search 全体は同じ order である。

### 契約を増やせる場合

次は current より明確に少ない仕事へ変えられる。

- small State を frontier ごと copy し、木の walk をなくす
- true aggregate Action で unary path を1回に compose する
- cheap snapshot で長い rollback path を restore に置き換える
- `apply_path()` / `rollback_path()` を追加し、問題側が path を batch 更新する
- State を永続化または copy-on-write 化し、複数 frontier を直接持つ

ただし全て workload 固有の cost model または新しい API 契約を必要とする。

## 実装状況と残りの優先順位

### P0: current tour の直接改善と測定

1、2、3のAction move、7はoptimized版へ実装し、差分試験を通した。4から6の詳細counterと8は残る。

1. 最初の `next_tour.insert()` を省き、`leaf[0]=0` で全edge caseの探索結果を照合する
2. 最初のentry jumpをprefix最大値から外し、`C=1`では`confirm_and_free(d+1)`とする
3. 固定widthのleaf capacity事前確保とActionのmove construction化を単独benchmarkする
4. `tour` / `next_tour`へ書いたID bytes、未使用先頭segment長、reallocation回数を数える
5. `copy_tour_path()`の`K`、copy ID数`R`、record-high数`J`を数える
6. `X`、`E_walk`、`dist(s,t)`、`T_all`、`T_keep`、`T_entry`、`U`、`P/L`、`G/E_live`、LCA距離を記録し、
   `X == 2E_walk-dist(s,t)`を検証する
7. 64 bit currentと32 bit slot-onlyを探索結果完全一致で比較する
8. `sizeof(Action)<=4`のtrivialかつStateから変更されない型でtiny inline backendを比較する

### P1: current layoutの追加比較

- Action slotをsorted ordinalへ合わせ、compact cand + permutationのcache損益を測る
- large Action / long depthでsurvivor + endpoint prefix解放を測る
- `K-J`がscratch / branch overheadより大きい形でrecord-high fused scanを比較する
- 64 bit pointer tokenはslot-onlyが帯域で負けた場合だけ比較する
- outer `gblock[depth]` lookupが支配するtiny Actionだけflat absolute arenaを比較する

### P1: leaf + LCP + parent-slot backend

次の条件を守るoracle版とcand導出compact版を実装し、差分試験を通した。

- 同じ candidate selector を使う
- 同じ親順と兄弟順を使う
- generation ごとの survivor digest を比較する
- State checksum を各葉で比較する
- parent metadata は Action と分離した SoA にする
- LCA は両側 parent walk で探さず、隣接 LCP depth を単調走査で引き継ぐ
- `W=1`、同一親兄弟、endpoint の親が全滅する case を個別に検証する
- slot-only current と比較し、古い64 bit current だけを基準にしない

`G/E_live` または dead Action payload が大きい場合だけ、同じ leaf + LCP traversal を refcount arena 上でも比較する。
そのparent-only arenaでparent-chain backfillとLCP処理が実測上支配した場合に限り、同じarena寿命の
ordered child / sibling + open DFS版を第二のCPU trade-offとして比較する。endpoint zombieを展開しないことと、
cursorを含むprefix re-rootを個別に検証する。

direct parent版の正しさをoracleにしてから、同じbackendのparent mapだけをunaryへ差し替える。
select sample間のlong zero run、急激なwidth縮小、BMI2あり / なしを分けて測る。

### P2: B世代path-block

direct parentで長いtarget suffixの依存load latencyが支配し、windowed decodeでも隠せない場合だけ試す。
`B_p=2,4,8,16`のboundary transpose read / write、completed block byte、temporary parent peak、使用前にprefix解放した
block比率を測る。`B_p=1`はdirect parentをaliasする負controlとする。

### P2: deferred selected-parent tour

`T_keep/T_all`が十分小さく`U + T_entry`が短い合成形状で、start trace snapshot込みのcostを測る。
snapshotなし版は実装候補にしない。endpoint parent全滅、start endpointだけが生存、`P=1`、最終generationを個別に検証する。

### P2: bidirectional anchor overlay

parent-slot が tour write を減らす一方で dependent miss に負け、slot-only tour が write bandwidth に負ける、という
両方のprofileが確認できた場合にだけ試す。片方向baseは候補にせず、forward / reverse 2本を持つ。
共通のLCP writeを相殺した`qW+2qM/B_o < qM`を満たすepochで`B_o=2,4,8,16`を比較し、
steady `2qM+qB_oW`と`2q(M_old+M_new)`以上になるflatten peakも測る。
generation entryでendpoint parentが生存、全滅、別anchorの3形状を作り、`f=0`相当のrollback / apply回数も照合する。

### P2: compose / State policy の整理

- Radix の compose が真に primitive work を減らすか測る
- `naive_beam_search.cpp` を small State backend として公平に最適化する
- cheap snapshot を持つ合成 State で partial checkpoint の損益分岐を測る

### 採用しない案

- current postorder を明示木 + 一般 LCA に戻す
- binary lifting / RMQ だけを追加する
- Action sequence の保持方法を決めずに virtual tree だけ作る
- profile なしで rope を実装する
- State / Action 契約を隠れて強める

## 必要な benchmark

特定の問題だけで結論を出さない。少なくとも次を直交させる。

### State cost

- no-op に近い State
- 数個の整数だけを更新する State
- 小配列を更新する State
- 大配列へ散在アクセスする State
- small trivially-copyable State
- non-copyable State

### 木形状

- beam の全葉が浅い位置で分岐し、`T/W` と `E_walk/W` が大きい
- 長い共通 prefix を持ち、`T/W` が小さい
- external endpoint pathが長く、`E_live/T`が大きい
- 各親から1子だけ残る
- 少数親から多くの兄弟が残る
- 世代ごとに親が大きく入れ替わる
- 古い dead slot が多く `G/E_live` が大きい

### 型サイズ

- trivial Action 1 / 2 / 4 / 8 byte
- non-trivial Action 8 / 32 / 128 byte
- Score 32 / 64 bit
- beam width 64 から大容量まで
- depth が LLC に収まる場合と収まらない場合

### CPU counter

- cycle / expanded leaf
- cycle / primitive State edge operation
- bytes written to tour
- L1 / LLC miss
- branch miss
- allocation count
- peak RSS
- `K/R`、`J/K`、`K-J`
- `CandIdx` / permutationのsort byte
- parent-chain load latency
- `N_live=E_live+1`、`N_cap/N_live`、endpoint zombie長、refcount release cascade長
- ordered treeのparent / child / sibling loadとunlink write
- path-blockのtranspose read / write、block hit率、使用前解放率
- `M^+`、`M^-`、`B_o`、flatten read / write、old/new base peak

## 最終評価

current `beam_search.cpp` は、質問にある高速化後の帰りがけ順方式である。この認識は正しい。

その次の構造として、単一 mutable State と edge-wise apply/rollback を維持したまま現実的なのは、

1. 未使用先頭segmentと精密prefix解放を加えた32 bit slot-only postorder
2. tiny trivialかつimmutableなAction向けのgblockなしdirect-Action postorder
3. leaf handle + adjacent LCP + directまたはunary parent map
4. `G/E_live`が大きい場合のrefcount parent-only arenaと、その後のnatural open DFS比較用ordered tree
5. parent鎖のlatencyが支配する場合のB世代path-block
6. tourとparent mapの中間となるbidirectional anchor stream + bounded parent overlay
7. start trace snapshot込みでsurvivorが極端に集中する場合のdeferred selected-parent postorder

である。leaf二重scanのrecord-high融合は独立したmicro-architecture候補で、`K-J`が追加overheadを上回る場合に有望になる。

先頭segment省略、prefix精密化、slot-onlyはoptimized版へ実装した。sorted ordinalは未実装のlayout候補である。
deferredはcurrentの構築時点を変えるpolicyだが、旧tourだけから後付け構築する案は不成立で、start trace snapshotが必要になる。
leaf + LCP + parent mapは明確に異なる構造、bidirectional anchor overlayは両表現をつなぐhybridである。
parent mapとanchor overlayはcurrentより速くなる可能性があるが、連続memory accessと2本のbase costがあるため無条件ではない。
unary parent mapはdense / fixed-width時のraw memoryがほぼ情報量下限だが、各edgeのselect costを払い、急縮小時はdirectより大きい。
refcount parent arena は、dead Action の即時解放に価値がある場合の memory-oriented 変種である。
ordered live treeも同じlogical live数を達成するが、parent-onlyよりnode metadataが大きく、State辺遷移下限も同じなので、
`G >> E_live`対策の第一実装にはしない。path-blockはparent依存を短くするが、memory圧縮ではなく、
transposeとshared-prefix重複を払う。

virtual tree / PatriciaはAction composeがなければprimitive Action供給をtour、parent chain、spanのいずれかで持つ必要があり、
`X`は減らない。constant factorとlocalityは供給形式によって変わるため、currentと同一costだという意味ではない。
compose が真に安い場合は `beam_search_radix.cpp` の方向が current より強くなり得る。
State が小さい場合は `naive_beam_search.cpp` の直接 copy が強くなり得る。

従って、汎用ライブラリとしての正しい結論は「current が最速なので他案はない」ではなく、

> edge-wise単一Stateの既定値としてcurrent postorderは非常に強く、まずslot-onlyとprefix/layout改善を行うべきである
> tiny immutable Actionならdirect inline、metadata write支配ならparent map / bidirectional anchor、small Stateならcopy、
> 集約可能ならRadixが勝つ領域がある。deferredはsnapshot costまで含めて初めて比較対象になる

である。
