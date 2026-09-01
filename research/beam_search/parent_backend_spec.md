# direct parent-slot backend の敵対的監査と実装仕様

## 対象と結論

対象は通常版の `titan_cpplib/ahc/beam_search/beam_search.cpp` である。
`beam_search_state.cpp`、`beam_search_state_turn.cpp`、`old/` は対象に含めない。

監査対象の新構造は次である。

```text
generation-local Action slot
+ 1世代前を指す direct parent slot
+ 実際の展開順に並べた frontier
+ frontier の隣接葉 LCP
```

結論は次になる。

- 後述する不変条件を全て守る direct parent-slot 案には、正当性を崩す反例を見つけられなかった
- `tour`、`next_tour`、`leaf`、`copy_tour_path()` は正しさを保ったまま除去できる
- State の `apply_op()` と `rollback()` の列は現行版と一致させられる
- 素朴な `parent_slot = parent_leaf` は不正であり、最小2候補で壊れる
- 世代開始時の `entry_lcp` を持たない実装も不正であり、survivor が endpoint から離れると壊れる
- prefix 解放で endpoint を集合から外す実装も不正である
- 完全同点の順序は現行の `std::sort` 自体が規定していないため、別backend間の形式的な同値には注意が要る
- 最初の正解実装では、現行の `cand` 入力順、sort、逆走査をそのまま残すべきである
- B世代ごとの path-block は成立するが、direct parent の低memory版ではない
- path-block は依存chainを約B分の1へ減らす代わりに、transposeと重複slot列を追加する
- path-blockのboundary前にprefixを解放するなら、epoch parent保持またはlive partial blockが必要になる
- runとunary系はlogical frontier ordinalへActionをgatherする別layoutであり、directのdrop-inではない
- unary bit cursorは成立するが、LCP直下でもchildに対応するbit位置の同期が必要になる

これは速度の結論ではない。
direct parent は毎世代の flat tour 再構築を消す一方、下りpathを世代間の依存loadで復元する。
正当性確認後に current postorder と同じworkloadで測定する必要がある。

## 現行版で区別すべき4種類の番号

同じ `int` に見える番号を混ぜることが、このbackendで最も危険である。

| 名前 | 意味 | 現行版での生成位置 |
|---|---|---|
| selector slot | `Candidates::next_beam` 内の物理位置 | candidate selector |
| Action slot | `gblock[d]` 内の物理位置 | `finalize_generation(d)` |
| frontier ordinal | 葉を実際に展開した順番 | `now_leaf_idx` |
| history node ID | 可視化用ノード番号 | `node_id_counter` |

現行版では Action slot は selector slotと一致する。
frontier ordinalは `cand` を末尾から走査した順番なので、通常はAction slotと一致しない。
history node IDは探索木の親参照に使ってはならない。

### `parent_leaf` の正確な意味

候補列挙中に渡す `parent_leaf` は、現在葉のfrontier ordinalである。

```text
parent_leaf = 今のgenerationで何番目に展開した葉から生成されたか
```

これは旧 `cand` のindexでも、Action slotでもない。
新しい子の実親slotは必ず次で求める。

```text
parent_slot[d + 1][child_slot] = frontier_slot[parent_leaf]
```

### 素朴な同一視が壊れる最小反例

root候補をselectorが次のslotへ格納したとする。

```text
gblock[1][0] = A
gblock[1][1] = B
cand          = [A, B]
```

generation 1はsortせず、loopを末尾から走査する。

```text
frontier ordinal 0 = B = Action slot 1
frontier ordinal 1 = A = Action slot 0
```

最初に展開したBから生成した子は `parent_leaf=0` を持つ。
ここで `parent_slot=parent_leaf` と書くと親slot 0のAを指し、正しい親slot 1のBを失う。

従って、少なくとも最初の実装では `frontier_slot` を独立配列にする。
後でActionをfrontier順にgatherする最適化は可能だが、別の変更として検証する。

## 記号とindex規約

深さはrootを0とし、Actionは到着nodeの深さに属する。

```text
action[d][s] : 深さdのnodeへ入るAction
parent[d][s] : そのnodeの深さd-1における親slot
trace[d]     : mutable Stateの現在pathにある深さdのslot
```

深さ1の `parent[1][s]` はrootを表すsentinelとし、通常のdecodeでは読まない。

深さdのfrontierを実展開順に次で表す。

```text
frontier_slot[j] : j番目に展開する深さdの葉slot
adj_lcp[k]       : frontier k と k+1 のLCA絶対深さ
entry_lcp        : generation開始endpointと frontier 0 のLCA絶対深さ
```

`frontier_slot.size()==C`、`adj_lcp.size()==max(0,C-1)` とする。
この文書では `adj_lcp[k]` を0-originの隣接区間と定義する。

generation開始時のStateは深さd-1にあり、`frontier_slot[0]` は深さdにある。
従って常に `0 <= entry_lcp <= d-1` である。

## 必須不変条件

### I1 generation block

深さdの有効slot `s` について、`action[d][s]` は親から `(d,s)` へ進むActionである。
`d>=2` なら `parent[d][s]` は深さd-1の親slotである。

### I2 frontier と `cand`

深さdの `cand` を現行と同じ向きで保持する場合、全ordinal `j` について次が成立する。

```text
frontier_slot[j] == cand[C - 1 - j].action_slot
```

generation 1もこの式を使うが、generation 1の `cand` はsortしない。

### I3 frontier ordinal

候補列挙へ渡す `parent_leaf` は現在のloop ordinal `j` と一致する。
candidate selectorのslot、`cand` index、Action slotを渡してはならない。

### I4 adjacent LCP

全 `0 <= k < C-1` について、`adj_lcp[k]` は
`frontier_slot[k]` と `frontier_slot[k+1]` のnode identity上のLCA深さである。

Actionの値が同じかどうかではなく、探索履歴nodeが同一かどうかでLCAを定義する。
同じ親から出た別candidateは、Action値が同じでもLCAは親深さになる。

### I5 entry LCP

`entry_lcp` はgeneration開始時のState endpointと最初のfrontier葉のLCA深さである。
最初の葉の親が開始endpointそのものなら `entry_lcp=d-1` になる。

### I6 State と trace

最初の葉へ入る前は、Stateと `trace[1..d-1]` がgeneration開始endpointを表す。
ordinal jの列挙直前は、Stateと `trace[1..d]` が `frontier_slot[j]` を表す。

### I7 順序

generation 2以降は、現行と同じ `cand` 入力列を同じcomparatorでsortし、末尾から先頭へ展開する。
実展開順の `parent_leaf` は単調非増加になる。

generation 1だけはsortしない。

### I8 parent の構築時点

`parent[d+1]` はcandidate selectorの最終結果が確定した後に構築する。
仮採用された候補へ先に書く場合は、replacementのたびに必ず上書きしなければならない。

正解実装では `finalize_generation()` 内で最終 `next_beam` だけから構築する。

### I9 prefix 境界

`freed_to` 以下のAction blockを解放した後、全将来LCPは `freed_to` 以上である。
target path復元は深さ `freed_to+1` のslotを得た時点で止める。

### I10 dangling parent を読まない

深さ `freed_to+1` のnodeが持つparent値は、解放済みblockを指していてよい。
その値は通常buildで読まない。

### I11 endpoint

通常generationの走査終了時、Stateはcurrent frontierの最後の葉 `C-1` にいる。
次generationの `entry_lcp` はこのendpointを必ず含めて計算する。

### I12 metadata処理の純粋性

LCP構築、parent構築、path-block transposeではState methodを呼ばない。
Actionのcopy、move、destructor時点を変更する最適化は、構造の正当性確認と分離する。

## ordered frontier の成立

### 必要な順序性

必要なのは辞書順そのものではなく、各探索木subtreeのfrontier葉が連続区間になることである。
以下ではこの性質をDFS-compatibleと呼ぶ。

### 世代更新で保存される理由

旧frontierがDFS-compatibleだと仮定する。
新候補を旧親ordinalごとの連続groupにし、親groupを旧順または逆順に並べる。

- ある旧ancestorの子孫親は旧frontier上の連続区間である
- その区間全体を逆にしても連続性は失われない
- 各親を、その親から出た子の連続blockで置き換えても連続性は失われない

従って新frontierもDFS-compatibleである。
同じ親内のscore順や完全同点順は、この区間性には影響しない。

root候補は全て同じrootの子なので、generation 1が帰納法の基底になる。

## LCP の区間minimum

DFS-compatibleな同一深さの葉列を `v[0..C)` とする。
任意の `p<q` について次が成立する。

