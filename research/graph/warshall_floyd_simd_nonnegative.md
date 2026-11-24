# warshall_floyd_simd_nonnegative.cpp

## SIMD

- 実装はコンパイル時に選択します。優先順は AVX-512 (F, VL)、AVX2、SSE2、逐次処理です。
- SSE2 は T が 32 bit の場合だけ使用します。

## 値の範囲

- T は 32 bit または 64 bit の符号付き整数です。
- 構築時と add_edge で与える辺の重みは 0 以上です。
- 到達不能な頂点対の距離は INF です。最短距離は INF 未満に収まる必要があります。
