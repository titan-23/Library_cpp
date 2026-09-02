# 通常版 beam search の調査判断

## 対象

対象は `titan_cpplib/ahc/beam_search/beam_search.cpp` と同じ固定深さ、単一 `State`、辺単位の
`apply_op()` / `rollback()` 契約である。`beam_search_state.cpp`、`beam_search_state_turn.cpp`、`old/` は対象外とする。

調査時の旧 `beam_search.cpp` は2026-09-02に `test/ahc/beam_search_baseline.cpp` へ移し、定数倍改善版を
現行の `beam_search.cpp` へ昇格した。以下で旧実装を扱う箇所は調査時点の基準版を指す。

調査は次の二系統へ分ける。

1. 現在の帰りがけ順方式を保った定数倍改善
2. 帰りがけ順を毎世代再構築しない新しい履歴構造

調査時の旧基準版も現行標準版も、明示木や完全 Euler tourではなくtraPの記事にある帰りがけ順方式である。
従って、比較対象は「明示木に LCA を足すこと」ではなく、帰りがけ順より後の表現である。

## 現時点の判断

### 状態遷移と木 metadata は別問題である

展開対象葉の誘導木を `H_tree`、辺数を `E`、State walk の始点と終点を `s`, `t` とする。単一 State を辺単位で
動かす限り、必要な状態操作数には次の下限がある。

```text
X >= 2E - dist(s, t)
```

通常版の固定深さ、DFS-compatible frontier、旧 frontier の終了 endpoint から始める条件では、現在の単調な葉順が
次を満たす。

```text
X = 2E - dist(s, last_target)
dist(s, last_target) = max_v dist(s, v)
```

従って固定した開始点と終了点だけでなく、開始点を固定して終了 target を自由に選ぶ場合の下限も厳密に達成する。
開始点まで自由にした `2E-diameter(H_tree)` を常に達成するという主張ではない。

一方、`tour` の再構築は状態操作ではない。初回の未参照 prefix を除くと、現在の `tour + endpoint trace` は
active trie の各辺 handle を1回ずつ持つ reverse front coding になるが、それを毎世代別 bufferへ書き直すことは
情報下限ではない。

従って結論は次になる。

- 一般 LCA、RMQ、binary liftingだけでは `X` を減らせない
- 現在の帰りがけ順方式が CPU cycleでも最速だという意味ではない
- 新構造が狙うべき対象は `next_tour` write、Action handle幅、path復元、cache missである

## 系統1: 現方式の定数倍改善

### 実装候補

| 候補 | 正しさ | 主な効果 | 判断 |
|---|---|---|---|
| 初回未参照 prefixを省く | 証明済み | 世代ごとに `first_lca_dist+1` handle writeを削除 | 実装済み |
| 32 bit世代内slot | 証明済み | tour、trace、candのhandle byteを半減 | 実装済み |
| entryをprefix最大値から外す | 証明済み | generation blockを早く解放 | 実装済み |
| `C=1` のprefix確定 | 証明済み | 唯一pathをその世代で確定 | 実装済み |
| Actionをmove constructionする | 値型契約で成立 | default constructionとmove assignmentを削除 | 実装済み |
| 確定prefixのActionをmoveする | 値型契約で成立 | 確定時のcopyをmoveへ置換 | 実装済み |
| `push_lazy()`を使う | 値型契約で成立 | 棄却Actionのcopyを削除 | 実装済み |
| loop開始時のtimer readを共有 | 成立 | 通常経路を3回から2回へ削減 | 実装済み |
| 未使用RNGを除く | 成立 | search初期化の固定費を削減 | 実装済み |
| record-high fused scan | 証明済み | leaf境界を `2K` から `K+J` へ減らす | 計測後に選ぶ |
| survivor + endpoint prefix | 証明済み | dead Actionをさらに早く解放 | 重いAction用policy |
| endpoint relocation + hull slice | 条件付きで成立 | survivorだけのprefixへ進める | 後回し |
| tiny Action inline | trait付きで成立 | gblock lookupとdead payloadを削減 | 別storage policy |

初回 prefixは `[0, leaf[0])` の全要素が到達不能である。最初の葉では何も出力せず `next_leaf[0]=0` とすれば、
全ての境界差と後続segmentは変わらない。`W=1` では tourが空になるのが正しい。

旧基準版の64 bit `ActionId` は generationを上位bitへ持つが、tour内のhandleを解釈する論理深さは常に分かる。
従って `uint32_t slot` と深さから `gblock[depth][slot]` を引ける。これは構造を変えず、依存loadも増やさない。