```text
LCP(v[p], v[q]) = min(adj_lcp[p..q))
```

右辺のrangeは `adj_lcp[p]` から `adj_lcp[q-1]` までである。

`v[p]` と `v[q]` のLCAより深い任意のsubtreeは、葉列上で連続区間を作る。
両端が同じ深いsubtreeに入るなら中間葉も全て入る。
逆に両端のLCAを跨ぐ境界が区間内に最低1個あり、その境界LCPが同じ深さまで下がる。

一般RMQは不要である。
新frontierの親ordinalが単調なので、問い合わせ区間を1回ずつscanできる。

## 新frontierとLCPの構築

深さdのcurrent frontierから、深さd+1の候補を確定した場面を考える。
sort後の `cand` を逆順に読んだdescriptorを `order[j]` とする。

```text
order[j] = cand[Wc - 1 - j]
p[j]     = order[j].parent_leaf
```

現行comparatorを使う限り、`p[j]` は単調非増加である。

### 新しい隣接LCP

```text
p[j-1] == p[j]
    next_adj_lcp[j-1] = d

p[j-1] > p[j]
    next_adj_lcp[j-1] = min(adj_lcp[p[j] .. p[j-1]))
```

同じ親の子同士は親depth dまで一致する。
異なる親なら、子同士のLCAは親葉同士のLCAと一致する。

### 次generationの entry LCP

current走査終了endpointはordinal `C-1` である。
新frontierの最初の子の親は `p[0]` なので次になる。

```text
p[0] == C-1
    next_entry_lcp = d

p[0] < C-1
    next_entry_lcp = min(adj_lcp[p[0] .. C-1))
```

この値を保存せず、次generationで `adj_lcp` だけを見る実装は不正である。

### 1本の下降scan

entry区間とgroup間区間は重ならない。
次の概念loopで旧 `adj_lcp` を合計高々 `C-1` 個読む。

```text
cursor = C - 1

descend_to(p):
    h = d
    while cursor > p:
        --cursor
        h = min(h, adj_lcp[cursor])
    return h

next_entry_lcp = descend_to(p[0])

for j in 1 .. Wc-1:
    if p[j] == p[j-1]:
        next_adj_lcp.push_back(d)
    else:
        assert(p[j] < p[j-1])
        next_adj_lcp.push_back(descend_to(p[j]))
```

`descend_to()` の `h` は呼出しごとにdへ戻す。
`cursor` だけを前回位置から継続する。

## 葉間遷移

深さdのordinal jへ入るとき、使うLCPを次とする。

```text
h = (j == 0 ? entry_lcp : adj_lcp[j - 1])
source_depth = d - 1 + (j != 0)
```

最初だけStateが深さd-1にあり、2葉目以降は深さdにある。

### 実装可能なdecode

```text
for k = source_depth down to h+1:
    state.rollback(action[k][trace[k]])

v = frontier_slot[j]
for k = d down to h+1:
    trace[k] = v
    if k > h+1:
        v = parent[k][v]

for k = h+1 to d:
    state.apply_op(action[k][trace[k]])
```

target suffix長を `r=d-h` とすると、parent loadは `r-1` 回である。
深さhのslotは現在のtraceと同じとLCPから分かるため、最後のparentを読む必要がない。

同一親の兄弟では `h=d-1`、`r=1` なのでparent loadは0回になる。

### 現行の `f` との対応

現行のparent-level距離を `q=(d-1)-h` と置く。

| 場面 | rollback数 | apply数 | parent load数 |
|---|---:|---:|---:|
| 最初の葉 | `q` | `q+1` | `q` |
| 2葉目以降 | `q+1` | `q+1` | `q` |
| 同一親の2葉目以降 | 1 | 1 | 0 |

これは現行の `rollback(lca_dist+f)` と `apply(lca_dist+1)` に一致する。
従ってLCPが正しければState methodの引数列と順番も一致する。

## 手計算例

### root、同一親兄弟、離れた親

rootのAction slotが `[A=0, B=1, C=2]` の順に格納されたとする。
generation 1は逆順に展開するためfrontierは次になる。

```text
F1 = [C, B, A]
adj_lcp = [0, 0]
entry_lcp = 0
```

深さ2の実展開順が次になったとする。

```text
F2 = [A/y, A/x, C/z]
parent ordinal = [2, 2, 0]
```

新LCPは次になる。

```text
entry: endpoint A -> parent A = 1
A/y -> A/x = 1
A/x -> C/z = min(F1.adj_lcp[0..2)) = 0
```

State操作は次になる。

```text
開始位置 A
apply(y)
rollback(y), apply(x)
rollback(x), rollback(A), apply(C), apply(z)
```

現行postorderと同じsimple pathだけを通る。

### Wが1へ縮み、親を飛ばす例

上のF2で、次に残る候補がparent ordinal 1の `A/x/u` だけだとする。
走査終了endpointはordinal 2の `C/z` なので、次のentry LCPは0である。

```text
F3 = [A/x/u]
adj_lcp = []
entry_lcp = LCP(C/z, A/x) = 0
```

次generationの最初に次を行う。

```text
rollback(z), rollback(C)
decode u -> x -> A
apply(A), apply(x), apply(u)
```

`adj_lcp` が空だから移動不要と判断すると、CのStateへuを適用して壊れる。
W=1でも `entry_lcp` は必要である。

### gapを1境界だけで近似すると壊れる例

深さの同じ旧葉4個の隣接LCPが次だとする。

```text
adj_lcp = [2, 0, 2]
```

親ordinal 3から0へ飛ぶ場合のLCPは `min(2,0,2)=0` である。
直前の1境界だけを見ると2を返し、rollbackを2段省いて壊れる。

## parent map の確定手順

### replacement後にだけ書く

beam width 1で、親Aの候補が一度selector slot 0へ入り、その後に親Bの良い候補が置換したとする。
仮採用時にだけparentを書けばslot 0はAを指したままになる。

従って次の順を固定する。

```text
全current葉を展開
candidate selectorの最終集合を確定
finalize_generation(d+1)
各最終selector slot sについてparentを設定
現行と同じcand sort
新frontierとLCPを構築
```

### finalize の概念コード

```text
for s in 0 .. candidates.size()-1:
    move candidates.next_beam[s].action -> action[d+1][s]

    p = candidates.next_beam[s].parent_leaf
    assert(0 <= p < frontier_slot.size())
    parent[d+1][s] = frontier_slot[p]

    cand.push_back({p, score, s, node_id})
```

深さ1だけparentをroot sentinelにする。

## generation loop の疑似コード

### root generation

```text
State state
state.init()
rootで現行と同じ順に候補列挙

if found_finished:
    現行と同じFinishedを返す
if candidates.empty():
    現行と同じNoCandidatesを返す

finalize_generation(1, root sentinel)
record_historyなら現行と同じsurvivor記録

candはsortしない
frontier_slot[j] = cand[C-1-j].action_slot
adj_lcp.assign(C-1, 0)
entry_lcp = 0
turns_done = 1
```

root候補は全てrootの子なので隣接LCPは0である。

### 通常generation

```text
for d = 1 .. max_turn-1:
    現行と同じ時点でwidthを取得しCandidatesをreset
    explored_per_turn = 0

    assert(cand.size() == frontier_slot.size())
    max_parent_jump = 0

    for j = 0 .. frontier_slot.size()-1:
        i = cand.size()-1-j
        c = cand[i]
        assert(c.action_slot == frontier_slot[j])

        h = (j == 0 ? entry_lcp : adj_lcp[j-1])
        source_depth = d-1+(j != 0)
        parent_jump = (d-1)-h
        max_parent_jump = max(max_parent_jump, parent_jump)

        rollback trace[source_depth..h+1]
        parent mapからtarget suffix trace[h+1..d]を復元
        apply trace[h+1..d]

        current_action = action[d][trace[d]]
        現行と同じenumerate_actionsを呼ぶ
        全submitへparent_leaf=jを渡す

    if found_finished:
        現行と同じFinishedを返す
    if candidates.empty():
        現行と同じNoCandidatesを返す

    現行と同じverbose logとhistory survivor記録
    confirm_and_free(d-max_parent_jump)

    finalize_generation(d+1)
    generation 2以降と同じcomparatorでcandをsort
    current frontierとsorted candからnext frontier、adj LCP、entry LCPを構築
    frontier blockをswap

    現行と同じ位置でparam.timestampを呼ぶ
    turns_done = d+1
```

