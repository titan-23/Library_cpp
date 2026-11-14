# warshall_floyd_simd_nonnegative.cpp

## SIMD

- 実装はコンパイル時に選ぶ。優先順は AVX-512 (F, VL)、AVX2、SSE2、逐次である。
- SSE2 は 32 bit の T だけで使う。

## 値の範囲

- T は 32 bit または 64 bit の符号付き整数である。
- 構築時と add_edge で与える辺の重みは 0 以上である。
- 到達不能な頂点対の距離は INF である。最短距離は INF 未満に収まる必要がある。
