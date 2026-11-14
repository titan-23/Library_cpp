# warshall_floyd_simd.cpp

## SIMD

- 実装はコンパイル時に選ぶ。優先順は AVX-512 (F, VL)、AVX2、SSE2、逐次である。
- SSE2 は 32 bit の T だけで使う。

## 値の範囲

- T は 32 bit または 64 bit の符号付き整数である。
- 距離は [-INF, INF] に収める。INF は到達不能を表す。
- 負閉路を経由しない頂点対の最短距離は (-INF, INF) に収まる必要がある。
