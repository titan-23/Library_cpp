# Warshall-Floyd 系の高速化の測定記録

2026-11-13 の記録です。WarshallFloyd、WarshallFloydSIMD、WarshallFloydPath、WarshallFloydSIMDNonnegative を対象にしました。

## 測定条件

- CPU は AMD Ryzen Threadripper 3990X (Zen 2)、コンパイラは GCC 12.3.0 です。
- ベンチマークには [warshall_floyd_benchmark.cpp](warshall_floyd_benchmark.cpp) を使用しました。n=600 で、追加する辺は 600 本です。
- 各操作の内容は次のとおりです。
  - build は負の辺のない密なグラフの構築です。各頂点から 8 本の辺を張ります。
  - build_neg は 30% が負の辺であるグラフの構築です。
  - add_edge は build の後に正の辺を追加する操作です。
- WarshallFloyd と WarshallFloydPath の INF は numeric_limits<T>::max() / 2 であり、この場合は SIMD による処理が実行されます。
- `-std=c++17 -O2 -DNDEBUG -march=native` でのビルド (native) と、`-march` を指定しない既定のビルド (SSE2 まで) を測定しました。
- 変更前のコードには HEAD (`9634bd5`) のヘッダを使用しました。CPU 4 に固定して交互に 3 回ずつ実行し、その中央値を記載しています。
- 表の ns は native の値です。build は 1 回の構築の時間を n^3 で割った値、add_edge は辺 1 本あたりの値です。native と既定のビルドの列は短縮率で、`1 - 後 / 前` で求めました。チェックサムは表の全行で一致しました。

## 変更点

- simd_int.cpp に SSE2 版を追加したため、既定のビルドの int は 4 要素ずつ処理されるようになりました。64 bit 整数は既定のビルドでは逐次処理のままです。
- WarshallFloyd に、WarshallFloydPath と同じ条件で SIMD による処理を追加しました。重みが -INF 未満の辺は -INF に丸めます。
- WarshallFloydSIMD の負閉路の伝播と、add_edge の neg_inf の更新を SIMD で処理するように変更しました。
- WarshallFloyd と WarshallFloydPath の構築の内側のループを端数のない部分と末尾の端数に分割し、dik についての場合分けをループの外に移動しました。
- 更新がない要素には値を格納しない方式を維持しました。選択した値を全要素に格納する方式は 4〜21% 遅くなりました。

## 結果

| 操作 | 前 ns | 後 ns | native | 既定のビルド |
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

- 既定のビルドでは 64 bit 整数の処理に SIMD を使用しておらず、そのコードも変更していません。それでも WarshallFloydSIMD<ll>.build は 14%、WarshallFloydSIMDNonnegative<ll>.add_edge は 6% 遅く計測されました。
- warshall_floyd_simd.cpp だけを HEAD 版に戻したビルドでも同じ速度だったため、今回の変更によるものではありません。同じ翻訳単位の他のヘッダが変更され、関数全体の生成コードと配置が変化した影響と考えられます。

## 検証

- [warshall_floyd_random.cpp](../warshall_floyd_random.cpp) で、4 クラスの int 版と long long 版を素朴な Floyd 法と比較しました。このテストでは距離、到達判定、負閉路の判定、経路復元、add_edge 後の状態を確認します。
- native、既定のビルド、SIMD を無効にしたビルドの 3 構成でテストが成功しました。AVX-512 についてはコンパイルできることだけを確認しました。
- SSE2 版の max に意図的に誤りを入れると、このテストが失敗することを確認しました。
