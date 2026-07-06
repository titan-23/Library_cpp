# titan_cpplib/graph レビュー

対象は25ファイル。

- bfs_path.cpp
- centroid_decomposition.cpp
- dijkstra.cpp
- dijkstra_path.cpp
- euler_tour.cpp
- get_scc_graph.cpp
- graph.cpp
- hashed_rooted_tree.cpp
- hld.cpp
- hld_edge_lazy_segment_tree.cpp
- hld_edge_segment_tree.cpp
- hld_lazy_segment_tree.cpp
- hld_segment_tree.cpp
- hungarian.cpp
- k_nearest_sources.cpp
- lca.cpp
- minimum_spanning_tree.cpp
- minimum_steiner_tree.cpp
- namori.cpp
- perfect_binary_tree.cpp
- rerooting_dp.cpp
- rooted_tree.cpp
- warshall_floayd_simd.cpp
- warshall_floyd.cpp
- warshall_floyd_path.cpp

重要度は3段階。

- **[バグ]** 誤動作・UB・コンパイル不能につながる
- **[注意]** 特定条件で問題になる
- **[軽微]** 動作に影響しない指摘

## バグ一覧(要約)

| ファイル | 内容 |
|---|---|
| warshall_floayd_simd.cpp | `ll` 未定義・`<climits>` 未 include で単体コンパイル不能。さらに AVX-512 命令を使っており AtCoder ジャッジ (Zen3) では SIGILL |
| get_scc_graph.cpp | groups だけ番号を反転しており、F・ids・ids_inv と対応しない |
| namori.cpp | なもりグラフ以外(木)を渡すと cycle 検出に失敗し `visit[-1]` で UB。`<random>`・`<climits>` を直接 include していない |
| minimum_spanning_tree.cpp | 重みなし・ソートなしなので「E が重み順」という暗黙の前提がある。前提を満たさなければ MST にならない |

## 横断事項

- **[軽微] `#pragma once` がないファイルが16個**。bfs_path、hld 系4つ、hungarian、k_nearest_sources、rooted_tree 以外すべて。expander は各ファイル1回のみ inline するので提出時は実害がないが、ローカルで複数ライブラリ経由の二重 include があると多重定義になる。
- **[軽微] 非 inline の自由関数**が dijkstra、bfs_path、minimum_spanning_tree、get_scc_graph、rerooting_dp にある。複数翻訳単位で ODR 違反。単一ファイル提出なら実害なし。
- **[軽微] 隣接リストの値渡し**が散見される(rerooting_dp、get_scc_graph、namori、centroid_decomposition、hld の build_list 等)。グラフ全体をコピーするので定数倍が悪い。特に rerooting_dp は `const vector<...> G` で `&` の付け忘れに見える。

## bfs_path.cpp

- 正しい。s==t で {s} を返す境界も問題ない。

## centroid_decomposition.cpp

- dfs / find_centroid / build のロジックは正しい。「サイズが total/2 を超える子へ降りる」方式で、部分木サイズは元の根からの値を使ってよいことも確認した。
- **[注意]** n==0 で `build(0)` が範囲外アクセス。頂点数1以上が前提。
- **[軽微]** コンストラクタが `vector<vector<int>> G` の値渡し + `G(G)` でコピーが2回走る。const 参照 + コピー、または move にできる。
- **[軽微]** dfs / find_centroid が再帰で、深さは成分サイズに比例する。AtCoder のスタックサイズなら実害なし。
- solve() の使い方コメント(パス重複・banned の扱い)は有用。

## dijkstra.cpp / dijkstra_path.cpp

- どちらも正しい。標準的な O((N+M)logN)。
- **[注意]** `d + c` を INF ガードなしで計算するので、INF は型の最大値の半分以下を渡す前提。コメントに明記がない。

## euler_tour.cpp

- 正しい。以下を確認した。
  - in/out 巡回(サイズ 2n)と touch 巡回(サイズ 2n-1)を1回の DFS で構築する部分。`nodeout[v] = 退場位置+1` の規約と、subtree/path 各クエリの区間の整合。
  - 4本の FenwickTree(頂点/辺 × subtree/path)への +w/-w の置き方と add_vertex / add_edge の更新位置。
  - LCA 用セグ木の `(depth << bit) + i` エンコード。bit は touch 巡回長に対して足りている。
