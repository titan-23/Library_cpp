# titan_cpplib/ds レビュー

===

memo

count_range, range_countの名前や引数名を統一したい

constにできるものはそうしたい

===

全115ファイル精査済み。

重要度は3段階。

- **[バグ]** 誤動作・UB・コンパイル不能につながる
- **[注意]** 特定条件で問題になる
- **[軽微]** 動作に影響しない指摘

## バグ一覧(要約)

| ファイル                                               | 内容                                                                                                     |
| ------------------------------------------------------ | -------------------------------------------------------------------------------------------------------- |
| bbst_node.cpp                                          | _prev のロジック誤り。_next/_prev はループで null 参照                                                   |
| deque.cpp                                              | _rebuild が壊れており、基本操作で誤った要素を返す・要素を失う                                            |
| multiset_sum.cpp / avl_tree_multiset.cpp               | pop() のデフォルト k=-1 で無限降下                                                                       |
| dual_commutative_segment_tree.cpp                      | tovector() const が非 const を呼びコンパイル不能                                                         |
| dual_segment_tree.cpp                                  | コンストラクタ(n, init) が init を無視                                                                   |
| dual_segment_tree_RUQ2.cpp                             | vector 版コンストラクタの時刻 off-by-one で範囲外参照                                                    |
| dynamic_lazy_segment_tree(_array).cpp                  | 完全な重複ファイル。print() の `++id` がコンパイル不能                                                   |
| dynamic_segment_tree_init.cpp                          | update() が欠損子の寄与を座標 mid() で計算(長さでない)                                                   |
| dynamic_list.cpp                                       | access/pop/set が bool 型のままで T が 0/1 に切り捨て                                                    |
| dynamic_wavelet_matrix.cpp                             | long long 版 bit_length が __builtin_clz(32bit) を使用                                                   |
| min_heap.cpp / max_heap.cpp                            | _down の `i<<1\|1 < n` が優先順位誤りで範囲外アクセス                                                    |
| offline_RUQ.cpp                                        | nxt のサイズ不足で範囲外アクセス                                                                         |
| lazy_rbst.cpp                                          | merge の集約順が非可換モノイドで誤り                                                                     |
| lazy_wb_tree.cpp                                       | set() の `stack = {node}` がコンパイル不能                                                               |
| wordsize_tree_set.cpp                                  | tovector が 0 を詰める。lt の実装誤り。fill の残留ビット                                                 |
| static_set.cpp                                         | StaticSet(missing) で n が未初期化                                                                       |
| wavelet_matrix_bit.cpp                                 | ライブラリファイルに main() が残存                                                                       |
| partial_persistent_union_find.cpp                      | `PartialPersistentArray` に既定コンストラクタがなく、`PartialPersistentUnionFind()` がインスタンス化不能 |
| persistent_set.cpp / persistent_multiset.cpp           | 空 vector で _build が範囲外参照。split/pop が未定義の _split_node を呼ぶ                                |
| persistent_multiset.cpp                                | find/remove がコンパイル不能(未定義変数 s, cnt)。add の既存キー経路が copy を木に繋がない                |
| persistent_lazy_wbtree.cpp / persistent_seg_wbtree.cpp | set() がコンパイル不能(stack の list 初期化と emplace_back)。copy() が自己呼び出しでコンパイル不能       |
| multiset_sum_splay.cpp                                 | vector 版コンストラクタと merge() がコンパイル不能(int への nullptr 代入等)                              |
| deletable_heap.cpp                                     | operator<< が未定義変数 action を参照                                                                    |
| cuckoo_hash_table.cpp                                  | 先頭に `raise NotImprementedError`(意図的な未完成品)                                                     |

## 横断事項

- **[注意] クラス名の衝突**。(1) multiset_sum.cpp と multiset_sum_qd.cpp が同名 `MultisetSum`(API も異なる: sum が値基準/添字基準)。(2) dual_segment_tree_RUQ.cpp と RUQ2.cpp が同名 `DualSegmentTreeRUQ`。(3) wavelet_matrix.cpp と wavelet_matrix_bit.cpp が同名 `WaveletMatrix`(テンプレート引数も異なり再宣言エラー)。同時 include で壊れる。
- **[軽微]** `<bits/stdc++.h>` をライブラリ内で使用: dynamic_bit_vector、dycone(規約違反)。
- **[軽微]** デストラクタなし・メモリ解放なしはポインタ系全般。競プロ用途では許容。

