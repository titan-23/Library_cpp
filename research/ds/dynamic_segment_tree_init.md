# dynamic_segment_tree_init.cpp

## 構造

- 区間 [0,N) を管理する経路圧縮付き動的セグメント木である。全要素の初期値をコンストラクタで指定する。
- 初期値を length 個結合した値は pow(init, length) で求める。length が 0 なら e() を使う。
- clear 後に更新した異なる位置が K 個なら、ノード数は 2K-1 個になる。K=0 なら 0 個である。
- 葉と分岐を別々のプールに保持する。葉には値だけを持たせ、位置は探索時に持ち回る。
- ノード番号は 0 が欠損、奇数が葉、正の偶数が分岐を表す。保持数の上限は 2^32-1 個である。

## 計算量の前提

- N は列の長さ、M は保持ノード数、M_other はコピー元またはムーブ元のノード数を表す。
- T の演算、コピー、ムーブ、破棄、述語の評価と pow は O(1) とする。
- ノード追加を含む計算量は償却計算量である。
- 問い合わせではノードを追加しない。
- pow が O(1) でない例として、[dynamic_segment_tree_util.cpp](../../titan_cpplib/ds/dynamic_segment_tree_util.cpp) の SegAffineInit がある。詳細は [dynamic_segment_tree_util.md](dynamic_segment_tree_util.md) にある。

## prod の実装

- l と r が別の子に分かれたあとは、左端側と右端側の経路を同じループで下る。2 つの経路の読み出しが重なる。
- 分かれるまでの経路では、子の選択を条件付き転送で書ける形にしている。短い区間では分かれるまでの経路が長いためである。
- inner_prod は呼び出し元のループに展開されると、測定では 5% ほど遅くなった。そのため `[[gnu::noinline]]` で展開させない。
- 測定の記録は [dynamic_segment_tree_prod_benchmark.md](../../test/ds/benchmark/dynamic_segment_tree_prod_benchmark.md) にある。

## max_right と min_left の実装

- scan_boundary は、境界まで下りながら残りの部分木と子の間の初期値を積み、境界に近い順に判定して境界を探す。
- Forward なら boundary 以上を左から結合し、そうでなければ boundary 未満を右から結合する。
- 木の外側の初期値は呼び出し元が調べる。scan_boundary は境界が見つかれば answer に入れて true を返す。
- 判定が失敗した部分木は descend_failed で下り、子の間の初期値も含めて境界の位置を求める。Forward なら左から、そうでなければ右から結合する。
- 初期値だけの区間では uniform_max_right と uniform_min_left が境界を探す。