- **[注意]** T が符号なし整数だと path 用の -w が壊れる。符号付き前提。
- **[軽微]** set_edge は u, v が隣接している前提だが assert がない。非隣接だとパス全体との差分を1辺に足してしまう。
- **[軽微]** lca_mul に空配列を渡すと範囲外。
- **[軽微]** コンストラクタの vertexcost が値渡し。

## get_scc_graph.cpp

- Tarjan 風 SCC 自体は正しい(lowlink の更新、処理済み頂点の order=n 化も確認)。
- **[バグ] 返り値の番号系が不整合**。groups は `groups[group_cnt-1-ids[v]]` でトポロジカル順に並べ替えているが、F・ids・ids_inv は生の番号のまま(こちらは逆トポロジカル順で、辺は大きい id から小さい id へ向く)。つまり groups[i] と F の頂点 i は別の成分を指す。ACL のように ids 自体を `group_cnt-1-ids[v]` に反転してから全部を作るべき。現状 groups と F を組み合わせると誤る。
- **[軽微]** G が値渡し。dfs が再帰。

## graph.cpp

- **[注意] 有向/無向の仕様が曖昧**。add_edge は G[u] にしか追加しない(有向)が、is_bipartite と minimum_spanning_tree は無向前提のアルゴリズム。無向で使うなら両方向 add_edge する必要があり、その場合 E に辺が2本入って MST のソート量が倍になる。クラスとしてどちらかに決めてコメント化すべき。
- **[軽微]** is_bipartite 末尾の `col[c] == -1` チェックはデッドコード。全頂点を外側ループで開始するので -1 は残らない。
- **[軽微]** topological_sort はサイクルがあると部分列を返す。検出は結果サイズの比較を呼び出し側でやる仕様なら、その旨のコメントがほしい。
- **[軽微]** get_G() がコピーを返す。const 参照返しでよい。
- minimum_spanning_tree はソートしてから Kruskal で正しい(単体の minimum_spanning_tree.cpp と挙動が違う点は後述)。

## hashed_rooted_tree.cpp

- 部分木の高さで乱数を引く木ハッシュ。mul / mod の桁あふれがないこと(61bit mod の縮約が1回の条件減算で足りること)を確認した。ロジックは正しい。
- **[軽微]** `uniform_int_distribution<u64> dist(47, (1ull<<61)-1)` の上限が MOD ちょうどで、R[i] == MOD ≡ 0 になり得る。確率は無視できるが上限は MOD-1 が正しい。
- **[軽微]** G が値渡し。dfs が再帰。

## hld.cpp

- 反復 DFS 2本(サイズ・親・深さ計算 → heavy を G[v][0] に置いて preorder 採番)のロジックは正しい。O(n)。
- **[注意] メンバ G を破壊的に変更する**。親辺を取り除き、G[v][0] を heavy child に並べ替えた上で public 公開している。「G[v] = v の子リスト(先頭が heavy)」として使えるのは便利だが、入力の隣接リストのつもりで触ると誤る。この仕様のコメントがない。
- **[軽微]** for_each_vertex_path が返す区間列は順序を保証しないので可換モノイド専用。非可換は HLDSegmentTree 側で対応済みだが、使い分けのコメントがあるとよい。
- **[軽微]** build_list が値渡し。

## hld_segment_tree.cpp

- 正しい。path_prod の非可換対応(u 側は rseg で u→head の向き、v 側は seg で head→v の向き、lres と rres の結合順)と、rseg の添字変換 `i → n-1-i` の全区間を確認した。
- get の「O(1)」表記は segment_tree.get が葉の直接参照なので正しい。

## hld_edge_segment_tree.cpp

- 正しい。辺属性(深い側の頂点に持たせる)での境界、同一列内最終区間の `nodein+1` による自辺の除外、subtree_prod の `nodein[v]+1` を確認した。
- 重み付き隣接リストからのコンストラクタも、G が両方向に辺を持つ前提で正しい。

## hld_lazy_segment_tree.cpp

- 正しい。path_apply / subtree_apply が seg と rseg の両方に対応区間で作用させている点、区間の対応(`[l, r) → [n-r, n-l)`)を確認した。
- **[軽微]** `(HLD&, int n)` コンストラクタは n == hld.n が前提だが検証がない。HLDSegmentTree は hld.n を直接使う形で、インターフェースが揃っていない。
- **[軽微]** print.cpp の include は未使用。コンストラクタの a が値渡し。