prefix確定では、世代開始 endpointから最初のcurrent葉へのentry jumpを、展開後frontierの共通prefixへ含める
必要はない。current葉が2個以上なら2個目以降の内部LCA距離だけを使い、1個ならcurrent葉自身まで確定できる。

survivor parentだけへの遅延 tour構築は低リスク案ではない。世代開始 endpointのpathは開始時traceにしかなく、
走査後には上書きされる。開始traceのsnapshot、上書き前のlazy保存、またはparent metadataなしには再構築できない。

`push_lazy()` は callback と vector fallback で所有権が異なる。callback の `Action&` は列挙側が再利用できるため、
採用時にもcopyする。vector fallbackだけを採用時moveにする。callbackをmoveする実装は探索結果を変える。

timerは世代全体を1 readにするのではなく、loop開始直後の値を残時間計算にも使い、通常経路を3 readから2 readへ減らす。
動的幅では観測時刻が変わるため、固定幅の完全同値と記録済み幅列のreplayを先に検証する。

### 現行標準版へ昇格した定数倍改善

調査中は `beam_search_optimized.cpp` として別実装し、外部interfaceを `BeamSearchWithTree` に合わせた。
検証後はこの実装を標準の `beam_search.cpp` へ昇格し、旧実装をtest用baselineとして残した。

相互作用が小さく正しさを確認できた次を実装した。

1. 未参照 prefix削除
2. 32 bit slot-only
3. entryを除いたprefix確定と `C=1`
4. Actionのmove construction、確定prefixへのmove、lazy保存
5. 重複timer readと未使用RNGの除去

record-high scan、survivor prefix、inline Action、候補selector刷新は個別に測れるよう後段へ分ける。

## 系統2: 新しい履歴構造

### 第1候補は direct parent + adjacent LCPである

各世代に次を持つ。

```text
action[d][slot]
parent[d][slot]
frontier ordinal -> depth d のslot
adj_lcp[i] = frontier[i-1] と frontier[i] のLCP絶対深さ
trace[d] = 現在State pathのslot
```

新しい子を親ごとに連続させると、隣接する子のLCPは次になる。

```text
同じ親       : 親の深さ d
異なる親 p,q : old_adj_lcp の区間最小値
```

親 ordinalは一方向へ進むため、全区間は重ならず、LCP構築は世代合計 `O(W)` である。RMQは不要である。

LCP深さを `h`、target深さを `d` とすると、遷移は次になる。

```text
trace[d..h+1] を rollback
target slotからparentを辿り、trace[d..h+1] を復元
trace[h+1..d] を apply
```

`parent[h+1]` は読まなくてもLCP nodeと一致すると分かる。suffix長を `r=d-h` とすると、parent loadは
`r-1` 回であり、同じ親の兄弟間では0回になる。

この構造が変えるのは LCA 計算ではなく、次世代の平坦 scheduleを毎回作らない点である。

```text
slot-only postorder write       : 4M_tour + 4W byte
direct parent oracle write      : 12W byte
frontier導出後のparent write   : 8W byte
```

`M_tour` は未参照の先頭segmentを除いた次世代streamのslot数である。最初のoracleはparent、独立frontier、LCPの
3配列を持つため、固定幅でwriteが減る境界は `M_tour>2W` になる。
`cand` からfrontierを導出する後続版だけは `M_tour>W` になる。
一方、postorderの連続readは、深さ方向のdependent parent loadへ変わる。

entryの親距離を `e` とすると、4 backendのState操作数とdirect parentの依存load数は次になる。

```text
X = 2e + 1 + 2M_tour
L_parent = e + M_tour - (W-1)
```

全targetが同じ親なら `M_tour=W-1` で、oracle版direct parentにwrite上の利点はない。全targetの親が異なっても
隣接親のLCAが祖父なら `M_tour=2(W-1)` なので、slot-onlyの `12W-8` byteとoracleの `12W` byteはほぼ同量になる。

### 成立するが初回実装に入れない変種