## 各ファイル

### area_of_union_of_rectangles.cpp
- **[注意]** `T p_all = seg.all_prod();` の p_all は packed 値(long long)。T=int だと上位ビットが失われ壊れる。`AOUFR::S` で受けるべき。
- **[注意]** X, Y が `vector<int>` 固定で、テンプレート T が int を超える座標に対応していない。
- **[軽微]** cnt を31ビットに詰めるため、区間長の総和が 2^31 以上で壊れる。

### avl_tree_bit_vector.cpp
- 128bit パック AVL の回転時の size/total 更新、挿入分割、削除(_pop_under)の d/fd 管理を確認した。正しい。
- **[注意]** select0/select1 に範囲チェックがなく、k が過大だと番兵ノード0で無限ループする。
- **[軽微]** _build は入力が 0/1 であることを検証しない。
- **[軽微]** 挿入分割時の `_bit_len[path.top()] < _W` 分岐は到達不能(直前に積んだのは満杯ノード自身)。
- FastStack は自動で伸長するため容量30初期値は問題ない(確認済み)。

### avl_tree_multiset.cpp
- **[バグ] pop() のデフォルト k=-1**。find_kth(-1) が左へ降り続け assert 失敗(または null 参照)。pop(-1) は使えない。
- **[軽微]** find_kth 内で long long の valsize を int t に詰めている。index の戻りも int。
- 他は multiset_sum.cpp と同一構造で正しい。デフォルトコンストラクタの missing 未初期化も同様。

### bbst_node.cpp
- **[バグ] _prev**。最後の `now->right->_max()` は `now->left->_max()` であるべき。
- **[バグ] _next/_prev のループ**。`now->par` が null になった後に `now->right` を読む(null 参照)。また `now->right->_min()` はノード側クラスのメソッドを呼ぶ形で、BBSTNode の static 関数は呼ばれない。現状どこからも使われていないため実害は未発生。
- **[軽微]** #pragma once も include もない。

### bloom_filter.cpp
- 正しい。splitmix64 の逐次ストリームで K 個のハッシュを生成する設計も問題ない。
- **[軽微]** insert が重複でも size を増やす(len の意味が「insert 回数」)。

### cuckoo_hash_table.cpp
- **[バグ(意図的)]** 先頭行が `raise NotImprementedError` で、コンパイル不能にしてある未完成ファイル。main とテストコードも同居。rehash が再構築中のテーブルへ挿入する等、実装も未完成。使用しないこと。

### cumulative_sum.cpp
- **[軽微] print() が acc[n-1] を飛ばす**。ループが `i < n-1` で acc(サイズ n+1)を出力し最後に acc.back()=acc[n] を出すため、acc[n-1] が出ない。
- **[軽微]** prod/sum が同一実装(非可逆モノイドに prod の名は不適)。

### deletable_heap.cpp
- **[バグ] DeletableMaxHeap::operator<<**。`os << action.d;` の `action` が未定義。使用した時点でコンパイルエラー。
- **[軽微]** Min 版に Max 版のような空チェック assert がない。

### deque.cpp
- **[バグ] _rebuild が壊れている**。
  - 2つのループの分岐が両方 `front_vec 優先` のため、要素が片側に寄ったまま再分配されない。例: `Deque({1,2,3,4})` に pop_front すると _rebuild 後も front が空のままで、`back_vec.back()=4` を返す(正しくは 1)。
  - `n = total/2` で 2 ループ合計 2n 要素しか移さず、総数が奇数のとき 1 要素消失する。総数1で pop すると空 vector の back() を読む UB。
  - push_front だけ積んで pop_back する場合も同様に誤る。
- 全面的な書き直しが必要。foldable_deque.cpp の rebuild は正しいので、そちらの方式に合わせられる。