## hld_edge_lazy_segment_tree.cpp

- 正しい。path_apply の u==v ガード、辺属性の区間境界、subtree_apply の rseg 側 `[n-nodeout, n-nodein-1)` を確認した。

## hungarian.cpp

- kopricky のコードの移植(出典リンクあり)。U×U の graph 行列でダミー列を持たせて U>V を処理する構造、双対の更新、増加路探索を確認した。出典どおりのロジックで問題は見つからなかった。
- min_cost_assignment の転置ラッパも割当の逆写像まで正しい。
- **[注意]** cost が空(行数0)だと `_cost[0]` で UB。min_cost_assignment 経由でも n==0 は素通りする。
- **[軽微]** 計算量のコメントがない(O(n³) 相当)。禁止辺を最大値で表すと桁あふれする旨の注意書きは適切。

## k_nearest_sources.cpp

- 多始点 Dijkstra + 遅延削除で「相異なる始点による近い方 K 個」を保持するロジックは正しい。update() の同一始点の in-place 改善、最悪要素の置換、pop 時の有効性チェックを確認した。
- **[注意] 計算量表記が過小**。update() 内に K 要素の線形走査があるので、実際は O(K(N+M)(K + log(KN)))。K が大きいときに表記 O(K(N+M)log(KN)) との差が効く。頂点ごとの dist をヒープや hash にすれば表記どおりになるが、K が小さい想定なら現状のコメント修正だけでよい。
- **[軽微]** queue/deque の残骸コメントと qu_push / qu_top / qu_pop のラッパは消してよい。

## lca.cpp

- preorder の親列に対する RmQ という方式。以下を確認して正しいと判断した。
  - `path[nodein[v]-1] = par[v]` の構造と、lca(u,v) = path[min(nodein[path[i]]) for i in [l, r)] が成立すること。
  - StaticRmQ.prod が [l, r) の最小値を返す仕様との整合。
  - path[n-1] は未設定(vector のゼロ初期化値)だが、クエリ区間 [l, r) は r ≦ n-1 なので参照されないこと。
- **[軽微]** `int s[n]` は VLA で標準外(GCC 拡張)。vector か固定長で書けるとよい。
- **[軽微]** 末尾コメントの `namspace` は typo。

## minimum_spanning_tree.cpp

- **[注意] 名前と実装が乖離**。辺に重みがなくソートもしないので、実体は「E の順に採用する全域森」であり、MST になるのは E が重み昇順に整列済みのときだけ。graph.cpp の同名メソッドはソートするので挙動が違う。前提(整列済み)をコメントに明記するか、重み付きにしてソートを入れるべき。

## minimum_steiner_tree.cpp

- 最短路 Prim ヒューリスティックによる近似解法。zp_ バッファの前回呼び出しからのリセット、経路上の頂点追加時の min_d 更新、swap-pop による端子除去、非端子葉の刈り込みを確認した。ロジックに誤りはない。
- **[注意]** 最短路同士が合流するとサイクルができ得るため、返る辺集合は木とは限らない(近似解法として許容範囲だがコメントに明記がない)。次数ベースの刈り込みはサイクル上の頂点を落とせない。
- **[注意]** 到達不能な端子があると best_idx == -1 で無言で break し、結果にその端子が含まれない。
- **[軽微]** `const vector<int>& path = dist_path.get_path(...)` は一時オブジェクトへの const 参照束縛で合法だが、値受けの方が読みやすい。
- build_prim_fullsearch の始点全探索と cost 集計(採用辺は元グラフの辺なので get_dist が辺重みに一致)も正しい。

## namori.cpp

- 次数1の剥がし込みによるサイクル検出、サイクル順の走査、木部分の高さ添字ハッシュ、サイクル列のロリハを回転×反転で最小化する正規化、いずれもロジックは正しい。`4*MOD + acc[r] - acc[l]` の u64 上での帳尻(アンダーフロー時も一周して正になる)と mod() の縮約も確認した。
- **[注意] 前提が「連結でサイクルをちょうど1つ持つ」**。木を渡すと is_cycle が全 false になり、`v = -1` のまま `visit[v]` で UB。assert かコメントがほしい。
- **[注意] include が不健全**。mt19937_64 等の `<random>` は hash_string.cpp 経由の間接取得で、hash_string の機能自体は使っていない。ULLONG_MAX の `<climits>` はどこにもなく、処理系の間接 include で通っているだけ。`<random>` と `<climits>` を直接 include し、hash_string.cpp の include は外すべき。
- **[軽微]** cycle / forest に getter がなく、外から使えるのは get_hash だけ。構築結果を使う想定なら公開が必要。
- **[軽微]** get_hash 呼び出しごとに全ハッシュを再計算する。同一 seed で複数グラフを比較する用途なら問題ない。