`confirm_and_free(d-max_parent_jump)` は最初の正解prototype用の互換境界である。
より深い安全な境界は後述する。

## prefix 解放

### 互換境界

各遷移のparent-level距離は次になる。

```text
q = (d-1)-h
```

entryと全adjacent transitionの最大値を `max_parent_jump` とすれば、現行の
`max_lca_dist` と同じである。

```text
confirm_and_free(d-max_parent_jump)
```

このmodeはAction destructorの時点も現行へ最も近く、最初のprototypeに向く。

### current frontier 全体による境界

generation開始endpointは走査終了後に不要になる。
走査終了時のState endpointはcurrent frontier最後の葉であり、将来親もcurrent frontierの部分集合である。

```text
C == 1:
    h_all = d
C >= 2:
    h_all = min(adj_lcp)

confirm_and_free(h_all+1)
```

これはentryを除外し、W=1ならcurrent葉まで確定する。
正しいが、現行よりAction destructorを早く呼ぶため、互換modeとは別に検証する。

### survivor parentとendpointによる境界

選択済み子の最小parent ordinalを `p_min`、current endpointを `e=C-1` とする。

```text
p_min == e:
    h_keep = d
p_min < e:
    h_keep = min(adj_lcp[p_min..e))

confirm_and_free(h_keep+1)
```

future entryと内部遷移が使う親は全て `[p_min,e]` にあるため安全である。
parent backendにはdead tour segmentがないので、current postorderよりdangling範囲管理は単純になる。

### endpointを外すと壊れる最小反例

current frontierが `[A,B]`、State endpointがB、survivor parentがAだけだとする。
survivor集合だけなら深さ1のAまで確定できるように見える。

しかし `trace[1]` はBであり、次generationの最初にBをrollbackしてAをapplyする必要がある。
Bをresult prefixへ確定してblockを解放すると直ちに壊れる。

Stateを先にAへrelocateしない限り、prefix集合へendpointを必ず含める。

### dangling parent の停止条件

prefix depth hまで確定した後、深さh+1のnodeのparentは解放済み深さhを指し得る。
decodeは次の条件で止める。

```text
trace[h+1] を書いたら停止
parent[h+1][trace[h+1]] は読まない
```

LCP既知版が `r-1` loadで止められることは、性能だけでなく寿命の正当性にも必要である。

## 終了処理

### `finished`

finished Actionはfrontierへ入らない。
現在葉の列挙中に見つけた場合、`trace[1..d]` はその親pathを正確に表している。

```text
best_finished_path = result_prefix
+ action[k][trace[k]] for k=freed_to+1..d
+ finished Action
```

現行と同じく、そのgenerationの全葉列挙を終えてからreturnする。
最初のfinishedでleaf loopをbreakすると、同じgenerationのより良いfinishedを失う。

`build_final_state()` は現在endpointを `trace` でrootへ戻し、best pathをrootから適用する。

### no candidates

rootで0候補ならStateはrootにあり、pathなしで返す。
通常generationで0候補ならStateは最後に展開した深さdの葉にいるが、final Stateは返さない。

現行と同じく `found_finished` を先に判定する。
finishedとnonfinished候補0件が同時に起きた場合はFinishedになる。

### max turn

max-turn時の `cand` は深さDの候補で、Stateは深さD-1の走査endpointにいる。
best candidateのpathは、そのcandidate slotからparentを逆に辿って別scratchへ復元する。

```text
slot = cand[best_idx].action_slot
for k = D down to freed_to+1:
    result_slot[k] = slot
    if k > freed_to+1:
        slot = parent[k][slot]
```

その後 `result_prefix` と深さ昇順のActionをmaterializeする。

この復元で `trace` を上書きしてはならない。
`materialize_final_state=true` では、Stateはまだ旧endpointにあり、rollbackへ元のtraceが必要だからである。

### `max_turn==1`

root候補確定後にgeneration loopへ入らない。
現行版のgeneration 1はsortされていないため、新backendもsortしてはならない。

best candidateは現行と同じ `cand` storage順をstrict `<` でscanして選ぶ。
pathはroot Action 1個だけなのでparentを読まない。

### final generationで省けるもの

深さDのparent mapは最終path復元に必要であり、省けない。
一方、深さDのfrontier LCPとentry LCPは次のtraversalがないためrelease版では構築を省ける。

最初のcorrectness prototypeでは共通pathを通し、差分oracle通過後に最後のLCP構築だけを省く方が安全である。

## BeamParam とtelemetry

`search()` は `BeamParam` の累積値も更新するため、返却Resultだけが観測可能状態ではない。

現行の `param.timestamp()` が受け取る `tour.size()` は、direct parent版には物理的に存在しない。
現行互換の論理値は、各frontier遷移のLCPから書かれたはずのsegment長を合計すれば得られる。

```text
compat_tour_size = sum over frontier targets j of (d-h[j])
h[0] = entry_lcp
h[j] = adj_lcp[j-1] for j>=1
```

これは現行が初回の未参照prefixも書く状態の値である。
先頭prefix除去後を基準にする場合はj=0の項を除く。

最初のdifferential backendでは、このcounterをmetadataだけで計算し、`pool_size_sum` の互換性を保てる。
本採用時にparent metadata量へ意味を変えるなら、公開telemetryの変更として明記する。

`is_adjusting=true` ではbackendの実行時間差そのものが次の幅を変える。
探索結果の構造同値testでは固定幅を使うか、現行版の幅列をrecordしてreplayする。

`turn_sum`、`beam_width_sum`、`width_hist` と `timestamp()` の呼出し回数は現行と一致させる。

## `record_history`

parent backendのmetadataへhistory node IDを流用しない。
`CandIdx.node_id` と `Submitter.parent_node_id` は現行のまま残す。

historyを一致させる条件は次になる。

- 葉の展開順を一致させる
- 各葉内のAction列挙順を一致させる
- `threshold()` を読む時点を一致させる
- duplicate hashのreplacement順を一致させる
- `node_id_counter` を同じ分岐でだけ増やす
- `record_turn_survivors()` を同じ時点で呼ぶ

snapshotの `active_node_ids` は `unordered_set` から生成され、配列順自体はAPIで規定されていない。
差分testではsnapshot配列をsetとして正規化し、history node列は順序付きで比較する。

## 完全同点

現行comparatorは次のkeyしか見ない。

```text
(parent_leaf, score)
```

両方が同じ要素について `std::sort` はstableではなく、標準は相対順を規定しない。
同点順は候補selectorのthreshold、duplicate hash、後世代の探索結果へ影響し得る。

### 最初のprototypeで守ること

- `Candidates::next_beam` の入力順を変えない
- 現行と同じ `CandIdx` 列を作る
- 同じ `std::sort` 呼出しを残す
- sort後の `cand` からfrontierを逆向きに構築する
- parent bucket、stable sort、index sort、Action gatherを同時に導入しない

値型やlayoutまで変えると、規格上はkey-equal順の一致を証明できない。
同一toolchainでの差分testに加え、完全同点の探索結果を明示testする。

### 形式的に順序を固定する方法

将来cross-backendで完全に規定したい場合は、selector slotまたは列挙ordinalを明示tie-breakにする。
これは現行版の結果を変え得るmigrationなので、parent backendだけで先行してはならない。

## B世代 path-block 案

### 目的

direct parentはtarget suffix長rに対し `r-1` 個のdependent parent loadを行う。
B世代ごとにroot-to-frontier pathをblock化し、古いpathをB辺単位で復元する案を考える。

anchor depthを `a=kB` とする。
anchor frontierの各node `(a,s)` へ次を持つ。

```text
path_block[a][s]   : 深さa-B+1..aのB個のAction slot
anchor_parent[a][s]: 深さa-Bの祖先slot
```

`path_block[a][s]` は深さ昇順に連続配置する。
最初のB未満の深さはpartial blockとして扱う。

### 正当性不変条件

全 `0<=j<B` について次を満たす。

```text
path_block[a][s][j]
    = node (a,s) のpath上にある深さ a-B+1+j のAction slot
```

`anchor_parent[a][s]` は同じpath上の深さa-Bのslotである。

この2条件があれば、adjacent LCP hまでのtarget suffixを次で復元できる。

1. current anchorより後ろのpartial epochをdirect parentで戻る
2. anchor nodeのpath blockを対応するtrace深さへ連続copyする
3. `anchor_parent` で1つ前のanchor nodeへ移る
4. hを含むblockではh+1より深い部分だけcopyし、anchor parentを読まずに止める
5. 完成したtraceを深さ昇順にapplyする