### double_ended_heap.cpp
- **[軽微]** 空ヒープでの pop/get は UB(assert なし)。replace_max のコメント「popして返し」と void 戻りが不一致。

### dual_commutative_segment_tree.cpp
- **[バグ] `tovector() const` が非 const の all_propagate() を呼ぶ**。インスタンス化するとコンパイルエラー。
- **[軽微]** n=0 で `__builtin_clz(0)` の UB。

### dual_segment_tree.cpp
- **[バグ] コンストラクタ(n, init) が init を使っていない**。_data はデフォルト構築のまま。
- **[軽微]** print() が n=0 で get(-1) を呼ぶ。n=0 で構築すると clz(0) の UB。

### dual_segment_tree_RUQ.cpp
- **[注意]** RUQ2.cpp と同名クラスのため同時 include 不可。

### dual_segment_tree_RUQ2.cpp
- **[バグ] vector 版コンストラクタの off-by-one**。構築後は `time == n`、`stamp.size() == n` であり、この状態で apply すると `data = time = n+1` に対し stamp の最大添字は n となり、get/tovector が `stamp[n+1]` を範囲外参照する。構築終了時の time を n-1 に合わせる(または apply 側の順序を (n, init) 版と揃える)必要がある。

### dual_segment_tree2D_RUQ.cpp
- タイムスタンプ2次元の apply/get/tovector とも正しい(stamp と data 添字の同期を両コンストラクタで確認)。
- **[軽微]** 空間は最悪 16hw 語(2べき切り上げ×4)。コメントの O(hw) は係数が大きい。

### dycone.cpp(OfflineDynamicConnectivity)

yosupo 提出の移植。処理を全て追った。想定入力(自己ループ・多重辺・存在しない辺の削除がない)の範囲で**ロジックは正しい**と判断した。

**アルゴリズムの構造**

- オフライン2パス。1パス目でクエリを記録し、辺の重みを「削除時刻の符号反転」で確定する。追加時刻 t_add の辺が時刻 t_del で消えるなら S[t_add] = -t_del、消えない辺は -INT_MAX。run() が再生する。
- この重みでの**最小全域森**を維持する。重みが小さい = 削除が遅いので、森は長生きする辺を優先して保持する。閉路ができたら max_edge で経路上の最大重み辺を探し、新辺の方が軽ければ差し替える(inner_add_edge)。木辺が削除される時刻には、カット性質よりそのカットを跨ぐ生存辺が存在しないことが保証され、group_count の増減が正確になる。
- 森の表現はレベル付き union-find。P[u], W[u] は「u のグループは重みレベル W[u] で P[u] のグループに併合される」を意味し、根への経路上で W は増加列になる。find(u, w) は W[u] <= w の間だけ登ることで「重み w 以下の辺だけを見た代表元」を返す。クエリは w=0 で全体の連結性になる(実辺の重みは全て負、根の W は 1)。
- sum[u], sz[u] はポインタ木での u の部分木集約。付け替えのたびに差分で維持し、根の値が成分全体の値になる。

**確認した処理**

- find の圧縮 `W[P[u]] <= W[u]`。低いレベルでの辺差し替え後は経路上に増加列でない箇所が生じるが、「レベル W[u] の時点で P[u] は既に P[P[u]] へ併合済みなので u は直接そこへ付く」という意味で正当。集約の差分(sum[P[u]] -= sum[u] のみで P[P[u]] は触らない)も、P[P[u]] が u を推移的に含んだままである点と整合する。削除されない辺同士(重み -INT_MAX が同値)もこの圧縮で平坦化され、通常の経路圧縮 union-find として振る舞う。
- sub_add の併合ループ。disconnect で両端の根経路の集約を開いてから、2本の増加鎖を重み順にマージする。各反復の connect(v, w) が直前に付け替えたノードの集約を新しい親へ再加算し、最後の connect(u) が残りを閉じる。各ノードの集約は最終的な親へちょうど1回ずつ加算される。rd(乱数優先度)は付け替え方向を決め、根経路上で rd は単調増加になる。
- sub_del。重みは辺ごとに一意(削除時刻が一意)なので、レベル w のポインタを重み一致で探して外すだけでよい。外した u の部分木の集約を全先祖から引く処理も正しい。レベル w のポインタは両端点のどちらかの根経路に必ず載るため、inner_delete_edge が両側から呼ぶ構造で網羅される。森にない辺の削除は両側とも空振りで無害。
- max_edge。find 2回で両経路が正規化された後、重みの小さい側を進めて合流点を見つける。返るのは合流レベルのポインタで、u-v パス上の最大重み辺に一致する。

