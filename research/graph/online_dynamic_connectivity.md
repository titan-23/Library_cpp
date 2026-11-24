# online_dynamic_connectivity.cpp

使用方法、計算量の根拠、計測は次の文書に記載しています。

- [how_to_use_online_dynamic_connectivity.md](../../titan_cpplib/graph/how_to_use_online_dynamic_connectivity.md)
- [online_dynamic_connectivity_benchmark_2026-11-12.md](../../titan_cpplib/graph/online_dynamic_connectivity_benchmark_2026-11-12.md)
- [online_dynamic_connectivity_benchmark_2026-11-12_round2.md](../../titan_cpplib/graph/online_dynamic_connectivity_benchmark_2026-11-12_round2.md)

## 方式

- Holm-de Lichtenberg-Thorup の完全動的連結性の方法です。
- add_edge が返す辺 ID ごとに多重辺を区別します。

## erase_edge の計算量

- 更新列全体での探索のコストは償却 O(log^2 n) です。
- 動的木を準備する前に木の辺を削除すると、prepare_dynamic() の構築のコストも加わります。
- 新しい階層の配列の初期化 O(n) は別に計上します。
- 単一の操作の最悪時間を保証するものではありません。

## prepare_dynamic

- 動的木をまとめて作成します。初回は O(n + 発行済み ID 数)、以後は O(1) です。
- 木の辺を初めて削除するときにも自動的に実行されます。削除時の初期化を避けたい場合に呼び出します。

## 連結成分のコピー

- 成分が初めて実際に分割されるまで、連結成分のコピーは正確です。
- 木でない辺の削除と、置換辺が見つかった削除では、この分割は変化しません。
- merge_partition は辺の追加時だけ呼び出し、コピーが有効であれば更新します。

## Forest の内部

- build_tour は tour の列から平衡な木を作成します。パスの場合でも再帰の深さは O(log n) です。
- build_initial の時点では全頂点が孤立しており、接続のマークは設定済みです。木ごとに反復処理で列挙し、Euler tour を線形時間で作成します。
- set_mark は、部分木の集約値が変化しなくなった時点で伝播を停止します。たどるステップ数を制限し、その後に splay するため、償却 O(log n) の上界は維持されます。
- same では、Euler tour が 1 点だけであれば、その頂点はこの森で孤立しています。この場合は、大きい可能性があるもう一方の tour にアクセスしません。
- cut は小さい方の成分の根を返し、reconnect がその根を再利用します。
- Euler tour の 1 成分は最大 3 * size - 2 ノードです。

## reconnect

- 昇格が必要になるのは、内部の木でない辺を次の階層へ移動する前だけです。
- 木でない辺が接していなければ探索の対象がありません。木の階層を変更しなければ、大きさの不変条件は全て維持されます。
- 小さい木を昇格する前に候補を 1 本だけ試します。両側をまたぐ辺であれば昇格せずに再接続できます。
- 試す候補を 1 本に限定するため、敵対的な入力でも追加のコストは各階層で O(log n) に収まります。