State操作列はdirect parent版と同じである。
変わるのはtarget traceを作るmetadata readだけになる。

### 依存load

古い部分の深さ方向依存は、edgeごとではなくblockごとの `anchor_parent` だけになる。
suffixが十分長ければ依存段数は概ね次になる。

```text
direct parent : r-1
path block    : current partial depth + ceil(older_depth/B)
```

B個のslot自体は全て必要だが、連続配列から読み、互いのaddressは前のslot値へ依存しない。

ただし各Action payloadは従来どおり世代別 `action[depth][slot]` から読む。
State更新も辺ごとに直列なので、全探索の依存chainがB分の1になるわけではない。

### 構築方法1: 各世代でpartial pathをcopy

epoch内の深さkの子ごとに、親のk-1 slotをcopyして新slotをappendする。

```text
1世代目: 1 slot / child
2世代目: 2 slots / child
...
B世代目: B slots / child
```

幅W一定なら1epochのwriteは約 `W*B*(B+1)/2` slotである。
1世代平均は `Theta(WB)` となり、Bを増やすほど悪化する。

この方式は構築が単純でも、direct parentの代替としては採用しない。

### 構築方法2: boundary transpose

epoch中は通常のdirect parentをB世代分だけ保持する。
boundary深さaで各anchor frontier nodeからB段parentを辿り、leaf-majorなpath blockへtransposeする。

```text
for each slot s at anchor depth a:
    v = s
    for depth = a down to a-B+1:
        block[s][depth-(a-B+1)] = v
        v = parent[depth][v]
    anchor_parent[s] = v
```

実装ではblockを深さ昇順に読むため、逆indexへ書く。
transpose完了後、完成済みepoch内のdirect parent配列は破棄できる。

幅W一定ならboundaryで約 `BW` parent read、`(B+1)W` slot writeを行う。
B世代で償却すると1世代あたり約W read/writeだが、direct parentの通常writeも既に払っている。

従って安定幅でのmetadata writeは概ね次になる。

```text
direct parent : qW / generation
full path block with temporary parents:
    qW + q(B+1)W/B ~= qW(2+1/B) / generation
```

boundary transposeはpartial copyより良いが、direct parentよりwriteが少ないわけではない。

### endpoint slotを省く変種

anchor node自身のslot sはlookup時に既に分かっている。
blockから深さaの末尾slotを省き、次だけを持てる。

```text
intermediate slots : 深さa-B+1..a-1のB-1個
anchor_parent      : 深さa-Bの1個
```

合計B wordなので、安定幅の完成block量はdirect parent全世代のword数とほぼ同じになる。
末尾Actionだけ別sourceになるが、trace[a]=sと直接書けば正しさは変わらない。

user-facingな「B個のAction slotが完全に連続」という性質を優先するfull版は、
anchor nodeごとに末尾slotとanchor parentの合計B+1 wordを持つ。

### memory

安定幅、未解放node総数Gを仮定する。

```text
direct parent:
    qG

full path block:
    約 qG(1+1/B)

末尾slot省略block:
    約 qG

両方式のcurrent epoch temporary parent:
    最大 qBW
```

Action payload `G*sizeof(Action)` はどちらも残る。
path-blockはdirect parentの圧縮表現ではなく、同程度以上のslot数を局所性へ交換する表現である。

幅が変動する場合、完成block量は次になる。

```text
full block          : sum over anchors a of (B+1) * W_a
末尾slot省略block   : sum over anchors a of B * W_a
current temporary   : sum over current epoch depths d of W_d
```

各世代のdirect parent量 `sum_d W_d` とは一致しない。
boundaryだけ幅が大きい場合はpath-blockが大幅に増え、boundaryで幅が小さい場合は減り得る。

### transposeで避けられない重複

同じanchor nodeの多数の子孫がboundaryで生きると、共有祖先slotが各leaf blockへ重複して書かれる。
per-leafの連続B-slot列をO(1)で得る条件では、この重複は避けられない。

共有prefixをspanやparent pointerへ戻すと、連続blockという性質を失い、path-chunkまたはdirect parentへ近づく。

transposeはleafごとにB段のdependent readを持つ。
複数leafをround-robinに辿ればmemory-level parallelismを出せるが、総readと総writeは減らない。

### path-block と prefix 解放

prefixがblock内部へ進んでも、将来decodeは `freed_to+1` より浅いslotを使わない。
block先頭にstaleなslotを残し、必要suffixだけ読むことは正しい。

ただしboundary transpose前にepoch内parent mapを解放すると、固定長blockを後から構築できない場合がある。
例えばB=4でdepth 6までprefix確定後、depth 8で深さ5から8のfull blockを作ろうとすると、
深さ6以下の親鎖は既に失われている。

実装は次のどちらかを選ぶ。

1. boundary transposeが終わるまでcurrent epochのparent mapを保持する
2. transposeを `max(a-B+1,freed_to+1)` で止め、live suffixだけのpartial blockを作る

1で旧Action blockまで保持する必要はないが、解放済みAction slotをblockへ書いてもdereferenceしてはならない。
2はblockごとのlive begin depthまたはlengthを追加し、固定B slotという前提を弱める。

次を守る。

- hを含むblockで `anchor_parent` を読まない
- 全深さが `freed_to` 以下になったanchor blockは解放してよい
- current partial epochのparent配列は、将来transposeしない範囲だけ解放する
- debug全走査でstale slotをActionへdereferenceしない

### 正しさの退化例ではなく性能の退化例

- `B=1` は依存loadを減らさず、block metadataとtransposeだけを増やす
- 兄弟集中で `r=1` が多いとdirect parentのparent loadは元々0回である
- LCPが常に浅いsuffixではなく、常に短いsuffixなら古いblockをほぼ読まない
- prefixがB世代より速く進むと、作ったblockを使う前に解放する
- boundary generationだけ幅が大きいと `BW_a` のmaterialize burstが大きい
- Bを大きくするとtranspose scratch、cache footprint、単発latencyが増える
- Bを小さくするとanchor dependency削減が小さい
- Stateのapply/rollbackが重いとmetadata改善が全体へ現れない
- Action loadは世代別配列のままなので、Action payloadがLLC missする場合はslot局所化だけでは足りない
- `G/E` が大きい場合、dead anchor recordをblock単位で保持する問題はdirect parentと同様に残る

### direct parentとのbreak-even

`Y` をdirect parentが読むparent slot数、`A_B` をpath-blockの露出anchor load数とする。
概念的な条件は次になる。

```text
direct parentのdependent latency Y * L_parent
>
path slotのstream read
+ A_B * L_anchor
+ boundary transposeの償却cost
+ 追加resident metadataによるcache cost
```

比較するBは `2,4,8,16` とする。
`B=1` はdirect parent相当の負controlとして残す。

windowed direct decodeも複数leafのparent missを重ねられる。
path-blockを採用する前に、同じresident memoryを増やさないwindowed decodeと比較する。

### path-block の判定

構造としては成立する。
ただし最初の新backendにはしない。

direct parentで次が同時に観測された場合の第二段比較にする。

- 長いtarget suffixが多い
- parent loadのLLC missと依存latencyが支配的
- B世代以上残る未確定historyが長い
- boundary transposeを償却できる幅と深さがある
- slot blockの追加working setがcache上限を超えない

## monotone parent run 案

### 提案の形

深さdのchild ordinalから深さd-1のparent ordinalへの写像を、同じ親が続くrunで持つ。

```text
ParentRun {
    uint32_t child_end
    uint32_t parent_ordinal
}
```

run rが表すchild区間は次になる。

```text
begin = (r == 0 ? 0 : runs[r-1].child_end)
end   = runs[r].child_end
parent(begin..end-1) = runs[r].parent_ordinal
```

1世代の子数をW、distinct parent数をPとすると、32 bit版の本体は `8P` byteである。
direct parentの `4W` byteより小さい必要条件は `P<W/2` になる。

### 現行の物理Action slotへ直接は使えない

現行のAction slotはcandidate selector slotであり、frontier ordinalとは一致しない。
`parent_leaf` のgroupはfrontier ordinal上で連続するが、物理child Action slot順には連続しない。

さらにrunが返す値を次の古いrun mapへ入力するには、値は古いgenerationの論理ordinalでなければならない。
単なる物理parent Action slotでは、古いrun列のchild ordinalとして解釈できない。

成立させる方法は次の2つである。

1. 各generationのAction slotをlogical frontier ordinalと一致させる
2. ordinal-to-Action-slotと逆写像を全generationへ別途持つ