**指摘**

- **[注意] 自己ループで構造が壊れる**。add_edge(u, u) は max_edge(u, u) が u の親辺を返し、差し替え条件 W[p] > w を満たすと実在の木辺を削除した上で sub_add(u, u, w) が早期 return するため、連結性と group_count が狂う。u の親辺に削除予定があれば(W > -INT_MAX)、削除されない自己ループでも発火する。assert(u != v) を入れるべき。
- **[注意] 存在しない辺の delete_edge**。mp の operator[] が値 0 を挿入し、`S[0] = -t` とクエリ0番の重みを破壊する(0番が ADD だと実害)。
- **[注意] 多重辺・二重削除は非対応**。同一辺の再 add は mp を上書きし、1本目が「削除されない辺」扱いになる。同一辺の二重 delete は削除時刻を後の方へずらす。契約をコメント化すべき。
- **[注意] run() は1回限り**。再生が構造を破壊的に更新するため、2回呼ぶと不正な結果になる。
- **[注意] 計算量の上界は未導出**。各操作は根までの鎖長に比例する。鎖長は成分に残る相異なる削除時刻の数で抑えられ、-INT_MAX の辺は圧縮で潰れる。乱択と圧縮込みのならし計算量の厳密な証明は本レビューではしていない(出典 AC と実測が根拠)。
- **[軽微]** GET_COUNT / GET_SIZE / IS_SAME の結果も vector<T> に入る。T が整数型以外だと型が不自然。
- **[軽微]** inner_add_edge の2回目の `sub_del(P[p], p, W[p])` は、1回目で P[p]=p, W[p]=1 に書き換わった後に引数が評価されるため常に空振りする。実害はないが死にコード。
- **[軽微]** disconnect が再帰で、深さは鎖長に比例する。
- **[注意]** `namespace titan23` 外・`<bits/stdc++.h>`・#pragma once なしと、規約に沿っていない。
- **[軽微]** メンバ宣言順と初期化リスト順が不一致(-Wreorder)。

### dynamic_bit_vector.cpp
- **[注意] 計算量**。バケット上限1000固定で再平衡なし。バケット数が n/500 まで増え、get_bucket/rank が O(バケット数) の線形走査のため、大きい n では O(√n) にならない。
- **[軽微]** `_access_pop_and_rank1` は k が範囲外だと未初期化変数を使う。`prek` 未使用。

### dynamic_fenwick_tree2D.cpp
- **[注意]** アロケータが static のため、同一テンプレート引数の全インスタンスがプールを共有する。`ma.reset()` を直接呼ぶと他インスタンスが壊れる(木の reset() は安全)。

### dynamic_fenwick_tree2D_RAQ.cpp / dynamic_fenwick_tree2D_RAQRSQ.cpp
- 2次元差分/重み付き差分(d0..d3)の式を検証した。正しい。
- **[軽微]** `x*h*w` の途中桁あふれは W 頼み(T が大きいとき注意)。static アロケータ共有は同上。

### dynamic_lazy_segment_tree.cpp / dynamic_lazy_segment_tree_array.cpp
- **[バグ] 2ファイルが完全に同一内容**。同名クラスなので両方 include すると再定義エラー。_array 版に配列化の意図があるなら未着手。
- **[バグ] print()**。`for (IndexType i = 0; i < u; ++id)` の `++id` は関数ポインタのインクリメントでコンパイル不能。
- 本体(childless ノードの pow 集約、propagate の子生成、prod2 の合成)は正しい。「子は両方あるか両方ないか」の不変条件も成立している。

