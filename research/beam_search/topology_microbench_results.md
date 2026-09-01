# topology metadata microbenchmark

## 目的

`beam_search.cpp` 全体ではなく、次の構造差だけを分離して比較した

- slot-only postorderのtourとleaf構築
- direct-parent compactのparentとLCP構築
- direct-parent oracleのparent、frontier、LCP構築
- slot-only postorderの連続suffix decode
- direct-parentのdepth別parent chain decode

候補選択、Action payload、Stateの`apply_op()`と`rollback()`、履歴、allocatorは含めていない

この測定はend-to-end benchmarkを置き換えるものではない

## kernel

各fixtureは深さが同じ親frontierと、親ordinalが単調な次frontierを作る

親ordinalには同一親groupと未選択親のgapを含め、隣接LCPは旧frontierの区間最小値から導出する

slot-only buildは先頭prefixを保存せず、2番目以降の葉についてLCPより下のsuffixとleaf境界を書く

direct-parent compact buildは次の2種類をそれぞれ`W`個書く

- depth `d` の各slotに対応するdepth `d-1` のparent slot
- `entry_lcp` 1個と`adjacent_lcp` `W-1`個

oracle buildは同じ単調区間scanを行い、さらに独立したfrontier slotを`W`個書く

decodeは最初の葉をtraceへ設定済みとして、内部の`W-1`遷移を測る

slot-onlyは連続suffixを読み、direct-parentはdepth別blockをslot依存で辿る

各呼出しでtraceを確保せず、fixture内のscratchを再利用する

## 正しさgate

各caseで次を確認し、不一致なら測定前にabortする

1. compactとoracleが作った全LCPが期待値と一致する
2. 2方式が復元した全内部遷移のsuffix checksumが一致する
3. 葉ordinal、LCP、全suffix slotを順序付きで混ぜた64 bit digestが一致する

結果TSVにある8 caseのdigestは全て異なる

## 条件

- CPU: AMD Ryzen AI 9 HX 370
- OS: Linux 6.18.33.2-microsoft-standard-WSL2 x86_64
- compiler: g++ 15.2.0
- flags: `-std=c++20 -O3 -DNDEBUG -march=native`
- affinity: CPU 0
- sample: 15
- sampleごとの処理量: 3200万unit以上
- 集計: median
- 5 kernelの実行順: sampleごとに巡回
- source SHA-256: `3875b11a29879d183c1c74ad49f846c8040ddd4be6c2cf51b291aba9a658c213`

生データは`topology_microbench_results.tsv`に残した

## 結果

`avg_suffix`は内部遷移1回あたりの平均suffix長になる

ratioはdirect-parentをslot-onlyで割った値で、1未満ならdirect-parent kernelが速い

| case | W | depth | avg_suffix | compact build | oracle build | parent decode |
|---|---:|---:|---:|---:|---:|---:|
| sibling alloc64K | 256 | 32 | 1.00 | 0.691 | 1.038 | 0.303 |
| shallow alloc512K | 256 | 32 | 32.00 | 0.657 | 0.507 | 5.410 |
| group4 alloc4M | 4096 | 64 | 1.25 | 0.749 | 0.918 | 0.444 |
| deep2 alloc32M | 4096 | 64 | 2.00 | 0.933 | 0.824 | 0.821 |
| shallow alloc32M | 4096 | 64 | 64.00 | 0.076 | 0.059 | 8.778 |
| mixed alloc32M | 4096 | 128 | 32.20 | 0.140 | 0.156 | 15.818 |
| sibling alloc32M | 16384 | 64 | 1.00 | 0.591 | 1.130 | 0.289 |
| half alloc32M | 2048 | 256 | 129.00 | 0.059 | 0.047 | 21.827 |

絶対時間は1 frontierあたりのmedianになる

| case | slot build us | compact build us | slot decode us | parent decode us |
|---|---:|---:|---:|---:|
| sibling alloc64K | 0.165 | 0.114 | 0.447 | 0.135 |
| shallow alloc512K | 0.586 | 0.385 | 1.100 | 5.950 |
| group4 alloc4M | 3.204 | 2.399 | 7.390 | 3.283 |
| deep2 alloc32M | 6.846 | 6.387 | 8.615 | 7.075 |
| shallow alloc32M | 83.026 | 6.280 | 33.353 | 292.779 |
| mixed alloc32M | 20.608 | 2.882 | 18.585 | 293.981 |
| sibling alloc32M | 13.142 | 7.769 | 28.436 | 8.232 |
| half alloc32M | 52.322 | 3.091 | 29.873 | 652.042 |

## 読み取れること

metadata構築では、postorderの書込量`M+W`に対してcompact parentは概ね`2W`になる

従って`M`が大きいcaseでは、parent buildの優位が大きい

一方でdecodeは読み出すslot数がほぼ同じでも、parent chainの次addressが直前のload結果に依存する

平均suffixが1から2のcaseではparent decodeが0.29から0.82倍だった

平均suffixが32から129のcaseではparent decodeが5.41から21.83倍になった

これはdirect-parentの構造的な利点がmetadata write削減、弱点が長いdependent chainであることと整合する

mixedは平均だけでなく最大suffixとdepth別blockのworking setにも影響されるため、平均suffixだけでは予測できない

oracleとcompactの小さい差は構造差だけで説明できないcaseがある

oracleは追加のfrontier writeを持つのにcompactより速いcaseもあり、code layout、cache状態、WSL2の揺れが残っている

従ってこの測定からoracleとcompactの僅差を順位付けしない

`deep2`のような20%前後の差も、単独machineで構造的優位と断定しない

## 限界

- slot buildの入力suffixは事前生成した連続配列で、実装中のhotなtraceとは配置が異なる
- 実slot decoderのleaf gap scanと`copy_tour_path()`の複数区間合成を直接再現していない
- 最初の葉への遷移はdecodeから除き、parent buildの`entry_lcp`計算とstoreだけを含めた
- parent historyはdepth別blockだが、未解放の死slotを含まないlive trie相当である
- Action payload、candidate、hash、State操作を除くため、全体速度への寄与率は分からない
- kernelごとのactive working-set量は異なり、TSVへ個別に記録した
- CPU pinningをしてもWSL2上の周波数、割込み、code alignmentの影響は残る

実装判断には4 backendのend-to-end benchmarkを主に使い、この結果は原因分解にだけ使う

## 再現

```bash
TOPOLOGY_CPU=0 TOPOLOGY_SAMPLES=15 TOPOLOGY_TARGET_UNITS=32000000 \
    bash research/beam_search/run_topology_microbench.sh
```
