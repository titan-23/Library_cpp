# 通常版 beam search の履歴構造コストモデル

## 対象と結論

通常版 `beam_search.cpp` と同じ探索順、候補集合を持つ次の4方式を比較する

1. 現行の64 bit `ActionId` 帰りがけ順
2. 先頭prefixを除いた32 bit slot-only帰りがけ順
3. `direct parent + frontier_slot + adjacent LCP` の正しさ優先版
4. frontierを `cand` から導出するdirect parent compact版

主要な結論は次になる

- 4方式の `State` 操作数は同じで、通常版の順序では木上walkの下限を達成する
- slot-only帰りがけ順の定常topology writeは `4M+4W` byteになる
- 独立 `frontier_slot` を持つdirect parent oracleの定常topology writeは `12W` byteになる
- 固定幅でwriteだけを比較すると、oracle版direct parentが少ない条件は `M>2W` になる
- `frontier_slot` を既存 `cand` から導出できた後だけ境界が `M>W` まで下がる
- direct parentのparent load数は `L=e+M-(W-1)` で、各suffix内ではaddress依存になる
- `P=1` では `M=W-1`, `L=e` となり、oracle版direct parentにwrite上の利点はない
- `P=W` でも隣接親のLCAが祖父なら `M=2(W-1)` なので、oracle版のwriteはほぼ同量になる
- direct parentが明確にwriteを減らすのは、異なる親間のsuffixが平均してさらに深い場合になる
- 定常resident topologyはslot-onlyが `8M+8W+4K`、oracle版parentが `4G+16W+4K` byteになる
- compact版parentの定常resident topologyは `4G+8W+4K` byteになる
- parent oracleのresident量が小さい条件は `G+2W<2M` になる
- parent compactのresident量が小さい条件は `G<2M` になる
- `G>>E_live` ではdirect parentが死んだ世代slotごとに4 byteを残し、明確に退化する

ここでのbyte数は論理loadとstoreであり、cache line転送量や実行cycleではない

## 比較の境界

次は全方式の共通項として主表から外す

- 候補selectorのscoreとhash処理
- `try_op()` が読むAction以外のデータ
- 新しいAction payloadの構築と `gblock` への保存
- `result_prefix` と最終結果Action列
- `CandIdx` のparent、score、history field
- `vector` header、allocator metadata、capacityの丸め

ただしAction payloadの保持量は `G/E_live` の判断に必要なので、memory節で共通項として戻す

`CandIdx` のhandleは現行64 bitからslot-only 32 bitへ縮むが、構造体全体の削減はABI依存になる

## 記号

深さ `d` のcurrent frontierを、実際の展開順に `v[0..W)` とする

generation開始時のStateは深さ `d-1` のendpoint `s` にいる

| 記号 | 意味 |
|---|---|
| `W` | 今回展開するcurrent frontier幅 |
| `N` | 今回の選択後に残るnext frontier幅 |
| `P` | current frontierに現れる異なる親group数 |
| `P_n` | next frontierに現れる異なる親group数 |
| `h_0` | `s` と最初のtargetのLCA絶対深さ |
| `e` | 最初のtarget親までの距離 `(d-1)-h_0` |
| `h_i` | `v[i-1]` と `v[i]` のLCA絶対深さ |
| `r_i` | 内部遷移のtarget suffix長 `d-h_i` |
| `M` | 先頭prefix除去後の次tour token数 `sum r_i` |
| `B` | 現行だけが書く未参照先頭prefix長 `e+1` |
| `F` | 全target suffixをtraceへ書くslot数 `B+M` |
| `L` | target祖先を復元するslot数 |
| `R` | current親ordinalの下降scanが跨ぐ旧leaf境界数 |
| `R_n` | next LCP構築が跨ぐcurrent `adj_lcp` 境界数 |
| `D` | 確定prefixより後ろのlive endpoint path長 |
| `K` | `trace` の確保slot数、現行実装では概ね `max_turn+1` |
| `E_live` | current frontierとendpointが必要とする未確定Action辺数 |
| `G` | 未解放generation blockにある論理Action slot数 |
| `G_cap` | slabを含むAction block capacityの合計 |
| `S_A` | `sizeof(Action)` |
| `q_0` | 現行handle幅、8 byte |
| `q` | slot幅、4 byte |
| `b` | leaf境界幅、4 byte |
| `l` | LCP深さ幅、4 byte |

`P` だけでは `R` は決まらない

使用親の間に子を持たない旧親があれば、`P=1` でも `R` は大きくなり得る