### dynamic_lazy_segment_tree_util.cpp
- **[注意]** 名前空間スコープで `pst(1e9, 1)` の構築と `pst.prod(l, r)` を実行する例示コードであり、include すると静的初期化時に走る。ライブラリとしては雛形をコメント化すべき。
- **[注意]** count を下位30bitに詰めるため u と和の上限が厳しい(count < 2^30、和は上位34bit)。u=1e9 は count がほぼ上限。

### dynamic_list.cpp
- **[バグ] bitvector からのコピー残り**。`bool access(int k)`、`T pop(int k)` 内の `bool res = ...`、`void set(int k, bool v)`。DynamicList<int> 等で値が 0/1 に切り捨てられる。3箇所とも T に直す必要がある。

### dynamic_segment_tree.cpp
- **[軽微]** print() のループ変数が int(IndexType が long long のとき破綻)、かつ u 全域を出力するので実用性が低い。prod に範囲 assert なし。

### dynamic_segment_tree_init.cpp
- **[バグ] Node::update()**。欠損子の寄与を `pow(dseg->init_val, mid())` で計算しているが、mid() は座標であり長さではない。l=0 のノード以外で誤った集約になる。正しくは左が `mid()-l`、右が `r-mid()`(いずれも `(r-l)/2`)。

### dynamic_wavelet_matrix.cpp
- **[バグ] bit_length のオーバーロード**。long long / unsigned long long 版が `64 - __builtin_clz(n)`(32bit 用)を使っており、sigma=5LL で _log=35 になるなど誤る。`__builtin_clzll` にすべき。
- **[注意]** コンストラクタ間の不整合: sigma のみの版は `bit_length(sigma-1)`、vector 版は `bit_length(sigma)`。
- **[軽微]** insert/kth_smallest に assert がない(DynamicWaveletTree にはある)。
- 本体のロジックは静的 WM と同型で正しい。

### dynamic_wavelet_tree.cpp
- **[注意]** select/select_remove は x が存在しない場合に null 参照または AVLTreeBitVector::select の無限ループに至る。assert か存在チェックが要る。
- **[軽微]** insert が最下層の下にも空ノードを作る(1レベル分の無駄)。pop で空になったノードを解放しない。

### euler_tour_tree.cpp
- **[軽微]** build() のコメント「O(logn)」は O(n) の誤り。cut で辺ノードのメモリは解放されない。

### fenwick_tree2D.cpp
- **[注意] 単一引数 sum(h, w) の assert が過剰**。`h < _h` だが prefix 排他境界としては `h <= _h` が正しく、最終行/列の get(h,w) (h=_h-1) が assert で落ちる。

### hash_dict.cpp
- **[軽微]** operator[] が V を値で返すため `d[k] = v` ができない(誤解を招く API)。contains_set は名前に反して chmin 動作。

### icpc_lazy_rbst.cpp
- **[軽微]** グローバルに `struct Random` と `trnd` を置く(名前空間外)。コメント内の `Node(key, id())` や `propagate(node)` はコンストラクタ/シグネチャと不一致(復活時に修正が要る)。

### lazy_link_cut_tree.cpp / link_cut_tree.cpp / link_cut_tree_sum.cpp
- **[注意]** cut は根に対して呼ぶと `c->left` が null で UB(assert なし)。
- **[軽微]** _splay の zig 判定が propagate 前のポインタで行われるが、_rotate 内で propagate 後の実ポインタを使うため正しさは保たれる(償却定数がわずかに悪化しうる)。

### lazy_rbst.cpp
- **[バグ] _merge_node の集約順**。r 側へ降りるとき `_update_lr(r, l)` が `r->data = op(r->data, l->data)` とするが、列としては l が前なので `op(l->data, r->data)` が正しい。非可換モノイド(アフィン合成など)で merge/insert/pop が誤る。可換なら影響なし。
- **[注意] reverse は非可換 op で誤り**。rdata を保持していないため、rev を立てても data が鏡像にならない。可換なら問題ない。
- **[軽微]** prod/apply の全被覆判定が `right < r`(`right <= r` でよい)。境界で枝刈りが1段無駄になるだけで正しさは保たれる。
- **[軽微]** static な trnd/path 共有(スレッド非対応)。