## perfect_binary_tree.cpp

- root / par / children / dep / la / is_ancestor / lca / dist / kth / get_path とも正しい。lca の `u >> bit_length(u ^ v)` が u==v を含めて成立することを確認した。
- **[軽微]** `assert(u <= numeric_limits<T>::max())` は恒真で意味がない。
- **[軽微]** T が符号なしだと par(root) の -1 が wrap する。la(u, k) は k がビット幅以上でシフト UB(公開 API 経由では起きない)。
- **[軽微]** 全メソッドが状態を持たないので static にできる。

## rerooting_dp.cpp

- 全方位木DP。上り DP(topo 逆順)、下り DP での prefix(ls)/suffix(rs) 分割、rs バッファの使い回しが毎頂点で上書きされてから読まれること、親寄与 dp[v][pdx[v]] が親の処理で設定済みであることを確認した。ロジックは正しい。
- **[注意] G が値渡し**(`const vector<vector<pair<int, E>>> G`)。`&` の付け忘れに見える。グラフ全体をコピーする。
- 末尾の使い方コメント(apply_vertex / apply_edge の図)は有用。

## rooted_tree.cpp

- lca(nodein 比較 + head 遡上)、la、path_kth_elm、is_on_path、is_ancestor / is_descendant とも正しい。
- is_passable_path も正しい。最深点を2回取り、2点の lca より真に上に P の頂点があれば false、残りが2本の枝の祖先で覆われるかを見る構造を確認した。
- get_diameter / get_diameter_path は重み付き BFS だが、木なので訪問順に依存せず距離は正しい。
- **[注意]** dep_weight と get_diameter の未訪問判定に -1 番兵を使う。T が符号なし、または負の辺重みで距離がちょうど -1 になるケースで壊れる。負重みでは double-sweep 自体が成立しない点も含め、非負前提をコメントにすべき。
- **[軽微]** ctor1 の `G = F;` は `G = move(F);` にできる。get_diameter と get_diameter_path の BFS はほぼ重複コード。末尾コメント `nsmaepace` は typo。

## warshall_floayd_simd.cpp

- **[バグ] 単体でコンパイル不能**。`ll` が未定義(format.cpp の `using ll = long long` が先に来る前提になっている)。LLONG_MAX に必要な `<climits>` もない。ライブラリファイルは自己完結という他ファイルの規約に反する。
- **[注意] AVX-512 命令(_mm512_*)を使用**。AtCoder の現行ジャッジ(AMD EPYC 7763、Zen3)は AVX-512 非対応で、-mavx512f でコンパイルしても実行時に SIGILL になる。実戦で使うなら _mm256(AVX2)版に書き直す必要がある。
- **[注意]** 内側ループで d[k*n+j] == INF をスキップしないため、負の辺重みがあると `d_ik + INF` が INF 未満になり INF 近傍のゴミが距離として残る。非負重みなら問題ない(min で INF が保たれる)。
- n の8の倍数への切り上げとパディング行の扱いは正しい(パディング頂点の対角は INF のままだが参照されない)。
- **[軽微]** ファイル名の floayd は typo。

## warshall_floyd.cpp

- 正しい。多重辺の min 取り、INF スキップによるオーバーフロー回避を確認した。
- add_edge の O(V²) 更新は、非負重みで「新辺を高々1回使う」性質に依拠しており正しい。負重みでは新辺を複数回使う経路を拾えない(実用上は問題になりにくい)。

## warshall_floyd_path.cpp

- 正しい。nxt(次に進む頂点)の初期化、緩和時の `nxt[i][j] = nxt[i][k]`、add_edge の i==s 分岐(`nxt[s][j] = t`)、get_path の復元を確認した。
- add_edge の負重みに関する注意は warshall_floyd.cpp と同じ。
