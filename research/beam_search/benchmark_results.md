# 固定深さ4 backendのend-to-end benchmark

## 結論

計測時の旧 `beam_search.cpp`、定数倍改善版、2種類のdirect parent版を同一の合成Stateと候補列で測定した
15ケースすべてで結果と候補streamが一致し、4 backendの`expanded`、`apply_ops`、`rollback_ops`も一致した

計測後に定数倍改善版を標準の `beam_search.cpp` へ昇格し、旧版を
`test/ahc/beam_search_baseline.cpp` へ移した。生TSVの `optimized` は現行標準版を指す。

現行標準版は15ケースすべてで旧基準版以下の時間だった
metadataの固定費が見えるケースでは最大33.9%短縮し、State差分更新を重くすると差は0.5%まで縮んだ

direct parent版は一律に速くない
現行標準版に対する3%を超える勝ちはcompact版の親入替ケースだけで、浅いcross-parent、深い共有prefix、分岐8、
大きなActionでは17%から43%遅かった
親入替の勝ちも7 sampleの単一環境による観測であり、既定backendを置換する根拠にはしない

parent oracleは独立した`frontier_slot`を持つcorrectness oracleである
parent compactはfrontierを`cand`から導出し、この独立metadataを省く
compact/oracleは全ケースで0.959倍から1.013倍だった
最大差の`serial_w1`は1 ms未満であり、metadata省略による汎用的なCPU時間短縮は確認できなかった

生データは[benchmark_results.tsv](./benchmark_results.tsv)、driverは[backend_benchmark.cpp](./backend_benchmark.cpp)、
再現scriptは[run_backend_benchmark.sh](./run_backend_benchmark.sh)に保存した

## 正しさgate

scriptは本計測より前に各backendを1回ずつ実行し、次の列を比較して不一致なら終了する
本計測後にも同じ比較を行う

- `expanded`
- `result_score`、`turns_done`、`status`、`path_length`
- 全Actionと最終Stateを含む`result_digest`
- `try_op()`ごとのparent、child、score、thresholdを順序付きで含む`stream_digest`

wall計測用はcounterなしのtemplate instantiationを使う
別のcounterありinstantiationでも`result_digest`を比較し、計測器が結果を変えていないことを各ケースで確認する

今回の60行では、同一ケースの上記gateが4 backend間ですべて一致した
TSVの事後検証では`apply_ops`と`rollback_ops`も全行で一致した
これは同じState walkを実行した証拠であり、metadataの読書きや依存loadまで同じという意味ではない

現行標準版、parent oracle、parent compactのAction lifecycleは15ケースすべてで一致した
従って親方式との比較にAction copyとmoveの差は混入していない
ただしbackend全体を別々にcompileした比較なので、コード配置やmetadata layoutまで固定した単一変更ablationではない

## 合成matrix

中心となる`core` 4ケースは幅`64, 512`と深さ`32, 128`の直交matrixである
これに次を加えた

| 軸 | ケース | 構成 |
|---|---|---|
| `W=1`, `P=1` | `serial_w1` | 幅1、分岐4、深さ256 |
| `P=1` | `sibling_p1` | 毎世代の生存子を1親から供給 |
| 浅いcross-parent | `shallow_reference` | root直下で256 lineageへ分岐 |
| 深い分岐 | `deep_branch` | 深さ48までunary、その後4分岐で幅256へ到達 |
| 親入替 | `parent_replacement` | 後から良いscoreが来てselector slotを繰り返し置換 |
| State操作cost | `state_work_32`, `state_work_128` | applyとrollbackへ可逆な整数演算を追加 |
| Actionサイズ | `action_words_8`, `action_words_32` | 実サイズ104 byteと296 byte |
| 分岐数 | `branch_8` | 幅256、深さ96、分岐8 |
| 列挙API | `vector_enumeration` | callbackと同じ候補をvector fallbackで列挙 |

最小Actionは40 byteである
scoreとhashは衝突と同点を避けた決定的な値を使い、完成候補は生成しない
全ケースは`MaxTurnReached`まで走り、final Stateをmaterializeする

`expanded`は`try_op`回数であり、expanded nodes相当として使う
batchは1 sample内で独立searchを繰り返す回数である
目標work単位を250,000としてAction word、State操作回数、synthetic workから1から128の範囲で自動決定する

## 測定条件

- 日時: 2026-09-01 16:02から16:04 JST
- OS: Linux 6.18.33.2-microsoft-standard-WSL2 x86_64
- CPU: AMD Ryzen AI 9 HX 370、24 logical CPU、L2 12 MiB、L3 16 MiB
- CPU affinity: logical CPU 0へ`taskset -c 0`で固定
- memory: 15 GiB
- compiler: GCC 15.2.0
- allocatorとlibc: glibc 2.43
- flags: `-std=c++20 -O3 -DNDEBUG -march=native`
- warmup: 2 sample
- measured repetitions: 7 sample
- statistic: wall timeのmedian
- width: 固定
- historyとverbose: off
- hash: 毎ターンclear
- final State: on