2は概ね追加 `4G` byteを使い、direct parentのmemoryを既に払う。
従ってrun版では1を前提にする。

これは前節までのdirect parent prototypeから独立したlayout変更である。
direct parentの正解oracleを通した後にだけ導入する。

### Action slotをlogical ordinalへ合わせる手順

現行の実展開順を変えずに、Actionだけをその順へgatherする。

```text
generation 1:
    selector順のcandはsortしない

generation 2以降:
    現行と同じ入力descriptorを同じcomparatorでsort

両方:
    logical slot j <- cand[C-1-j] のAction
```

`cand` storage自体は現行と同じ向きに残し、各descriptorの `action_slot` をlogical slotへ更新する。
これによりloopは従来どおり `cand` を逆走査でき、max-turnのbest scan順も維持できる。

Actionを先にselector slotへmoveしてから再gatherすると、Action moveが2回になる。
run版のfinalizeはsort用small descriptorだけを先に作り、Actionを最終slotへ1回だけmoveする必要がある。

完全同点の `std::sort` 順は別問題として残る。
最初のrun prototypeでは現行と同じdescriptor入力とsort呼出しを維持する。

### run列の向き

logical slotを実展開順 `0..W-1` とする。
現行は次generation候補をparent昇順へsortし、逆向きに展開する。

従って、child ordinalが増えるとparent ordinalは単調非増加になる。
同じparentの子は1つの連続runになる。

```text
child ordinal : 0 1 2 3 4 5
parent        : 5 5 3 3 3 0
runs          : (2,5), (5,3), (6,0)
```

canonicalな木の左右方向はgenerationごとに反転して見える。
しかしordinalを毎回実展開順で振り直せば、各1世代mapは常に非増加で統一できる。

canonical ordinalを固定する別設計ではorientation bitが必要になる。
最初のrun版では実展開順ordinalだけを使用する。

### 祖先ordinalの向きはdepthごとに交互になる

各1世代mapをantitoneな関数とみなす。
current frontier ordinal jが増えると、1世代前の親ordinalは減少する。

2個のantitone mapを合成するとnondecreasing、3個ならnonincreasingになる。
従って固定depth kにおけるcurrent frontier祖先ordinalは次になる。

```text
(current_depth-k) が偶数 : 単調非減少
(current_depth-k) が奇数 : 単調非増加
```

各depthのcursorは1回のfrontier巡回中には一方向だけ動く。
ただし次generationではparityが反転するため、同じrun列のcursor方向も反転する。

cursor実装はforward専用にせず、現在child ordinalに応じて前後へ動ける必要がある。

### trace対応run cursor

各有効depth kについて、次を保持する。

```text
trace_ordinal[k]
run_cursor[k]
```

`run_cursor[k]` は `trace_ordinal[k]` を含む `parent_run[k]` のrun indexである。

child ordinal xへcursorを合わせる操作は次になる。

```text
while x >= runs[cursor].child_end:
    ++cursor

while cursor > 0 && x < runs[cursor-1].child_end:
    --cursor
```

最後にrunのbegin、endへxが入ることをassertする。
parent queryは `runs[cursor].parent_ordinal` を返す。

### 初回cursor位置

新しいcurrent generation mapでは、最初に展開するchild ordinalは必ず0である。
従って最初のquery時に `run_cursor[current_depth]=0` とできる。

古いdepthのcursorはgenerationをまたいで保持する。
generation開始時のtraceは旧endpointを表すため、各cursorもそのendpoint pathのrunに同期している。

最初のtargetへdecodeするときは、同期済み位置からtarget ancestor ordinalまでcursorを動かす。
各mapの端から毎generation探索し直す必要はない。

prefix解放でmapを捨てるときは対応cursorも捨てる。

### run cursorをLCPで省く場合

LCPをhとしてtarget suffixを復元すると、深さh+1のnodeからparentを読む必要はない。
run cursorだけ古いsource nodeの位置へ残る危険があるように見える。

しかしsourceとtargetの深さh+1 nodeは、同じ深さh nodeを親に持つ。
run encodingでは同じ親のchildrenは1つのrunなので、両nodeは同じrunに属する。

従って `run_cursor[h+1]` は移動しなくてもtarget nodeを含み、同期条件を保つ。
`trace_ordinal[h+1]` 自体はsource値からtarget値へ更新する。
深さh+2以上はparent queryを行うため、通常どおりcursorを更新する。

LCPで飛ばしたtransition列は、各depthで見れば元の単調列の部分列である。
cursorの一方向性も失われない。

### traversal中の償却量

各depthのquery child ordinalが単調なら、cursorが跨ぐrun数はその巡回で高々そのmapのrun数である。

ただしcurrent frontierが参照しないdead ordinalのrunも、両端を跨ぐとcursor scanへ現れる。
activeな全古いmapについて最悪次を読み得る。

```text
sum(active generations k) P_k runs / current generation
```

これはlive suffixのparentだけを直接読む `Y` より大きくなり得る。
`G>>E` や遠いordinalだけが生存する形では、run cursorがdead gapを大量に横断する。

初回だけbinary searchし、その後cursorを使う方式も可能だが、trace同期cursorが正しく保てるなら通常は不要である。
debug、final path、順不同queryにはrandom lookupを別に残す。

### final path のrandom lookup

max-turnで選ぶbest candidateは、現在State endpointや最後のfrontier ordinalとは限らない。
通常traversal用cursorをbest pathへ動かすと、`build_final_state()` が使うcurrent traceとの同期を壊す。

final pathはcursorとtraceを変更せず、各depthで次を行う。

```text
run = lower_bound(runs.child_end, child_ordinal+1)
parent_ordinal = run.parent_ordinal
```

1回の最終復元なので `O(D log P)` は許容候補になる。
必要ならrun cursor配列をcopyし、current traceからbest pathへ動かす方法とbenchmarkする。

finished結果は現在traceを使うためrunのrandom lookupを必要としない。

`max_turn==1` はroot Action 1個だけで、run mapを読まない。

### run map の prefix 解放

深さ `freed_to+1` のrunが返すparent ordinalは解放済みdepthを指し得る。
direct parent版と同様、LCPで停止するためそのparent値を読まない。

cursorは同じ親run内に留まるので、child interval判定だけで同期を保てる。
prefix以下のrun blockはgeneration単位で解放する。

### memory と構築cost

1世代のraw byteは次になる。

```text
direct parent : 4W
run parent    : 8P
```

- `P=1` なら8 byteであり、大きな兄弟groupを非常に小さく表せる
- `P=W/2` でdirectと同量になる
- `P=W` なら8W byteでdirectの2倍になる

run構築はsort後parent列を1回scanし、parentが変わる境界だけemitする `O(W)` 処理である。
既存sort結果を使うため追加sortは不要である。

ただしlogical slotへのAction gatherが前提になる。
run単体のcostへ、gather用descriptor、source index、Action moveの局所性を含める必要がある。

### directとの位置づけ

runはdistinct parentが少ない疎なgroup表現である。
`8P < 4W`、すなわち `P<W/2` のときだけraw byteが32 bit directより小さい。

Pが非常に小さい場合はrunが単純で強い。
PがWに近い場合は次節のbitvectorまたはdirectが強い。
generationごとのP/Wとzero-parent gapで形式を選ぶ余地がある。

### 退化例

- `P==W` ではparentごとに1 childとなり、runはdirectの2倍byteになる
- group平均長が2以下ではraw memory上の利益がない
- dead ordinal gapを跨ぐたび、使わないrunもcursorがscanする
- generationごとに向きが反転するのにforward-only cursorを使うと、先頭位置へ戻すscanが発生する
- cursorをtraceと同期しないと、最初のtargetごとに端からrunを探す必要がある
- LCPで省いた深さのcursor同期を無条件に捨てると、同じrunであるという不変条件を利用できない
- max-turn best pathは順不同なので、cursorだけではworst-case線形scanになる
- logical slot gatherを別costとして数えない評価は不公平である
- fully tied candidateのsort順を同時に変えると、速度差と探索順差を分離できない
- Wが小さくrun vectorがL1にある場合、boundary branchがdirect loadより遅くなり得る
- State操作が重い場合、parent metadata差は全体へ現れない

### run map の判定

構造として成立する。
ただしdirect parent配列のdrop-in replacementではなく、logical slot layoutを伴う別variantである。

次の順で試す。

1. direct parent prototypeで正しさを固定する
2. logical Action slot gatherだけを導入し、完全同点を含む探索順を再確認する
3. logical layout上のdirect parentをoracleにする
4. run mapへ差し替え、trace cursorを全depthで検証する
5. `P/W`、cursorが跨いだrun数、random lookup数を計測する