| 方式 | 得るもの | 失うもの | 判断 |
|---|---|---|---|
| windowed parent decode | 複数葉chainでMLPを作る | scratch read/writeと制御 | direct版の次 |
| unary parent map | 約 `W_prev+W_cur` bit | rank/select命令 | memory版候補 |
| Elias--Fano parent | width急減時も圧縮 | high/low decode | memory版候補 |
| path-chunk radix rope | unary pathの連続readとdead payload回収 | header、refcount、COW | 長いunary用 |
| 双方向anchor overlay | parent依存をB段に制限 | base 2本とflatten peak | 両極計測後 |
| refcount parent arena | `G*S_A` をlive量へ近づける | free listと更新write | 大Action用 |

圧縮 parent mapは、親ごとの子数を `1^count 0` と書けば、幅が同程度のとき約2 bit/nodeになる。
これは親関係の情報量下限に近い。ただし各深さのselect結果が次深さのqueryになるため、速度版とは限らない。

windowed decodeは、次の4、8、16葉のmetadata chainだけを先にround-robinで進める。State操作は従来順に後から
実行するため探索順を変えない。LLC missを複数重ねられるときだけ有効で、兄弟中心ではscratchが純増する。

anchor overlayは片方向baseでは成立しない。snake順のanchor方向が世代ごとに反転するため、forwardとreverseの
front codingを両方持つ必要がある。平均writeは概ね `qW+lW+2qM/B` だが、steady memoryはbase 2本、flatten時は
旧baseと新baseを同時に持つ。direct parentとslot-only postorderの間に実測上の空白がある場合だけ試す。

### 実装済みのdirect parent oracle

`beam_search_parent.cpp` を新設し、外部 interfaceを現在の `BeamSearchWithTree` と合わせる。

正しさoracleとして次の単純な構成を実装した。

- `uint32_t` direct parent
- `uint32_t` slot
- absolute-depth `uint32_t` adjacent LCP
- selector slotとfrontier ordinalを分ける独立 `frontier_slot`
- parent metadataとAction payloadを分けるSoA
- 現在と同じ候補selector、親group順、score順、State呼出し順

unary、Elias--Fano、windowed decode、anchor overlayは、このdirect版との結果一致とprofileを得てから追加する。

### frontier導出版の構造ablation

`beam_search_parent_compact.cpp` は、独立した `frontier_slot` を持たず、確定済みの `cand` から次を導出する。

```text
frontier_slot[j] = cand[C-1-j].action_slot
```

旧 `cand` はcurrent frontierそのものなので、次世代候補で上書きする前に全childのparent slotを確定する。
これによりparent oracleと同じState walkと親表現を保ちながら、1世代のtopology writeを `12W` から `8W` byteへ、
固定幅の定常resident topologyを `4G+16W+4K` から `4G+8W+4K` byteへ減らす。

この版も深さ方向の `L_parent` 回の依存loadと `4G` byteのparent blockを残す。
従ってoracle固有のfrontier配列を除くablationであり、parent方式が常に速いことを示す設計ではない。

### 4 backend実測後の判断

CPU 0固定、warmup 2、7反復の15ケースでは、現行標準版は旧基準版に対して0.661倍から0.995倍だった。
一方、parent oracleとparent compactは現行標準版に対して多くのケースで遅く、3%を超えて短かったのは
compact版の親入替ケースだけだった。compact/oracleは0.959倍から1.013倍で、独立frontier metadataの削除による
汎用的なCPU時間短縮は確認できなかった。

従って、今回の測定範囲では現行標準版を通常利用し、2種類のparent版は構造比較と特定の木形状をprofileするための
backendとして残す。これは1台、1 compiler、合成15ケースの結果であり、現行標準版を全環境で最速とする主張ではない。
3%未満または1 ms未満の差は勝敗判定に使わない。
詳細値は `benchmark_results.md`、topology kernelの分離測定は `topology_microbench_results.md` を参照する。

## 無条件に新構造へ置換しない理由

初回 prefix削除後のcurrent表現は、endpoint traceを含めるとactive trieの各辺handleをちょうど1回持つ。
つまりcurrentの弱点は論理要素数の無駄ではなく、その列を毎世代書き直すことである。

parent方式はwriteを減らす代わりに、次のslotが前のload結果に依存する。典型的な勝敗は次になる。

| workload | 有利と予想する方式 |
|---|---|
| `M_tour/W` が小さく全metadataがcache内 | slot-only postorder |
| `M_tour/W` が大きくparentがcache内 | direct parent |
| direct parentだけLLCを超える | succinct parentまたはwindow decode |
| 長いunary chain、Action合成可能 | Compose / Radix |
| State copyがAction往復より安い | naive |
| `G/E_live` とAction payloadが大きい | refcount arenaまたはpath-chunk |

