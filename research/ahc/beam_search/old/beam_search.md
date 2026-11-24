# beam_search.cpp

[old/beam_search.cpp](../../../../titan_cpplib/ahc/beam_search/old/beam_search.cpp) についての補足です。

## 参考

- https://eijirou-kyopro.hatenablog.com/entry/2024/02/01/115639

## get_next_beam の引数

- `state` は探索を進める State です。
- `turn` は search ループのグローバルなターン番号で、履歴の記録に使用します。
- `depth` は探索木の局所的な深さです。
  - 一本道を圧縮した後は 0 になり、`turn != depth` となる場合があります。
  - `depth == 0` のときは tree をたどらず、state から直接展開します。