### lazy_segment_tree.cpp
- **[注意]** n=0 で `bit_length(-1)` → size が 2^32 相当になり壊滅する。n>=1 を assert すべき。
- **[軽微]** max_right 冒頭の `if (l == size)` は `l == n` の意図(f(e())=true なら実害なし)。operator<< はvector 用 << (print.cpp)に依存するが本ファイルは include していない。

### lazysegutil.cpp
- **[注意]** 区間更新系の ID が numeric_limits::max のため、データに max 値を使うと壊れる(慣習的だが明記なし)。

### linear_cum_sum.cpp
- **[注意] API が未完成**。`T sum(int l, int d, int k, ll a, ll b)` は b が未使用で、a も assert(a==1) 固定。呼び出せる形になっていない。
- **[軽微]** 空間 O(nB)=O(n√n)。コメントに明記なし。

### max_heap.cpp / min_heap.cpp
- **[バグ] _down のループ条件**。`while (i<<1|1 < n)` は演算子優先順位により `(i<<1) | (1 < n)` と解釈される。n≥2 では常に真になり、葉に達したとき `a[u]`(u≥n)の範囲外読み取り、場合により範囲外 swap(書き込み)が起きる。`while ((i<<1|1) < n)` が正しい。両ファイル共通。pushpoop/replace も _down 経由で影響。
- **[軽微]** 空での pop/get は UB。`pushpoop` の綴り。

### multiset_sum.cpp
- **[バグ] pop() のデフォルト k=-1**。find_kth(-1) は左端まで降りて assert 失敗(NDEBUG では null 参照)。負の添字対応をするなら事前に `k += len()` が要る。
- **[注意]** `data`(部分和)の型が T。T=int だと総和が容易にあふれる。bisect_left_sum は負のキーで greedy が壊れる(非負前提を明記すべき)。
- **[軽微]** デフォルトコンストラクタで missing 未初期化。index/index_right の戻りが int(valsize は long long)。

### multiset_sum_qd.cpp
- **[注意]** クラス名が multiset_sum.cpp と同じ `MultisetSum` で、sum の意味も異なる(こちらは添字区間、あちらは値未満)。名前変更を推奨。
- **[軽微]** operator[] が範囲外で return なしに関数末尾へ到達(UB)。

### multiset_topk.cpp
- **[軽微]** 名前は TopK だが実体は「小さい方から K 個」。sum() 呼び出しまで rebuild されない設計は明記した方がよい。

### offline_RUQ.cpp
- **[バグ] nxt のサイズ不足**。`nxt(n)` だが、塗り終えたセルで `nxt[l]=l+1` とした後の find が nxt[n] を読む。n=1 で apply(0,1,v) するだけで範囲外参照。nxt を n+1 要素にし nxt[n]=n とするのが正しい。

### offline_dynamic_connectivity.cpp / offline_dynamic_connectivity_sum.cpp
- 辺区間の分解、undo 順、start/edge_data のパッキングとも正しい。
- **[注意]** `int todo[bit_length(query_count)<<2]` は VLA(GCC 拡張)。query_count=0 だとサイズ0配列に書き込み UB。
- **[軽微]** delete_edge は存在しない辺で find の結果未チェック(UB)。

### old_dynamic_fenwick_tree2D.cpp
- unordered_map ネストの旧版。正しいが遅い。新版(dynamic_fenwick_tree2D.cpp)と役割が重複。

### partial_persistent_array.cpp / partial_persistent_union_find.cpp
- **[バグ] partial_persistent_union_find.cpp がコンパイル不能**。`PartialPersistentArray` に既定コンストラクタがないのに、`PartialPersistentUnionFind()` がメンバ `par` を既定構築しようとするため、インスタンス化でエラーになる。`PartialPersistentArray() {}` を追加すれば解消する。include を補って単体コンパイルできるようにした結果、表面化した。

### persistent_lazy_segment_tree.cpp / persistent_segment_tree.cpp
- **[軽微]** tovector の `vector<T> a(len()); a.resize(_len);` は冗長。copy_from は 0 番兵もコピーする(無害な無駄)。

