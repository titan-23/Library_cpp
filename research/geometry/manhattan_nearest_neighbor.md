# manhattan_nearest_neighbor.cpp

[`titan_cpplib/geometry/manhattan_nearest_neighbor.cpp`](../../titan_cpplib/geometry/manhattan_nearest_neighbor.cpp) の `ManhattanNearest` は、点を有効化・無効化できる2次元マンハッタン距離の最近傍探索である。

## 点の登録と個数

- コンストラクタには、将来追加する可能性がある点をすべて渡す。
- 点IDは、コンストラクタへ渡した vector の添字とする。
- `add(id)` で点を1個追加し、`remove(id)` で1個削除する。
- 同じIDを複数個保持できる。個数が正なら検索対象になる。
- `add_all()` は、個数0の全点を1個にする。

## 検索

- `nearest` は、追加済みの全点からの最近傍を返す。
- `nearest_four` は、閉じた4象限それぞれの最近傍を返す。
- 同距離ではIDが小さい点を返す。
- 該当する点がなければ `-1` を返す。
- 方向の順番は NE, NW, SW, SE である。
- 象限は境界を含む。このため、軸上の点やクエリと同じ座標の点は複数方向の答えになりうる。

## 計算量

- 構築と一括追加は O(n log(n)) である。
- 追加、削除、検索は O(log^2(n)) である。
- メモリは O(n log(n)) である。
