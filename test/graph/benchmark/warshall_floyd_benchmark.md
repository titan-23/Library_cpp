# Warshall-Floyd 系の高速化の測定記録

2026-11-13。WarshallFloyd、WarshallFloydSIMD、WarshallFloydPath、WarshallFloydSIMDNonnegative を対象にした。

## 測定条件

- CPU は AMD Ryzen Threadripper 3990X (Zen 2)、コンパイラは GCC 12.3.0。
- ベンチマークは [warshall_floyd_benchmark.cpp](warshall_floyd_benchmark.cpp)。n=600、追加する辺は 600 本。
- build は負の辺のない密なグラフ (各頂点から 8 本)、build_neg は 30% が負の辺のグラフ、add_edge は build のあとに正の辺を追加する。
- WarshallFloyd と WarshallFloydPath の INF は numeric_limits<T>::max() / 2 で、SIMD の経路を通る。
- `-std=c++17 -O2 -DNDEBUG -march=native` (native) と、`-march` なしの既定ビルド (SSE2 まで) を測った。
- 変更前は HEAD (`9634bd5`) のヘッダ。CPU 4 に固定し、交互に 3 回ずつ実行した中央値。
- 表の ns は native の値で、build は 1 回の構築を n^3 で割った値、add_edge は辺 1 本あたりの値。短縮は `1 - 後 / 前`。チェックサムは全行で一致した。

## 変更点

- simd_int.cpp に SSE2 段を加えたので、既定ビルドの int は 4 要素ずつ処理するようになった。64 bit 整数は既定ビルドでは逐次のままである。
- WarshallFloyd に、WarshallFloydPath と同じ条件で SIMD の経路を加えた。重みが -INF 未満の辺は -INF に丸める。
- WarshallFloydSIMD の負閉路の伝播と、add_edge の neg_inf の更新を SIMD にした。
- WarshallFloyd と WarshallFloydPath の構築の内側のループを、端数のない部分と最後の端数に分け、dik についての場合分けをループの外に出した。
- 更新がない要素へ書き込まない形は残した。選んだ値を全要素に書き込む形は 4〜21% 遅かった。

## 結果

| 操作 | 前 ns | 後 ns | native | 既定ビルド |
|---|---:|---:|---:|---:|
| WarshallFloyd<int>.build | 1.2 | 0.3 | 78.0% | 51.4% |
| WarshallFloyd<int>.build_neg | 1.4 | 0.3 | 79.6% | 62.4% |
| WarshallFloyd<int>.add_edge | 1157368.9 | 201667.4 | 82.6% | 66.2% |
| WarshallFloydSIMD<int>.build | 0.1 | 0.1 | -2.8% | 51.6% |
| WarshallFloydSIMD<int>.build_neg | 0.7 | 0.2 | 69.8% | 60.9% |
| WarshallFloydSIMD<int>.add_edge | 68852.8 | 66483.5 | 3.4% | 52.1% |
| WarshallFloydPath<int>.build | 0.4 | 0.3 | 24.7% | 18.0% |
| WarshallFloydPath<int>.build_neg | 0.5 | 0.4 | 24.6% | 37.9% |
| WarshallFloydPath<int>.add_edge | 216358.1 | 209930.8 | 3.0% | 61.6% |
| WarshallFloydSIMDNonnegative<int>.build | 0.1 | 0.1 | 1.4% | 70.6% |
| WarshallFloydSIMDNonnegative<int>.add_edge | 30370.7 | 30258.0 | 0.4% | 71.4% |
| WarshallFloyd<ll>.build | 1.2 | 0.4 | 64.1% | 12.9% |
| WarshallFloyd<ll>.build_neg | 1.4 | 0.5 | 64.8% | 10.7% |
| WarshallFloyd<ll>.add_edge | 1243848.8 | 392348.5 | 68.5% | 7.4% |
| WarshallFloydSIMD<ll>.build | 0.3 | 0.3 | 1.7% | -13.9% |
| WarshallFloydSIMD<ll>.build_neg | 0.7 | 0.4 | 41.6% | 9.1% |
| WarshallFloydSIMD<ll>.add_edge | 154984.5 | 155617.1 | -0.4% | 0.3% |
| WarshallFloydPath<ll>.build | 0.6 | 0.5 | 14.1% | -0.8% |
| WarshallFloydPath<ll>.build_neg | 0.8 | 0.7 | 15.1% | 2.4% |
| WarshallFloydPath<ll>.add_edge | 396517.7 | 357828.9 | 9.8% | 0.1% |
| WarshallFloydSIMDNonnegative<ll>.build | 0.1 | 0.1 | 8.3% | -0.4% |
| WarshallFloydSIMDNonnegative<ll>.add_edge | 60836.0 | 55533.5 | 8.7% | -5.9% |

## 注意

- 既定ビルドの 64 bit 整数は SIMD を使わず、コードも変えていない。それでも WarshallFloydSIMD<ll>.build が 14% 遅く、WarshallFloydSIMDNonnegative<ll>.add_edge が 6% 遅く測られた。
- warshall_floyd_simd.cpp だけを HEAD 版に戻したビルドでも同じ速さだったので、今回の変更によるものではない。同じ翻訳単位の他のヘッダが変わり、関数全体の生成コードと配置が変わった影響とみられる。

## 検証

- [warshall_floyd_random.cpp](../warshall_floyd_random.cpp) で、4 クラスを int と long long について素朴な Floyd 法と比べた。距離、到達判定、負閉路の判定、経路復元、add_edge 後の状態を確かめる。
- native、既定ビルド、SIMD を無効にしたビルドの 3 構成で通った。AVX-512 はコンパイルだけを確かめた。
- SSE2 段の max をわざと誤らせると、このテストが失敗することを確かめた。
