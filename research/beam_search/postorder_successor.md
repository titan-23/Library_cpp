# 帰りがけ順版より先の構造候補

## この文書の結論

`beam_search.cpp` は、二重連鎖木や通常の Euler tour をそのまま走査する版ではない。
[traP の記事](https://trap.jp/post/2920/)にある `trace / tour / leaf / cand` の帰りがけ順版を実装している。
`leaf` が隣接葉間の LCA 距離を符号化し、`tour` が次の葉へ下る Action 列を供給する。

したがって、以前の「LCA 相当の処理は実装済み」という説明は、現行コードの仕組みを説明しただけであり、
「この構造が最速」や「これより良い構造は存在しない」という意味にはならない。そのように読める表現は不正確だった。

現行の `State` 契約を変えず、単一の mutable `State` を使う限り、葉間の `apply_op()` と `rollback()` の回数を
構造だけで大きく減らす余地は小さい。一方、状態遷移以外の木メタデータ処理には、次の改善余地がある。

1. 初回の未使用 `next_tour` prefix を生成しない
2. prefix確定から世代開始時のentry jumpを外し、1葉ならその葉まで確定する
3. `tour` を残したまま ActionId を 64 bit から世代暗黙の 32 bit slot にする
4. 世代別 `parent_slot`、順序付き frontier、隣接葉 LCP を持ち、`tour / next_tour / leaf` を完全に除く
5. 小さな trivially-copyable Action を tour、trace、frontier に直接置き、`gblock` 自体を除く
6. State copy が適度に安い型向けに、部分木単位の checkpoint を持つ hybrid backend を用意する

特に 4 は、現行の帰りがけ順版に対する本当の構造的な比較対象になる。世代ごとの `next_tour` 構築を
`O(最終生存候補数)` の親 slot と LCP の書き込みへ置き換えられる。ただし、連続コピーの一部が
世代をまたぐ依存ロードに変わるため、全 workload で速いとは断定できない。置換ではなく backend を分けて比較すべきである。

## 現行実装が保持しているもの

監査対象は `titan_cpplib/ahc/beam_search/beam_search.cpp` である。

| データ | 内容 | 主なアクセス |
|---|---|---|
| `gblock[d][s]` | 深さ `d` で採用された Action | ActionId からの間接参照 |
| `trace[d]` | 現在の State へ至る深さ `d` の ActionId | 葉間遷移時の連続アクセス |
| `tour` | 前世代の葉集合が張る木の帰りがけ順 | 経路復元時の連続読み出し |
| `leaf` | 帰りがけ順の区間境界 | LCA 距離の区間最大値を単調走査で計算 |
| `cand` | 今回展開する葉と、その親葉番号 | `(parent_leaf, score)` で整列後に逆走査 |
| `next_tour` | 今回の走査中に作る次世代用の帰りがけ順 | `trace` の suffix を連続追記 |
| `next_leaf` | `next_tour` の葉区間境界 | 葉ごとに1要素追記 |

`ActionId` は `(generation << 24) | slot` の 64 bit 値である。`act(id)` は generation と slot を復号し、
`gblock` の vector descriptor と Action 配列を順に参照する。

### 1世代の木メタデータ処理

次の記号を使う。

| 記号 | 意味 |
|---|---|
| `C` | 今回展開する `cand` 数 |
| `L` | `leaf.size()`、すなわち前回展開した葉数 |
| `T` | 帰りがけ順配列に格納する ActionId 数 |
| `X` | 葉間移動に必要な `apply_op()` と `rollback()` の合計回数 |
| `G` | 解放されていない世代 block の Action slot 総数 |

現行コードの主な仕事量は次のようになる。

- LCA 距離を求める `leaf` の境界走査は世代合計で高々 `L-1` 区間
- `copy_tour_path()` の境界走査も世代合計で高々 `L-1` 区間
- `copy_tour_path()` が `tour` から `trace` へコピーする ID 数は、次の葉へ下る辺数以下
- `next_tour.insert()` は今回の葉集合を表す帰りがけ順を構築するため、合計 `Theta(T)` 個の 64 bit ID を書く
- `next_leaf` は `C` 個の 32 bit 境界を書く
- 実際の状態遷移は `X` 回

`li` と `parent_leaf` は単調に減るため、LCA 計算自体は `O(C^2)` ではない。一般的な RMQ や binary lifting が
直接消せるのは、この線形で軽い境界走査だけである。

一方、`next_tour` は次世代の候補選択結果が確定する前に、現在の全 `cand` を走査しながら構築される。
そのため、最終的に子を1つも残さない現在葉の経路も、その世代では `next_tour` に書かれる。
これは帰りがけ順方式を単純かつ連続アクセスにする代わりに払っている帯域である。

### `next_tour` と endpoint trace の正確な分担

深さ `d` で実際に展開する葉を順に `v[0], ..., v[C-1]` とする。世代終了時の State と `trace` は
最後の葉 `v[C-1]` にある。`next_tour / next_leaf` のうち、次世代で使う区間には次の不変条件がある。

```text
[next_leaf[i], next_leaf[i+1])
    = v[i] から LCA(v[i], v[i+1]) までの ActionId suffix
```

従って endpoint `v[C-1]` の root path は `trace` が保持し、それ以外の枝を `next_tour` が帰りがけ順に1回ずつ持つ。
通常の Euler tour のように全辺を2回持つわけではなく、共通幹を毎世代 `next_tour` へ丸ごとコピーするわけでもない。

現在の実装には、この不変条件に含まれない prefix が1つある。最初の loop iteration でも
`next_tour.insert()` を実行し、その直後の `next_leaf[0]` が prefix 終端になる。

```text
unused prefix = [0, next_leaf[0])
```

この insert は最初の親 pathを `trace` へ復元する前に行われる。最初の親が旧 endpoint と異なる場合、
下位 `trace` は旧 endpoint、`trace[d]` だけは最初の current Actionなので、prefixは一貫したroot-to-leaf pathですらない。
後続処理がこれを読まないことを前提にした番兵的な書き込みとみなせる。

しかし `copy_tour_path()` が読む最小 offset は `leaf[parent_leaf]` であり、`parent_leaf >= 0` なので
常に `leaf[0]` 以降である。LCA 計算は `leaf` の差だけを読み、最終 path 復元も同じ `copy_tour_path()` を使う。
従って、この prefix 内の ActionId は一度も読まれない。

### 最初の `next_tour.insert()` は省略できる

最初の iteration だけ `next_tour.insert()` を行わず、展開後に `next_leaf.push_back(0)` とすればよい。
`lca_dist` の計算、State の rollback / apply、`max_lca_dist` の更新はそのまま残す。

これが成立する理由は次のとおりである。

- 葉が1個なら、その葉の全 path は endpoint `trace` にあり、`tour` は空でよい
- 最初の親が旧 endpoint と異なっても、その移動は現在の State を作るためだけに必要で、次世代木には不要
- 葉が2個以上なら、2回目の insert が `v[0]` の必要な suffix を初めて格納する
- `confirm_and_free()` が使う `max_lca_dist` は insert と独立なので変わらない
- max-turn の path 復元は `leaf[0]` 以降しか読まず、葉1個なら `copy_tour_path()` の loop 自体が空になる
- 完成解は現在の `trace` から作るため prefix を参照しない

削減量は世代ごとに `first_lca_dist + 1` 個の64 bit ActionId write である。`tour.size()` を表示するログと
`BeamParam::pool_size_sum` は小さくなるが、後者は現在の幅計算には使われていない。時間計測型の動的幅では、
処理時間が変わるため他の高速化と同様に幅が変わる可能性はある。

この変更は parent-slot よりはるかに小さく、32 bit slot-only 化と独立に先行して試す価値がある。

## reverse front coding としての厳密な大きさ

### 定式化

確定済み prefix の深さを `b`、現在葉の深さを `d`、未確定高さを `H=d-b` とする。
展開順の葉 path を `P[0], ..., P[L-1]` とし、各 path は深さ `b+1` から `d` までの Action edge列とする。

隣接葉の LCP 深さを絶対深さで `h[i] = depth(LCA(P[i], P[i+1]))` と置く。
初回の未使用 prefix を除いた tour は、概念上次の suffix の連結である。

```text
S[i] = P[i][h[i]+1 .. d]
tour = S[0] || S[1] || ... || S[L-2]
trace suffix = P[L-1]
```

`leaf[0]=0`、`leaf[i+1]-leaf[i]=|S[i]|=d-h[i]` である。前から path を保存する通常の front coding と逆に、
右 endpoint の pathを基準に左葉のsuffixを並べるため、reverse front codingと呼べる。

### `H + |tour| = E` の証明

`E` を、確定 nodeをrootとし、表現対象の `L` 葉が張るtrieの異なる未確定edge数とする。次の条件を仮定する。

1. 全葉が同じ深さ `d` にある
2. 葉順が DFS 順で、任意区間の両端 LCP が区間内の隣接 LCP 最小値になる
3. 各 `S[i]` が LCA の直下から葉までの suffix を過不足なく持つ
4. `trace` が右 endpoint `P[L-1]` を持つ
5. 表現に初回の未使用 prefix や同じ edge の別コピーを加えない

endpoint path 上の `H` edge は trace に1回ずつある。endpoint path 上にない任意の edge `e` を1本取る。
DFS 順では `e` の部分木に属する葉は連続区間になり、endpointはその区間に含まれない。

その区間の最も右の葉を `P[i]` とすると、`P[i+1]` は `e` の部分木外なので `h[i]` は `e` より浅い。
従って `e` は `S[i]` に入る。部分木内の境界ではLCAが `e` 以上なのでsuffixに入らず、部分木外の境界では
左葉 `P[j]` が `e` を持たない。部分木の左境界もsuffixを持つ側の `P[j]` が外側なので、別のsuffixには入らない。

よって endpoint 外の各 edge は tour にちょうど1回、endpoint edge は trace にちょうど1回あり、次が成立する。

```text
E = H + sum(i=0..L-2, d-h[i])
  = H + |tour|
```

確定 prefix まで含む全 trie edge数を数えるなら、`result_prefix.size()` を両辺へ加える。
これは vector capacity ではなく、論理的に有効な Action / ActionId の要素数に対する等式である。

従ってprefix除去後のsnapshotだけを見れば、現行表現は論理active edgeごとにちょうど1個のhandleを持つ。
同じedge集合を明示的に保持したまま要素数だけを減らす余地はない。改善点はhandleのbit幅、毎世代の再配置write、
選択前に全current葉をmaterializeする時期、またはparent-slotのように別のancestry表現へ移すことにある。

### 現行コードで条件が成立する理由

初世代では全候補がrootの子である。その後は候補を `parent_leaf` ごとに連続させ、親groupを逆順に処理する。
親group全体の向きが反転しても、各部分木の葉が連続する性質は保たれる。同じ親内のscore順や完全同点順は、
その親の子順を決めるだけなのでDFS順の条件を壊さない。

2回目以降の `next_tour.insert()` は、直前葉から隣接葉のLCAまでrollbackしたのと同じ長さのtrace suffixを出す。
親間距離が `lca_dist` なら現在葉Actionを加えた長さは `lca_dist+1=d-h[i]` であり、条件3も成立する。

ただし現行の初回prefixを残すと、その長さを `B=next_leaf[0]` として次になる。

```text
H + |tour| = E + B
```

この `B` は live trie edgeの情報ではない。初回prefix除去後に初めて厳密な等式になる。

### 等式が壊れる反例

- 葉順が `Aの葉, Bの葉, Aの別葉` のように同じ部分木へ再入場する場合、Aへのedgeがsuffixとendpointに重複する
- 世代skipで葉深さが異なる場合、固定長 `H` と `d-h[i]` の式をそのまま使えない
- 同じhistory nodeを複数leafとして重複登録し、その重複をLCP深さ `d` として扱わない場合、余計なsuffixが入る
- survivor parentだけをlive集合と定義しながら全active葉のtourを数える場合、表現集合と `E` の集合が一致しない
- Compose版のghostやRadix版の合成辺をprimitive edgeと同じ1要素として数える場合、edgeの単位が一致しない

標準版の `E` は「次候補が実際に参照する最小trie」ではなく、「その世代で展開した全current葉の表現trie」である。
子を残さなかった葉も1世代はこの集合に含まれるため、どの集合の `E` かを測定時に区別する。

## `confirm_and_free()` 境界の精密化

### 現行の entry jump は安全だが保守的

loopの `turn=t` で展開する深さ `t` の葉を、実際の処理順に `V[0], ..., V[C-1]` とする。
Stateは世代開始時には旧葉集合のendpointにあり、世代終了時には `V[C-1]` にある。

`confirm_and_free(L)` は深さ `L` 未満、すなわち `L-1` までを `result_prefix` へ移して
generation blockを解放する。共通prefixの最深depthが `h` なら、正確な呼出しは `h+1` である。

現行の `max_lca_dist` には次の2種類が混ざっている。

- 世代開始endpointから `V[0]` の親へのentry jump
- `V[j-1]` の親から `V[j]` の親への内部transition

entry endpointは世代終了後のfrontierには含まれないため、前者はcurrent葉集合の共通prefixを制限しない。
現行境界は解放し過ぎることはないが、entry jumpが内部最大値より大きい世代では浅過ぎる。

### A: 内部transitionだけなら全current葉のLCPになる

`j>=1` の内部transitionで得るparent-level `lca_dist` を `a[j]` とする。隣接するcurrent葉自身は
子edgeを1本ずつ持つため、そのrollback距離は `a[j]+1` である。DFS順では全葉のLCPは隣接LCPの最小なので、
全current葉の共通prefix depthは次になる。

```text
D_all = 0                                      if C == 1
D_all = 1 + max(a[j] for j in [1, C))         if C >= 2
h_all = t - D_all
confirm_and_free(h_all + 1)
```

従って `C>=2` では内部最大値を `m` として `confirm_and_free(t-m)`、`C=1` では
`confirm_and_free(t+1)` が正確である。後者へ複数葉用の `m=0` を代入した `confirm_and_free(t)` では、
唯一のcurrent Actionを1世代余分に保持する。

内部最大値は現在のloopで最初のiterationを除いて更新すれば得られ、追加scanは不要である。
同じ親のcurrent葉が複数ある場合も `a[j]=0`、葉自身の異なる子edgeにより `D_all=1` となる。

### `C=1` で深さ `t` の block を解放できる理由

この判定を行う既存位置では、展開と候補選択が完了し、Stateと `trace` は唯一の `V[0]` にある。
深さ `t` のActionを `result_prefix` へコピーした後は、次の事実が成り立つ。

- 次世代候補の `parent_leaf` はすべて0で、最初のentryはrollbackを必要としない
- `copy_tour_path(0,0)` は空rangeである
- 初回未使用prefixを残す実装でも、`leaf.size()==1` なのでそのprefixは読まれない
- 次世代の兄弟間移動は深さ `t+1` の子Actionだけをrollbackする
- max-turnのpathは深さ `t` までを `result_prefix` から、最後の子をlive blockから得る
- `build_final_state()` は未確定suffixを戻した後、`result_prefix` のActionでrootまで戻せる

従って次候補が1個以上ある通常の世代末では安全である。初期候補生成直後はStateがまだrootにあるため、
単に `cand.size()==1` というだけで同じ解放をしてはならない。判定できるのはその葉を実際に展開した後である。

### B: survivor parentと現在endpointだけへ絞る

候補選択後、深さ `t+1` のsurvivorが参照するcurrent葉ordinal集合を `P` とする。
現在Stateのendpoint `e=C-1` を加えた `R=P union {e}` だけが、次世代の最初から最後まで必要になる。

候補が空でないとき `q=min(P)` とする。`e` は最大ordinalなので、DFS順の集合 `R` のLCPは
両端 `V[q]` と `V[e]` のLCAに等しい。current葉間のrollback距離は `next_leaf` から直接読める。

```text
r[k] = next_leaf[k+1] - next_leaf[k]
D_R  = 0                                  if q == e
D_R  = max(r[k] for k in [q, e))          otherwise
h_R  = t - D_R
confirm_and_free(h_R + 1)
```

初回未使用prefixが残っていても、差 `r[k]` からその長さは消える。survivor parentが複数でも、
全ordinalが `[q,e]` 内にあり、ordered trieの区間性により両端だけで集合全体のLCPが決まる。

この境界は A の全current葉境界以上に深く、`q=0` なら同じである。`q=e`、すなわち全survivorが
現在endpointを親に持つ場合は、`C>1` でも深さ `t` のgeneration block全体を解放できる。

### dead tour ID が読まれないことの証明

Bを使うと、future tourである `next_tour`、swap後の `tour` には解放済みgenerationを指すdangling IDが残り得る。
安全性は、全格納IDが有効ではなく、
将来読めるrangeのIDだけが有効という弱い不変条件に依存する。

次世代では `li=e` から始まり、target `parent_leaf` は `P` の要素だけを単調非増加順に取る。
従ってLCA走査と `copy_tour_path()` が調べるintervalの和集合は `[q,e)` の部分集合であり、
未使用先頭prefixとordinal `q` 未満のdead intervalは読まれない。

さらに任意の `k in [q,e)` について `r[k]<=D_R` である。そのintervalのActionIdは深さ
`t-r[k]+1` から `t` のsuffixなので、最浅でも `h_R+1` である。従って読まれるtour ID、trace rollback、
再applyはいずれも解放した深さ `h_R` 以下を参照しない。

DFS区間性は `h_R` を集合 `R` 自体の正確なLCPとみなすために必要である。順序が `A, B, A` なら、
両端AだけのLCPは深くてもcurrent traversalは中間Bを跨ぐ。上のrange max式はそのjumpを含めて保守的になり安全だが、
両端の明示LCAだけで深く解放する実装は壊れる。現行の親group順では部分木が連続するため、この差は発生しない。

このdangling ID方式は現行のアクセス箇所には成立するが、将来debug codeが `tour` 全体を走査して
全IDへ `act()` を呼ぶと壊れる。安全な実装にはreadable rangeだけを検査するassertか、dead prefixの物理破棄が必要である。

### endpointを外すと壊れる最小反例

深さ1のcurrent葉を `V[0]=A`, `V[1]=B` とし、Stateはendpoint `B` にあるとする。
次候補が `A` の子だけなら、survivor parent単体のLCPは深さ1である。しかし `trace[1]` は `B` なので、
ここで深さ1を確定すると `B` をresult prefixへ入れてAとBのblockを解放する。

次世代の最初のentryは `B` をrollbackして `A` をapplyする必要があり、直ちに解放済みIDを参照する。
endpointを集合へ含めればLCPはrootになり、解放しない。endpointを外せるのは、解放前にStateとtraceを
選択済みendpointへ正しくrelocateした別設計だけである。

### 終了経路と Action 寿命

既存と同じ位置、すなわち `found_finished` とno-candidateの早期return後にだけ適用するなら、終了処理は次になる。

- finished pathは最適化前に `build_best_path()` でmaterializeされ、この境界変更を通らない
- no-candidateではsurvivor集合が空なので式Bを使わず、そのままreturnする
- max-turnの `copy_tour_path()` はbest survivor parentからendpointまでだけを読み、上の `[q,e)` 証明に含まれる
- `materialize()` は `freed_to` より深いIDだけを読み、確定側は `result_prefix` のActionを使う
- `build_final_state()` も未確定traceだけを `act()` で戻し、確定側は `result_prefix` で戻す
- 次世代の `last_action` は深さ `t+1` の新しいcandidate Actionであり、解放した親Actionではない

`confirm_and_free()` は endpoint traceのActionをコピーしてから `gblock[d]` をclearする。
従って構造上は安全だが、ActionとStateには既存の早期解放と同じ寿命契約が必要である。

- `State` と子Actionは、親Actionのaddressやreferenceを `enumerate_actions()` 終了後まで保持しない
- `apply_op()` 済みActionのobject addressをStateが保持せず、rollback引数またはState自身の値だけで戻せる
- Actionのcopyとdestructor時刻に、探索意味論を変える外部副作用がない

Bではdead IDを残したままslabが即座に次generationへ再利用され得るため、これらを曖昧にしてはならない。
Actionがこの契約を満たせば、探索順、`try_op()`、`apply_op()`、`rollback()` の呼出し列は変わらない。

### コストと推奨順

Aは既存loop中の最大値更新条件を変えるだけで、追加配列もscanもない。entry jumpが大きい世代と `C=1` で有効なので、
初回未使用prefix除去と組み合わせる低risk候補である。

Bは選択後にsurvivorの最小parent ordinalを `O(W)` で求め、`next_leaf[q..e]` を最大 `O(C)` scanする。
解放が早まるほどlive Actionとheap資源は減るが、`vector` capacityはslab poolへ移るためRSSが直ちに減るとは限らない。
全格納ID有効という単純な不変条件も失う。標準backendではまずAを試し、BはActionが重い場合のpolicyとして測るのがよい。

## survivor hull slice と endpoint relocation

### 案の定式化

前節と同じく深さ `t` のcurrent葉を `V[0], ..., V[C-1]`、現在のState endpointを `e=C-1` とする。
深さ `t+1` のsurvivor candidateが参照するparent ordinal集合を `P` とし、次を置く。

```text
q = min(P)
p = max(P)
e = C - 1
```

次世代が実際に展開される場合、現行のsort後reverse走査で最初に使うparentは必ず `p` である。
そこで世代末にStateとtraceを `V[e]` から `V[p]` へ移し、`V[q..p]` だけを残す。

現行のfuture tourを `T=next_tour`、境界を `F=next_leaf` と書くと、論理sliceは次になる。

```text
T'             = T[F[q] .. F[p])
F'[i-q]        = F[i] - F[q]               for i in [q, p]
parent_leaf'   = parent_leaf - q
endpoint'      = p - q = F'.size() - 1
```

内部のdead葉は残し、`q` より左と `p` より右のdead葉だけを落とす。これはsurvivor集合そのものではなく、
ordered leaf列上の最小包含区間、すなわちsurvivor hullのsliceである。

### relocation は次世代最初のparent遷移と同じ State 操作になる

`V[e]` から `V[p]` までのrollback距離を次で求める。

```text
d_ep = 0                                          if p == e
d_ep = max(F[k+1] - F[k] for k in [p, e))        otherwise
```

relocationは概念上次の順で行う。

```text
rollback trace[t], trace[t-1], ..., trace[t-d_ep+1]
copy_tour_path(T, F, p, e, trace.begin() + t + 1)
apply trace[t-d_ep+1], ..., trace[t]
```

元実装の次loopを `turn=t+1` として読むと、最初のcandidateに対して全く同じparent Action列を
rollback、applyし、その直後に深さ `t+1` のchild Actionをapplyする。relocation後はparentが新endpointなので、
次loopの最初は `lca_dist=0` となりchild Actionだけをapplyする。

従ってState method呼出しだけを射影した列は次のように一致する。

```text
original : rollback(e -> lca), apply(lca -> p), apply(first child), enumerate
relocate : rollback(e -> lca), apply(lca -> p), apply(first child), enumerate
```

追加のrollbackやapplyを行う案ではない。ただし2つのparent applyとchild applyの間に世代末処理が入るため、
完全なプログラムevent列まで同じではない。この差は後述の副作用と動的幅で重要になる。

### slice の半開区間と off-by-one

reverse front codingではinterval `[F[i],F[i+1])` が `V[i]` の隣接LCA直下から葉までを持ち、
最後のendpoint pathはtraceが持つ。relocation後のtraceは `V[p]` なので、必要なintervalは
`i=q, ..., p-1` だけであり、正しい終端は `F[p]` である。`F[p+1]` まで含めてはならない。

境界例は次になる。

- `q=p` なら `T'` は空、`F'={0}`、endpointと全survivor parentは0
- `p=e` ならrelocationは空で、右端を落とす処理もない
- `q=0` でも開始offsetは `F[0]` なので、現行の未使用先頭prefixは同時に落ちる
- `C=1` なら `q=p=e=0` で、空tourと単一境界になる

`parent_leaf -= q` は大小関係と同値関係を保つ。`Candidates::next_beam` の配置順とAction slotを変えずに
remapし、同じcomparatorでsortすれば、親group順、score比較、逆走査順は変わらない。

### 内部dead葉を残しても再構築できる理由

隣接suffix `S[i]=T[F[i]..F[i+1])` は、global endpointがどこかには依存しない。
従って `S[q] || ... || S[p-1]` と、新endpoint `V[p]` のtraceだけで `V[q..p]` のtrieを表せる。

次世代のtarget parentはremap後も単調非増加で、`p-q` から始まってsurvivorだけを訪れる。
gapにあるdead葉はLCA range scanの中に現れるが、`copy_tour_path()` が必要なprefixを選ぶため、
dead葉自体をState endpointとして展開することはない。その次のtourにはsurvivor childだけが入るので、
内部dead葉も1世代後には自然に消える。

これは選択後にtourを一から再構築する遅延案と異なり、full `next_tour` と終了endpoint traceが既にある。
開始endpoint pathを失う問題はなく、単なる既存表現の部分列である。

### relocation後はsurvivor parentのLCPまで確定できる

relocation後のState endpoint `V[p]` 自身がsurvivor parentなので、確認すべき集合は `P` だけになる。
slice内の最大隣接rollback距離と共通prefix depthは次である。

```text
D_P = 0                                          if q == p
D_P = max(F[k+1] - F[k] for k in [q, p))        otherwise
h_P = t - D_P
confirm_and_free(h_P + 1)
```

現行のDFS順では `V[q]` と `V[p]` の間の全葉が同じprefix部分木にあり、内部dead葉はLCPを浅くしない。
従って `h_P` はsurvivor parent集合の正確なLCPである。順序がDFSでない場合もrange max式は
実際のtraversalが跨ぐ最浅点まで保守的に下がるが、両端だけの明示LCAで代用すると危険である。

slice後の全intervalは最浅でもdepth `h_P+1` から始まり、traceも `V[p]` にある。
従ってこの案では前節Bと違い、切り出したtour内に解放済みgenerationのdangling IDを残さずに済む。

`q=p` ならparent Actionの深さ `t` まで確定できる。survivor candidateが1個だけでも、そのchildはまだStateへ
applyされていないため、深さ `t+1` まで確定することはできない。childまで確定するには別のState移動が必要である。

### found、no-candidate、max-turn

適用条件は、`found_finished==false`、`candidates.size()>0`、かつ深さ `t+1` のfrontierを
次loopで実際に展開することである。

- finishedなら既存どおり先にreturnし、best path構築後に不要なrelocationを加えない
- no-candidateなら `P` が空で `q,p` を定義できず、そのままreturnする
- 次loopがあるなら、そこでfinishedまたはno-candidateになっても最初のparent遷移は元々必要なので前倒しは同じworkである
- max-turn到達直前の世代では次loopのentryが存在しないため、relocationとsliceを両方行わない

max-turnでsliceだけを行うと、traceは旧endpoint `V[e]` のままなのに表現endpointを `V[p]` とするため壊れる。
relocationも行う場合、`materialize_final_state=false` では本来なかったState操作を追加し、trueでは
`build_final_state()` のrollback列を `V[e]` 起点から `V[p]` 起点へ変える。従ってmaterialize指定にかかわらず、
最終世代では既存表現を保つのが同一呼出し列の条件である。

最終世代でも前節Bの `P union {e}` によるprefix確定だけなら安全だが、このhull slice案とは分けて扱う。

### 動的幅と世代境界のevent順

固定幅かつ決定的Stateでは、親ordinalのaffine remapにより候補比較と探索順を同じにできる。
一方、単純に前世代の `param.timestamp()` より前へrelocationを移すと、次が変わる。

- relocation時間が次世代ではなく前世代の `time_sum` に入る
- 次の `get_beam_width()` より前にwall timeを消費する
- slice後の `tour.size()` がtelemetryへ入る

現在の `get_beam_width()` は `pool_size_sum` を使わないためtour size差自体は幅へ影響しないが、elapsed timeと
`time_sum` の差で時間調整型の幅は変わり得る。従って素朴な世代末実装は動的幅でbit-exactではない。

parent遷移だけの計測位置を揃えるなら、旧世代のtimestamp後、次世代の `get_beam_width()` とreset後、
最初のchild apply前にrelocationするphaseへ置ける。これは元実装のparent遷移位置とほぼ同じで、
next-generation timerへ計上できる。ただしslice/remapという新規仕事の時間差まで消すことはできない。

### Action副作用と厳密同値性

relocationで呼ぶState methodとAction引数の順は、元の最初のentryからparent部分を抜き出したものと一致する。
それでも次の型では観測結果が変わり得る。

- `apply_op()` や `rollback()` がState以外のglobal RNG、時刻、I/Oを観測または変更する
- State methodが呼出し間のwall timeや別threadの状態に依存する
- Actionのcopy、move、destructorが外部状態を変更する
- Stateまたはchild Actionが親Actionのaddressを呼出し後まで保持する

特にsurvivor LCP確定でAction blockのdestructor時点が1世代早まる。通常の差分State契約では、State methodは
渡されたAction値だけで決定的にStateを更新し、Actionは外部副作用のないself-containedな値である必要がある。
この契約下では探索上のState呼出し列を保てるが、任意のC++副作用まで含む一般型には同値ではない。

### 帯域と実装上の評価

この案はfull `next_tour` を既に構築した後に切るため、その世代の不要枝writeは減らさない。
relocationのState操作と `[p,e)` のinterval scanは、元の次世代最初のentryを移しただけで総数は同じである。
一方、早期confirm用の `[q,p)` range maxは、次世代の内部transitionでも同じrangeを走査するため追加readになる。
物理sliceやleaf正規化を行うなら、そのloopへ最大値計算を融合できるが、追加の線形処理自体は残る。

物理的なcontiguous sliceを新vectorへコピーすれば `Theta(F[p]-F[q])` のread/writeが増える。
同じvectorの先頭へ詰めても同量のmoveが必要で、`resize()` や `erase()` だけではcapacityとRSSは減らない。

低帯域にするなら `tour_base=F[q]` と論理終端 `F[p]` を持つspan、またはchunk viewとしてsliceし、
leaf offsetだけを正規化する方法がある。この場合は即時のcapacity削減はなく、copy_tour_pathにbase加算が増える。

利点の中心は次になる。

- endpointを外したsurvivor-only LCPまでgeneration blockを早く解放できる
- outer dead葉を次世代の論理表現から除き、全格納ID有効の不変条件を保てる
- slab再利用と、Action自身が所有するheap資源の早期解放ができる

`P` が少なくても `q=0,p=e` ならsliceは全体のままで、relocationもない。従って `|P| << C` だけでは速くならず、
survivor ordinalのhull幅 `p-q+1` と、確定が深くなる量を計測する必要がある。

速度目的の既定案としてはAのentry除外より複雑である。重いActionの寿命短縮またはtourの論理memory上限が重要な
backendとして、固定幅oracleでState call trace、候補digest、最終Action列を比較してから評価するのがよい。

## current / next tour buffer の再利用

### 単純な同一vector上書きが壊れる理由

現在の `copy_tour_path()` は `li` を減らしながら、旧 tour の高い leaf interval から低い intervalへ消費する。
一方、`next_tour` は論理index 0から末尾へ増える。旧vectorの先頭へそのまま新出力を書くと、まだ読んでいない
低index intervalを上書きする。

最小形として旧葉を2個、`leaf={0,r}` とし、current葉の処理順を親index `1, 1, 0` とする。
初回prefixを除くと最初の葉では出力も旧tour readもない。2個目は同じ親なので旧tourを読まず、1個目の
current ActionIdを新tourのindex 0へ出力する。その後、親0へ移る `copy_tour_path(0,1)` は旧tourの
`[0,r)` を必要とするが、先頭要素は既に上書きされている。

高index側から逆向きに書く方法も一般には安全でない。同じ兄弟列では旧tourの高index intervalもまだ未消費であり、
出力をtailへ置くと今度はそこを上書きする。さらに物理的に逆向きへ置いたsegment列は、次世代が期待する
論理的な suffix 順と逆になる。

### 単一bufferに必要な生存量条件

ある時点 `j` で、今後本当に読む旧tour要素数を `U[j]`、既に生成して保持すべき新tour要素数を `N[j]`、
利用可能slot数を `M` とする。再計算も外部退避もしない単一bufferには、少なくとも全時点で次が必要である。

```text
U[j] + N[j] <= M
```

旧tourの初期sizeを `T_old`、その時点まで安全に死んだ旧slot数を `F[j]=T_old-U[j]` とすれば、同値に

```text
N[j] <= (M-T_old) + F[j]
```

となる。vector capacityの末尾余白と、二度と読まないと証明できた旧slotだけが新出力に使える。
単にsource iteratorが通過したかではなく、`copy_tour_path()` の将来のrangeに入らないことが必要である。

この容量条件は必要条件にすぎない。連続した次tourを同じ向きで要求すると、空きslotが物理的に分散している場合の
relocationと、移動先が別sourceになるcycleも解決しなければならない。

### 理論上の単一buffer方式

次の追加仕事を許せば、単一arenaへ近づけることは可能である。

1. State走査前にLCA距離だけを先に走査し、`T_new` と各出力segment長を求める
2. current parent集合から、旧tourのどのsource slotが将来必要かを求める
3. sourceからdestinationへの写像を作り、上書きcycleを退避1要素で解く
4. 新current ActionIdを、dropされる旧slotまたはarena末尾余白へ挿入する
5. 最後に新 `leaf` offsetを構築する

これは通常のstreaming copyではなく、in-place compaction / permutationになる。source写像やvisited bitを
`O(T)` 持てばbuffer削減を別metadataで相殺し、bitをActionIdの空き領域へ埋めればID表現の契約が複雑になる。
randomなcycle追跡も、現在の連続 `vector::insert` よりcacheに不利である。

`T_new` を数えるだけなら旧 `leaf` とsorted `cand` の単調走査で可能である。出力ActionIdも旧 `tour`、
世代開始時 `trace`、`cand` から導出できるため、Stateの再走査までは不要である。ただし別のmetadata traceで
`copy_tour_path()` 相当をprepassするか、sourceからdestinationへの写像を保存するか、構築後にmetadataだけを
second passする必要がある。開始時traceを破壊後まで使う方式なら、そのsnapshotも必要になる。

従って「同じcontiguous vectorを1 passで前から上書きする」方式は成立せず、完全in-place方式は理論上可能な条件があっても
高速化の既定候補にはしにくい。

### chunk再利用

tourを固定長chunkの論理列として持てば、旧intervalを消費し終えたchunkをfree listへ戻し、`next_tour` の末尾chunkへ
再利用できる。物理順と論理順を分離できるため、高index側から解放されたchunkを新suffixのappend順へ接続できる。

次世代の `leaf` は論理offsetのまま持ち、`copy_tour_path()` がchunk境界を跨ぐときだけchunk tableを参照する。
peak chunk数は概ね次で抑えられる。

```text
max_j(ceil(U[j]/chunk_size) + ceil(N[j]/chunk_size)) + partial chunk
```

枝を早くdropできる世代では2本のvector capacity合計より小さくなる。一方、旧chunkを長く読めず新出力が先に増える世代では、
peakは `T_old + T_new` に近づく。chunk内部の一部だけが未読だと、そのchunk全体を再利用できない断片化もある。

CPU上の代償は次になる。

- range copyごとのchunk境界branch
- chunk IDからdata pointerへの間接参照
- `leaf` rangeが複数chunkに分かれる場合のloop
- free list操作と、世代をまたいだ物理局所性の低下
- 最後にcontiguousへ戻すなら追加copyと一時領域

従ってchunk版はpeak memory制約向けpolicyとしては成立するが、連続vector版より速いとは予想しにくい。
まず初回prefix除去、32 bit slot-only、parent-slotを測り、double-buffer容量が実際に問題になった場合に検討する。

### 単一bufferが単純に成立する限定条件

- 葉が1個で、初回prefix除去後の `tour` が常に空
- old tourを以後読まないことが選択前に分かり、全出力を作る前に破棄できる
- arena末尾の未使用capacityだけで `T_new` 全体を保持できる
- 全時点で上の生存量条件を満たし、chunk/free-list layoutを許す
- parent-slotやsnapshotからpathを再生成でき、旧tourを先に破棄してsecond passを許す

同じcontiguous layout、同じ1-pass走査、追加metadataなしという3条件を同時に保った一般単一buffer化には反例がある。
現行のping-pong vectorはpeakを使う代わりに、最も単純な連続read/writeと安定したcache特性を得ている。

## survivor parent だけを遅延 materialize する案

### 目的

現行版は深さ `d` の全 `C` 葉を展開しながら、選択結果が分かる前にその全葉用の `next_tour` を作る。
深さ `d+1` の最終候補が参照する distinct parent が `P` 個だけなら、次世代が実際に必要とするのは
その `P` 葉と現在の State endpoint だけである。

遅延案では展開中の `next_tour` 構築を止め、選択後に次を行う。

1. 最終候補の旧 `parent_leaf` から distinct な展開 ordinal を集める
2. 現在 State がいる最後の展開 ordinal `C-1` も一時葉として加える
3. その `K = P` または `P+1` 葉だけの compact `tour / leaf` を作る
4. survivor の旧 ordinal を compact ordinal へ単調に remap する
5. 現行と同じ順番で `finalize_generation()` と `cand` の sort を行う

endpoint が survivor parent なら `K=P`、そうでなければ `K=P+1` である。

### endpoint 一時葉は省略できない

現在の loop 終了時、State は全 `C` 葉のうち最後に展開した葉にいる。この葉を compact 木から消し、
従来どおり `li = leaf.size()-1` とすると、`li` が示す葉と実 State が一致しない。

例えば最後の葉の子がすべて脱落し、遠い部分木の親から出た子だけが生き残る場合、次世代最初の rollback は
survivor parent の Action を現在適用中だと誤認する。これは即座に State を壊す。

旧 ordinal の昇順で survivor parent を並べ、最後に `C-1` を追加すれば、一時 endpoint は compact 葉列の末尾になる。
次世代の最初の遷移だけがそこから最大 survivor ordinal へ移り、その後は通常の単調走査へ戻る。
一時葉を親に持つ候補はないため、その次の `next_tour` には自然に残らない。

幅1でも同じである。

- 唯一の survivor parent が endpoint: compact 葉1個、空 tour
- 唯一の survivor parent が endpoint 以外: survivor と一時 endpoint の葉2個

State を選択直後に survivor endpoint へ移す設計も可能だが、State 操作の呼出時点が世代境界をまたぐ。
副作用、RNG、計測まで現行と揃えるには、一時葉を残す方が安全である。

### 選択後の `tour / leaf / cand` だけでは一般には再構築できない

遅延案には見落としやすい情報寿命がある。世代開始時、旧葉列の最後の endpoint path は `tour` ではなく
`trace` にしか存在しない。世代を走査し終えると `trace` は新しい endpoint path に上書きされている。

旧葉が root 直下の異なる部分木 `A`, `B` にあり、世代開始時は `B`、終了時は `A` に State がある場合を考える。
旧 `tour` は `A` 側から戻る suffix を持つが、旧 endpoint `B` の suffix は開始時 `trace` にしかない。
次の survivor parent が `B` 側でも、終了後の `trace`、旧 `tour / leaf`、現在の末尾 Action だけから
`B` の root path を復元することはできない。`gblock` は Action を持つだけで親関係を持たないためである。

従って full `next_tour` を作らない遅延版は、次のいずれかを追加で行う必要がある。

- 世代開始時の未確定 `trace` を別 buffer へ保存する
- `trace` の各深さが初めて上書きされる直前に、開始時の値だけを lazy に保存する
- parent-slot のような別の ancestry metadata を持つ

full trace snapshot は未確定深さを `H` として毎世代 `O(H)` 個の ID read/write を追加する。
lazy 保存なら、開始 endpoint と全訪問親の LCA より下で初めて上書きされる suffix だけに減らせるが、
最悪は同じ `O(H)` である。何も保存せず「選択後に旧 tour から再構築する」だけでは成立しない。

### snapshot を持つ場合の compact tour 構築

開始時 trace の metadata copy を `rebuild_trace` とし、旧葉 endpoint を `rebuild_li = leaf.size()-1` とする。
必要な current 葉を元の展開 ordinal 昇順に処理する。現行では
`ordinal -> cand[cand.size()-1-ordinal]` で current 葉 descriptor を得られる。

各必要葉について次を行う。

1. descriptor の旧 `parent_leaf` まで、旧 `leaf` を単調に走査して LCA 距離を得る
2. 最初の必要葉以外では、直前葉の `rebuild_trace` suffix を compact tour へ追記する
3. `copy_tour_path()` と current ActionId で `rebuild_trace` をその葉の path に更新する
4. compact leaf 境界を追記し、旧 ordinal から compact ordinal への対応を記録する

必要 ordinal は単調増加し、その descriptor の旧 `parent_leaf` は単調非増加なので、旧 leaf 境界の走査区間は重ならない。
最後に必ず current endpoint を処理するため、`rebuild_trace` と実際の `trace` が一致することを debug assert に使える。

compact tour の先頭 insert も前節の理由で不要であり、最初の leaf 境界は0にできる。

### ordinal remap と探索順

旧 ordinal から compact ordinal への写像は単調増加である。従って、任意の候補対について
`old_parent_a < old_parent_b` と `new_parent_a < new_parent_b` は同値であり、同じ親同士も同じ値へ写る。

候補選択中は旧 ordinal のままにし、選択終了後、`Candidates::next_beam` の Action 配置を変えずに
`parent_leaf` だけを remap する。その後に現行と同じ `finalize_generation()` と comparator を使えば、
Action slot、親 group、親内 score 順、逆向き展開順を維持できる。

完全同点の `std::sort` 順は標準自体が規定しないが、入力列と全比較結果が同じなら同一処理系では通常同じになる。
厳密な仕様にするなら enumeration ordinal が必要であり、これは遅延案とは別の変更である。

### prefix 解放

compact 集合に endpoint を含めれば、次世代最初の移動に必要な全枝が表現へ残る。compact 葉間の LCA 距離は、
旧葉間で飛ばした区間の最大距離を引き継ぐため、次世代の `max_lca_dist` と `confirm_and_free()` は正しく働く。

ただし最初の試作では、現世代の `confirm_and_free()` を現在と同じ `max_lca_dist` で同じ位置に保つべきである。
survivor 集合の共通 prefix を使えばより深く解放できる可能性はあるが、解放時点と Action destructor の順番が変わる。
また compact 再構築は、古い世代 block を slab pool へ移す前に完了させる方が検証しやすい。

### 追加 read/write と損益分岐

先頭の未使用 prefix を除いた現行表現では、endpoint path は `trace`、それ以外の live 辺は `tour` にある。
全 current 葉用の tour 長を `T_all`、survivor parent と endpoint 用を `T_keep` とすると、概算は次になる。

```text
eager:
    trace から next_tour へ T_all 個の ID write

deferred:
    開始 trace の snapshot または lazy-save
    旧 tour から survivor path suffix を rebuild_trace へ再読込
    rebuild_trace から compact tour へ T_keep 個の ID write
    W 個の parent remap と K 個の leaf 境界 write
```

eager は State 遷移中に既に hot な `trace` を出力する。deferred は同じ path の一部を選択後に再度復元する。
従って `P << C` だけでは不十分であり、`T_all - T_keep` が snapshot、再読込、remap より十分大きい必要がある。

特に次は deferred が負ける反例である。

- 全 parent が1個以上の survivor を持ち `P=C`: full writeをほぼ減らせず second passだけが増える
- 全葉が長い共通幹と末端の短い分岐だけを持つ: 共通幹は元々 endpoint trace にあり、eager tourへ書かれない
- 幅1で endpoint が唯一の survivor parent: 省ける tour writeは0だが snapshot/replayを追加する
- survivor が少なくても離れた部分木に散り、`T_keep` が `T_all` に近い
- endpoint が survivor 集合から遠く、一時葉を加えることで大きな枝が残る
- eager の trace copy がL1/L2内で、deferred の旧 tour 再読込がLLC missになる

既に eager `next_tour` を構築した後で compact する方式は正しいが、`T_all` writeを一度払った後に
さらに filter readと `T_keep` writeを追加する。速度の既定案ではなく、次世代 working setをどうしても縮めたい場合の
memory policyに限るべきである。

結論として、survivor-parent遅延版は開始 endpoint traceの保存を加えれば成立する。ただし連続 postorder の利点を残しつつ
不要枝 writeを削れる反面、parent-slot版にはない second passが必要である。`T_keep/T_all` が継続的に小さい workload 用の
独立policyとしては比較価値があるが、標準版の無条件な置換候補ではない。

### 状態遷移回数とメタデータ処理を分けて考える

2葉間の状態を移すには、現在葉から LCA まで rollback し、LCA から次葉まで apply する必要がある。
単一 State と辺単位の API だけを許すなら、この単純路上の各 Action を省略できない。

現行版は親部分木ごとに葉を連続させる DFS 順を維持し、各葉間をその単純路で直接移動する。
従って、少なくとも現在の葉順に対する状態遷移回数は最小である。通常の Euler tour のように毎世代 root へ戻らず、
一方の端の葉から他方の端の葉まで開いた走査にしている点も既に効いている。

ただし、これは `tour` の構築と ActionId のコピーまで最小であることを意味しない。以下の案が減らすのは主に
`X` ではなく、`T` に比例する木表現の再構築である。

## 本命案: parent-slot、順序付き frontier、隣接葉 LCP

### 表現

固定深さでは深さ `d` の親は必ず深さ `d-1` にあるため、親の generation を保存する必要がない。

```text
action[d][slot]       : 現行の gblock[d][slot]
parent[d][slot]       : 深さ d-1 の親 slot、uint32_t
frontier[i]           : 深さ d の葉 slot、現行 cand と同じ DFS 順
adj_lcp[i]            : frontier[i-1] と frontier[i] の LCA の絶対深さ、i >= 1
trace[d]              : 現在の State が乗っている経路の slot
```

候補列挙中には、展開順 `parent_leaf` から実ノード slot への対応だけを一時的に持つ。

```text
visited_slot[parent_leaf] = 展開した cand の action slot
parent[d+1][new_slot] = visited_slot[selected_candidate.parent_leaf]
```

これにより候補選択と `gblock` の配置を現行のまま保ちつつ、親鎖だけを追加できる。

### 新世代の隣接 LCP は線形時間で作れる

深さ `d` の旧 frontier を実際の展開順に並べる。旧 frontier の葉 `i` を親とする新候補には、現行コードと同じ
`parent_leaf = i` が付く。新 frontier も親ごとに連続する順序へ並べる。

新 frontier の隣接する2葉 `a`, `b` の親 ordinal を `p`, `q` とすると、LCP は次のように求められる。

```text
p == q : LCP(a, b) = d
p != q : LCP(a, b) = min(old_adj_lcp[min(p,q)+1 .. max(p,q)])
```

同じ親から出た異なる子の LCA は親自身である。異なる親の場合、DFS 葉順に並んだ2葉の LCA 深さは、
その間にある隣接葉 LCP の最小値になる。文字列を辞書順に並べたときの LCP と同じ性質である。

現行コードは選択候補を `parent_leaf` 昇順に sort し、逆順に展開する。従って、実際の新 frontier 展開順では
親 ordinal が単調非増加になる。異なる親へ移るたびに参照する旧 LCP 区間は互いに重ならないため、
range minimum 用の RMQ は不要であり、旧 `adj_lcp` を右から左へ合計 `O(C)` 回読むだけでよい。
新 LCP の書き込みを含めても世代合計 `O(C + W)` である。

現行の向きを保った概念例は次のようになる。

```text
q[j] = new_frontier[j] の parent_leaf    // q は単調非増加

if q[j-1] == q[j]:
    new_adj_lcp[j] = d
else:
    new_adj_lcp[j] = min(old_adj_lcp[q[j]+1 .. q[j-1]])
```

完全同点に対する `std::sort` の順序は未規定だが、同じ親内では LCP が常に `d` なので LCP 計算には影響しない。
探索結果まで一致させるには、後述のとおり候補列の入力順と comparator を現行と同じにする。

### 世代間の最初の遷移

各世代の走査終了時、State は旧 frontier の展開順で最後の葉 `C-1` にいる。次世代で最初に展開する葉の親を
`q[0]` とすると、最初の遷移だけは深さが異なるため、新 `adj_lcp` ではなく次を使う。

```text
entry_lcp = d                              if q[0] == C-1
entry_lcp = min(old_adj_lcp[q[0]+1 .. C-1]) otherwise
```

この末尾区間と、新 frontier の異なる親間で読む区間も重ならない。従って `entry_lcp` を含めても旧 LCP の
総走査量は `O(C)` のままである。これは現行コードで `f == 0` の最初の候補に対し、`li` から
`cand.back().parent_leaf` まで調べる処理に対応する。

### parent-slot だけで LCA を求める基準案

現在葉と次葉の深さがどちらも `d` の場合、次葉の親鎖を上りながら現在の `trace` と比較できる。

```cpp
int k = d;
uint32_t v = next_slot;
while (trace[k] != v) {
    state.rollback(action[k][trace[k]]);
    trace[k] = v;
    v = parent[k][v];
    --k;
}
for (int j = k + 1; j <= d; ++j) {
    state.apply_op(action[j][trace[j]]);
}
```

各反復は、必ず1回の rollback と将来の1回の apply に対応する。LCA だけを高速に求めてもこれらの状態遷移は
消えないため、binary lifting は不要である。最初の葉だけは State が深さ `d-1` にいる場合があるので、
次葉の末尾 Action を `trace[d]` に積んで親へ1段上ってから同じ loop に入る。これは現行の `f` の特別処理に対応する。

この loop は概念コードであり、確定接頭辞の境界、深さ0、最初の葉を含む実装では sentinel を置いて範囲外参照を防ぐ。

### 隣接 LCP を使う遷移

隣接 LCP があれば、現在葉からどの深さまで戻るかは親鎖を読む前に分かる。まず現在の `trace` を LCP 深さまで
rollback し、その後に次葉側の親鎖だけで suffix を復元する。

```cpp
for (int k = d; k > lcp; --k) {
    state.rollback(action[k][trace[k]]);
}

trace[d] = next_slot;
for (int k = d; k > lcp + 1; --k) {
    trace[k - 1] = parent[k][trace[k]];
}

for (int k = lcp + 1; k <= d; ++k) {
    state.apply_op(action[k][trace[k]]);
}
```

最後の `parent[lcp+1][trace[lcp+1]]` は読まなくてよい。LCP が正しければ、その親は現在の `trace[lcp]` と
一致することが分かっているためである。debug build だけ読み、等しいことを assert できる。

従って距離を `r = d-lcp` とすると、parent-slot だけの基準案は次葉側の親 slot を `r` 回読むのに対し、
隣接 LCP 版は `r-1` 回でよい。同じ親の兄弟間では `r=1` なので親 slot を1回も読まない。
現在葉側はどちらも `trace` から rollback でき、親鎖を上る必要はない。

LCP 版は各遷移の依存ロードを1回減らす代わりに、世代ごとに旧 LCP の連続走査と新 LCP の書き込みを行う。
その量は `O(C+W)` で、既存の `leaf / next_leaf` と同じ程度の32 bit帯域である。従って無条件に速いとはいえないが、
親鎖ロードを連続 LCP 配列へ移せるため、parent-slot を試作するなら有力な既定候補である。

### `tour / next_tour / leaf` を完全に除けるか

除ける。必要な情報は次の3種類で尽きる。

1. 葉をどの順に展開するか: ordered frontier
2. どの深さまで戻るか: `entry_lcp` と `adj_lcp`
3. LCA から次葉へどの Action を適用するか: 世代別 `parent_slot` と `action`

`adj_lcp` だけでは3を復元できず、`parent_slot` だけでは LCA を知るまで親鎖を読む必要がある。両者を組み合わせると、
帰りがけ順配列が担っていた境界情報と下り経路情報を分離して保持できる。

最終解の経路は、選んだ葉 slot から `parent_slot` を確定接頭辞まで辿り、逆順に Action を並べれば復元できる。
探索途中で終了候補を得た場合は、現在の `trace` がその親経路なので現行と同じ方法を使える。

### 現行方式との仕事量の差

| 処理 | 帰りがけ順版 | parent-slot 版 |
|---|---:|---:|
| 状態遷移 | `X` | `X` |
| LCA 用境界走査 | `O(L)` | 旧隣接 LCP の単調走査 `O(C)` |
| 下り経路の ID 復元 | 連続した `tour -> trace` copy | suffix 1辺目以外の親 slot 依存ロード |
| 次世代表現の構築 | `Theta(T)` 個の 64 bit ID copy | 候補ごとの 32 bit parent と LCP write |
| 葉境界 | 2本の vector | 2本の隣接 LCP vector、frontier は `cand` と共有可能 |
| Action 保存 | `gblock` | 同じ `gblock` |

この案の本質は「LCA を速くする」ことではなく、「帰りがけ順を毎世代作り直さない」ことである。

### メモリ

vector の capacity 高水位を単純化すると、主要な追加メタデータは概ね次のようになる。

```text
現行版          : 8 * (capacity(tour) + capacity(next_tour)) + 4 * (leaf 2本) byte
parent-slot 版  : 4 * G + 4 * (capacity(adj_lcp) + capacity(next_adj_lcp))
                   + frontier / trace の小配列
```

frontier は既存の `cand` を逆向きに参照できる。展開 ordinal `p` の実親 slot も
`cand[cand.size()-1-p].action_slot` から得られるため、現行と同様に全 `cand` を必ず展開する契約なら
`visited_slot` を別 vector にしなくてもよい。

`G` には、既に子孫を失ったものの世代 block ごと残っている Action slot も含まれる。生存木が疎で `G >> T` なら
parent-slot の方が多くのメタデータを持ち得る。反対に、多くの世代 slot が現在木に寄与し、2本の `tour` capacity が
大きい場合は parent-slot が小さくなり得る。

親 slot も Action block と同じ世代単位で再利用できる。ただし Action block を `result_prefix` へ移して解放するとき、
以後の親鎖が解放済み block を参照しないよう、確定深さを仮想 root としてそこで必ず走査を止める必要がある。

### キャッシュ特性

帰りがけ順版の強みは、`leaf` と `tour` が連続し、`std::copy` 相当の処理が広いメモリ帯域を使えることである。
parent-slot 版は `parent[d][slot]` を読んでから次のアドレスが決まるため、1本の親鎖内でロードを並列化しにくい。
隣接 LCP を持てば suffix の最上段の親ロードを省けるが、それより深い鎖の依存関係は残る。

ただし現行版も、下り経路の各 Action を適用するときは generation ごとの `gblock` を参照する。
parent-slot 版が新たに増やす依存ロードは親の 32 bit slot であり、その代わり `next_tour` 全体の読み書きを除ける。

従って予想される傾向は次のとおりである。

- `T/C` が大きく State 操作が軽い: parent-slot が勝つ可能性が高まる
- 共通接頭辞が長く `T` が小さい: 帰りがけ順の連続配列が有利になりやすい
- 親 block が L1/L2 に収まる: parent-slot の依存ロードの不利が小さい
- 未確定深さと `G` が大きく親 block が LLC 外へ出る: parent-slot が不利になりやすい
- `try_op()` や `apply_op()` が重い: どちらの構造差も全体時間へ現れにくい

## 探索結果を同じに保つ条件

parent-slot 版は、次を守れば現行版と同じ葉を同じ順に展開できる。

1. `cand` の入力順と `(parent_leaf, score)` の `std::sort` を変えない
2. `cand` を現行と同じ逆順に展開する
3. `parent_leaf` を展開時の ordinal として残し、`visited_slot` から実親 slot へ変換する
4. 候補列挙順、閾値を読む時点、hash 重複時の置換条件を変えない
5. 葉間で現在葉から LCA へ rollback し、LCA から次葉へ apply する順番を変えない
6. `gblock` 内の Action の配置と寿命を変えず、`last_action` に同じ Action を渡す
7. 終了候補を見つけた時点と、履歴用 `node_id` の発行順を変えない

現行の比較関数は `parent_leaf` と `score` が同じ要素の順序を規定しない。異なる backend 間で bit-exact な順序まで
仕様にするなら、列挙 ordinal を最終比較キーへ追加する必要がある。これは現行結果自体を変え得るため、別の仕様変更である。

### 危険な不変条件

- `parent[d][slot] < action[d-1].size()` が常に成立する
- `adj_lcp[i]` が ordered frontier の `i-1`, `i` の LCA 深さと一致する
- 新 frontier の親 ordinal が単調であり、LCP の range-min 走査区間が重ならない
- 最初の遷移だけは `entry_lcp` を使い、深さの異なる旧葉と新葉を同深さとみなさない
- 現在の `trace` と実際の mutable State が常に同じノードを表す
- rollback 前に `trace[k]` を上書きしない
- 親鎖を積む方向と apply する方向を逆にしない
- 幅が世代ごとに変わっても、親 slot は親世代の block に対する index として解釈する
- slab を再利用しても、生きた子から古い内容へ到達しない
- 確定接頭辞の Action を二重に rollback または結果へ二重追加しない
- 最初の候補、候補1個、同じ親の兄弟、親を飛ばす場合、幅1を個別に検証する
- `ActionId` の現在の 24 bit slot 上限を変更する場合、範囲外を silent に切り捨てない

## より小さな変更で試せる案

### 32 bit slot-only 帰りがけ順

`tour` と `leaf` は残し、`trace / tour / next_tour` には generation を含む 64 bit ActionId ではなく、
その Action の世代内 slot だけを保存する。`tour` から `trace[d]` へ復元した後は深さ `d` から `gblock[d]` を選べる。

これにより帰りがけ順配列の帯域をほぼ半減できる。現在も slot は 24 bit に制限される設計なので、32 bit storage は
自然である。範囲検査と「tour entry を必ず trace 上の深さへ復元してから Action を参照する」という不変条件が必要になる。

これは木構造を変えないため、parent-slot より先に比較する価値が高い。

### tiny Action inline 帰りがけ順

`Action` が小さく trivially copyable な場合、`tour / trace` だけでなく current frontier の `CandIdx` にも
Action 自体を置けば、標準版から `gblock` と ActionId を完全に除ける。

世代開始時の Action 所有関係は次のようになる。

```text
tour + trace : current frontier の親が張る履歴木
cand         : 今回展開する frontier の末尾 Action
Candidates   : 列挙中の次世代 Action
```

`tour` の各区間と endpoint `trace` を合わせると、履歴木の各 live edge はちょうど1回現れる。
今回の葉 Action はまだ履歴表現に入れず、`cand` が1個ずつ所有する。

### 世代走査中の寿命

2個目以降の葉へ移るとき、現在の処理順は次である。

1. 現 `trace` suffix の Action で rollback する
2. その suffix を `next_tour` へ移す
3. 旧 `tour` から次親の suffix を `trace` へ復元する
4. `cand.action` を `trace[d]` へ移す
5. `trace` suffix を apply し、`trace[d]` を `last_action` として列挙する

手順1の後、`next_tour` へ移す trace suffix は State から外れており、直後に上書きされる。
また `li` は単調に進むため、`copy_tour_path()` が消費した旧 tour 区間はその世代で再読されない。
従って所有権だけを見ると、Action を旧表現から新表現へ destructive move できる。

最初の葉では前節の未使用 prefix を作らず、`cand.action` を直接 `trace[d]` へ移す。最後の葉の Action は
endpoint trace に残り、それ以外の current 葉 Action は `next_tour` に入る。全 `cand` を1回ずつ展開する限り、
旧 cand を破棄する時点で必要な Action は tour または trace へ移り終えている。

`last_action` は moved-from の `cand.action` ではなく、移動先の `trace[d]` を参照しなければならない。
State が Action のアドレスを呼出し後まで保存することは、vector 再確保もあるため禁止契約になる。

### prefix と返却 path

確定した共通 prefix は `trace` から `result_prefix` へ移せる。以後の葉間遷移は確定境界より上へ戻らないため、
moved-from の trace prefix を再び使う必要はない。先頭の未使用 tour prefixを消しておけば、確定 Action の不要な複製も残らない。

max-turn では、最後に結果を返すため tour と trace を消費できる。一方、探索途中でより良い終了候補を見つけた場合は、
探索を継続したままその時点の path を `best_finished_path` へ保存する必要がある。この経路は現在の tour / trace と
同時に生存するため、Action の値コピーが必要になる。`materialize_final_state` でも、現在 State を rollback する path と
返却 path の両方を一時的に必要とする。

従って安全な既定条件は少なくとも次である。

- Action が小さく、安価な値コピーができる
- copy 後の2値が独立した通常の値として扱える
- destructor と move に特殊な外部副作用がない
- `apply_op()` / `rollback()` 後の Action 内容をコピーしても意味が保たれる

実装上は `sizeof(Action) <= 8` と `is_trivially_copyable_v<Action>` を入口にしつつ、ライブラリ利用者が
inline storage を明示 opt-in する policy が安全である。型特性だけでは、外部 payload の寿命やアドレス依存性までは分からない。

### 4 byte と 8 byte の損益

未解放 generation slot 数を `G`、2本の tour capacity 合計を `T_cap`、未確定深さを `H` とする。
主要領域の概算は次になる。

```text
現行 ID 版:
    G * sizeof(Action) + 8 * T_cap + 8 * H + CandIdx の 8 byte ActionId

inline 版:
    sizeof(Action) * T_cap + sizeof(Action) * H + cand ごとの inline Action
```

両方式とも `Candidates` 内には次世代 Action 用領域があるため、比較式から省いている。

`sizeof(Action) == 4` なら tour / trace の帯域が64 bit ID版の半分になり、`G * 4` byte の Action blockも消える。
`act(id)` の shift、mask、generation vector、Action 配列という間接参照もなくなる。

`sizeof(Action) == 8` では tour / trace の copy byte は同じだが、`G * 8` byte と Action lookupを除ける。
`CandIdx` の sort は ActionId の代わりに同じ8 byte Actionを動かすので、trivial Actionなら大きな悪化はない。

`sizeof(Action) > 8` では tour 帯域が増える。特に深い分岐木では同じ Action値を世代間の帰りがけ順へ何度も移すため、
canonical Actionを1個だけ `gblock` に置くID版が有利になりやすい。

inline版は dead generation slotを残さず、現在の誘導木にあるActionだけを tour / traceへ保持する。
ただし double buffer の両capacityは確保されたままなので、peak allocationは
`sizeof(Action) * (capacity(tour) + capacity(next_tour))` を含む。非trivial Actionをmoveした後も、
moved-from objectのstorage自体はclearまで残る。

### move-only / nontrivial Action が一般版にならない反例

`Action` が `unique_ptr<UndoRecord>` を所有する move-only 型だとする。単調 tour 消費だけなら ownership transfer は可能に見えるが、
探索途中の `best_finished_path` を保存するには、探索木で引き続き rollback に使う同じ Action を複製できない。
所有権を結果へ移すと継続探索が壊れ、移さないと返却 path を作れない。

nontrivial copyable Actionでも、copyが rollback log のdeep copyを行うなら、`next_tour` と `copy_tour_path()` のhot pathで
constructor / destructor費用が64 bit ID copyを大きく上回り得る。self pointerや外部arena内addressを持つActionでは、
vector move後の意味も追加契約なしには保証できない。

現在の標準版自体も `best_finished_path`、`Candidates::get_best()`、一部の `push()` 経路でAction copyを要求する。
従って move-only対応をinline版の利点として扱うべきではない。move-only専用版を作るなら、終了pathをActionではなく
安定node IDで保持し、探索終了後に一度だけmaterializeする別API設計が必要になる。

この案はtraPの記事が `Node` を最小にすることを強調している方向と一致する。汎用ライブラリではID版を既定に残し、
`InlineTinyAction` storage policyまたは別backendとして比較するのが妥当である。

### flat 32 bit Action handle

世代と slot の組ではなく、再利用可能な flat arena の 32 bit index を Action handle にすれば、`act()` の
vector-of-vectors 間接参照も減らせる。prefix 解放後の範囲再利用、arena 再確保、動的ビーム幅を安全に扱う必要がある。
固定 stride の ring arena は速いが、最大幅と未確定深さに強い上限を要求する。

## postorder successor や chunked tour

### node ごとの successor

各 live node に postorder の次 node を指す successor を持たせれば、子の挿入と死んだ枝の unlink によって
`next_tour` の全再構築を避けられる可能性がある。しかし successor だけでは、ある葉から次の葉へ下る Action 列を
復元できない。結局、親 pointer、深さ、子または葉順の情報が必要になる。

また、走査は連続 vector から node pool の pointer chasing に変わる。二重連鎖木版より帰りがけ順配列が速かったという
[eijirou の追記](https://eijirou-kyopro.hatenablog.com/entry/2024/02/01/115639)と同じ不利を再導入する。
従って、successor 単体より、子・兄弟 pointer を不要にした
`parent_slot + ordered frontier + adjacent LCP` の方を先に試すべきである。

### chunked / rope 形式

帰りがけ順を固定長 chunk に分け、変更部分だけ copy-on-write する案なら、長い連続走査と部分更新を折衷できる。
ただし frontier は毎世代すべて1段伸び、多数の chunk に変更が散る。chunk 境界、参照数、断片化、定期 rebuild の費用が増え、
単純な2本の vector より速くなる条件は狭い。parent-slot の測定後に検討する案である。

## State 契約を広げる構造

### 部分木 checkpoint

単一 State という制約を外し、DFS frontier を複数の連続区間に分けて区間根の State snapshot を保持すると、
区間間の rollback と apply を State copy に置き換えられる。葉ごとに State を持つ極端が `naive_beam_search.cpp` である。

State copy が全履歴コピーより安く、差分操作が高価な中間領域では、部分木 checkpoint が標準版と naive 版の両方より
速い可能性がある。一方、State が RNG、外部参照、アドレス依存データを持つ場合は copy の意味を明示する必要がある。

### Action 合成または path API

実際の状態遷移回数 `X` を減らすには、複数 Action を1回で処理する能力が必要である。

- `Action::compose()` が安く完全なら、既存の Compose / Radix backend を使う
- `State::apply_path(span<Action>)` と `rollback_path(span<Action>)` を optional hook にし、State 側で batch 更新する
- 十分に小さい State なら、葉 State を直接保持する naive backend を使う

これらは木メタデータだけの変更ではなく、Action または State の契約を広げる。

## 比較試作の順序

1. 現行版へ計測だけ追加し、`T`, `X`, `G`, leaf 境界読出し数、ID copy byte を世代ごとに記録する
2. 初回の未使用 tour prefix を除き、探索結果と pathを回帰比較する
3. 32 bit slot-only 版を作り、木構造を変えずに ID 幅の効果を分離する
4. 64 bit ActionId のまま parent-chain 版を作り、`tour` 除去の効果を分離する
5. 親鎖比較版と adjacent-LCP 版を分け、親 load、LCP scan、cycle を比較する
6. parent slot と trace を 32 bit 化する
7. survivor-parent遅延版は `T_keep/T_all` と endpoint snapshot量を計測してから比較する
8. tiny Action inline policy と checkpoint hybrid を型・State コスト別に比較する

比較時は wall time だけでなく、cycle、instruction、branch miss、L1/LLC miss、memory bandwidth、peak RSS を取る。
合成 State では少なくとも次の軸を独立に振る。

- Action サイズ: 4 / 8 / 32 / 128 byte
- `apply_op()` と `rollback()` のコスト
- `try_op()` のコスト
- ビーム幅と深さ
- 共有接頭辞の長さ
- 1世代で生存子を持つ親の割合
- 動的ビーム幅

候補の score、hash、親 ordinal、Action 列を世代ごとに比較し、構造差だけの試作では同一性を確認する。

## 現時点の優先順位

| 優先度 | 案 | 判断 |
|---:|---|---|
| 1 | 初回の未使用 tour prefix を除去 | 最小変更で、世代ごとに未読 ID write を確実に消せる |
| 2 | entryを除外したprefix確定 | 追加scanなし。1葉ではcurrent Actionも即時解放できる |
| 3 | 32 bit slot-only 帰りがけ順 | 低リスクで、現行の最大の連続 ID 配列を半分にできる |
| 4 | parent-slot + ordered frontier + adjacent LCP | `next_tour` を除く本命。勝敗は木形状と cache に依存 |
| 5 | tiny Action inline + no gblock | 4 / 8 byteの値型で有力。明示storage policyが必要 |
| 6 | survivor + endpoint基準のprefix確定 | Actionが重い場合向け。追加scanとdangling ID管理が必要 |
| 7 | survivor hull slice + endpoint relocation | dangling IDを避けるが、slice帯域と世代境界管理が増える |
| 8 | survivor-parent delayed tour | `T_keep/T_all` が継続的に小さい場合だけ比較価値がある |
| 9 | flat Action arena | lookup は軽くなるが、寿命と再利用の設計難度が高い |
| 10 | chunked tour / successor tree | 複雑さと pointer chasing の割に優位条件が狭い |
| 非推奨 | binary lifting / RMQ の追加だけ | 状態遷移を減らさず、前処理とメモリが増える |

この順位はコード監査から得た仮説であり、速度測定の結論ではない。
[Rafbill の比較実装](https://gitlab.com/rafaelbocquet-cpcontests/euler-tour-beam-search/-/raw/main/README.md)でも、
小さい State では履歴方式や単純方式が Euler tour より速い条件が示されており、単一の木表現を万能とみなすべきではない。