採用候補は、`P<W/2` が継続し、cursor scanがdirect parent loadより少ないworkloadである。

## unary child-count bitvector と monotone bit cursor

### 前提と符号化

run版と同じく、Action slotを実展開順のlogical ordinalへgatherした後のvariantとして扱う。
物理selector slotのままではchild ordinalからAction slotへの別写像が必要になり、drop-in replacementにはならない。

1世代前の幅をR、現在幅をC、子を持つdistinct parent数をPとする。
実展開順のparent列 `p[x]` は単調非増加なので、次の反転ordinalは単調非減少になる。

```text
u(x) = R - 1 - p[x]
```

parent `R-1-u` の子数を `c[u]` とし、単一unary列を次で作る。

```text
U = 1^c[0] 0 1^c[1] 0 ... 1^c[R-1] 0
```

`U` の長さは厳密に `R+C` bitである。
0-originのx番目の1の位置をbとすると、次でparentを復元できる。

```text
b    = select1(U, x)
u    = b - x
parent(x) = R - 1 - u
```

例えば `R=4`、`p=[3,3,1]` なら `u=[0,0,2]`、`U=1100100` になる。
ゼロ子parentの0を省くと `u` が詰まり、元のparent ordinalを復元できない。

`C==0` では次frontier自体がない。
bitvectorとcursorを作らず、finished判定後のno-candidate分岐へ入る。

### 単一unary列のcursor不変条件

各有効depth kで次を保持する。

```text
trace_ordinal[k] = x
unary_cursor[k].bit_position = select1(U_k, x)
unary_cursor[k].reversed_parent = bit_position - x
```

`reversed_parent` は毎回引き算で求めてもよく、cache fieldは必須ではない。

次queryのchild ordinalをyとする。

- `y>x` なら `(y-x)` 個先の1までbit cursorを前進する
- `y<x` なら `(x-y)` 個前の1までbit cursorを後退する
- word間はpopcountで必要な1の個数を減らし、word内selectで停止位置を求める
- 1個ずつの移動ならmasked wordに対するctzまたはclzで次の1を探せる

bit位置を独立fieldにしない実装も可能である。
`x` と `u` を保持すれば `bit_position=x+u` で再構成でき、前後両方向へ進める。

一方、forward用のnext-word位置だけを持ち、正確なbit位置または `x+u` を保持しないcursorは不成立である。
次generationでquery方向が反転したとき、前の1へ戻れず、端からの再scanかrandom selectが必要になる。

### 初回位置と世代ごとの向き

新しいcurrent mapで最初に展開するchild ordinalは0である。
先頭parentを `p[0]` とすると、最初の1の位置は `R-1-p[0]` になる。

```text
x0            = 0
bit_position0 = R - 1 - p[0]
```

高ordinal側にゼロ子parentがあっても、この位置を構築中に保存すればleading zero列をscanしない。
無条件に `bit_position0=0` とする実装は、`p[0] != R-1` の最小例で壊れる。

古いmapのcursorは、generation終了時のState endpoint pathへ同期したまま保持する。
新generationの最初のtargetへは、そのendpoint位置から動かす。

current depthからmap depthまでの差が偶数ならchild queryは単調非減少、奇数なら単調非増加になる。
次generationではparityが反転するため、同じmapのcursor方向も反転する。

forwardとreverseの両方で、最初のtargetを含むquery列は単調である。
これは最初のsourceを旧frontier endpointとして含めることに依存する。

### unary cursorをLCPで省く場合

LCPをhとして深さh+1のparent queryを省く点はrun版と同じである。
ただしcursor同期の処理はrun版と同一ではない。

sourceとtargetの深さh+1 nodeは同じparentを持つため、unary列では同じ連続1区間に入る。
child ordinalをxからyへ変えると、この区間内には0がないので次が成立する。

```text
bit_position += y - x
x = y
```

parent decodeは不要だが、このcursor更新自体は必要である。
run cursorは同じrun indexに留まればよいのに対し、unary cursorの1位置はchildごとに異なる。

新しいcurrent depthの最初のtargetだけはsource nodeが存在しない。
前節の `x0=0` でcursorを先に初期化しておけば、LCPが旧depthまで届いてqueryを省いても同期する。

prefix境界がhの場合も深さh+1のparentをdecodeしない。
mapを残すなら上記の同一1区間更新だけ行い、mapごと解放するならcursorも同時に捨てる。

### 二bitvectorへの分離

同じ情報を次の2本へ分ける案も成立する。

```text
group_start[x] = xがparent groupの先頭なら1
used_parent[u] = parent R-1-uが1個以上の子を持つなら1
```

raw bit数は `C+R` で単一unary列と同じである。
64 bit wordへ別々に丸めると、単一列より最大1 word多くpaddingする。

`rank1(bits, end)` を半開区間 `[0,end)` の1の個数とすると、random lookup式は次になる。

```text
g    = rank1(group_start, x + 1) - 1
u    = select1(used_parent, g)
p[x] = R - 1 - u
```

ordered query用cursorは `(x,g,u)` と、必要なら現在groupのbegin、endを持つ。
xを前進するときは `(old_x,x]` のgroup start数だけgを増やし、used parentの1も同数だけ前進する。
後退時は `(x,old_x]` を数え、gとused parentの1を同数だけ後退する。

各group startと各used parentは1対1に対応する。
従って両cursorを同じ個数だけctzまたはclzで動かせばparent ordinalを同期更新できる。

長い同親区間では、次のgroup startを一度求めてcacheできる。
targetがその境界内なら比較1回で同じparentを再利用でき、used parent列を読まない。

LCP直下ではsourceとtargetが同じgroupなので、xだけを更新し、g、u、group境界を維持する。
単一unary列のような `bit_position += y-x` は不要である。

generation parityが反転するため、group startとused parentの両cursorにforwardとreverseが必要になる。
next-set位置だけを持つ片方向実装は、単一unary列と同じ反例で端からの再scanへ退化する。

### dead zero-parent gap と償却量

単一unary列ではchild ordinal差と反転parent ordinal差が同じbit列の距離になる。
二bitvector版では前者をgroup start列、後者をused parent列で別々にscanする。

1回のfrontier巡回中、各depthのqueryは一方向なので、各cursorが同じwordを往復することはない。
word-level実装なら、1 mapで読む上限はquery両端間の概ね次の量になる。

```text
single unary : (abs(delta child) + abs(delta reversed_parent)) / 64 words
two bitsets  : abs(delta child) / 64 + abs(delta reversed_parent) / 64 words
```

丸めと境界maskを除けば同じorderであり、active map全体を跨ぐ最悪値は `ceil((R+C)/64)` wordである。

子を持たないparentが長く続くと、その区間は単一unaryの0列またはused parentの0列になる。
ctzやclzは0 wordを一命令で越えられず、word loadとzero判定は残る。

従って1だけをK個ごとにsampleしても、long zero gapのrandom lookup時間は制限できない。
上限が必要ならwordごとのrank directory、0側sample、またはrun表への切替が必要になる。

二bitvectorはgroup startのzero wordも読む。
Pが小さくCとRが大きい場合、明示runはdead child区間とdead parent区間の両方を読み飛ばせる。

### raw byte のbreak-even

alignmentとindexを除く1世代の量は次になる。

| 表現 | raw byte | ordered queryの主な処理 |
|---|---:|---|
| 32 bit direct | `4C` | edgeごとにdependent load 1回 |
| explicit run | `8P` | 越えたrunごとにend比較とrecord load |
| single unary | `(R+C)/8` | 越えたwordのload、popcount、word内select |
| two bitsets | `(R+C)/8` | 2 streamのload、popcount、ctzまたはclz |

single unaryと二bitvectorのraw bit数は等しいが、命令列とpaddingは等しくない。

unary系がdirectより小さい条件は次になる。

```text
(R + C) / 8 < 4C
R < 31C
```

`R=C=W` ならunary系は `W/4` byte、directは `4W` byteで、raw量は16分の1になる。
一方、`R>31C` の急縮小世代ではunary系のraw bitvector自体がdirectより大きい。

runとunary系の境界は次になる。

```text
8P < (R + C) / 8
64P < R + C
```

`R=C=W` なら `P<W/32` でrunが小さく、`P>W/32` でunary系が小さい。
小さいgenerationではword padding、generation header、cursor配列、rank/select indexも含めて比較する。

