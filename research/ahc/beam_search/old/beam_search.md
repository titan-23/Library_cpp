# beam_search.cpp

[old/beam_search.cpp](../../../../titan_cpplib/ahc/beam_search/old/beam_search.cpp) についての補足。

## 参考

- https://eijirou-kyopro.hatenablog.com/entry/2024/02/01/115639

## get_next_beam の引数

- `state` は探索を進める State である。
- `turn` は search ループのグローバルなターン番号で、履歴の記録に使う。
- `depth` は探索木の局所的な深さである。
  - 一本道を圧縮した後は 0 になり、`turn != depth` となり得る。
  - `depth == 0` のときは tree を辿らず、state から直接展開する。
