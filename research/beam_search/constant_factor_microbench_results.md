# 通常版beam searchの定数倍microbenchmark

## 結論

- `push_lazy()` は棄却率が高いほど有効で、採用率0%ではこの環境で1.75倍から16.94倍だった
- 採用率50%では0.999倍から1.052倍、100%では0.985倍から1.037倍でほぼ同等だった
- `clear/reserve/emplace_back()` は全Action幅で速く、このActionモデルでは1.49倍から1.74倍だった
- `Timer::elapsed()` を1回減らす固定費はこの環境で約35.4 nsだった
- いずれも独立kernelの結果であり、beam search全体が同じ倍率で速くなることは意味しない

`push_lazy()` とmove constructionは低リスクな実装候補として支持される

ただし実Actionのcopy、move、default constructionがこの固定長payloadと異なる場合は再計測が必要になる

## 再現方法

```bash
bash research/beam_search/run_constant_factor_microbench.sh
```

scriptは次のrelease条件でコンパイルする

```text
g++ -std=c++20 -O3 -DNDEBUG -march=native
```

測定時の既定値は次になる

| 項目 | 値 |
|---|---:|
| warmup | 2回 |
| sample | 11回 |
| 集計 | median |
| beam width | 2048 |
| candidate submission | 65536回/sample |
| block payload | 32 MiB/sample |
| timer iteration | 1000000回/sample |

各値はscriptの引数で変更できる

```bash
bash research/beam_search/run_constant_factor_microbench.sh \
    --warmup 3 --samples 15 --beam-width 4096 --candidate-ops 131072 \
    --timer-iterations 2000000 --block-mib 64
```

## 測定環境

| 項目 | 値 |
|---|---|
| 測定日 | 2026-09-01 |
| OS | Linux 6.18.33.2-microsoft-standard-WSL2 x86_64 |
| CPU | AMD Ryzen AI 9 HX 370, 12 core, 24 thread |
| cache | L1d 576 KiB, L2 12 MiB, L3 16 MiB |
| compiler | g++ (Ubuntu 15.2.0-16ubuntu1) 15.2.0 |
| git base | `fe5bda90dcebbb7ef1ff641516376d3ff1fce7fe` |

測定対象を再確認できるよう、現在の入力ファイルのSHA-256を記録する

| 入力 | SHA-256 |
|---|---|
| `constant_factor_microbench.cpp` | `de4e992a7630340be20173a0aafd8b7935acc575657cf23b57a00219011af1ae` |
| `candidates.cpp` | `6fa7b1529d46ecaca7b6ada4ffac686a8108ad64e267a4e0e54550f979f1edba` |
| `beam_param.cpp` | `b5a9098de983353f58ab97c578aa6a25da72cd7f65a78dc65b1c44cd2510d3e3` |
| `timer.cpp` | `cef5d3cba808be3f3c3171b880b1d049e1cb94515700a486aff4d5ba2a198eb3` |
| `run_constant_factor_microbench.sh` | `4f4a1eb813cb26db1a6032783e3cb205f8e6676ba401a726252456c59b484da7` |

CPU affinity、周波数、他processは固定していない

WSL2上の単一machineで得た数値なので、絶対値は対象machineで取り直す必要がある

## Kernel 1 `Candidates::push()` 対 `push_lazy()`

### 意味論

実装中の `Candidates` をそのまま使い、beamを満杯にしてから候補を投入した

- 採用候補は現在のworst scoreより必ず小さく、hashは一意にした
- 棄却候補はthresholdで棄却され、hash lookupへ進まない
- 採用位置は固定個数の1をshuffleしたmaskで決め、周期的なbranch patternを避けた
- lvalueはcallbackが同じActionを再利用する経路を表す
- rvalueはvector fallbackが各Actionを `std::move()` する経路を表す
- setup、入力Action構築、結果digestは計測区間から外した
- 採用時のhash table更新、segment tree更新、table拡張は両方式とも計測に含めた

Actionは8、32、128、512 byteの固定長inline payloadとした

copyとmoveはpayload全体を転送し、compiler barrierで棄却側の値引数copyも消えないようにした

timing用Actionではcounterを無効にし、lifecycle確認だけ別の同型Actionで実行した

### 結果

単位はns/submissionで、`x` は `push / lazy` になる

`x > 1` なら `push_lazy()` が速い

