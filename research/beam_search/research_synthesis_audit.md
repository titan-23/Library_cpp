# beam search 調査統合監査

## 結論

2026-09-01の初回監査時点に存在したresearch文書、3 backend実装、実測TSV、再現driverを照合した

- State walkのfree-endpoint下限は、文書にある4条件の下で成立する
- `cost_model.md` と `decision.md` のdirect parent write式はoracle版とderived版を正しく分けている
- 古い文書にはderived版の式を無印のdirect parentへ使う箇所があり、実装済みoracleの式と混同できる
- 記録済みの `parent/optimized` はprefix Actionのcopy/move差を含み、純粋な構造ablationではない
- parent oracleを既定backendへ昇格させる根拠はない
- cand導出compact版は今実装してよい段階だが、別backendのablationとして扱うべきである
- compact版もdependent parent loadを消さないため、現状の17%から40%の負けを逆転するとは予想しない

監査中に次を再実行し、assertion failureがないことを確認した

```text
cost_model.py
parent_backend_model.py --trials 1000 --max-depth 12 --max-width 8
                        --exhaustive-depth 3 --exhaustive-width 4

topologies=6382
generations=22997
transitions=89640
open_walk_generations=22997
end_free_generations=22997
```

## 指摘への対応状況

初回監査後に指摘を反映し、現在は4 backendを同一source hashで再測定済みである

- `decision.md`、`performance_audit.md`、`structural_options.md`へ実装後の状態を追記した
- optimized版とparent版のprefix Action保存を揃え、4方式のAction lifecycle一致を確認した
- direct parentのoracle版とcand導出版について、write量とresident量の式を分離した
- 説明用の公開要約では `M_tour`、`L_parent`、`H_tree` を使うよう統一した
- cand導出版を `beam_search_parent_compact.cpp` として実装した
- 4方式の差分試験、sanitizer、interface試験、end-to-end benchmarkを再実行した
- 計測前後でbackendと共有ソースのhash一致を確認した

最終model実行では30,000 randomと幅5・深さ3までの73,085 topology全探索を含む103,093 topology、
489,230世代、3,256,939遷移を検証した。C++差分試験はrelease 5,000 random、ASan/UBSan 1,000 random、
`_GLIBCXX_DEBUG` 1,000 randomを通過した。

最新の数値と判断は `benchmark_results.md` を正本とする
以下の指摘は、修正理由と監査経緯を残すために初回監査時点の記録として保存する

## 重大度別の指摘

### 高: 実装後の正本が実装前の文章のままになっている

[decision.md](./decision.md) は通常版の正本とされているが、次が未来形のまま残る

- 54行から61行: 実装済み項目の判断が `実装する`
- 85行: `最初の実装ファイル`
- 177行: `新構造の実装ファイル`
- 215行: `実装前の検証ゲート`
- 276行以降: 完了済みの実装順

[performance_audit.md](./performance_audit.md) は全backend順位の正本とされているが、次が現状と一致しない

- 13行: `現段階では性能を変えるコードは実装しない`
- 30行: direct parentを独立設計段階として記述
- 146行から148行: `まだベンチマーク結果ではない`

[structural_options.md](./structural_options.md) にも次が残る

- 15行: コードとbenchmarkを実装していない
- 52行: parent-mapを未実装案として扱う
- 507行から508行: slot-onlyとparentを試作前として扱う

修正案は各文書の歴史的本文を全面改稿することではなく、冒頭へ状態boxを置くことになる

```text
調査時点の設計文書
実装状況: optimized済み、parent oracle済み
実測状況: benchmark_results.mdを参照
現在の判断: decision.mdの実装後追記を参照
```

`decision.md` の表は `実装済み`、`未実装`、`保留` の3状態へ更新する必要がある

### 高: 記録済みの `parent/optimized` は構造だけの比較ではない

[benchmark_results.md](./benchmark_results.md) 85行は次のように書かれている

```text
parent/optimizedが新しい履歴構造だけを見る主比較
```

実測に使った両実装には少なくとも次の差があった

