# beam_fast_old.cpp

## record_turn_survivors

- 当該ターンで生存した node_id を `candidates.next_beam` から集める。
- そのターンの履歴のうち、生き残らなかった `status==0` のものを 1 に直し、スナップショットを積む。
- `candidates.next_beam` を読むだけで、探索状態は変更しない。
- [beam_search.cpp](../../../../titan_cpplib/ahc/beam_search/beam_search.cpp) と同じロジックである。

## copy_tour_path

- 経路復元の処理である。親 leaf へ向かう祖先の分の action を tour から復元する。
- `tour[leaf[k]..leaf[k+1])` を、末尾 `dst_end` の手前にランク順に貼り込む共通ロジックである。
- `dst_end` は親パス末尾の一つ後ろを指す。これは葉の action を書く位置にあたる。
