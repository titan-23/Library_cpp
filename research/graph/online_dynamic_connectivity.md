# online_dynamic_connectivity.cpp

使い方、計算量の根拠、計測は次の文書にある。

- [how_to_use_online_dynamic_connectivity.md](../../titan_cpplib/graph/how_to_use_online_dynamic_connectivity.md)
- [online_dynamic_connectivity_benchmark_2026-11-12.md](../../titan_cpplib/graph/online_dynamic_connectivity_benchmark_2026-11-12.md)
- [online_dynamic_connectivity_benchmark_2026-11-12_round2.md](../../titan_cpplib/graph/online_dynamic_connectivity_benchmark_2026-11-12_round2.md)

## 方式

- Holm-de Lichtenberg-Thorup の完全動的連結性の方法である。
- add_edge が返す辺 ID ごとに多重辺を区別する。

## erase_edge の計算量

- 更新列全体での探索費用は償却 O(log^2 n) である。
- 動的木を準備する前に木の辺を削除すると、prepare_dynamic() の構築費用も加わる。
- 新しい階層の配列の初期化 O(n) は別に数える。
- 単一の操作の最悪時間を保証するものではない。

## prepare_dynamic

- 動的木をまとめて作る。初回は O(n + 発行済み ID 数)、以後は O(1) である。
- 木の辺を初めて削除するときにも自動で実行する。削除時の初期化を避けたいときに呼ぶ。

## 連結成分の写し

- 最初に成分が実際に分かれるまで、連結成分の写しは正確である。
- 木でない辺の削除と、置換辺が見つかった削除では、この分割は変わらない。
- merge_partition は辺の追加時だけ呼び、写しが有効なら更新する。

## Forest の内部

- build_tour は tour の列から平衡な木を作る。パスでも再帰の深さは O(log n) である。
- build_initial の時点では全頂点が孤立しており、接続の印は設定済みである。木ごとに反復で列挙し、Euler tour を線形時間で作る。
- set_mark は、部分木の集約値が変わらなくなった時点で伝播を止める。たどる歩数を制限し、その後に splay するので、償却 O(log n) の上界は保たれる。
- same では、Euler tour が 1 点だけならこの森で孤立した頂点である。もう一方の、大きいかもしれない tour には触れない。
- cut は小さい方の成分の根を返し、reconnect がそれを再利用する。
- Euler tour の 1 成分は最大 3 * size - 2 ノードである。

## reconnect

- 昇格が要るのは、内部の木でない辺を次の階層へ移す前だけである。
- 木でない辺が接していなければ探す対象がない。木の階層を変えずにおけば、大きさの不変条件は全て保たれる。
- 小さい木を昇格する前に、候補を 1 本だけ試す。両側をまたぐ辺ならすぐ再接続できる。
- 試す候補を 1 本に限るので、敵対的な入力でも追加の費用は各階層で O(log n) に収まる。