byteが小さいことは速度の十分条件ではない。
directがL1にある場合、1 loadをbit scan、popcount、select、branchへ置き換えるunary系は負けやすい。

runはP個の明示境界だけを読む。
unary系はPが中程度以上でrun recordがcacheを圧迫し、packed word scanが連続化できる場合に有望である。

### 構築costと深いparent chain

単一unary列はゼロ初期化した `R+C` bitへ、各child xについて `x+u(x)` のbitを立てれば作れる。
二bitvectorはparentが変わるchildと使用parentに1 bitずつ立てる。

両方ともparent列の追加sortは不要だが、bit領域のclearまたは全word overwriteが必要になる。
runはC個のparent列をscanしてP recordを出し、directはC個のparent値を書く。

どのbitvectorもActionのlogical slot gather costを消さない。
このgatherとbitvector構築を別々に計測せず、parent metadataだけを比較してはならない。

cursorはrandom selectを避けても、深さkのparent結果が深さk-1のquery ordinalになる依存chainを残す。
二bitvector版もこのchainを短くせず、edgeごとに2 streamのcursor stateを扱う可能性がある。

従ってbitvectorは主にworking setとbandwidthを減らす案であり、directのdependency latencyをB分の1にする案ではない。

### final path と順不同query

max-turnのbest leafは通常cursorの現在位置と一致しない。
通常cursorを直接動かすとfinal State用traceとの同期を壊すため、次のいずれかが必要になる。

- cursor配列をscratchへcopyし、現在位置からbest pathへ前後scanする
- single unaryへrandom `select1(x)` indexを付ける
- 二bitvectorへ `rank1(group_start,x+1)` と `select1(used_parent,g)` のindexを付ける

cursor copyは1回だけだが、best ordinalが遠いとactive bitvectorの大部分を読む。
二bitvectorのindexed lookupはrankとselectの2段で、single unaryのselect 1段より命令が多い。

finished結果は現在traceを使うためbitvectorのrandom lookupを必要としない。
debugの順不同queryもfinal pathと同じindexまたは別oracleを使う。

### 成立条件と反例

- logical child ordinalを実展開順へ揃える
- parent labelを反転し、実展開順の非増加列をunaryの非減少列へ変換する
- ゼロ子parentを表す0またはused parent上の位置を失わない
- cursorへ正確なbit位置、またはそれと同値な `(x,u)` を保持する
- generation parityに応じてforwardとreverseの両方を実装する
- LCP直下でもchild位置だけはtargetへ同期する
- 新mapの最初の1を構築時に保存し、leading zero列を先頭からscanしない
- final random lookupで通常trace cursorを破壊しない

これらを外す最小反例は反例監査表へまとめる。

### unary系の判定

表現の代数、cursorの前後移動、LCP skip同期、二bitvectorのgroup-parent同期には反例を見つけられなかった。
再実行可能な抽象oracleでもdirect、run、single unary、二bitvectorのdecodeが一致した。

ただし64 bit wordを使う実際のforward/reverse select、境界mask、rank directoryは未実装かつ未監査である。
BMI2、broadword、table fallbackの命令比較も未計測であり、速度優位は結論にしない。

最初のbitvector prototypeではsingle unaryと二bitvectorを同時実装しない。
logical directをoracleに一方ずつ追加し、zero gapとP/Wの計測後に他方を比較する。

## 反例監査の一覧

| 仮実装 | 最小反例または判定 |
|---|---|
| `parent_slot=parent_leaf` | root候補2個でordinalとslotが反転し不正 |
| generation 1もscore sort | root後の展開順が現行から変わる |
| `entry_lcp` なし | endpoint B、survivor parent Aで不正 |
| 同一親兄弟でparentを1回読む | 正しいが不要。LCP既知なら0回でよい |
| gapの端1個だけでLCPを求める | `[2,0,2]` の親3から0で不正 |
| provisional candidateへparentを固定 | beam slot replacementで古い親が残る |
| W=1で `min(adj_lcp)` | empty rangeで不正 |
| survivorだけでprefix確定 | dead endpointのrollback Actionを早期解放して不正 |
| depth h+1からparentをもう1回読む | prefix解放後のdangling parentを読む |
| final pathをtrace上へ復元 | final State rollbackのsource traceを破壊する |
| max-turn用parentを省く | best leafの祖先pathを復元できない |
| index sortへ同時変更 | 完全同点の展開順が現行と一致する保証がない |
| path-blockを毎世代prefix copy | 1epoch `Theta(WB^2)` writeへ退化 |
| path-blockをmemory削減とみなす | 安定幅でもdirectと同程度以上で反証 |
| transpose前にepoch parentを全解放 | prefixがblock内へ進んだ後にfull blockを構築できない |
| runを物理Action slot順に作る | parent groupが連続せず不正 |
| runのparent slotを次runのordinalに使う | 物理slotと論理ordinalが異なり不正 |
| run cursorを常にforwardへ進める | generation parity反転で逆方向queryに対応できない |
| runだけでfinal pathを復元 | best leafが順不同でlinear scanへ退化 |
| unaryを非増加parent labelのまま作る | child順とunary group順が逆になり不正 |
| unaryからゼロ子parentの0を除く | parent ordinalのgapを復元できない |
| 新unary cursorをbit位置0に置く | 最大parentが未使用なら最初の1はleading zero後にある |
| unary cursorをforward専用にする | 次generationのparity反転で後退できない |
| unaryのLCP直下でcursorを更新しない | runと違いsourceとtargetの1位置が異なり同期を失う |
| 1側sampleだけでzero gapを制限する | 長いゼロ子parent列をword scanするworst caseが残る |
| 二bitvectorの2 cursorを別々に進める | group rankとused parent rankがずれて誤parentになる |
| final lookupで通常bit cursorを動かす | current traceとの同期を壊しfinal State復元が不正になる |

## 必要なoracle test

### 実施済みの抽象反例探索

再実行可能な抽象modelを `research/beam_search/parent_backend_model.py` に保存した。
default条件は次になる。

```text
seed                  : 20260901
random topology       : 30,000件
random depth          : 2..15
random width          : 1..8
exhaustive topology   : depth 3、各width 1..3の単調非増加parent列を全列挙
physical slot         : random permutation、全探索ではidentityとreverseを交互に使用
```

これに加え、64 bit境界、128以上のparent gap、幅1対1024の急縮小と急拡大を7件固定している。

default実行はminimal slot反例1件、固定7件、random 30,000件、全探索393件を検査した。
合計256,126 generations、1,153,150 transitionsで全assertを通過した。

追加で `--seed 2 --trials 0 --exhaustive-depth 3 --exhaustive-width 5` も実行した。
全探索73,085件を含む219,272 generations、965,120 transitionsで全assertを通過した。

検査対象は次になる。

- physical slot direct parentとexplicit pathの一致
- entry LCPとadjacent LCPのrange-min式
- logical parent run cursorの前後移動とLCP skip同期
- single unary cursorのbit位置、前後移動、LCP skip同期
- 二bitvectorのgroup rankとused parent rankの同期
- run、single unary、二bitvectorのrandom lookup

以前の一時modelで得た個別件数はscriptが残っていないため、再現可能な最終根拠には使わない。
上記のseed付き統合modelを本書の抽象検証結果とする。

これは形式証明ではない。
後述の決定的な全探索と実際のcandidate selectorを含むdifferential testを置き換えない。

任意条件での再実行例は次になる。

```bash
python3 research/beam_search/parent_backend_model.py \
    --seed 1 --trials 100000 --max-depth 20 --max-width 16 \
    --exhaustive-depth 3 --exhaustive-width 4
```

### 1 topology kernel の全探索

Stateやcandidate selectorを含めず、small ordered treeを列挙する。

- depth 1から5
- frontier幅 1から5
- 親ordinalの全単調非増加列
- 同じ親のrun
- 親ordinalのgap
- 世代ごとの幅増減

各generationでexplicit node treeから次を計算し、backend結果と比較する。

- `frontier_slot`
- 全 `adj_lcp`
- `entry_lcp`
- 全childの `parent_slot`
- targetごとのroot-to-leaf slot列
- transitionごとのrollback Action列
- transitionごとのapply Action列
- prefix境界より下のparentを読んでいないこと

### 2 現行postorderとの differential test

Actionへ一意のedge ID、pre-state fingerprint、post-state fingerprintを持たせる。
Stateは不正なparentへActionを適用した時点でassertする。

両backendで次を順序付き比較する。

