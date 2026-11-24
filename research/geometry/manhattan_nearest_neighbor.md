# manhattan_nearest_neighbor.cpp

[`titan_cpplib/geometry/manhattan_nearest_neighbor.cpp`](../../titan_cpplib/geometry/manhattan_nearest_neighbor.cpp) の `ManhattanNearest` は、点の有効化と無効化ができる 2 次元マンハッタン距離の最近傍探索です。

## 点の登録と個数

- コンストラクタには、将来追加する可能性がある点をすべて渡します。
- 点 ID はコンストラクタへ渡した vector の添字とします。
- `add(id)` で点を 1 個追加し、`remove(id)` で 1 個削除します。
- 同じ ID を複数個保持できます。個数が正であれば検索対象になります。
- `add_all()` は個数 0 の全点を 1 個にします。

## 検索

- `nearest` は追加済みの全点のうちクエリの最近傍を返します。
- `nearest_four` は閉じた 4 象限それぞれの最近傍を返します。
- 距離が同じ場合は ID が小さい点を返します。
- 該当する点がなければ `-1` を返します。
- 方向の順番は NE, NW, SW, SE です。
- 象限は境界を含みます。このため軸上の点やクエリと同じ座標の点は、複数の方向の答えになる場合があります。

## 計算量

- 構築と一括追加は O(n log(n)) です。
- 追加、削除、検索は O(log^2(n)) です。
- メモリは O(n log(n)) です。
