# dynamic_segment_tree_init.cpp

## 構造

- 区間 [0,N) を管理する経路圧縮付きの動的セグメント木です。全要素の初期値をコンストラクタで指定します。
- 初期値を length 個結合した値は pow(init, length) で求めます。length が 0 の場合は e() を使用します。
- clear の後に更新した異なる位置が K 個の場合、ノード数は 2K-1 個になります。K=0 の場合は 0 個です。
- 葉と分岐ノードを別々のプールに保持します。葉が保持するのは値だけで、位置は探索の途中で求めます。
- ノード番号は 0 がノードのない状態、奇数が葉、正の偶数が分岐ノードを表します。保持するノード数の上限は 2^32-1 個です。

## 計算量の前提

- N は列の長さ、M は保持しているノード数、M_other はコピー元またはムーブ元のノード数を表します。
- T の演算、コピー、ムーブ、破棄、述語の評価と pow は O(1) とします。
- ノードの追加を含む計算量は償却計算量です。
- 問い合わせではノードを追加しません。
- pow が O(1) でない例として [dynamic_segment_tree_util.cpp](../../titan_cpplib/ds/dynamic_segment_tree_util.cpp) の SegAffineInit があります。詳細は [dynamic_segment_tree_util.md](dynamic_segment_tree_util.md) に記載しています。

## prod の実装

- l と r が別の子に分かれた後は、左端側と右端側の経路を同じループで下ります。これにより 2 つの経路の読み出しが並行して行われます。
- 分かれるまでの経路では、子の選択を条件付き転送で実現できる形にしています。短い区間では分かれるまでの経路が長いためです。
- inner_prod が呼び出し元のループにインライン展開されると、測定では 5% ほど遅くなりました。そのため `[[gnu::noinline]]` でインライン展開を抑止しています。
- 測定の記録は [dynamic_segment_tree_prod_benchmark.md](../../test/ds/benchmark/dynamic_segment_tree_prod_benchmark.md) に記載しています。

## max_right と min_left の実装

- scan_boundary は、boundary の位置まで下りながら残りの部分木と子の間の初期値をスタックに積み、boundary に近い順に判定して境界を探します。
- Forward の場合は boundary 以上を左から結合し、そうでない場合は boundary 未満を右から結合します。
- 木の外側の初期値は呼び出し元が調べます。scan_boundary は、境界が見つかった場合にその位置を answer に格納して true を返します。
- 判定が失敗した部分木は descend_failed で下り、子の間の初期値も含めて境界の位置を求めます。Forward の場合は左から、そうでない場合は右から結合します。
- 初期値だけの区間では uniform_max_right と uniform_min_left が境界を探します。
