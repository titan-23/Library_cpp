# warshall_floyd_simd.cpp

## SIMD

- 実装はコンパイル時に選択します。優先順は AVX-512 (F, VL)、AVX2、SSE2、逐次処理です。
- SSE2 は T が 32 bit の場合だけ使用します。

## 値の範囲

- T は 32 bit または 64 bit の符号付き整数です。
- 距離は [-INF, INF] に収めます。INF は到達不能を表します。
- 負閉路を経由しない頂点対の最短距離は (-INF, INF) に収まる必要があります。