固定幅の定常比較では `N=W` とし、oldとnextのstream長も同じ `M` と仮定する

## 1世代の厳密な構造量

### tour tokenとtrace復元

内部遷移について次が成り立つ

```text
M = sum_{i=1}^{W-1} r_i
B = e + 1
F = B + M
L = e + sum_{i=1}^{W-1}(r_i-1)
  = e + M - (W-1)
F = W + L
```

`F=W+L` はコード上の次の2種類のtrace storeに一致する

- 各target自身のslot storeが `W` 回
- `copy_tour_path()` またはparent decodeによる祖先slot storeが `L` 回

現行の最初の `next_tour.insert()` は `B` tokenを書き、その全てが次generationで未参照になる

slot-only版はこの `B` tokenだけを省き、内部遷移用の `M` tokenは維持する

### State操作数

rollbackとapplyは次になる

```text
rollback = e + M
apply    = e + 1 + M
X        = 2e + 1 + 2M
```

direct parentも既知LCPまで同じslot列を復元するため、この回数とAction順は変わらない

generation開始endpointと全targetを含む最小部分木を `H` とし、最後のtargetを `t` とする

通常版のDFS-compatibleな単調順では次の等式になる

```text
X = 2|E(H)| - dist(s,t)
```

従って同じ順序、単一mutable State、辺単位APIのまま `X` を減らす余地はない

`enumerate_actions()` は `W` 回、`try_op()` は列挙候補数だけ呼ばれ、4方式で同一になる

### parent load数

direct parentはsuffix長 `r` に対して最後のLCP nodeのparentを読まないため、loadは `r-1` 回になる

1世代の合計は次になる

```text
L_parent = e + sum_{i=1}^{W-1}(r_i-1)
         = L
```

総load数はpostorderがold tourからcopyするslot数 `L` と同じになる

差はpostorderのsourceが連続streamで、direct parentの次addressが直前のload結果に依存する点にある

最大のdepth方向dependent chainは次になる

```text
C_dep = max(e, max_i(r_i-1))
```

同じ親の兄弟では `r_i=1` なのでparent loadもdependent chainも0になる

### 親group数との関係

各親group内の遷移数の合計は `W-P` で、その全てが `r_i=1` になる

異なる親group間のsuffix長を `r_g` とすると次になる

```text
M = W - P + sum_{g=1}^{P-1} r_g
L = e + sum_{g=1}^{P-1}(r_g-1)
```

異なる深さ `d-1` の親同士ではLCAが高くても `d-2` なので `r_g>=2` になる

従って次の下限がある

```text
M >= W + P - 2
L >= e + P - 1
```

## 論理readとwrite

以下は再確保、vector size更新、iterator演算を除いた配列要素のbyte数になる

`trace` を介するState操作とtarget path復元は、handle幅を除けば全方式で共通になる

### 現行64 bit帰りがけ順

```text
read
    State操作用trace                 8X
    old tourからの祖先復元           8L
    traceからnext_tourへのemit        8F
    current leaf境界scan              別記

write
    target pathのtrace復元            8F
    先頭emit用の重複trace seed         8
    next_tour                         8F
    next_leaf                         4W
```

topologyの永続writeだけなら次になる

```text
8(B+M) + 4W
```

現行コードは最初のemit前に `trace[d]` をseedし、最初のloop内でも同じtarget slotを再度書く

先頭prefixを省く版ではこの1回のseed storeも不要になる

### 32 bit slot-only帰りがけ順

```text
read
    State操作用trace                 4X
    old tourからの祖先復元           4L
    traceからnext_tourへのemit        4M
    current leaf境界scan              別記

write
    target pathのtrace復元            4F
    next_tour                         4M
    next_leaf                         4W
```

topologyの永続writeだけなら次になる

```text
4M + 4W
```

先頭prefixを消しても最初のtargetへ入るtrace復元 `B` とState操作は消えない

### direct parent oracle

正しさ優先版は `parent` と `frontier_slot` を別配列で持つ

`entry_lcp` 1個と `adj_lcp` `W-1` 個を合わせ、current遷移ではLCPを `W` 個読む

next frontierについてもentry 1個とadjacent `N-1` 個を合わせ、LCPを `N` 個書く

```text
read
    State操作用trace                 4X
    target ancestryのparent          4L、depth方向に依存
    current entryとadjacent LCP       4W
    next LCP構築用のold adjacent LCP  4R_n

write
    target pathのtrace復元            4F
    next parent                       4N
    next frontier_slot                4N
    next entryとadjacent LCP           4N
```