### persistent_set.cpp
- **[バグ] _build が空 vector で範囲外参照**。`build(0, 0)` が `a[0]` を読む。persistent_wbtree にはある空チェックがない。
- **[バグ] split(k) / pop(k) が存在しない `_split_node` を呼ぶ**。実在するのは `_split_node_key` と `_split_node_idx` のみ。呼んだ時点でコンパイルエラー(`_split_node_idx` が意図と思われる)。
- **[注意]** _build は sort のみで unique しない。重複入力で Set の不変条件が壊れる。
- **[注意]** check() が print.cpp の PRINT_GREEN に依存(このファイルは include しているので可)。
- **[軽微]** balance_check の `!weight_left()*DELTA >= ...` は優先順位誤りで常に素通り(デバッグ関数)。const/非 const の get(int) がロジック違いで二重定義(結果は同じ)。
- add/remove/_split_node_key/_merge_with_root の copy-on-write 経路は正しい。

### persistent_multiset.cpp
- **[バグ] find() がコンパイル不能**。ループ内の `s.emplace(node);` の `s` が未定義。find は add/remove から呼ばれるため、この3メソッドは使えない。
- **[バグ] remove() がコンパイル不能**。`node->cnt_subtree -= cnt;` の `cnt` が未定義(remove(T key) に cnt 引数がない)。
- **[バグ] add() の既存キー経路のロジック誤り**。`node = node->left->copy()` が作った複製を親に繋いでおらず、cnt/cnt_subtree の更新が孤児ノードに落ちる。`node->left = node->left->copy(); node = node->left;` が正しい形。
- **[バグ] split/pop が未定義の `_split_node` を呼ぶ**(set 版と同じ)。
- **[バグ] _build が空 vector で範囲外参照**(set 版と同じ)。
- **[注意] cnt と size の意味が混在**。get(k) const・index・index_right は多重度(cnt_subtree)基準だが、len() と非 const get(k) はノード数(size)基準。_build は重複を cnt に集約せず別ノードで持つ。多重度 API とノード数 API が同居しており、どちらの意味でも一貫しない。
- **[軽微]** balance_check の優先順位誤り(デバッグ関数)。

### persistent_lazy_wbtree.cpp
- **[バグ] set() がコンパイル不能**。`stack<SizeType> path = {node};` は std::stack の explicit コンストラクタにより不可。さらに `path.emplace_back(node);` は stack に存在しないメンバ。
- **[バグ] copy()**。`return _new(copy(root));` が自分自身(引数なし const メンバ)を引数付きで呼ぶ形でコンパイルエラー。`ma.copy(root)` が意図。
- **[注意] set() は k が根の位置のとき番兵を汚す**。初回ループで `pnode = 0` のまま `ma.tree[0].right = node` を書く。size[0]=0 のため実害はほぼないが、番兵の不変条件が壊れる。
- **[注意] reverse は非可換 op で誤り**。rdata を保持せず、rev 伝播時に data を鏡像に組み替えないため、非可換モノイドでは reverse 後の prod が誤る。可換なら問題ない。
- **[軽微]** prod/apply の全被覆判定が `right < r`(性能のみ)。static アロケータ共有と `ma.reset()`(rebuild)の他インスタンス破壊は persistent 系共通。

### persistent_seg_wbtree.cpp
- persistent_lazy_wbtree から lazy を除いた同系。指摘も同じ。
- **[バグ] set() がコンパイル不能**(stack の list 初期化・emplace_back)。
- **[バグ] copy() の自己呼び出し**(`_new(copy(root))`)。
- **[注意] reverse は非可換 op で誤り**(rdata なし)。set() の番兵書き込みも同じ。

### multiset_sum_splay.cpp
- **[バグ] vector コンストラクタがコンパイル不能**。`root = nullptr;`(root は int)、`ma.d[node].left->par`(int に ->)、`update(ma.d[node])`(int 引数に Node& を渡す)の3点。
- **[バグ] merge() がコンパイル不能**。`other.root = nullptr;`(int への nullptr 代入)。
- **[注意] clear() が共有プールを破壊する**。static な ma を reset するため、同じ型の他インスタンスが全て無効になる。
- **[注意]** count_sumlim は非負要素前提(負があると greedy が壊れる)。コメントに明記がない。
- **[軽微]** `<bits/stdc++.h>` 使用。`d.size() > ptr` の符号比較警告。