| B | source | accept | push | lazy | x | kept | digest |
|---:|---|---:|---:|---:|---:|---:|---|
| 8 | lvalue | 0% | 2.860 | 1.272 | 2.248 | 0 | `31776f1f2fbc8bde` |
| 8 | lvalue | 1% | 4.595 | 2.962 | 1.551 | 655 | `197234b969172bd9` |
| 8 | lvalue | 10% | 17.716 | 16.899 | 1.048 | 6553 | `1d5c9e4a20d628be` |
| 8 | lvalue | 50% | 78.911 | 77.707 | 1.016 | 32768 | `1273ac6fd0b6a64c` |
| 8 | lvalue | 100% | 142.319 | 144.075 | 0.988 | 65536 | `622e6245991a7391` |
| 8 | rvalue | 0% | 2.224 | 1.272 | 1.749 | 0 | `31776f1f2fbc8bde` |
| 8 | rvalue | 1% | 3.490 | 2.485 | 1.404 | 655 | `1857a0f2f5f691d0` |
| 8 | rvalue | 10% | 13.889 | 12.972 | 1.071 | 6553 | `a215f73f5d291ec1` |
| 8 | rvalue | 50% | 60.845 | 60.404 | 1.007 | 32768 | `98886cda02650763` |
| 8 | rvalue | 100% | 142.019 | 142.716 | 0.995 | 65536 | `ec25b6cd57acb617` |
| 32 | lvalue | 0% | 2.531 | 1.027 | 2.463 | 0 | `bf32ba26e42ad42a` |
| 32 | lvalue | 1% | 3.788 | 2.279 | 1.662 | 655 | `c145b2425495adb1` |
| 32 | lvalue | 10% | 13.955 | 12.867 | 1.085 | 6553 | `d394cf20cb7788c6` |
| 32 | lvalue | 50% | 60.949 | 60.432 | 1.009 | 32768 | `15d4aad00aaf0119` |
| 32 | lvalue | 100% | 141.013 | 142.933 | 0.987 | 65536 | `5ae27caefb023533` |
| 32 | rvalue | 0% | 2.547 | 1.034 | 2.463 | 0 | `bf32ba26e42ad42a` |
| 32 | rvalue | 1% | 3.847 | 2.296 | 1.676 | 655 | `7978917a3ee4d169` |
| 32 | rvalue | 10% | 14.247 | 13.022 | 1.094 | 6553 | `923f051c97ef2d6f` |
| 32 | rvalue | 50% | 81.444 | 80.015 | 1.018 | 32768 | `f60dd9e52b1051c2` |
| 32 | rvalue | 100% | 142.057 | 144.184 | 0.985 | 65536 | `cdb1df8a59d2b6ff` |
| 128 | lvalue | 0% | 2.852 | 1.265 | 2.254 | 0 | `31adb03e64c29280` |
| 128 | lvalue | 1% | 4.099 | 2.509 | 1.634 | 655 | `be7345dbdfac90a2` |
| 128 | lvalue | 10% | 14.682 | 13.224 | 1.110 | 6553 | `9ded892f82895ad8` |
| 128 | lvalue | 50% | 62.963 | 61.443 | 1.025 | 32768 | `30e8dfa38a888e6` |
| 128 | lvalue | 100% | 144.283 | 144.906 | 0.996 | 65536 | `9faa11a494f5af63` |
| 128 | rvalue | 0% | 3.110 | 1.020 | 3.050 | 0 | `31adb03e64c29280` |
| 128 | rvalue | 1% | 4.584 | 2.347 | 1.953 | 655 | `30cb345bd619b1ae` |
| 128 | rvalue | 10% | 15.815 | 13.726 | 1.152 | 6553 | `beae93cf75baae82` |
| 128 | rvalue | 50% | 64.977 | 64.166 | 1.013 | 32768 | `3002c4191928e57` |
| 128 | rvalue | 100% | 109.881 | 110.720 | 0.992 | 65536 | `e38e40919bc3fa7b` |
| 512 | lvalue | 0% | 10.842 | 0.640 | 16.937 | 0 | `b7fb2ae6a0edb33c` |
| 512 | lvalue | 1% | 12.211 | 2.072 | 5.893 | 655 | `f423f1554b2242d9` |
| 512 | lvalue | 10% | 22.596 | 14.144 | 1.598 | 6553 | `d3acc4ccd80c9e8c` |
| 512 | lvalue | 50% | 72.491 | 68.933 | 1.052 | 32768 | `98d2fc42b4a5513c` |
| 512 | lvalue | 100% | 119.211 | 115.992 | 1.028 | 65536 | `3de11213b826440b` |
| 512 | rvalue | 0% | 15.049 | 0.965 | 15.587 | 0 | `b7fb2ae6a0edb33c` |
| 512 | rvalue | 1% | 17.733 | 4.068 | 4.359 | 655 | `e1c18a97a724b9a7` |
| 512 | rvalue | 10% | 31.598 | 26.997 | 1.170 | 6553 | `e499d77dcf49f4f6` |
| 512 | rvalue | 50% | 88.360 | 88.491 | 0.999 | 32768 | `a8b973ffec6b7432` |
| 512 | rvalue | 100% | 134.617 | 129.824 | 1.037 | 65536 | `9b1c2b8bd50b672a` |

### Action lifecycle

1000 submission当たりの件数を数えた

`A` は1000 submission当たりの採用数で、測定点では0、10、100、500、1000になる

| source | method | copy ctor | move ctor | move assign | destruct |
|---|---|---:|---:|---:|---:|
| lvalue | value | 1000 | A | A | 1000+A |
| lvalue | lazy | A | 0 | A | A |
| rvalue | value | 0 | 1000+A | A | 1000+A |
| rvalue | lazy | 0 | A | A | A |