topologyの永続writeだけなら次になる

```text
12N
```

current target handleは各方式で `W` 個読む

postorderは `CandIdx` のhandle、oracle版parentは `frontier_slot` から読むため、比較表では共通扱いにする

ただしoracle版は同じslotを `CandIdx` と `frontier_slot` の両方へ書いて保持する

### leaf境界scanの上限

現行コードでは親ordinalが単調に下がり、loop全体で跨ぐ旧leaf境界数は `R` になる

`lca_dist` loopと `copy_tour_path()` をソース式のまま数えると次になる

```text
leaf要素参照 = W + 4R
```

同じloop内の `leaf[k]` を1回だけloadする自明なCSE後は `W+3R` load以下になる

実際に触れる異なるleaf要素は高々 `R+1` 個で、cache line転送はこの論理参照数より小さくなり得る

direct parentはcurrent LCPを `W` 個読み、next LCP構築で `R_n` 個を一方向に読む

`P-1<=R` だが、dead parent gapがあるため `R` を `P-1` で置き換えてはいけない

### read byteの集約式

自明なleaf CSE後の論理readを集約すると次になる

```text
current 64 bit = 8X + 8L + 8F + 4(W+3R)
slot-only     = 4X + 4L + 4M + 4(W+3R)
parent oracle = 4X + 4L + 4W + 4R_n
```

最後のparent式の `4W` はcurrent entryとadjacent LCPのreadになる

各式はcurrent target handleの `W` readを除く

postorderは `CandIdx`、parent oracleは `frontier_slot` からhandleを読むため、別counterとして比較する

同じcache lineの反復readも論理byteへ数えるため、hardwareのmemory trafficとは一致しない

## writeだけの損益分岐

### 固定幅のoracle版

`q=b=l=4`, `N=W` とする

```text
slot-only postorder : 4M + 4W
direct parent oracle: 12W
```

oracle版direct parentのwriteが少ない条件は次になる

```text
M > 2W
```

これはwrite subsystemだけの境界で、実時間の十分条件でも必要条件でもない

### frontierをcandから導出する後続版

正当性確認後に次を直接使えるなら、独立 `frontier_slot` のwriteを消せる

```text
frontier_slot[j] = cand[W-1-j].action_slot
```

この版のtopology writeは `8W` となり、境界は次になる

```text
M > W
```

`M>W` を最初のoracle版へ適用するのは1配列分の過小評価になる

### 動的幅

current postorderのscheduleはcurrent frontierを全て展開中に作るため、writeは `M` と `W` で決まる

direct parentのnext表現はselector確定後に作るため、writeは `N` で決まる

oracle版の一般形は次になる

```text
4M + 4W > 12N
```

幅が急減して `N<<W` ならdirect側が有利になり、幅が急増するなら逆になる

terminal generationではpostorderがscheduleを先に書いた後で終了を検出する

parent mapをselector確定後にだけ作る実装なら、finishedとno-candidateでnext topology writeを省ける

max-turnではbest childのpath復元用parentは必要だが、未使用のnext frontierとLCPは省ける

## cycleの損益分岐

byte量だけでは連続loadとdependent loadを比較できないため、係数を実測からfitする

| 係数 | 意味 |
|---|---|
| `c_sr` | 連続stream readのcycle per byte |
| `c_sw` | 連続stream writeのcycle per byte |
| `ell_p` | parent 1 loadの平均露出cycle |
| `C_leaf(W,R)` | leaf境界scanとbranchのcycle |
| `C_lcp(W,R_n)` | LCP read、minimum、group branchのcycle |
| `C_ctl` | backend固有loopとvector固定費 |

共通のState操作、Action参照、trace read/writeを消去した概算は次になる

```text
C_slot = c_sr*4(L+M) + c_sw*(4M+4W) + C_leaf(W,R) + C_ctl_slot

C_parent = ell_p*L + c_sw*12N + C_lcp(W,R_n) + C_ctl_parent
```

direct parentが勝つ概算条件は `C_parent<C_slot` になる

`ell_p` には4 byte転送だけでなく、次addressを待つ依存latencyを含める

parent blockがL1にある場合とLLC外にある場合で `ell_p` は大きく変わる

同じbyte数でも次の傾向になる

- `L` が大きくparent missが多い場合はslot-onlyが有利
- `M` が大きくparentがcache内ならdirect parentが有利
- `R` が大きいdead gapではdirectの保存済みLCPがleaf再scanを減らす
- State操作が重い場合は共通項 `C_state*X` が支配し、backend差が隠れる