従って汎用ライブラリでは既存方式を消さず、backendを明示選択できる形にする。`sizeof(State)` や
`sizeof(Action)` だけによる自動選択は行わない。

定常resident topologyの概算はslot-onlyが `8M_tour+8W+4K`、oracle版parentが `4G+16W+4K` byteになる。
oracle版parentが小さい条件は `G+2W<2M_tour` である。
`G>>E_live` ではparent metadataもdead Action payloadも残るため、
direct parentをmemory対策として選ばない。この領域はrefcount parent arenaまたはpath chunkの対象になる。

## 検証結果と追加実装のゲート

### 同値性

固定幅で次を世代ごとに比較する。

- candidateの `(score, hash, parent ordinal)` multisetと順序
- `try_op()`、`apply_op()`、`rollback()`、`enumerate_actions()` の呼出し列
- 各葉到着時のState checksum
- prefix Action列と最終Action列
- status、score、turns、final State
- record historyのsurvivor集合

完全同点に対する `std::sort` の順は仕様化されていない。既存と同じ入力列とcomparatorを保つ互換試験に加え、
将来はenumeration ordinalを明示tie keyにするモードを別に検討する。

callback列挙の同値試験には、1個のActionを更新しながら繰り返しsubmitするcaseを必ず含める。callbackは採用時copy、
vector fallbackは採用時moveとし、全棄却、一部置換、history on/offを分けて検証する。

動的幅は高速化で時刻が変わるため、まず幅列をrecord/replayして構造だけを比較する。その後、同じ時間予算で
score分布とturn数を測る。

### 必須edge case

- `W=1`
- root直下だけの浅い木
- 全候補が同じ親の兄弟
- 毎回異なる親
- 最初のtarget parentが旧endpointと異なる
- 旧endpointの子が全て脱落する
- survivor parentが1個だけ残る
- beam幅が増減する
- 候補0、finished、max turn
- `materialize_final_state` のon/off
- callback列挙とvector fallback
- `record_history` のon/off
- 同score、同hash、hash衝突を模した入力

## benchmarkの判定軸

State処理を含む統合時間だけでは原因を分離できないため、二層で測る。

1. 親列と木形状を固定した topology kernel
2. 候補選択と合成 Stateを含む end-to-end search

最低限記録する量は次である。

- `W`, 分岐数、深さ、`M_tour/W`, `G/E_live`, survivor parent比
- entry距離、内部LCP距離、未参照prefix長
- tour read/write byte、parent write、parent load
- dependent loadのL1/L2/LLC miss
- Actionのconstruct/copy/move/destruct
- Stateのapply/rollback/try回数
- cycles、instructions、branch miss、peak RSS

`BeamParam::pool_size_sum` は公開されているため、互換tour長と物理metadata量を混同しない。初回実装では既存が記録した
はずの論理tour長を `timestamp()` へ渡し、backend固有の物理byteはbenchmark counterへ分離する。

合成形状は兄弟集中、浅い分岐、comb、安定した複数枝、毎世代の大幅入替え、幅の急減を含める。
StateとActionのサイズ、操作cost、重複率、候補採用比も独立に振る。

## 実施順と現在地

1. `parent_backend_model.py` の順序、LCP、prefix、親表現、State walk検証は完了
2. `cost_model.py` の `W=1`、`P=1`、`P=W`、`G>>E_live` のbyte再計算は完了
3. State操作counterと同値性digestは実装済み
4. 32 bit slot版は実装済みで、現在は標準の `beam_search.cpp`
5. 64 bit baselineと32 bit標準版の初回比較は完了
6. `beam_search_parent.cpp` の独立frontier付きdirect parent版は実装済み
7. 3方式の初回合成benchmarkは完了
8. `beam_search_parent_compact.cpp` のcand導出版は実装済み
9. 4方式の最終差分試験と同一source hashでの再測定は完了
10. succinct parent、window decodeはprofileで依存loadが支配すると確認してから追加する

この順序なら、既存方式の改善と新構造の効果を混ぜずに判断できる。

実装前の独立監査は `red_team.md` と `constant_factor_gate.md` に記録した。構造modelは既定の30,000 randomを含む
256,126世代と、幅5・深さ3までの73,085 topology全探索で失敗がない。さらに100,000 randomを含むstressでは
1,120,860世代、7,209,407遷移についてprefix復元とState walk下限を確認した。これは形式証明の代替ではないため、
実装したC++ backendでも差分試験を行う。