```text
optimized: result_prefix.push_back(act(...))
parent   : result_prefix.push_back(move(act(...)))
```

TSVでもAction lifecycle差が確認できる

```text
parent_replacement copy ctor: optimized 26299, parent 26208
parent_replacement move ctor: optimized 24794, parent 24885
sibling_p1        copy ctor: optimized 48991, parent 48897
sibling_p1        move ctor: optimized 24797, parent 24891
```

差はcopyからmoveへの置換数と一致し、履歴表現以外の変更が混ざっている

監査中にoptimized側もprefix Actionをmoveするよう修正され、現在sourceのこの差は解消した

ただし記録済みTSVは修正前optimizedを測った値なので、実測説明は過去の差を含んだままになる

ほかにもprefix判定loop、compat telemetry計算、最終path復元の実装が異なる

従って85行は次の程度へ弱めるべきである

```text
parent/optimizedは共通の主要定数倍改善を含む2実装の主比較だが、純粋な構造ablationではない
```

再測定では現在sourceを使い、oracleとcompactを含む各比較で共通Action lifecycleを照合する

### 中: 記号の統一宣言と実際の局所記号が矛盾している

[README.md](./README.md) 29行から30行と[benchmark_plan.md](./benchmark_plan.md) 34行から35行は
ディレクトリ内で同じ記号を同じ意味へ揃えると宣言する

一方で[cost_model.md](./cost_model.md) は次の局所定義を使う

| 記号 | 共通定義 | cost modelの定義 |
|---|---|---|
| `B` | 1状態の分岐数 | 未参照先頭prefix長 `e+1` |
| `F` | active target-turn pool数 | traceへ書くslot数 `B+M` |
| `L` | 可変ターン総葉数 | parent decode load数 |
| `R` | 平坦木の実読取token数 | current leaf境界scan数 |
| `D` | 探索深さ | 未確定endpoint path長 |

[decision.md](./decision.md) 20行は `H` を誘導木に使うが、共通定義の `H` はcross-turn hash履歴数になる

`局所記号は記号表を優先` と `同じ記号を同じ意味で使う` は同時には成立しない

修正案は通常版の正本だけ説明名へ変えることになる

```text
B_entry, F_trace, L_parent, R_leaf, D_tail, H_tree
```

特に公開要約では `M_tour` と `L_parent` を使い、単独の `M`, `L` を避けるべきである

### 中: direct parent write式は正本では正しいが旧文書が無印でderived式を使う

[cost_model.md](./cost_model.md) と[decision.md](./decision.md) の式は整合している

```text
slot-only postorder: 4M + 4W byte
parent oracle      : 12N byte
parent derived     :  8N byte
```

固定幅 `N=W` ならoracleがwriteを減らす境界は `M>2W`、derivedは `M>W` になる

oracleの12Nは次の3配列を各4N byte書くためである

```text
parent
frontier_slot
entry_lcp + adjacent_lcp
```

一方で[postorder_external.md](./postorder_external.md) 1269行から1275行は
`direct parent + adjacent LCP = qW_c + l(W_c-1)` とだけ書く

これは `frontier_slot` をcandから導出するderived版なら正しいが、現在のoracle実装には適用できない

[postorder_external.md](./postorder_external.md) 249行から256行もparent配列だけをcurrent tour全体と比較している

[postorder_alternatives.md](./postorder_alternatives.md) 2493行のmemory `4G + G*S_A` もLCPとfrontierを省く

修正案は全比較表へ `oracle` または `cand-derived` の列を明記することになる

```text
parent oracle steady topology : 4G + 16W + 4K
parent derived steady topology: 4G +  8W + 4K
```

capacity高水位とvector headerを含まない近似であることも同じ行に残す必要がある

### 中: 初回benchmarkはcost modelの原因変数を取っていない

[benchmark_results.md](./benchmark_results.md) は実時間、State操作数、Action lifecycleを正しく保存している

baselineとdriverのsource hashは監査時点でも一致した

