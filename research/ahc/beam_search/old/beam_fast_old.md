# beam_fast_old.cpp

## record_turn_survivors

- 当該ターンで生存した node_id を `candidates.next_beam` から収集します。
- そのターンの履歴のうち、生存しなかった `status==0` のものを 1 に変更し、スナップショットを追加します。
- `candidates.next_beam` を参照するだけで、探索の状態は変更しません。
- [beam_search.cpp](../../../../titan_cpplib/ahc/beam_search/beam_search.cpp) と同じロジックです。

## copy_tour_path

- 経路を復元する処理です。親 leaf へ向かう祖先の分の action を tour から復元します。
- `tour[leaf[k]..leaf[k+1])` を末尾 `dst_end` の手前にランク順でコピーする共通のロジックです。
- `dst_end` は親パスの末尾の 1 つ後ろを指します。これは葉の action を格納する位置にあたります。