## resident memory

### capacityを使う一般式

次のcapacity合計を定義する

```text
T_cap = capacity(tour) + capacity(next_tour)
J_cap = capacity(leaf) + capacity(next_leaf)
F_cap = capacity(frontier) + capacity(next_frontier)
A_cap = capacity(adj_lcp) + capacity(next_adj_lcp) + entry scalar数
P_cap = 未解放parent block capacityの合計
```

Action payloadと候補descriptorを除くresident topologyは次になる

```text
current 64 bit = 8T_cap + 4J_cap + 8K
slot-only     = 4T_cap + 4J_cap + 4K
parent oracle = 4P_cap + 4F_cap + 4A_cap + 4K
```

`vector::clear()` はcapacityを返さないため、論理sizeを減らしても高水位RSSが直ちに下がるとは限らない

### 固定幅の定常近似

oldとnextが同じ長さで、capacityがsizeへ近い場合を考える

```text
current 64 bit = 16(B+M) + 8W + 8K
slot-only     = 8M + 8W + 4K
parent oracle = 4G + 16W + 4K
parent derived= 4G + 8W + 4K
```

全方式へ共通するAction payloadは概ね次になる

```text
G_cap * S_A
```

実際には `vector<vector<...>>` のheader、alignment、slab poolのcapacityも加わる

### parent oracle対slot-only

oracle版parentのresident topologyが小さい条件は次になる

```text
4G + 16W + 4K < 8M + 8W + 4K
G + 2W < 2M
```

先頭prefix除去後はcurrent active treeについて `E_live=M+D` が成り立つ

generation blockは少なくとも全live edgeを含むため `G>=E_live` になる

従ってoracle版parentが小さくなるための必要条件は次になる

```text
M > D + 2W
```

独立frontierを消したderived版なら条件は次になる

```text
G < 2M
```

この差からも、独立frontier版は正しさoracleとして測り、最終memory版とは分ける必要がある

### `G>>E_live`

slot-only tourのcurrent論理handle数は `M+D=E_live` に比例する

direct parentは子孫を失ったslotにもparentを残すため、`4G` byteに比例する

`G=kE_live` かつ `W,D` が小さいとき、parent metadata対slot streamの比は概ね `k/2` まで悪化する

Action payload `G_cap*S_A` も両方式に残るため、大きいActionではtopology差よりdead payloadが支配する

この退化を解くにはdirect parentではなく、refcount arenaやpath chunkなどlive量へ回収する構造が必要になる

## 退化例

### `W=1`

```text
P = 1
M = 0
L = e
rollback = e
apply = e+1
X = 2e+1
```

`e=0` ならState操作はchildのapply 1回だけになる

slot-onlyのtourは空で、topology writeは `next_leaf` の4 byteだけになる

oracle版parentはparent、frontier、entry LCPの12 byteを書くため、常にwriteが多い

derived版でもparentとentry LCPの8 byteが必要になり、slot-onlyより4 byte多い

### `P=1`

全targetが同じ親の兄弟なら次になる

```text
M = W-1
L = e
X = 2e+1+2(W-1)
```

内部遷移のparent loadは0で、最初のentryだけが `e` loadを使う

固定幅のtopology writeは次になる

```text
slot-only      = 8W-4
parent oracle  = 12W
parent derived = 8W
```

oracle版は `4W+4` byte多く、derived版も4 byte多い

parentのwrite削減を目的に `P=1` workloadへoracle版を選ぶ根拠はない

### `P=W`

各targetが異なる親なら全内部遷移がcross-parentになる

cross suffix長の平均を `r_bar` とすると次になる

```text
M = (W-1)r_bar
L = e + (W-1)(r_bar-1)
```

最も浅い差である `r_bar=2` では次になる

```text
slot-only write = 12W-8
parent oracle   = 12W
```

oracle版は8 byte多く、write量は事実上同程度になる

`r_bar=3` ならslot-onlyは `16W-12` byteとなり、十分大きいWでoracle版がwriteを減らす

ただしparent loadは `e+(W-1)(r_bar-1)` 回となり、深い分岐ほど依存latencyも同時に増える

### `G>>E_live`

同じ `W`, `M`, `D` ならslot-only topologyはGに依存しない

oracle版parentだけが `4G` byteで増え、`G/E_live` に比例して不利になる

Action payloadも `G_cap*S_A` のままなので、direct parentをmemory対策として選んではいけない

## 数値例

全例で `q=b=l=4`, `e=0`, `D=K=32`, 固定幅を仮定する