optimizedはprefix Actionのmove化、parentは最終generationのfrontier/LCP省略が実測後に入り、hashが変わった

```text
optimized benchmark : 485550502686e24caebf31f9ad475ebf7bbfe1ac8c988ac96921c84503c9e701
optimized audit-time: ff53313f26d5cb8c0dabbc84fcdc4d221df43faadb966ce15e8419a9b477fc7e
parent benchmark    : 52a30543c925145afcf3fbcb23e59faff892c40ff3e2aa9abd45612949340abd
parent audit-time   : a68e7e7f781c9af79ad285be81d3dd8de34ae349caaf4dbf96cbbad27054f6cb
```

初回の記録済み結果は記載hashの版に対して有効だが、監査時点の現在版3 backend実測ではなかった

最終generationの差だけなので構造結論を直ちに反転させる根拠はないが、最終報告前に同条件で再測定する必要がある

ただしparentの勝敗を説明する主要量はTSVにない

- `M_tour/W`
- `L_parent`
- 最大dependent chain
- `G/E_live`
- parent、frontier、LCPのlogical write
- L1、L2、LLC miss
- peak RSS

従って134行から135行のdependent load原因説は、文書自身が断っているとおり構造からの推論になる

`parent_replacement` の3.8%差も、1台、非pin、7 sample、1系列だけなので `明確な勝ち` は強すぎる

次の表現が測定精度と合う

```text
今回の系列では3.8%短かったが、再現系列と構造counterなしには安定した勝ち領域と断定しない
```

幅は最大512であり、benchmark planにある4096、65536の帯域とcapacity領域はまだ測れていない

Actionも40、104、296 byteに限られるため、要約の `大きなAction` は `今回の104/296 byte Action` と限定する

### 低: microbenchmarkの再現識別子が弱い

[constant_factor_microbench_results.md](./constant_factor_microbench_results.md) はcommitだけを記録し、未commit sourceを含む

backend benchmarkと同様に次のhashを保存した方が再現しやすい

- `constant_factor_microbench.cpp`
- `candidates.cpp`
- `beam_param.cpp`
- compiler full version string

## State walkのfree-endpoint下限監査

### 固定終点の下限

開始点 `s`、必須target集合、終了点 `t` を含む最小部分木を `H_tree` とする

木の各辺を `s-t` path上かどうかで分ける

- `s-t` path外の辺は、その先のtargetへ入り戻るため最低2回通る
- `s-t` path上の辺は最低1回通る

従って任意のwalkに次が成り立つ

```text
X >= 2|E(H_tree)| - dist(s,t)
```

各off-path subtreeをDFSし、最後に `s-t` pathを進めば等号を達成できる

### 現行順が等号を達成する理由

通常loopでは次が成立する

1. old frontierは同一深さでDFS-compatible
2. generation開始Stateはold frontierの走査終了endpoint `s` にある
3. targetはold frontier葉の子なので全て同一深さ
4. targetのparent ordinalはendpoint側から反対側へ単調非増加

ordered treeでは任意の辺の下にあるtarget ordinalは連続区間になる

この順でtargetを訪れると、最後のtarget path上の辺だけ1回、ほかの誘導木辺を2回通る

従って実walkは次を満たす

```text
X = 2|E(H_tree)| - dist(s,last_target)
```

### free endpointでも最小になる理由

全targetは同一深さなので、target `v` までの距離は次になる

```text
dist(s,v) = 2 * ((d-1) - depth(LCA(s,parent(v)))) + 1
```

endpointからparent ordinalを単調に離すと `depth(LCA(s,parent(v)))` は単調非増加になる

従って最後のtargetが `s` から最遠になる

```text
dist(s,last_target) = max_v dist(s,v)
```

終点をtargetから自由に選ぶときの下限は次になる

```text
2|E(H_tree)| - max_v dist(s,v)
```

現行順はこの下限も達成する

これは固定した開始点 `s` に対してtarget順まで自由にした場合のState edge call数として最小になる

### 主張へ必ず付ける境界

下限は次を意味しない