### sparse_table.cpp / sparse_table_min.cpp
- **[注意]** SparseTable は重なり合う区間を op するため冪等演算限定だが、その旨のコメントがない(sum を渡すと誤る)。
- **[軽微]** min 版: n=0 で clz(0) UB。

### splay_node.cpp
- **[注意] split(node, key) の null 参照**。全要素が key より大きいとき find_splay が nullptr を返し、直後の `node->key` で落ちる。

### sqrt_segment_tree.cpp
- **[軽微]** all_prod の `s = s = op(s, t)` の二重代入。set が O(√n)(可逆なら O(1) 化可能)。

### static_multiset.cpp / static_set.cpp
- **[バグ] StaticSet(T missing) が n を初期化しない**(このコンストラクタで作ると len() が未定義値)。
- **[軽微]** デフォルトコンストラクタの missing(-1) は数値以外の T で不成立。

### std_set.cpp
- **[軽微]** デフォルトコンストラクタの missing(-1)。get_min/get_max は空で UB。

### wavelet_matrix_bit.cpp
- **[バグ] ファイル末尾に main() が残っている**。include すると main 重複でリンクエラー。
- **[注意]** クラス名が wavelet_matrix.cpp と同じ WaveletMatrix でテンプレート引数が異なるため、同時 include で再宣言エラー。
- **[軽微]** `#pragma unroll` は GCC では未知プラグマ。

### wavelet_matrix_2d_sum.cpp
- 同じ座標への登録を別々の点として保持する、静的な2次元点集合の総和版。
- `range_sum`, `count_sum_lt`, `range_count`, `kth_y`, `sum_k_smallest_y`, `sum_k_largest_y` を持つ。
- x・yとも座標圧縮しているため負の座標を扱える。総和APIでは負の重みも扱える。

### wavelet_matrix_2d_min.cpp / wavelet_matrix_2d_monoid.cpp
- min版は各レベルの静的RMQにより `range_min/max`, `range_argmin/argmax` を `O(log n)` で処理する。同値なら先に登録した点を選ぶ。
- monoid版は各レベルのSegment Treeにより長方形内の `range_prod` を `O(log^2 n)` で処理する。演算は可換モノイドに限定される。

### wb_tree.cpp
- **[注意]** `ALPHA`/`BETA` がグローバル(名前空間外)の const で名前汚染。
- **[軽微]** pop 内の d 二重宣言(外側未使用)、set() の `pnode->left = node = node->left` などの無意味な再代入。

### wb_tree_seg.cpp
- **[軽微]** balance_check の優先順位バグ(lazy_wb_tree と同じ、デバッグ関数のみ)。set() の root シャドウ。

### lazy_wb_tree.cpp
- **[注意] reverse は非可換 op で誤り**(lazy_rbst と同じ。rdata を保持していない)。
- **[バグ] set()**。`stack<NodePtr> path = {node};` は std::stack の explicit コンストラクタによりコンパイル不能。呼ぶとエラー。
- **[軽微]** balance_check の `!weight_left()*DELTA >= ...` は優先順位誤りで機能していない(デバッグ関数)。
- **[軽微]** prod/apply の全被覆判定 `right < r`(lazy_rbst と同じ、性能のみ)。

### wordsize_tree_set.cpp
- **[バグ] tovector()**。`a[idx] = 0;` としており全要素が 0 になる。`a[idx] = v;` が正しい。
- **[バグ] lt()**。`return le(v+1);` は「v 以下」を返し得る。`v == 0 ? -1 : le(v-1)` が正しい。gt は正しい。
- **[注意] fill(n)**。全レベルを全ビット1にした後、data[0] の最終ワードしかマスクしない。n が「最終ワードに届かない」場合、n..u-1 のビットが立ったままになる。n==u(全埋め)以外では使えない。
- **[軽微]** remove の引数型が u64(他は int)。