全行でdefault constructionとcopy assignmentは0だった

value版の採用時に増えるmove constructionは、aggregateの候補一時objectを作る現在の `push()` に由来する

counterは言語上のlifecycleを観測可能にするため、productionのtrivial Actionでは一部をcompilerが消す場合がある

`push_lazy()` は棄却候補のcopyまたはmove constructionを正確に0へ減らした

### 読み方

採用率0%から10%では、selector本体に入る前のAction転送を除く効果が明確に出た

採用率50%ではselectorのhashとsegment tree処理が支配し、差は小さくなった

採用率100%の差は概ね数%以内で、table seed、CPU周波数、cache状態の揺れと分離できない

本結果から `push_lazy()` の100%採用時の高速化や低速化を主張しない

## Kernel 2 世代Action blockの構築

### 意味論

両方式ともdestination vectorへ必要capacityを事前確保し、slab poolを再利用する状態を模擬した

```text
old: clear -> resize(W) -> W回のmove assignment
new: clear -> reserve(W) -> W回のemplace move construction
```

1 sample当たりの転送payloadを約32 MiBへ揃え、幅2048のblockをAction幅に応じて反復した

### 結果

単位はns/Actionで、`x` は `old / new` になる

| B | resize+assign | emplace | x | Action/sample | digest |
|---:|---:|---:|---:|---:|---|
| 8 | 0.684 | 0.393 | 1.740 | 4194304 | `707a760d0dc765f0` |
| 32 | 0.853 | 0.511 | 1.669 | 1048576 | `b20e8f5727845b06` |
| 128 | 2.359 | 1.358 | 1.737 | 262144 | `28af887f9a3fcfce` |
| 512 | 17.121 | 11.474 | 1.492 | 65536 | `6b60ffaa109cf72a` |

1 block、1000 Action当たりのlifecycleは次になった

| method | default ctor | move ctor | move assign |
|---|---:|---:|---:|
| resize+assign | 1000 | 0 | 1000 |
| emplace | 0 | 1000 | 0 |

countは共通の `clear()` とscope終了時のdestructionを除き、構築部分だけを捕捉した

固定長payloadではdefault constructionもpayloadをzero fillし、moveもpayload全体を転送する

実Actionのdefault constructionがO(1)の場合や、heap所有Actionのmoveがpointer交換だけの場合は倍率が変わる

capacity不足のallocationも測っていないため、これはpool再利用後のsteady stateの比較になる

## Kernel 3 `Timer::elapsed()` 1回分

実装と同じ `titan23::Timer::elapsed()` を1 iterationに1回呼ぶloopと2回呼ぶloopで比較した

| 1 call | 2 calls | 差分 | digest |
|---:|---:|---:|---|
| 35.351 ns | 70.708 ns | 35.357 ns | `43b27db01b62e903` |

差分は追加の1 call当たり約35.4 nsを示す

1000 generationで約35 us、100000 generationで約3.5 msに相当する

clock実装、OS、仮想化、vDSOによる差が大きいため、他環境へ35.4 nsを転用しない

差分はpaired sampleではなく2つのmedianの差なので、短い測定ではsubtraction noiseも含む

## 最適化除去への対策

- Actionのcopy、move、default construction、assignment後にcompiler memory barrierを置いた
- 各kernelは結果Action、score、parent、accepted数からdigestを作った
- sampleごとのdigestを混合し、最終digestを標準出力へ出した
- lifecycle版は別にstatic counterを読み、timing版へcounter更新costを入れなかった
- timer版は全ての `elapsed()` 戻り値を加算してdigestへ変換した

記録した全digestは0ではなく、value版とlazy版の最終候補内容も一致した

digestは正当性testの代用ではなく、benchmark対象の削除を防ぐための観測値になる

## 測定限界

- 固定長inline payloadは全Action型を代表しない
- compiler barrierはnon-elidableな転送を作るが、実Actionに同じbarrierがあるわけではない
- `Candidates` のrandomized hash seedはsampleごとに異なる
- accepted hashを全て一意にしたため、duplicate置換の比率は測っていない
- rejected候補は全てthreshold rejectで、hash duplicate rejectは測っていない
- Action生成cost、`try_op()`、Stateのapplyとrollback、tour操作は測っていない
- CPU pinningとfrequency固定をしていないため、数%の差は判定材料にしない
- end-to-endの改善率はState cost、候補数、採用率、Action型、widthごとに別途測る必要がある

## cost modelとの接続

未参照先頭prefixの削除と32 bit slot-only化は、実装を重複させず解析値を使う

[cost_model.md](./cost_model.md) では現行の永続topology writeを `8(B+M)+4W` byteと導出している

先頭prefix削除後の32 bit slot-only版は `4M+4W` byteになり、dependent loadは増えない

ここで `B=e+1` は次generationから一度も読まれない先頭token数になる

これらは論理byte数であり、cache line転送量や実cycleを直接表さない

本microbenchmarkはAction lifecycleとtimer固定費を補い、prefixとhandle幅の評価はcost modelへ委ねる