compile時間とdigest計算はwall timeに含めない
1 sample内のSearch構築、search、内部buffer破棄、Result破棄は含める
時間は外側の`steady_clock`でbatch全体を測り、search 1回当たりへ割り戻す

unpinnedの予備系列はhybrid CPU上のcore migrationによる系統差が見えたため破棄した
検証agentの終了前に開始したpinned系列も途中で中断して破棄した
掲載値はrepository内の他のCPU処理とcompileを停止した後に最初から取り直した単独系列である

## 測定対象hash

計測開始直前と終了直後に次のSHA-256を取得し、全ファイルが一致することを確認した
先頭2件は現在のbaselineと標準版に対応する昇格前snapshotであり、現在のファイル自体のhashではない

```text
baseline snapshot  05c1fb50b75744c9edb143ba67879da37b3c28713eff61a3964a3b5c91752df7
standard snapshot  1a9eaedf083edf174444bfc213fa4f2520795de6526016a9dc56d90e9ca084a3
parent oracle      a68e7e7f781c9af79ad285be81d3dd8de34ae349caaf4dbf96cbbad27054f6cb
parent compact     ccad6362c1c2da822ecac34ce4bf22c4cb7a4ac39e1ae2ae3fcf513d79099da6
candidates.cpp     6fa7b1529d46ecaca7b6ada4ffac686a8108ad64e267a4e0e54550f979f1edba
beam_param.cpp     b5a9098de983353f58ab97c578aa6a25da72cd7f65a78dc65b1c44cd2510d3e3
timer.cpp          cef5d3cba808be3f3c3171b880b1d049e1cb94515700a486aff4d5ba2a198eb3
benchmark driver   0ebccc0e5601037e0e572f1a5b6705ec186b0e69b96ad262c69b08b3226127d3
benchmark script   9d3a64f21d305c659ffd5fa4d30d4abcb43e2b1a01d3e1e534462a4c85620c4d
```

## 実測値

比率は小さいほど速い
`oracle/standard`と`compact/standard`は最も近いwhole-backend比較であり、構造だけの隔離測定ではない
`compact/oracle`はfrontier metadata省略をより直接に見る比較だが、別binaryのcode layout差は残る

| case | baseline ms | standard/base | oracle/standard | compact/standard | compact/oracle |
|---|---:|---:|---:|---:|---:|
| `serial_w1` | 0.038 | 0.704 | 1.104 | 1.059 | 0.959 |
| `sibling_p1` | 2.017 | 0.850 | 1.021 | 0.999 | 0.979 |
| `core_w64_d32` | 0.200 | 0.888 | 1.111 | 1.068 | 0.961 |
| `core_w64_d128` | 1.878 | 0.905 | 1.308 | 1.288 | 0.985 |
| `core_w512_d32` | 2.027 | 0.883 | 1.085 | 1.085 | 1.000 |
| `core_w512_d128` | 18.412 | 0.922 | 1.397 | 1.414 | 1.012 |
| `shallow_reference` | 5.091 | 0.905 | 1.390 | 1.379 | 0.993 |
| `deep_branch` | 1.720 | 0.987 | 1.206 | 1.167 | 0.968 |
| `parent_replacement` | 1.535 | 0.661 | 0.980 | 0.962 | 0.981 |
| `state_work_32` | 111.853 | 0.979 | 1.022 | 1.036 | 1.013 |
| `state_work_128` | 538.227 | 0.995 | 1.007 | 1.003 | 0.997 |
| `action_words_8` | 6.015 | 0.969 | 1.325 | 1.318 | 0.995 |
| `action_words_32` | 8.608 | 0.956 | 1.172 | 1.170 | 0.998 |
| `branch_8` | 7.043 | 0.832 | 1.434 | 1.425 | 0.993 |
| `vector_enumeration` | 4.955 | 0.912 | 1.384 | 1.393 | 1.007 |

## Action lifecycle

counterはwall測定とは別の非trivial Action型で1 searchだけ採った
Searchが結果を返す時点までを数え、呼出し側が受け取ったResultの破棄は含めない
`shallow_reference`の値は次のとおりである

| backend | default ctor | copy ctor | move ctor | copy assign | move assign | destruct |
|---|---:|---:|---:|---:|---:|---:|
| baseline | 73,729 | 48,992 | 42,832 | 0 | 67,408 | 165,457 |
| standard | 49,153 | 42,928 | 24,576 | 0 | 42,832 | 116,561 |
| parent oracle | 49,153 | 42,928 | 24,576 | 0 | 42,832 | 116,561 |
| parent compact | 49,153 | 42,928 | 24,576 | 0 | 42,832 | 116,561 |