- `enumerate_actions()` 直前のState fingerprint
- `try_op()` の呼出し順とthreshold
- `apply_op()` のAction ID列
- `rollback()` のAction ID列
- generationごとのsurvivor `(score, hash, parent ordinal, edge ID)`
- 最終Action列
- status、score、turns_done
- materialized final State

`is_adjusting=false` を基本にする。
動的幅は現行から幅列をrecordし、新backendへreplayして構造だけを比較する。

### 3 ordinal とslot

- root selector slotを2個以上作る
- replacementでselector slotの親を変更する
- sort後Action slotとfrontier ordinalが全て異なるpermutationを作る
- 各 `parent_leaf` が `frontier_slot[parent_leaf]` へ写ることを確認する

### 4 LCP とendpoint

- 同一親の兄弟だけ
- 全葉が別root child
- adj LCPが `[2,0,2]` になるgap
- endpointの子が全滅し、離れた親だけ生存
- survivor parentがendpoint自身
- frontier 5葉から次frontier 1葉、その後4葉へ増える

### 5 prefix free

- W=1の長いchain
- 複数葉の長い共通prefix
- survivor parentとendpointだけが深いprefixを共有
- prefixが一度に複数世代進む
- freed blockをpoisonし、ASan/UBSanでAction参照がないことを確認
- 深さ `freed_to+1` のdangling parentをdebug counterで読んでいないことを確認

互換境界、current-frontier境界、survivor+endpoint境界を別々に試す。

### 6 finished

- root列挙中にfinished
- generationの最初、中間、最後の葉からfinished
- 同generationでfinished scoreが複数回改善
- finishedとnonfinished survivorが同時に存在
- finishedがありnonfinished候補が0件
- `materialize_final_state=true/false`

最初のfinishedで探索を止めず、同generationのbest finishedを返すことを確認する。

### 7 no candidates

- rootで0候補
- 深さ1以降で全葉が0候補
- INF rejectだけで0候補
- duplicate hash予約だけで0候補

status、turns_done、path空、final State nullを現行と比較する。

### 8 max turn

- `max_turn=1`
- best leafの親がState endpointと異なる
- best pathが複数parent blockを跨ぐ
- `freed_to>0`
- best score完全同点
- final Stateあり、なし

返却Action列をrootから別Stateへreplayし、score対象nodeと一致することを確認する。

### 9 record history

- `record_history=false/true` の両template instantiation
- accepted、replaced、duplicate、INF、finishedを全て発生させる
- node列は順序付き比較
- snapshot active IDはsetとして比較
- 全nodeのparent IDとstatusを比較

### 10 完全同点

- 全候補が同じscore、異なるhash
- 同じ親かつ同じscore
- 異なる親かつ同じscore
- beam境界が全て同点
- duplicate hashも同点

同一build内で現行版と展開traceを比較する。
cross-toolchainの保証が必要なら、両backendへ共通tie ordinalを導入した後に別migration testを行う。

### 11 path-block

Bを1、2、3、4とし、direct parentをoracleにする。

- LCPがblock内部、block境界、anchor直上にある
- max_turnがBの倍数でない
- prefixがblock内部へ進む
- boundary前にprefixが進み、保持版とlive partial版の両方でtransposeする
- current partial epochだけで遷移が終わる
- 3個以上のblockを跨ぐfinal path
- boundary直前と直後でwidthが急増、急減する
- 多数葉が同じblock prefixを共有する
- transpose後にtemporary parent blockをpoisonする

全targetについてdecodeしたslot path、State call列、final pathをdirect parentと比較する。

### 12 monotone parent run

- `P=1`、`P=W/2`、`P=W`
- parent ordinalに大きなgapがある
- current depthから各祖先depthまでのorientation parityが交互になる
- LCPが深く、途中mapをqueryしない
- LCP直下のchildだけが変わり、cursorが同じrunに残る
- endpointから最初のtargetへ逆方向にcursorが動く
- generation境界で全古いcursorの向きが反転する
- dead runを多数飛ばす
- max-turn best leafが中央ordinalにある
- `freed_to+1` のparent runを読まずに停止する

logical-layout direct parentをoracleにし、全depthで次をassertする。

```text
run_cursor[k] のrun intervalが trace_ordinal[k]を含む
run queryのparent ordinalがdirect parent[k][trace_ordinal[k]]と一致する
```

### 13 unary parent map

single unaryと二bitvectorを別test targetにし、logical-layout direct parentをoracleにする。

- `R=C`、`R=31C`、`R=32C`、`C>>R`
- `P=1`、`P<W/32`、`P=W/32`、`P=W`
- 最大parentが未使用でleading zero列がある
- 使用parent間に64、65、255、256以上のzero gapがある
- child groupがword境界の直前と直後で始まる
- current depthから各mapまでのparityが交互になる
- generation境界で同じmapのcursorがforwardからreverseへ変わる
- LCP直下で同じparentの遠いchildへ飛ぶ
- prefix境界h+1のparentを読まずにcursorだけ同期する
- max-turn best leafが先頭、中央、末尾ordinalにある
- final scratch lookup後も通常trace cursorが変わらない

single unaryでは各depthで次をassertする。

```text
bit_position == select1(U, trace_ordinal)
single_decoded_parent == direct_parent[trace_ordinal]
```

二bitvectorでは次をassertする。

```text
group == rank1(group_start, trace_ordinal + 1) - 1
parent_position == select1(used_parent, group)
split_decoded_parent == direct_parent[trace_ordinal]
```

portable fallback、BMI2版、forward、reverseへ同じbitvector corpusを与える。
ASan、UBSanでword先頭のreverse maskと末尾paddingを検査し、bit完全一致後にだけ性能を比較する。

## 実装順の提案

1. 32 bit direct parent、独立 `frontier_slot`、32 bit absolute LCPでcorrectness backendを作る
2. 現行のcandidate selector、`cand` sort、Submitter、history分岐を変更せず移植する
3. prefixは現行互換境界だけを使う
4. topology kernelとend-to-end differential testを通す
5. max-turn pathをparent chainのscratchで復元する
6. current-frontier prefix、survivor+endpoint prefixを別々に有効化して再検証する
7. 最終generationの不要LCP構築を省く
8. slot幅、CandIdx layout、sort gatherはそれぞれ単独で比較する
9. direct parentのdependent missが実測で支配した場合だけwindow decodeを試す
10. `P/W` が小さい場合、logical slot版directをoracleにrun mapを比較する
11. 幅が安定しrunが大きい場合、single unaryをlogical directと比較する
12. single unaryのzero gapまたはselect命令が支配する場合だけ二bitvectorを比較する
13. それでも長いchain latencyが残る場合にpath-block `B=2,4,8,16` を比較する

最初からAction slotをfrontier ordinalへgatherしたり、unary parent、path-blockを同時導入すると、
順序差と構造差を切り分けられない。

## 最終判定

direct parent-slot + ordered frontier + adjacent LCPは、現行postorderより構造的に異なる実装候補として成立する。
正しさの鍵はLCAそのものではなく、次の対応を崩さないことである。

```text
frontier ordinal -> Action slot
adjacent/entry LCP -> rollback停止depth
target Action slot -> generation parent chain
```

最も重要な反例は、ordinalとslotの同一視、entry endpointの省略、prefix集合からendpointを外す処理である。
これらを避ければ、Stateの辺遷移列を現行版と一致させたままtourを除去できる。

B世代path-blockも正しく構成できるが、slot総数を減らす構造ではない。
依存parent loadを連続path-slot readとboundary transposeへ交換するcache policyである。
eager prefix freeと併用する場合は、transposeまでepoch parentを保持するかlive suffixだけをblock化する。
direct parentのprofileで長いdependent missが確認された後にだけ比較するのが妥当である。

monotone parent runも成立するが、現行のselector Action slotへそのまま適用できない。
logical frontier ordinalへActionをgatherした上で、少数parentの長いrunを `8P` byteへ圧縮するvariantである。
cursorは1巡回中に一方向だがdepth parityで向きが変わり、final pathにはrandom lookupが必要になる。

single unaryと二bitvectorも同じlogical layout上で成立する。
幅が同程度ならraw parent metadataはdirectの16分の1だが、selectとbit scanの命令を追加する。
二bitvectorはgroup境界と使用parentを別cursorで同期できるが、deep parent dependencyとdead zero-word scanは残る。

従ってgeneration-localな候補は `4C` のdirect、`8P` のrun、`(R+C)/8` のbitvectorになる。
raw byteだけで固定せず、幅比、P、cursor span、zero gap、random lookup回数を同じ探索で記録して選ぶべきである。
