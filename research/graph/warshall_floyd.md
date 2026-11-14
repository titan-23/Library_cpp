# warshall_floyd.cpp

## 値の範囲

- INF は到達不能を表す。
- 負閉路を経由しない頂点対の最短距離は (-INF, INF) に収まる必要がある。
- 重みが -INF 未満の辺は -INF として扱う。

## SIMD

- T が 32 bit または 64 bit の符号付き整数で、INF <= numeric_limits<T>::max() / 2 + 1 のとき、主要なループを SIMD で処理する。
- 使える命令セットのうち、AVX-512 (F, VL)、AVX2、SSE2 の順に優先する。SSE2 は 32 bit の T だけで使う。
- 条件を満たさないときは、同じ処理を逐次に行う。
- SIMD の処理には [SimdInt](../../titan_cpplib/others/simd_int.cpp) を使う。