`G=10E` 以外は死んだslotがない理想値 `G=E_live=M+D` とする

### 1世代のtopology write

| case | `M` | `L` | `X` | 64 bit現行 | slot-only | parent oracle | parent derived |
|---|---:|---:|---:|---:|---:|---:|---:|
| `W=1` | 0 | 0 | 1 | 12 | 4 | 12 | 8 |
| `W=64,P=1` | 63 | 0 | 127 | 768 | 508 | 768 | 512 |
| `W=64,P=W,r=2` | 126 | 63 | 253 | 1272 | 760 | 768 | 512 |
| `W=64,P=W,r=4` | 252 | 189 | 505 | 2280 | 1264 | 768 | 512 |

単位はbyte per generationになる

### 1世代の論理read

dead parent gapがない最小値 `R=R_n=P-1` を使う

| case | 64 bit現行 | slot-only | parent oracle |
|---|---:|---:|---:|
| `W=1` | 20 | 8 | 8 |
| `W=64,P=1` | 1784 | 1016 | 764 |
| `W=64,P=W,r=2` | 4556 | 2780 | 1772 |
| `W=64,P=W,r=4` | 8588 | 4796 | 3284 |

単位はlogical byte per generationで、current target handle readを含まない

parent列のbyte数は小さいが、`4L` の各loadがdepth方向に依存するため、この表だけで速度比較はできない

### resident topology

| case | `E_live` | `G` | 64 bit現行 | slot-only | parent oracle | parent derived |
|---|---:|---:|---:|---:|---:|---:|
| `W=1` | 32 | 32 | 280 | 136 | 272 | 264 |
| `W=64,P=1` | 95 | 95 | 1792 | 1144 | 1532 | 1020 |
| `W=64,P=W,r=2` | 158 | 158 | 2800 | 1648 | 1784 | 1272 |
| `W=64,P=W,r=4` | 284 | 284 | 4816 | 2656 | 2288 | 1776 |
| `G=10E_live` | 284 | 2840 | 4816 | 2656 | 12512 | 12000 |

単位はbyteで、Action payload、candidate descriptor、vector headerを含まない

この表ではoracle版parentは深い分岐でのみslot-onlyより小さくなる

frontier導出版は理想的な `G=E_live` なら早い段階でmemoryが小さくなるが、`G>>E_live` では同じく退化する

## benchmarkで必要なcounter

数式と実装を照合するため、generationごとに次を記録する

- `W`, `N`, `P`, `P_n`
- `e`, `B`, `M`, `F`, `L`
- `R`, `R_n`
- rollback、apply、enumerate、tryの回数
- old tour read、next tour readとwriteのtoken数
- trace readとwriteのslot数
- leaf要素参照数と異なるcache line数
- parent load数とsuffixごとのdependent chain長
- entryとadjacent LCPのread、write、minimum更新数
- `G`, `G_cap`, `E_live`, `G/E_live`
- 各vectorのsizeとcapacity
- cycles、instructions、branch miss、L1 miss、LLC miss、memory bandwidth

次のassertをdebug counterへ置ける

```text
F == B + M
F == W + L
rollback == e + M
apply == e + 1 + M
X == rollback + apply
E_live == M + D
G >= E_live
```

direct parentとslot-onlyでState call列が一致することを、counterだけでなくAction identity列でも比較する

## 再現方法

`cost_model.py` は同じ式と既定の退化例を再計算する

```sh
python3 research/beam_search/cost_model.py
```

任意の均質cross suffix例は次で計算できる

```sh
python3 research/beam_search/cost_model.py \
  --width 64 --parents 64 --cross-suffix 4 \
  --unfixed-depth 32 --trace-capacity 1000
```

scriptのmemory列もAction payloadを含まず、oldとnextのcapacityがlogical sizeと等しい定常近似になる

## 実装判断

32 bit slot-onlyはhandle byteを半減し、先頭prefixを除き、dependent loadを増やさないため第一実装に適する

direct parent oracleは構造として成立し、`frontier_slot` を含む正しいcostで比較する

独立frontierを持つoracleと、cand導出版を別backendへ実装し、冗長配列の費用を分離して測る

direct parentの主な勝ち筋は `M/W` が大きく、`G/E_live` が小さく、parent blockがcache内にある場合になる

主な負け筋は `W=1`, `P=1`, 浅いcross-parent、parentのLLC miss、`G>>E_live` になる

従ってdirect parentをslot-onlyの無条件な後継にはせず、同一interfaceの別backendとして比較するのが妥当になる