- CPU cycleが最小
- metadata read/writeが最小
- 開始点も自由な `2E-diameter` を常に達成
- 可変深さtargetで成立
- State copy、compose、checkpoint、一括path適用を許しても成立
- applyとrollbackの実costが等しい

`X` は辺単位APIの呼出し回数であり、時間ではないと毎回明記する必要がある

`H` はhash履歴と衝突するため、式では `H_tree` または単に `T_req` を使う方が安全になる

## cand導出compact版を今実装するか

### 判定

今は実装してよい段階になる

ただし目的はparent方式を採用することではなく、oracle固有の冗長配列を外した構造ablationを得ることになる

根拠は次になる

- oracleの構造正当性と既知反例は差分testとsanitizerで確認済み
- `frontier_slot[j] == cand[C-1-j].action_slot` は仕様の不変条件として既に確認済み
- 非terminal generationごとに4N byteのwriteとcurrent/next合計8W byteのresident容量を除ける
- current target handle readもfrontier配列から既に読むcandへ統合できる
- cost modelが予測する `M>W` のderived境界を実測できる

一方で期待値は限定する

- depth方向のdependent parent load数 `L_parent` は変わらない
- parent blockの `4G` とdead slot保持は変わらない
- adjacent LCPのread、write、構築scanは変わらない
- 現在のoracleも最終generationの不要frontier/LCP構築は既に省いている
- oracleがoptimizedへ17%から40%負けたケースをfrontier 1配列だけで逆転する根拠はない

### 実装上の注意

単に `frontier_slot` を削るだけでは `finalize_generation()` が壊れる

現在の処理は次の順になる

```text
current frontier_slotを参照してnext parentを作る
candをclearする
next candを作る
```

derived版ではcurrent frontier mappingがold `cand` 自体なので、old candをclearする前にparentを全件確定する必要がある

安全な順序は次のどちらかになる

1. parent blockをold candから先に構築し、その後candをclearしてnext candを作る
2. `next_cand` を別bufferへ構築し、完了後にswapする

accessorは次で足り、Action slotとfrontier ordinalを同一視してはならない

```text
frontier_slot(j) = cand[cand.size()-1-j].action_slot
```

Actionをfrontier順へgatherする変更は別案であり、compact版へ混ぜない

### 実装と測定のgate

新ファイルを `beam_search_parent_compact.cpp` とし、oracle版は残す

最低限次を満たしてから判断する

1. 既存differential全caseへ4 backend目として追加
2. callback再利用Action、root unsorted、replacement、gap、W=1、dynamic width replayを再実行
3. ASan、UBSan、`_GLIBCXX_DEBUG` を再実行
4. benchmarkへcompactを追加し、同じsource hashとdigest gateを保存
5. `M_tour/W`, `L_parent`, `G/E_live`, metadata writeを実測counter化
6. oracle対compactでState call列とAction lifecycleを完全一致させる

compactがoracleより速いことは期待できるが、optimizedより速いbackendになるかはこの4方式実測まで未確定になる

## 推奨する文書更新順

1. `README.md` へ実装済みファイル、差分test、初回実測、未測定領域を追記
2. `decision.md` を実装後statusへ更新し、下限の4条件を本文へ展開
3. `benchmark_results.md` の純粋ablation表現と `明確な勝ち` を弱める
4. `performance_audit.md` 冒頭へ設計時点の文書である注記を追加
5. `postorder_external.md` と比較表へoracle、derivedの区別を追加
6. 共通記号を `M_tour`, `L_parent`, `H_tree` へ統一
7. compact版を追加して4 backend実測を別結果として保存

## 最終判断

現在の最も強い結論は次になる

```text
現行postorderは指定条件下のState辺呼出し数を厳密に最小化する
slot-only optimizedは初回合成測定で有力だが、個別ablationと大幅beam測定が残る
direct parent oracleは構造として正しいが、既定置換を支持する実測はない
cand-derived compactは次に測るべき低リスクなparent ablationであり、最速性は未確定
```