現行標準版では旧基準版に対してdefault constructionが33.3%、copy constructionが12.4%、
move constructionが42.6%、move assignmentが36.5%、destructionが29.6%減った
これはcombined版の差であり、個々の変更の寄与を分離したablationではない

親入替ケースではcopy constructionが旧基準版97,722回からstandard、oracle、compact共通の26,208回へ減った
一方、standard、oracle、compact間では全lifecycle counterが一致する
従って旧基準版からの33.9%短縮をparent構造へ帰属できず、parent構造固有の差は標準版比で最大約4%に留まる

## 読み取り

現行標準版は`serial_w1`で29.6%、`branch_8`で16.8%、親入替で33.9%短縮した
Action lifecycle削減、32 bit slot、未参照prefix削除などをまとめた効果である
個別変更の寄与はこのend-to-end測定からは分離できない

State workを32、128へ増やすと現行標準版と旧基準版の差は2.1%、0.5%になった
4 backendのState操作数は一致しているため、State操作が支配するとmetadata高速化が隠れるというcost modelと整合する

oracleの親入替は現行標準版の0.980倍だが、3%未満なので測定ノイズ範囲とみなす
compactは親入替で3.8%速かった
一方、浅いcross-parentではoracleが39.0%、compactが37.9%、分岐8では43.4%、42.5%遅い
direct parentはtour再構築を消す代わりに、深さ方向の依存parent loadとparent metadataを追加する
これを負け領域の原因とみるのは構造からの推論であり、hardware counterによる確定ではない

Actionを104 byte、296 byteへ増やしても、oracleは現行標準版より32.5%、17.2%遅く、
compactは31.8%、17.0%遅かった
standard、oracle、compactのAction lifecycleは全ケースで同じであり、
今回のAction payload範囲だけを理由にparent版を選ぶ根拠はない

従って今回の判断は次になる

- 現行の `beam_search.cpp` を通常利用する
- parent oracleは独立frontierを維持するcorrectness oracle
- parent compactはmetadataを減らす比較backendだが、今回のCPU時間ではoracleに対する一律の勝ちはない
- 2種類のparent版は既定置換ではなく、実Tree形状でprofileして選ぶbackend
- parent版の次の研究はdependent load、parent metadata byte、`M_tour/W`を同時に取るmicrobenchmarkが必要
- State操作が高価な利用者ではbackend差よりState側の差分更新を優先

## 独立検証

同じ4 backendでreleaseの5000 randomとASan/UBSanの1000 randomを通した
さらに`_GLIBCXX_DEBUG`の1000 randomでは4本のstdoutが完全一致し、debug診断はなかった
この検証後の定数倍改善版変更はコメント表記だけであり、benchmark gateは変更後hashで再実行した

`beam_result.cpp`の通常、history、nonmovableを4 backendでcompileして実行した
sampleの`a.cpp`と`gamex.cpp`も4 backendでcompileした
失敗はなく、既知の未使用引数、signedness比較、sample側警告だけを確認した
`gamex.cpp:102`の`reserve(4)`済みlocal vectorへの`push_back`でGCC 15.2の`-Warray-bounds`も確認した
backendのstack配列に対する警告ではなく、compileは成功しているためfalse positiveと判断した

```bash
BEAM_RANDOM_CASES=5000 bash test/ahc/beam_search_differential.sh

BEAM_SANITIZE=1 BEAM_RANDOM_CASES=1000 \
ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 \
UBSAN_OPTIONS=print_stacktrace=1:halt_on_error=1 \
bash test/ahc/beam_search_differential.sh
```

## ノイズと未測定項目

この結果は1台、GCC 1版、1回の7 sample系列だけであり、信頼区間ではない
CPU affinityは固定したが、governor、boost、SMT、WSL2 host負荷は固定していない
3%未満の差と1 ms未満のsearch時間は方向を確定できる精度として扱わない

`perf stat`、cycles、instructions、cache miss、peak RSS、Clang、portable flags、NUMA、動的幅、history on、
finished候補、hash重複、同点、可変ターンは未測定である
合成Stateは比較可能なTree形状を作るためのもので、実問題の品質や枝刈り効果を代表しない
特定testの速度を一般化した結果でもない

## 再現

repository rootから次を実行する

```bash
BEAM_BENCH_CPU=0 bash research/beam_search/run_backend_benchmark.sh \
    2 7 research/beam_search/benchmark_results.tsv 250000
```

scriptは4 binaryを別々にcompileするため、同名classのinterfaceを変えずに同じdriverを使う
digest gateが失敗した場合はTSVを上書きせず終了する
build directoryは`/tmp/beam-search-benchmark.*`に残し、実行終了時に表示する

保存済みTSVは昇格前のbackend名 `optimized` を保持する。現在のscriptで再実行した場合は同じ実装を
`standard` として出力する。
