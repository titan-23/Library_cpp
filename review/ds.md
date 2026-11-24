# `titan_cpplib/ds` 理論レビュー

## 全ファイルの確認結果 (2026-09-03)

対象コミット `0859f40bb7e4f8f56478b227815cddc62a7753ea` の `.cpp` 122 件と `range_set_design.md` の計 123/123 件を、以下の旧本文を参照する前に全文再読しました。コンパイル、テスト、ベンチマークは実行していません。

### 現在も利用停止を推奨する実装

- `cuckoo_hash_table.cpp:4,30-42,94-99,123-128`: C++ ではない構文、同じファイル内の `main`、正しく動作しない `rehash` と `size` があります。
- `multiset_sum_splay.cpp:203-215`: `rep`, `Node`, `NodeId` が未定義であり、`child` の型にも誤りがあるため、`vector` を構築できません。
- `dual_commutative_segment_tree.cpp:25,110-113`: 遅延値の型が F ではなく T であり、`const` の公開関数から非 `const` の伝播処理を呼びます。
- `partial_persistent_union_find.cpp:14,18-22`: 既定コンストラクタのない `PartialPersistentArray` を後から代入するため、構築できません。
- 永続 WB 木群: `copy` や `set` を実体化できない問題、根の更新が失われる問題、非可換演算に対する反転の誤り、記憶領域の初期化後に古い木を走査する問題があります。
- 動的遅延セグメント木の 2 つの実装: `Node::lazy` を初期化しないまま `composition` を呼び、`print` は関数名 `id` を加算しようとします。

### 旧本文へ追加する確定事項

1. `lazy_wb_tree.cpp:396-402` の `stack<NodePtr> path={node}` は有効なコンストラクタ呼出しではないため、`set` を実体化できません。
2. `persistent_lazy_wbtree.cpp:417-452`, `persistent_seg_wbtree.cpp:374-409` には、`copy` / `ma.copy`、`const` な複製処理、不正な `stack` の初期化、存在しない `emplace_back` の使用が混在しています。
3. `fenwick_tree_RAQ.cpp:60-61`, `fenwick_tree_RAQRSQ.cpp:88-89` の出力演算子は、単体で読み込んだ場合には存在しない `ostream << vector<T>` を必要とします。
4. `multiset_sum_qd.cpp:104-121` は存在しない値を `discard` しても総和を先に減らします。また `remove` の副作用を `assert` の中に記述しているため、`NDEBUG` 指定時は何も削除しません。
5. `binary_trie_multiset.cpp:65-75,211-212,382-408` は既定状態が不定です。集合版も含め、ビット幅外の XOR と上限の計算が公開関数の間で一致しません。多重集合版 `binary_trie_multiset.cpp:66-75` と集合版 `binary_trie_set.cpp:76-80` の `(T)1<<bit` は、狭い T への切捨てや符号ビットの移動も起こすため、十分広い符号なし型で確認するようにします。
6. `deletable_heap.cpp:47-52,113-118` では、空の状態で削除を指定した値を後から追加すると削除の指定と相殺され、追加した要素が削除されます。
7. `lazy_rbst.cpp:87-113` は右を根にする併合で `op(right,left)` を保存します。`:55-61,224-231` の反転も子の交換だけで、非可換な集約値を反転しません。
8. `segment_tree2D.cpp:55-94` は更新と問い合わせで積の順序が逆転し、説明にない可換性を必要とします。
9. `lazysegutil.cpp:30-82` は最大値を「作用なし」を表す値として使用するため、その値自体を代入できません。`lazy_segment_tree.cpp:114-142` は余分な葉へも作用し、番兵値に対する算術演算で桁あふれを起こします。
10. `persistent_segment_tree.cpp:211-227`, `persistent_lazy_segment_tree.cpp:269-286` の `copy_from` はコピー元の長さを確認しないため、長さの異なる木をコピーすると木の形が不正になります。
11. `dynamic_segment_tree.cpp:51-53,84-87` と `dynamic_segment_tree_init.cpp:58-60,98-102` は、2 の累乗への切上げ、ビットシフト、中点の計算が大きな上限で桁あふれし、論理上の上限も次の 2 の累乗まで広げます。
12. `wordsize_tree_set.cpp:44-50,79-85` は `WordsizeTreeSet(0)` の公開上の上限を 1 としながら保存領域を確保せず、`add(0)` が範囲外参照になります。
13. `sparse_segment_tree2D.cpp:58-91,128-141` は `set` だけ座標のチェックがなく、負の座標を左端の葉、上限以上の座標を右端の葉へ対応させます。
14. `dycone.cpp:182-193`, `dycone_sum.cpp:240-253` は `run` 後の最終状態と問い合わせ列を残すため、2 回目の `run` を初期状態から再生しません。総和版は更新も重複して適用します。
   また `dycone.cpp:128,207-209` と `dycone_sum.cpp:166,273-275` は既定コンストラクタで `group_count_` を初期化せず、直後の `group_count()` が不定値を参照します。
15. `imos.cpp:20-47` は差分列そのものを `build` で累積するため、再度の `build` または `build` 後の更新で、元とは異なる列になります。
16. `area_of_union_of_rectangles.cpp:41-103` は型 `T` の座標を `int` へ縮小します。空入力では `ZX.len()-1` の -1 が巨大な `vector::size_type` へ変換され、通常は `vector` の構築に失敗します。これを回避しても、空の `nX.back()` は不正です。幅が正で高さが 0 の矩形では、イベントの処理順が不正になります。圧縮後の `y` の添字が 0 なら区間が残り、それ以外なら追加前に削除され、他の矩形が覆わない部分の被覆数が負になります。その結果、`:25` では負値の左シフトも起こります。面積の積が値型の範囲に収まることも確認しません。
17. `dynamic_wavelet_matrix.cpp:180-201` の `topk` は、`l<r, k=0` のとき全種類を返し得ます。深さを `char` へ格納するため、`char` が符号なしの環境では、空でない区間の探索が葉へ到達する直前に深さ 255 となり `_v[255]` を範囲外参照します。`_log=0` なら空区間でも同じです。`char` が符号付きの環境では、`_log=0` の空区間にも件数 0 の項目を返します。`:204-221` の `select` は位置をキー型 T へ格納するため、長い列と狭いキー型の組合せでは値を切り捨てます。
18. `wavelet_matrix_bit.cpp:194-209` は、負の `upper` や表現ビット幅以上の `upper` を下位ビットだけで扱い、`range_freq` の結果に誤りが生じます。キーが非負という条件であれば、`upper<=0` で 0、`upper>=2^log` で区間長を返す必要があります。`:185-191` の公開関数 `sum` は先頭に無条件の `assert(false)` があり、`assert` が有効な場合は全ての呼出しを停止します。
19. `wavelet_matrix.cpp:45-50`, `dynamic_wavelet_matrix.cpp:54-56`, `wavelet_matrix_sum.cpp:298-315`, `wavelet_matrix_fenwick.cpp:291-309`, `dynamic_wavelet_tree_sum.cpp:356-376` は `sigma-1` をチェックより先に計算し、減算結果が型の範囲外になる値で桁あふれします。先頭の 2 つの実装には、正の値であることを確認する `assert` 自体もありません。
20. `offline_RUQ.cpp:15-52` は Union-Find の添字に値型 `T` を使用するため、`T=string` では実体化できず、狭い整数型では添字がラップアラウンドします。
21. `partial_persistent_array.cpp:39-45` は `t<-1` のとき `upper_bound` の先頭より前を参照します。Union-Find 側との時刻に関する条件も一致しません。
22. `std_multiset.cpp:31-54,98-105` は負の個数を渡すと、内部の要素数と頻度を逆向きに更新します。
23. `static_set.cpp:126-131`, `std_set.cpp:97-103`, `std_multiset.cpp:135-141` は近傍距離を値型のまま減算し、整数の端点で桁あふれします。
24. `dynamic_fenwick_tree2D.cpp:68,71-73,115-139` は公開上のサイズへさらに 1 を加え、点を扱う公開関数で元の終端座標を要素として受理します。`dynamic_fenwick_tree2D_RAQRSQ.cpp:82,85-98` も終端を 1 つ超える区間まで受理します。両実装の `h+1`, `w+1` は型の上限で桁あふれし、後者の `:208-215` は重み型 `W` との演算より先に `h*w` を計算します。旧版 `old_dynamic_fenwick_tree2D.cpp:19-40,44-52,71-80` は既定コンストラクタのサイズが不定で、終端点への `add` は何も行わずに終了し、終端点への `get` は内部の `assert` に違反します。
25. 同じ 2 次元 Fenwick 木群 `dynamic_fenwick_tree2D.cpp:15-22,32-49,58-68,143-147,201-215` と `dynamic_fenwick_tree2D_RAQRSQ.cpp:29-36,46-63,72-82,102-106,219-233` は、静的な記憶領域を全てのインスタンスで共有し、暗黙のコピーがサイズと根の番号を複製します。空でない木のコピーは既存ノードを共有し、更新が双方へ波及します。一方を `reset` すると共有ノードがフリーリストへ登録され、その後のノードの再利用で他方も不正な状態になります。さらに記憶領域のノード番号、次の番号、フリーリストにも座標型 `T` を使用するため、狭い `T` ではノード数の増加でラップアラウンドするか桁あふれします。ノード番号の型を座標型から分離する必要があります。
26. `linear_cum_sum.cpp:21-65` は `d=0` で `l%d`, `end/d`, `l/d` が整数のゼロ除算となり、表 `S[0]/Si[0]` も構築されません。`k=0` も空和を返さず要素を参照し得るため、関数の先頭で 0 を返す必要があります。それ以外では `d>0,k>0,l>=0` が必要です。
27. `dynamic_lazy_segment_tree_util.cpp:13-39` は負の更新値を左へビットシフトし、未定義動作になります。30 ビットへ詰め込む方式も長さと和の桁が混ざるため、構造体の別々のメンバーへ分けるようにします。
28. `icpc_lazy_rbst.cpp:17` の `trnd`、上記の補助実装 `:13-44` の非 `inline` 自由関数と大域変数、`area_of_union_of_rectangles.cpp:16-35` の AOUFR 自由関数 5 個は、複数の翻訳単位から読み込むと一意定義規則に違反します。補助実装の末尾には、例示用の木、座標、問い合わせの結果まで名前空間の変数として定義されており、読み込むだけで動的初期化と不要な大域状態が追加されます。
29. `DynamicLazySegmentTree`, `DualSegmentTreeRUQ`, `DynamicFenwickTree2D`, `MultisetSum`, `WaveletMatrix` の各系列の別実装が、同じ名前空間で同名クラスを再定義します。
30. `cumulative_sum2D.cpp:24-29` は入力配列の行数が `h` 未満、または参照する行の列数が `w` 未満なら範囲外参照し、容量の積を `int` で計算して桁あふれします。
31. `wb_tree_seg.cpp:41-50` の `!weight_left()*DELTA >= ...` は演算子の優先順位に誤りがあり、不平衡を検出しません。
32. 多数の公開型は、既定構築後に有効な空状態にならず、直後の公開関数が不定値を参照するか、未構築の保存領域を参照します。
   - 既定構築の直後に、値を返す公開関数を呼ぶだけで問題が表面化するものは、`union_find.cpp:15,19,68-70`, `union_find_advance.cpp:17-23,81-83`, `weight_union_find.cpp:13-19,64-66`, `undoable_union_find.cpp:13-19,61-63`, `undoable_union_find_sum.cpp:14-21,90-92`, `cumulative_sum.cpp:15,19,49-50`, `bit_vector.cpp:15-20,102-104`, `sparse_table.cpp:16,21,62-63`, `sparse_table_min.cpp:18,22,59-60`, `disjoint_sparse_table.cpp:15,19,68-69`, `multiset_sum.cpp:41-43,191,273-334,428-430`, `multiset_sum_qd.cpp:22-26,57,133-172`, `static_RmQ.cpp:18-19,57,88-90` です。
   - 空列として使用すると、未初期化のサイズを参照するか、未構築の保存領域を参照するものは、`fenwick_tree.cpp:14,23,101-105`, `segment_tree.cpp:15-20,73-75,133-138`, `merge_sort_tree.cpp:15-25,49-61`, `dual_segment_tree.cpp:20-24,111-120`, `dual_segment_tree_RUQ.cpp:16-20,68-82`, `dual_segment_tree_RUQ2.cpp:17-22,73-83`, `dual_commutative_segment_tree.cpp:24,53,135-143`, `wavelet_matrix.cpp:16-20,43,262-263`, `wavelet_matrix_bit.cpp:17-20,49,52,273-274`, `sortable_array.cpp:25,239,348-365`, `sortable_segment_tree.cpp:26-31,257,384-406`, `offline_RUQ2D.cpp:21-29,86,117-120`, `cumulative_sum2D.cpp:19-23,39-47`, `fenwick_tree2D.cpp:14-17,69-78`, `segment_tree2D.cpp:16-21,73-94`, `sparse_segment_tree2D.cpp:13-14,115,138-141`, `sparse_segment_tree2DFAST.cpp:15-16,70,104-116`, `dual_segment_tree2D_RUQ.cpp:18-29,117-145` です。
   - 同種の問題は `linear_cum_sum.cpp:16-19,39,49-65`, `dynamic_segment_tree_init.cpp:21-23,64-70,96,105-106`, `static_range_mode_query.cpp:19-25,58,84-135` にもあります。空構造として全メンバーを初期化するか、代入専用なら既定コンストラクタを削除して明記するようにします。
33. `wavelet_matrix_bit.cpp:14,19,50` はテンプレート引数 `log` を制限せず、コンストラクタで `1ull << log` を計算します。`WaveletMatrix<uint64_t,64>` では型のビット数と同じ 64 だけ左へシフトして未定義動作になります。`0<=log && log<64` と `log<=numeric_limits<T>::digits` をコンパイル時にチェックするか、64 ビット全幅を別に処理する必要があります。

このほか、`dual_segment_tree.cpp:26-43,117-120` は n を指定するコンストラクタの `init` を格納せず、`tovector` が論理上の長さではなく内部の 2 の累乗の長さを返します。`dual_segment_tree_RUQ2.cpp:28-35,41-52` は `vector` 入力のコンストラクタの後、最初の更新で時刻添字を 1 つスキップし、`stamp` を範囲外参照します。`offline_dynamic_connectivity.cpp:73-81,89-105` は存在しない辺の削除で末尾の反復子を参照し、問い合わせ 0 件では長さ 0 の作業配列へ値を格納します。

`lazy_segment_tree.cpp:51-63` は `n=0` で `bit_length(-1)` が `int` のビット幅となり、`1 << ビット幅` という型の幅以上のビットシフトへ進みます。`RangeSet` の隣接併合の判定自体は `x±1` を使用しませんが、点からの構築、追加、削除を行う `range_set.cpp:92-94,127-129,171-173` の `x+1` と、範囲用の公開関数 `:40-55,100-123,132-167,187-191,439-443` の `r-l` などは型 `T` の中で先に評価され、どちらも整数の端点で桁あふれします。

### `gomi` とリポジトリ直下のテスト

`titan_cpplib/gomi/**` 6/6 件も再読しました。`gomi_link_cut_tree.cpp:234-257` は遅延値を `id` の関数ポインターと比較し、併合の成功時に連結成分数を減らしません。永続遅延 WB 木 2 件には `copy` / `set` を実体化できない問題と根の更新が失われる問題があり、永続配列・永続セグメント木群には空入力の参照外し、WB 木の診断処理には `!` の優先順位の誤りがあります。実験用であれば公開ライブラリから隔離するようにします。

直下の `test/**` 38 件も静的に確認しました。DS 側では DyCone のテストとベンチマークの 4 ファイルにある 5 つの読み込み先が旧名 `dy_cone*.cpp` のままで、コンパイルできません。`test/dwm/gen.py` は `10**18` までの値を生成しますが、読み込む側は `int` です。`test/dset/mk_test.py` は 100000 件と宣言しながら命令 3 を出力せず、実際の件数が少なくなります。`test/sta_se/a.cpp:64` は 1 件少なく処理し、読み込んだ `StaticSet` を一度もテストしません。`test/dic/main.cpp:156` は主な比較処理がコメントアウトされ、Wavelet Matrix の `topk` を対象とする 2 つのテストは返却件数と順序を確認しません。

`test/dwm/main.cpp:438-450` に埋め込まれた AVL ビット列は、最上位ビットから格納しているにもかかわらず局所添字をそのままビットシフトに使用し、逆の位置へ値を設定します。同 `:855-859` の `DynamicBitVector` は設定時に順位の記録を更新しませんが、現在のテスト処理ではその型自体を使用しません。`test/dset/main.cpp:46-51,109-139` に埋め込まれた `MultisetSum` も、既定状態が不定で、削除後の区画の和を更新せず、範囲外の `operator[]` では戻り値がありません。これらを修正するまで、テスト結果を品質の根拠とすることはできません。

`test/dycone/bench.cpp:26-41` と `bench_conn.cpp:27-41` は、`atoi` の失敗、`n<=0`、`q<0`、不正な動作種別を確認せず、ゼロによる剰余、巨大な領域確保、検証しないままの正常終了になり得ます。`stress.cpp:29-35`, `stress_sum.cpp:30-36` の引数は反復数だけですが、0、不正な文字列、負値を 0 回実行の正常終了として返すため、何もテストしていないにもかかわらず成功として扱われます。

### 定数倍高速化

- `segment_tree.cpp:37-42`, `dual_segment_tree.cpp:26-44`, `dual_segment_tree_RUQ.cpp:21-29`, `dual_segment_tree_RUQ2.cpp:23-29`, `merge_sort_tree.cpp:26-42`, `segment_tree2D.cpp:22-45` は、`bit_length(n)` から容量を求めるため、`n` が 2 の累乗のとき余分な 1 段が生じます。`n=0` と上限を先に確認し、`max(1,n)` 以上で最小の 2 の累乗を `std::bit_ceil` 相当の処理で求めると、格納量は 1 次元で約半分、2 次元で最大約 1/4 になります。`dual_segment_tree.cpp` は下記の `tovector` の返却長を先に修正する必要がありますが、それ以外の公開結果は変化しません。不利な点は、符号なし型での上限計算と `n=0` の処理を各実装でそろえて検証する手間が増えることです。
- `dynamic_fenwick_tree2D.cpp:15-49,143-147,201-215` と `dynamic_fenwick_tree2D_RAQRSQ.cpp:29-63,102-106,219-233` では、所有関係を修正した後も、既存の連続した `vector` と事前の容量確保を維持するようにします。一括した固定長領域にすれば節点確保のコストをさらに小さくできる可能性があり、計算結果は変化しません。ただし必要な最大節点数を見積もりにくく、過大に確保するとメモリ使用量が増加し、領域不足時の処理も必要になるため、実測して選択するようにします。
- `dual_segment_tree.cpp:117-120` の `tovector` は内部容量全体ではなく先頭 `_n` 要素だけを返せば、余分な要素の複製を避けることができます。これは現在の返却長も変更するため、先に公開仕様を修正する必要があります。`dynamic_wavelet_tree_sum.cpp:971-1001` の `tovector` は、ビット列だけが必要であるにもかかわらず各節点で重み列も作成するため、ビット列だけを取り出す処理へ分ければ、結果を変更せずに確保と複製を減らすことができます。

### 現行実装と一致しない旧記述

旧一覧の `deque.cpp` は現存せず、現存する `fixed_deque.cpp` が旧一覧に含まれていなかったため、「122 件」は件数だけが一致します。旧本文にある `segutil` の単位元、`bit_length(0)`、`bit_length` 呼出しの曖昧性は現行で修正済みです。以下の旧本文は履歴として参照し、上記の追加と訂正を優先します。

レビュー日: 2026-08-12
対象: 設計メモの `range_set_design.md` を除く `titan_cpplib/ds` 直下の `.cpp` 122 ファイル

## 結論

このディレクトリを現状のまま「任意のテンプレート引数と公開関数について安全なライブラリ」として扱うことはできません。単独ファイルでも構文上成立しない `cuckoo_hash_table.cpp`、具体的な型で使用するとコンパイル不能になる複数のテンプレート、未初期化値を参照する動的遅延セグメント木、列の順序を保存しない `Deque`、格納方法と更新方法が一致しない `PersistentArray` など、利用を停止すべき問題があります。

一方で、すべての実装に問題があるわけではありません。AVL 木と B 木によるビット列、Disjoint Sparse Table、区間積を保持するスタックと両端キュー、通常の Union-Find 群、`PersistentSet` と `PersistentMultiset`、2 次元 Wavelet Matrix 群などについては、後述する型、値域、所有権の前提の下で、本レビューでは本体の不具合を認めませんでした。

優先順位は次のとおりです。

| 重要度 | 意味 |
|---|---|
| 重大 | コンパイル不能、未初期化値の参照、基本操作だけで不変条件が成り立たなくなる問題 |
| 重要 | 通常の入力でも誤答や未定義動作になる境界、型の縮小、順序の破壊 |
| 注意 | 可換性・冪等性・非負性・一度だけ使用できること・引数を消費することなど、公開されていない必須条件 |
| 改善 | 極端な値、移植性、一意定義規則、計算量表示、診断用関数に関する問題 |

最初に利用を停止すべき対象は次のとおりです。

| 対象 | 判定の要点 |
|---|---|
| `cuckoo_hash_table.cpp` | C++ でない文、正しく動作しない再構築、同じファイル内の `main` があり未完成 |
| `dynamic_lazy_segment_tree*.cpp` | 全節点の `lazy` が未初期化。`print` もコンパイル不能 |
| `multiset_sum_splay.cpp` | 複数のコンパイル不能箇所に加え、総和と所有権の不変条件も不成立 |
| `persistent_array.cpp` | 一括更新がコンパイル不能で、格納方法とも一致しない。空列の出力も未定義動作 |
| `partial_persistent_union_find.cpp` | メンバーを既定構築できず、クラス自体を構築不能 |
| `persistent_lazy_wbtree.cpp`, `persistent_seg_wbtree.cpp` | `copy` と `set` がコンパイル不能。修正しても根の更新・反転・再構築が不正 |
| `deque.cpp` | 再構築が順序を変更し、奇数長では要素を失う |
| `dual_segment_tree_RUQ2.cpp` | `vector` から構築した後の最初の更新で時刻番号の添字が範囲外 |
| `dynamic_list.cpp` | 要素型 `T` を `bool` へ縮小する |
| `dynamic_bit_vector.cpp` | `set` が `rank` と `select` 用の集約値を更新しない |

## 方法と判定基準

ユーザーの指定どおり、テスト、コンパイル、ベンチマーク、対象プログラムの実行は行っていません。次の項目をソース上で追跡しました。

- 木・バケット・永続節点・遅延作用・時刻番号・部分和の表現の不変条件
- `op` の結合順序、単位元、作用の合成順、反転時の順序
- 空、長さ 1、2 の冪の境界、最大値、負値、存在しない要素に対する制御フロー
- テンプレートの宣言が暗に必要とする型変換、演算、符号、ビット幅
- `copy`, `merge`, `split`, `reset` 後の節点の所有権と版の間の共有
- 公称の計算量を成立させる高さ・バケット数・再構築条件

「追加の確定問題なし」は無条件の正しさの証明ではありません。公開コメントから読み取ることができる通常の入力の条件で、コードから反例を構成できる問題を今回認めなかったという意味です。`assert` だけに依存する条件は、`NDEBUG` で無効になることも考慮しました。

## 1. コンパイル・定義・組み込み上の重大問題

### 1.1 未完成またはインスタンス化不能

#### `cuckoo_hash_table.cpp:3,28-39,97-101,111-130`

`raise NotImprementedError` は C++ の文ではなく、ファイル単体で構文エラーになります。さらに `rehash()` は表を空にせずハッシュ用の乱数だけを変更して既存の表へ再挿入し、`insert()` は重複確認より前に要素数を増やします。ハッシュ位置の意味を変更した時点で、既存要素を新しい空の表へすべて再配置しなければ、探索の不変条件を維持できません。テスト用の `main` も同じファイルにあるため、ライブラリの対象から隔離し、全面的に再実装すべきです。

#### `multiset_sum_splay.cpp:197-227`

未定義の `rep`、`int` 型の子に対する `->par`、`update(int)` への `Node` 渡し、`int` 型の `root` への `nullptr` 代入があり、該当部分はコンパイル不能です。これらを修正しても、結果を作成するコンストラクタが総和 `S` を初期化せず、`merge` が `S` を加算せず、`clear` が `S` を 0 にせず、ソートされていない `vector` から二分探索木を作成します。静的なメモリアロケータの `reset` は同じ型の全インスタンスを無効にします。部分的な修正ではなく隔離が妥当です。

#### `partial_persistent_union_find.cpp:13-19`

メンバーの `PartialPersistentArray<int> par` を既定構築してから代入しようとしますが、`PartialPersistentArray` には `vector` を受け取るコンストラクタしかありません。`PartialPersistentUnionFind(int n)` はコンストラクタ本体へ入る前に成立しません。`par(vector<int>(n,-1))` と初期化リストで構築する必要があります。

#### `persistent_array.cpp:114-169`

`multiset()` の 4 引数版 `lower_bound` へ渡す無名関数が 1 引数しか取らず、二項述語になっていません。述語だけを修正しても不正です。`_build`, `get`, `set` は要素 `k` をヒープ添字 `k+1` のビット列に沿って配置する一方、2 種類の一括更新は各節点を区間 `[l,r)` のセグメント木としてたどります。例えば `[10,20]` の添字 0 を一括変更すると、再帰は根の左の子、すなわち元の添字 1 を変更します。格納方式をどちらかへ統一しなければなりません。

同ファイル `219-228` では、空配列でもキューを `{0}` で開始し、長さ 0 の `vector` の `a[0]` へ値を格納します。

#### `persistent_lazy_wbtree.cpp:416-451`, `persistent_seg_wbtree.cpp:373-408`

- `copy(root)` はメモリアロケータの `ma.copy(root)` ではなく、引数なしのメンバー関数 `copy()` への不正な呼出しになっています。
- `copy() const` から `const` でない `_new()` を呼びます。
- `stack<SizeType> path={node}` は有効な `std::stack` の初期化ではありません。
- `std::stack` に存在しない `emplace_back` を呼びます。

これらを機械的に修正しても、更新対象が根なら新しい節点を番兵 0 の右の子へ接続し、返される局所変数 `root` は更新前の複製のままです。根への更新が失われるため、再設計が必要です。

#### `persistent_wbtree.cpp:223,287-288`

`copy() const` から `const` でない `_new()` を呼びます。さらに空木で `ma.copy(0)` すると、0 でない根かつ要素数 0 の擬似節点を作成し、`tovector()` は要素を 1 個返します。空木は根 0 を共有して返すべきです。

#### 補助節点と一部のメンバー関数

- `bbst_node.cpp:55-76`: `_next` と `_prev` が `Node` に存在しないメンバー関数 `_min` と `_max` を呼びます。最大・最小から親へ移動する場合はヌルポインターも逆参照します。また `_prev` の子がある分岐は右部分木ではなく左部分木の最大でなければなりません。
- `lazy_wb_tree.cpp:399-402`: `set()` の `stack<NodePtr> path={node}` が成立しません。
- `dual_commutative_segment_tree.cpp:108-110`: `tovector() const` が非 const の `all_propagate()` を呼びます。
- `dynamic_lazy_segment_tree.cpp:227-230`: 反復変数 `i` ではなく、関数ポインターである非型テンプレート引数 `id` を `++` します。
- `fenwick_tree_RAQ.cpp:59-61`, `fenwick_tree_RAQRSQ.cpp:87-89`: `friend` として定義された出力関数が、このファイル群内では定義されない `vector<T>` の `operator<<` へ依存します。

### 1.2 同名定義と一意定義規則

以下は代替実装であっても同じ `namespace titan23` にある同名クラスであるため、同じ翻訳単位へ任意に `include` できません。

| 型名 | 衝突するファイル |
|---|---|
| `DynamicLazySegmentTree` | `dynamic_lazy_segment_tree.cpp`, `dynamic_lazy_segment_tree_array.cpp` (内容も同一) |
| `DualSegmentTreeRUQ` | `dual_segment_tree_RUQ.cpp`, `dual_segment_tree_RUQ2.cpp` |
| `DynamicFenwickTree2D` | `dynamic_fenwick_tree2D.cpp`, `old_dynamic_fenwick_tree2D.cpp` |
| `MultisetSum` | `multiset_sum.cpp`, `multiset_sum_qd.cpp`, `multiset_sum_wbt.cpp` |
| `WaveletMatrix` | `wavelet_matrix.cpp`, `wavelet_matrix_bit.cpp` |

`icpc_lazy_rbst.cpp:8-16` はヘッダー相当のファイルで `inline` でない大域変数 `trnd` を定義し、複数の翻訳単位から `include` すると一意定義規則に違反します。`dynamic_lazy_segment_tree_util.cpp:12-43` も `inline` でない関数と名前空間直下の使用例を定義し、`include` するだけで構築と問い合わせを実行します。使用例は別ファイルへ移し、定義方法を整理する必要があります。

## 2. 基本操作で不変条件が成り立たなくなる問題

### 2.1 列・ビット列

#### `deque.cpp:18-41`

この両端キューが表す列は `reverse(front_vec)+back_vec` ですが、`_rebuild()` はその順序を維持して二分していません。`{1,2,3,4}` から `pop_front()` すると 4 を返します。総数が奇数なら `2*floor(n/2)` 個しか移さず 1 個消失し、1 要素では両方の `vector` を空にしてから `back()` を参照します。列全体を一度配列へ展開し、前半を逆順で `front_vec`、後半を `back_vec` へ格納する必要があります。

#### `dynamic_list.cpp:83-104`

`access` の戻り値、`pop` の一時値、`set` の引数が `bool` のままです。`DynamicList<int>({2})` の `access/pop` は 1 を返し、`set(0,7)` も 1 を保存します。すべて `T` へ修正する必要があります。

#### `dynamic_bit_vector.cpp:112-116`

`set()` は格納されたビットだけを変更し、バケット内の 1 の個数 `bucket_data` と全体の個数 `tot_one` を更新しません。`[0]` を `set(0,1)` すると `access(0)==1` ですが `rank1(1)==0` になります。旧値との差分を両方の集約値へ反映する必要があります。

同実装は固定長のバケットを線形探索し、小さいバケットの併合や全体の再構築を行わないため、バケット数が Θ(n) になり得ます。`get_bucket`, `rank`, `select` の最悪計算量は O(n) であり、対数時間として扱うことはできません。入力の各バイトも 0 または 1 へ正規化されず、不正な `select` は未初期化の位置を使用するか、値を返さない分岐へ到達します。

#### `bit_vector.cpp:17,24-27`

`build()` が `acc[i+1] += ...` であるため冪等ではありません。同じオブジェクトへ 2 回 `build()` すると `rank` が重ねて加算されます。`acc[i+1]=acc[i]+popcount(...)` とするべきです。既定コンストラクタも `n` と `bsize` を初期化しません。`set` 後には `build` が必要という利用手順も明示されていません。

### 2.2 遅延セグメント木と双対セグメント木

#### `dynamic_lazy_segment_tree.cpp`, `dynamic_lazy_segment_tree_array.cpp:51-75`

`Node` のコンストラクタが `lazy(id())` を初期化しません。`F` が数値型なら不定値であり、最初の区間全体への `apply` だけで `composition(f,lazy)` が不定値を参照します。これは結果の誤りに留まらず未定義動作です。両ファイルは内容が同一であるため、同じ問題があります。

さらに、指定された上限を `1 << bit_length(u_)` へ置き換えるため、2 の冪でも容量を倍にし、本来の上限外を公開関数が受け付けます。大きい符号付き整数型ではシフトも未定義動作になります。公開上の上限と内部容量を分け、安全に 2 の冪へ切り上げるべきです。生ポインターに対するデストラクタやコピー制御もなく、コピーすると同じ木を共有します。

#### `dual_commutative_segment_tree.cpp:23`

作用を保存する配列が `vector<T>` です。作用型は `F` であるため `vector<F>` でなければなりません。`T` と `F` の間に暗黙変換がない正当な指定ではコンパイル不能になり、変換可能でも値を縮小し得ます。`n==0` では `clz(0)` にも到達します。

#### `dual_segment_tree.cpp:24-30,115-145`

`(n,init)` コンストラクタが `init` を使用しません。`tovector()` は公開上の長さ `n` ではなく内部容量 `_size` 個の要素を返し、`print()` は空のとき `get(-1)` を呼びます。各コンストラクタは `n==0` で `clz(0)` にも到達します。

#### `dual_segment_tree_RUQ2.cpp:30-54`

`vector` から構築した直後は `time=n`, `stamp.size()==n` です。最初の更新が `++time` を節点へ格納し、その後に追加される値の添字は `n` ですが、節点は `n+1` を指します。例えば `n=1` で最初の更新後、`get()` は `stamp[2]` を参照します。時刻番号は値を保存した `vector` の実際の添字から直接決定すべきです。

### 2.3 集合・多重集合・ヒープ

#### `bloom_filter.cpp:64-89`

`contains_insert` は未登録時にビットを立てますが `size` を増やしません。その操作だけを使用すると `empty()` が `true` のままで、`clear()` は途中で終了してビットを消去しません。`clear` は無条件にビットを消去するか、`size` の意味を統一する必要があります。通常の `insert` における `size` も、異なる要素数ではなく呼出し回数です。

#### `multiset_sum_qd.cpp:103-121`

`discard` は存在確認より前に `S-=key` を実行します。空集合の `discard(5)` が `false` を返しても `all_prod()==-5` となります。`remove` は処理自体を `assert(discard(key))` の中に記述しており、`NDEBUG` では削除を一切実行しません。状態を変更する処理は `assert` の外で実行しなければなりません。

#### `multiset_sum.cpp:427-432`

空集合の `all_prod()` がヌルの根を逆参照します。`pop()` は既定値 `k=-1` を正規化せず `find_kth(-1)` へ渡すため、`assert` の失敗またはヌルポインター参照になります。多重度は `long long` ですが、順位計算と公開関数が途中で `int` へ縮小します。また `val<=0` を受理するため、要素数の不変条件を満たさない状態にすることができます。

`avl_tree_multiset.cpp` も既定構築時の `missing` が未初期化であり、順位を `int` へ縮小します。`add` と `remove` では、個数が正であることと、存在数を超えて削除しないことを確認すべきです。

#### 二分トライ木

- `binary_trie_multiset.cpp:64`: 既定コンストラクタが `root`, `bit`, `limit` と `vector` を初期化せず、直後の `len()` でも範囲外になります。
- 同 `162-198,311-339`: `get/pop` が順位を確認しません。
- `binary_trie_set.cpp:164-166` と多重集合版 `210-212`: `all_xor` がビット幅外を受理し、`contains`, `get_min` と `get(k)` が異なる値を示します。マスクまたは範囲チェックが必要です。
- `(T)1 << bit` は符号なし整数と有効なシフト幅を必要とします。

#### ヒープ類

- `min_heap.cpp:75-89`, `max_heap.cpp:92-106`: 空の状態で `pushpop` と `replace` が `a[0]` を参照します。`pushpop` は入力値を返すことができますが、`replace` には非空という条件が必要です。
- `double_ended_heap.cpp:110-159`: 空での `pop`, `get`, `replace` は未定義動作になります。内部の `vector` が公開され、取得関数も変更可能な参照を返すため、外部からヒープの不変条件を満たさない状態にすることができます。
- `deletable_heap.cpp:46-52,112-118`: 現在存在しない値を `erase` すると遅延削除用の値が永久に一致せず、記録上の長さと実要素数に差が生じます。現在の多重度以下だけを削除するという条件、または頻度表が必要です。

## 3. 順序・代数的条件の問題

### 3.1 演算の順序を交換できない場合

#### `lazy_rbst.cpp:54-59,86-112`

右の木を `merge` 後の根に選んだ場合も、通りがけ順の走査では常に左の木の後に右の木が続きます。しかし `_update_lr(r,l)` は `op(r,l)` を計算します。1 要素の `[a]` と `[b]` を `merge` するだけで、文字列連結なら `ba` となります。また `reverse` は左右の子を交換しても集約値 `data` を逆順の値へ変更しません。正順と逆順の 2 種類の集約値を保持するか、可換演算だけに限定しなければなりません。

`lazy_wb_tree.cpp:72-78,366-372`, `persistent_lazy_wbtree.cpp:107-140,387-394`, `persistent_seg_wbtree.cpp:102-119,344-351` にも同じ反転処理の問題があります。

#### 2 次元セグメント木

`segment_tree2D.cpp:62-95` は一般の `op` を受け取る形ですが、`set` で更新節点が右の子なら `op(right,left)` を計算し、`prod` も右側の標準区間を単一の累積値へ前から加えます。1 行 `["a","b","c"]` の全積が `"cab"` になり得ます。2 次元長方形には自然な全順序もないため、可換モノイドだけに対応すると明記するのが妥当です。

`sparse_segment_tree2D.cpp` と `sparse_segment_tree2DFAST.cpp` も 2 次元の区間分解を行うため、可換モノイドを必要とします。高速版 `108-113` は右側区間の順序も逆にします。

#### その他の演算条件

| 実装 | 必須条件 |
|---|---|
| `sparse_table.cpp:44-49` | 重なる 2 区間を `op` するため、冪等なモノイドが必要。加算 `[1,2,3]` の `[0,3)` は 8 になる |
| `multiset_topk.cpp:17-57` | 任意要素を `op(product,inv(v))` で除くため可換群が必要 |
| `fenwick_tree.cpp:76-98` | `bisect_left` と `bisect_right` には全要素が非負で、接頭和が単調であることが必要 |
| `b_tree_bit_vector_sum.cpp:586-621` | `min_count_sum_ge` には重みが非負という条件が必要。`[10,-9]` では、目標値 5 へ途中で到達するにもかかわらず、総和だけで到達しないと判定する |
| `undoable_union_find_sum.cpp` | 加減乗除と整数倍、`size` による除算が厳密に元へ戻る型 |
| `dycone_sum.cpp` | 加法群と `T * int`、全中間値がオーバーフローしないこと |
| `wavelet_matrix_*sum` の重み選択 | 重みの接頭和が単調になる非負重み |

`segutil.cpp:35-39` の添字付き最小値・最大値は、単位元が実要素と衝突します。`SegMinIdx<int>` の実要素 `{INT_MAX,5}` と単位元 `{INT_MAX,-1}` では単位元が選択されます。添字に使用する単位元を比較規則に合わせる必要があります。

`lazysegutil.cpp:29-38,63-82` は区間代入の単位元に `numeric_limits<T>::max()` を使用するため、その値そのものを代入できません。代入の有無を別に保持する作用が必要です。また `add`, `chmin`, `chmax` の一部は余分な葉の単位元を保存せず、`LazySegmentTree::all_apply` が余分な葉まで作用させます。例えば `n=3` の加算・最小値では、余分な葉の `INT_MAX+1` が符号付き整数のオーバーフローになります。全体作用は実際の区間 `[0,n)` へ限定するべきです。

## 4. 永続性・所有権・引数を変更する公開関数

### 4.1 再構築の順序

`persistent_lazy_segment_tree.cpp:303-306`, `persistent_lazy_wbtree.cpp:526-529`, `persistent_seg_wbtree.cpp:483-486` は、メモリアロケータを `reset()` してから古い木を `tovector()` します。遅延値や反転フラグがあれば、列へ展開する途中の `propagate()` が添字 1 から節点を再確保し、現在参照している根や子そのものを置き換えます。必ず古い列を先に展開し、その後に `reset` と `build` を行う必要があります。

メモリアロケータはテンプレートの特殊化ごとに静的に共有されるので、`reset` は同じ型の別の版や別インスタンスもすべて無効にします。この意味でもインスタンスのメンバー関数として安全ではありません。

### 4.2 `copy`, `merge`, `split`

- `persistent_lazy_segment_tree.cpp:269-285`, `persistent_segment_tree.cpp:213-229`: `copy_from` が長さの一致を確認しません。長さ 2 の木へ長さ 1 の根を移すと、記録上は 2 要素ですが根は葉になります。
- `persistent_set.cpp:228-230`, `persistent_multiset.cpp:294-296`: 公開された `merge` は `max(left)<=min(right)` を必要とします。`{2}` と `{1}` を `merge` すると二分探索木の順序が成り立たなくなります。和集合ではなく、ソート済みの 2 列を連結する関数なら、その意味が伝わる名前とチェックが必要です。
- `icpc_lazy_rbst.cpp`, `lazy_rbst.cpp`, `lazy_wb_tree.cpp`, `wb_tree.cpp`, `wb_tree_seg.cpp`: `merge` と `split` は節点を直接付け替えますが、元のオブジェクトや `other.root` を残します。節点を共有するオブジェクトが後続の操作で互いを不正な状態にするため、引数の木を消費することを明示して根を無効化するか、ムーブだけを許可する公開関数にする必要があります。
- `dynamic_fenwick_tree2D.cpp` と RAQ, RAQRSQ の派生は、静的メモリ領域と既定のコピー・ムーブにより根を共有します。コピー側の `reset` 後に解放済み節点が再利用されると、元の木の内容が置き換えられます。コピー禁止、または節点を共有しないコピーと所有権を移すムーブが必要です。

`sortable_array.cpp` と `sortable_segment_tree.cpp` は、静的メモリ領域を使用するためコピー禁止という制約を既にコメントに記載しています。上記の実装にもこの種の制約を適用し、同じように明記すべきです。

## 5. 境界、値域、時刻、利用段階

### 5.1 面積・長さ・添字型

#### `area_of_union_of_rectangles.cpp`

`all_prod()` の型は `long long` ですが `T p_all` へ縮小します。`T=int` で同一の単位矩形を 2 回追加すると、整数へ詰め込んだ値は `(2LL<<31)|1` であり、一般的な処理系では縮小後に 1 となるため、最小被覆数を 0 と解釈して面積 0 を返します。座標を保存する `vector` と座標圧縮も `int` 固定であるため、`T=long long` でも `2^32` などの値が失われます。

矩形が 0 個なら `ZX.len()-1` が巨大な `size_t` へ変換されます。高さ 0 の矩形 `d==u` では、削除のイベントが追加より前の帯で処理済みとなり、その後を誤って被覆します。符号付き整数への詰め込みをやめ、`{min_cover,min_length}` を表す構造体、独立した座標型と面積型、空入力と高さ・幅 0 の矩形に対する処理を用いるべきです。

#### `range_set.cpp:39-54,99-122,186-190`

`r-l` を `T` で評価してから `long long` へ加えます。既定の `RangeSet<int>` でも `[INT_MIN+1,INT_MAX)` の長さは、差を求める時点で符号付き整数のオーバーフローになります。`T=long long` の全域は `sum_len` 自体にも収まりません。差を求める前に十分広い型へ変換し、表現可能な範囲を確認する必要があります。

#### 動的セグメント木

`dynamic_segment_tree.cpp:86-114` と `dynamic_segment_tree_init.cpp:100-129` は `1ll << bit_length(u_)` を使用し、2 の冪でも倍へ丸めます。`IndexType=int,u_=2^30` は格納時にオーバーフローし、符号付き `long long` の符号ビットへのシフトも未定義動作になります。`set` には範囲チェックがなく、負値を左端、上限以上を右端の葉へ対応させます。公開上の上限を内部容量とは別に保持する必要があります。

### 5.2 空入力・無効位置

- `lazy_segment_tree.cpp:54-73`: `n=0` で `bit_length(-1)` から `1<<32` へ進み、未定義動作になります。
- `sparse_table_min.cpp:21-23`: 空の vector で `__builtin_clz(0)` に到達します。
- `splay_node.cpp:152-168`: `{5}` をキー 4 で `split` すると `find_splay` がヌルポインターを返し、直後に逆参照します。
- `wordsize_tree_set.cpp:46-52`: 全体の値域が 0 のとき、メンバー `u=1` ですが階層を保存する `vector` は空です。`add(0)` は `assert` を通過して `data[0]` を参照します。コンストラクタと `fill` も入力値を値域と照合しません。
- `sparse_segment_tree2D.cpp:127-140`: `set` だけ座標のチェックがありません。`H=W=1` の `set(5,5,v)` は `(0,0)` を更新します。
- `offline_dynamic_connectivity*.cpp:90-107`: 問い合わせが 0 件のとき長さ 0 の可変長配列を作成し、`todo[1]` へ値を格納します。可変長配列自体も標準 C++ ではありません。
- 同 `74-83`: 存在しない辺を削除すると `find()==end()` を逆参照します。
- `link_cut_tree*.cpp` の `cut`: 表現木の根では `left` がヌルポインターになります。`split(u,v)` も非隣接なら指定した辺ではなく `v` の直前を切断します。親の存在と隣接性が必須条件です。

### 5.3 時刻と一度だけ許される操作

- `dycone.cpp:178-190`, `dycone_sum.cpp:236-252`: `run()` 後も問い合わせ列と最終状態を残すため、2 回目は最終状態から全履歴を再生します。総和版は更新も二重に適用します。一度実行したことを表すフラグで、2 回目を拒否するべきです。
- `imos.cpp:37-47`: `build()` が内部の差分列を接頭和の列へ破壊的に変更します。2 回目の `build` と `build` 後の `add` では結果に誤りが生じます。利用段階を明示し、誤った順序の呼出しを拒否する必要があります。
- `BitVector::build` も同様に、現状では 1 回だけに限らなければ結果に誤りが生じます。
- RUQ 類の `int` 型の時刻番号は、十分な更新回数でオーバーフローし、時刻の大小比較が成立しなくなります。

### 5.4 Wavelet Matrix 系

- `dynamic_wavelet_matrix.cpp`: `sigma>0`、位置、値域を確認しません。`sigma=4,x=4` は上位ビットを切り捨て、0 として格納します。`sigma=1` の空構造を `pop` すると要素数だけが -1 になります。`select` の位置を `T` へ格納するため、キー型が小さいと要素数を縮小します。上位 k 件を求める処理は深さを単なる `char` へ格納するため、`char` が符号なしの環境では 0 から 255 へラップアラウンドします。整数型によっては `bit_length` の多重定義から呼出し先を決定できません。
- `dynamic_wavelet_tree.cpp`: `vector` を受け取るコンストラクタだけキーの範囲を確認しません。`select` と `select_remove` は存在数を確認せず、存在しない子 `-1` を参照し得ます。総和版も `select` 系に同じ問題があります。
- `wavelet_matrix.cpp`: `sigma>0`、全値が `[0,sigma)` 内、直接公開された `select` では `k<count(x)` が必要です。下位の `BitVector` が `select` の失敗時に返す -1 を次の階層の `rank` へ渡すと、負の添字になります。
- `wavelet_matrix_bit.cpp:184-208`: 公開された `sum()` は常に `assert(false)` となる未実装関数です。ビット数固定版の `range_freq` は、上限がビット幅外のとき処理せず、`log=3, upper=8, data=[1,2]` を 0 件と返します。
- `wavelet_matrix_sum.cpp:283-289`, `wavelet_matrix_fenwick.cpp:276-283`, `dynamic_wavelet_tree_sum.cpp:344-349`: 分位点を求める式 `(total%den)*num` は、最終値が型に収まっても途中でオーバーフローし得ます。`__int128` などで積を計算してから切り上げ除算する必要があります。
- 総和版と Fenwick 木版は、コンストラクタ内の `assert` より前に `sigma-1` からビット数を計算します。符号なしの 0 ではアンダーフローし、符号付き最小値では減算自体が未定義動作になります。

### 5.5 その他の境界・型

- `offline_RUQ.cpp:15-50`: Union-Find の添字 `nxt` が `vector<T>` になっています。`T=string` では成立せず、`uint8_t,n>=256` では添字がラップアラウンドします。`vector<int>` へ分離する必要があります。`n==a.size()` も確認するようにします。
- `partial_persistent_array.cpp:38-45`: `t<-1` では `upper_bound` が返した位置より前を添字 -1 として参照します。Union-Find 側もこの時刻を受け付けます。`update` 時刻が狭義単調増加であるという条件も、両クラスで一致していません。
- `std_multiset.cpp:30-53`: 負の `cnt` を渡すと、`insert` は負の多重度を作成し、`erase` と `remove` は個数を増やします。`tovector` の反復変数は `int` ですが、多重度は `long long` です。
- `static_set.cpp`, `std_set.cpp`, `std_multiset.cpp` の隣接値計算と、`RangeSet` の 1 点区間は、整数の端で `T` の符号付き整数オーバーフローを起こし得ます。
- `pbds_multiset.cpp`: `int id_counter` は約 `INT_MAX` 回でオーバーフローし、右端の添字に使用する番兵とも衝突します。
- `old_dynamic_fenwick_tree2D.cpp`: 座標型は `T` ですが、サイズと `set` 引数が `int` です。
- `dynamic_fenwick_tree2D_RAQRSQ.cpp:207-214`: `h*w` を座標型 `T` で先に計算します。`T=int,W=long long,h=w=50000` でも、`long long` へ昇格する前にオーバーフローします。`v.d0*W(h)*W(w)` の順にする必要があります。
- 動的 2 次元 Fenwick 木群は、公開上のサイズへ `+1` した、番兵を含む内部のサイズを公開関数の `assert` に使用し、宣言した領域より 1 つ外を受理します。固定長の `State[70]` は座標のビット幅が 69 以下であることも暗黙に必要とします。
- `fenwick_tree2D.cpp`: 容量の積に対する `assert` は `vector` の確保後に実行されるため確保を防ぐことができず、4 引数の `sum` は公開上の終端より 1 大きい値まで受け付けます。単一の接頭和問い合わせにおける上端のチェックは、内部のサイズ `H+1` と整合しています。
- `linear_cum_sum.cpp:48-65`: 引数 `b` を使用せず、`a` は `a==1` を `assert` するだけです。`d==0` では未構築の `S[0]` を参照します。固定係数専用なら不要な引数を削除し、`d>0,k>=0` を確認すべきです。

## 6. 理論上整合していると確認した主な部分

問題の有無の確認に加えて、次の中心部分についてはコード上の不変条件を追跡しました。

- `avl_tree_bit_vector.cpp`: 小区画があふれた際の移送、AVL 回転後の `size` と `ones`、`rank` と `select` の分岐は整合しています。`select` には範囲チェックの `assert` もあります。
- `b_tree_bit_vector.cpp`: 葉と内部節点の分割、借用、併合、累積情報、葉の連結、小表現と木表現の切替え、コピー・ムーブ・破棄は整合しています。
- `disjoint_sparse_table.cpp`: 非可換モノイドでも、左側の接尾積の後に右側の接頭積を `op` する順序は正しいです。
- `foldable_stack.cpp`, `foldable_deque.cpp`: 前側と後ろ側の集約順を分けており、非可換演算でも正しく動作します。
- `fenwick_tree_RAQ.cpp`, `fenwick_tree_RAQRSQ.cpp` と動的 2 次元版の差分式: 加法群と有効な境界の下で変換式は正しいです。
- `dycone.cpp`, `dycone_sum.cpp`: 問い合わせ時刻を重みとする最大森、削除時刻以上の祖先探索、自己辺を対象外とする処理、多重辺を扱うスタックは、`run()` を一度だけ呼ぶ前提の下で整合しています。乱数優先度で記録列を作成するため、期待計算量が O((n+q) log n) となる議論も成立します。
- `PersistentSet`, `PersistentMultiset`: 空列からの構築、重複除去、`split`、コピーオンライト、多重度管理を追跡しました。残っている主な注意点は、`merge` に入力順序の条件があることです。
- `WaveletMatrix2DSum`, `WaveletMatrix2DMin`, `WaveletMatrix2DMonoid`: 座標圧縮と各階層の写像を確認しました。モノイド版は可換性を既に明記しています。
- 通常の `UnionFind`, `UndoableUnionFind`, `PersistentUnionFind` と重み付き版は、それぞれが必要とする演算の下で、親が保持する要素数と差分ポテンシャルの式が整合しています。

## 7. 全ファイルの確認結果

表の「本体問題なし」は前述の意味であり、無効な添字、空での `pop`、算術オーバーフローまで無条件に保証するという意味ではありません。

### A--D

| ファイル | 判定 |
|---|---|
| `area_of_union_of_rectangles.cpp` | 重要: 型の縮小、空入力・高さや幅が 0 の矩形、整数への詰め込み時のオーバーフロー |
| `avl_tree_bit_vector.cpp` | 本体問題なし |
| `avl_tree_multiset.cpp` | 重要: `missing` が未初期化、順位の型・個数の条件 |
| `avl_tree_set.cpp` | 本体問題なし。生ポインターのコピーと寿命に注意 |
| `b_tree_bit_vector.cpp` | 本体問題なし |
| `b_tree_bit_vector_sum.cpp` | 重みが非負という条件 |
| `bbst_node.cpp` | 重大: 次・前の節点を求める補助関数が不正 |
| `binary_trie_multiset.cpp` | 重要: 既定構築、順位・個数・シフト幅 |
| `binary_trie_set.cpp` | 重要: 排他的論理和のビット幅、空集合の最小・最大 |
| `bit_vector.cpp` | 重要: `build` が冪等でない、既定値・利用手順 |
| `bloom_filter.cpp` | 重要: `contains_insert`, `size`, `clear` の不整合 |
| `cuckoo_hash_table.cpp` | 重大: 未完成・構文不成立 |
| `cumulative_sum.cpp` | 本体問題なし。加法群が前提 |
| `cumulative_sum2D.cpp` | 行の長さが異なる入力・次元の積のオーバーフロー |
| `deletable_heap.cpp` | `erase` は存在数以下だけ削除できるという条件 |
| `deque.cpp` | 重大: 再構築が列を保存しない |
| `disjoint_sparse_table.cpp` | 本体問題なし。結合則が前提 |
| `double_ended_heap.cpp` | 空での操作と、変更可能な内部配列の公開 |
| `dual_commutative_segment_tree.cpp` | 重大: 作用の型、`const`、空からの構築 |
| `dual_segment_tree.cpp` | 重要: `init` を使用しない、返す長さ、空からの構築 |
| `dual_segment_tree2D_RUQ.cpp` | 本体の式は正しい。境界・時刻番号に注意 |
| `dual_segment_tree_RUQ.cpp` | 同名クラスとの衝突・時刻番号の上限 |
| `dual_segment_tree_RUQ2.cpp` | 重大: `vector` から構築した後の時刻番号が不整合 |
| `dycone.cpp` | 本体は整合。`run` は一度だけ実行可能 |
| `dycone_sum.cpp` | 本体は整合。`run` は一度だけ実行可能・加法群が必要 |
| `dynamic_bit_vector.cpp` | 重大: `set` が集約値を更新しない、最悪 O(n) |
| `dynamic_fenwick_tree2D.cpp` | 静的メモリ領域の所有権・境界 |
| `dynamic_fenwick_tree2D_RAQ.cpp` | 差分の式は正しい。基底クラスの制約を継承 |
| `dynamic_fenwick_tree2D_RAQRSQ.cpp` | 重要: 座標の積のオーバーフロー、境界 |
| `dynamic_lazy_segment_tree.cpp` | 重大: `lazy` が未初期化、`print`、容量 |
| `dynamic_lazy_segment_tree_array.cpp` | 重大: 上のファイルと内容も問題も重複 |
| `dynamic_lazy_segment_tree_util.cpp` | 重要: 整数への詰め込みで未定義動作・桁上がり、大域的な使用例 |
| `dynamic_list.cpp` | 重大: `T` を `bool` へ縮小、最悪 O(n) |

### D--M

| ファイル | 判定 |
|---|---|
| `dynamic_segment_tree.cpp` | 重要: シフト、公開上の境界、`set` の範囲 |
| `dynamic_segment_tree_init.cpp` | 存在しない子の区間長は整合。シフトと公開上の境界に問題 |
| `dynamic_wavelet_matrix.cpp` | 重要: 対応する整数型、値域、`select` と上位 k 件 |
| `dynamic_wavelet_tree.cpp` | 重要: `vector` 内の値域、`select` 系 |
| `dynamic_wavelet_tree_sum.cpp` | `select` 系、分位計算の途中のオーバーフロー |
| `euler_tour_tree.cpp` | 重要: `build` 後の成分数。森、`link`、`cut` の条件 |
| `fast_stack.cpp` | 本体問題なし。空での `top`, `pop` は禁止 |
| `fenwick_tree.cpp` | 本体問題なし。値による二分探索には全要素が非負という条件が必要 |
| `fenwick_tree2D.cpp` | 本体の式は正しい。領域確保・端点チェックに問題 |
| `fenwick_tree_RAQ.cpp` | 本体の式は正しい。出力関数の依存に問題 |
| `fenwick_tree_RAQRSQ.cpp` | 本体の式は正しい。出力関数の依存に問題 |
| `foldable_deque.cpp` | 本体問題なし |
| `foldable_stack.cpp` | 本体問題なし |
| `hash_dict.cpp` | 探索本体は整合。値を返す `operator[]`、SSE2 への依存、容量上限 |
| `hash_set.cpp` | 本体問題なし。大容量でのシフトと `int` の上限 |
| `icpc_lazy_rbst.cpp` | 一意定義規則・破壊的変更に伴う所有権 |
| `imos.cpp` | 式は正しい。`build` は一度だけ実行可能 |
| `index_set.cpp` | 本体問題なし。値域チェック・内部 `vector` の公開 |
| `lazy_link_cut_tree.cpp` | パスに対する処理の本体は整合。`cut`, `link`, `split` に前提あり |
| `lazy_rbst.cpp` | 重要: 非可換演算での `merge` と反転、所有権 |
| `lazy_segment_tree.cpp` | 重要: `n=0`、余分な葉への `all_apply` |
| `lazy_wb_tree.cpp` | 重大: `set` がコンパイル不能。非可換演算での反転・所有権 |
| `lazysegutil.cpp` | 重要: 代入用の番兵値、単位元を保存しない作用 |
| `linear_cum_sum.cpp` | 重要: 未使用の公開引数、`d=0`、オーバーフロー |
| `link_cut_tree.cpp` | 本体は整合。`cut` と `split` に前提あり |
| `link_cut_tree_sum.cpp` | 可換な集約の下で整合。`cut` と `split` に前提あり |
| `max_heap.cpp` | 空での `pushpop` と `replace` |
| `merge_sort_tree.cpp` | 構築本体は整合。呼出し関数の順序は非可換な用途では定義されない |
| `min_heap.cpp` | 空での `pushpop` と `replace` |
| `multiset_sum.cpp` | 重要: `pop` の既定値、空集合の総和、個数の型 |
| `multiset_sum_qd.cpp` | 重大: `discard` 後の総和、`NDEBUG` 時に `remove` が何も実行しない |
| `multiset_sum_splay.cpp` | 重大: コンパイル不能、総和、メモリアロケータ |
| `multiset_sum_wbt.cpp` | 単体の本体はおおむね整合。同名クラスとの衝突・個数は正という条件 |
| `multiset_topk.cpp` | 可換群と、`K` は非負という条件 |

### O--S

| ファイル | 判定 |
|---|---|
| `offline_RUQ.cpp` | 重要: 添字型が `T`、初期 `vector` の長さ |
| `offline_RUQ2D.cpp` | 追加の確定問題なし |
| `offline_dynamic_connectivity.cpp` | 重要: 問い合わせ 0 件、可変長配列、存在しない辺の削除 |
| `offline_dynamic_connectivity_sum.cpp` | 同上 |
| `old_dynamic_fenwick_tree2D.cpp` | 同名クラスとの衝突・座標型の縮小 |
| `partial_persistent_array.cpp` | 重要: 負の時刻に関する条件 |
| `partial_persistent_union_find.cpp` | 重大: 構築不能、時刻に関する条件 |
| `pbds_multiset.cpp` | 識別番号の符号付き整数オーバーフロー |
| `pbds_set.cpp` | 追加の確定問題なし |
| `persistent_array.cpp` | 重大: 一括更新、空での `tovector` |
| `persistent_lazy_segment_tree.cpp` | 再構築の順序、`copy_from` する木の長さ |
| `persistent_lazy_wbtree.cpp` | 重大: `copy` と `set`、根、反転、再構築 |
| `persistent_multiset.cpp` | `merge` する 2 集合の大小関係に条件あり |
| `persistent_seg_wbtree.cpp` | 重大: `copy` と `set`、根、反転、再構築 |
| `persistent_segment_tree.cpp` | `copy_from` する木の長さに条件あり |
| `persistent_set.cpp` | `merge` する 2 集合の大小関係に条件あり |
| `persistent_stack.cpp` | 追加の確定問題なし |
| `persistent_union_find.cpp` | 追加の確定問題なし |
| `persistent_wbtree.cpp` | 重大: `copy` の `const` 違反・空木のコピー |
| `persistent_weighted_union_find.cpp` | 追加の確定問題なし |
| `range_product_dc.cpp` | 追加の確定問題なし |
| `range_set.cpp` | 重要: 長さの差のオーバーフロー |
| `segment_tree.cpp` | 本体問題なし。`lazy_*` は専用の利用手順が必要 |
| `segment_tree2D.cpp` | 重要: 非可換演算で順序を破壊 |
| `segutil.cpp` | 単位元と実際の値の衝突 |
| `sortable_array.cpp` | コピー禁止という制約あり。追加問題なし |
| `sortable_segment_tree.cpp` | コピー禁止という制約あり。追加問題なし |
| `sparse_segment_tree2D.cpp` | `set` の境界・可換性 |
| `sparse_segment_tree2DFAST.cpp` | 可換性が必要 |
| `sparse_table.cpp` | 冪等性が必要 |
| `sparse_table_min.cpp` | 重要: 空入力で `clz(0)` |
| `splay_node.cpp` | 重要: `split` でヌルポインターを逆参照 |
| `sqrt_segment_tree.cpp` | 追加の確定問題なし |
| `static_RmQ.cpp` | 追加の確定問題なし |
| `static_multiset.cpp` | 追加の確定問題なし |
| `static_range_mode_query.cpp` | 追加の確定問題なし |
| `static_set.cpp` | 本体は整合。隣接値を求める際の整数の端に注意 |
| `std_multiset.cpp` | 重要: 負の `cnt`、順位の型、隣接値 |
| `std_set.cpp` | 隣接値を求める際の整数の端に注意 |

### U--W

| ファイル | 判定 |
|---|---|
| `undoable_union_find.cpp` | 追加の確定問題なし |
| `undoable_union_find_sum.cpp` | 厳密に元へ戻る除算を含む演算型が必要 |
| `union_find.cpp` | 追加の確定問題なし |
| `union_find_advance.cpp` | 追加の確定問題なし |
| `used_set.cpp` | 追加の確定問題なし |
| `wavelet_matrix.cpp` | 値域・直接公開された `select` の条件 |
| `wavelet_matrix_2d_min.cpp` | 追加の確定問題なし |
| `wavelet_matrix_2d_monoid.cpp` | 可換性は明記済み。追加問題なし |
| `wavelet_matrix_2d_sum.cpp` | 追加の確定問題なし |
| `wavelet_matrix_bit.cpp` | 重要: `sum` が未実装、ビット幅外の `range_freq`、同名クラスとの衝突 |
| `wavelet_matrix_fenwick.cpp` | 分位計算の途中のオーバーフロー、`sigma` をチェックする順序 |
| `wavelet_matrix_sum.cpp` | 分位計算の途中のオーバーフロー、`sigma` をチェックする順序 |
| `wb_tree.cpp` | 節点を直接変更する `merge` と `split` の所有権 |
| `wb_tree_seg.cpp` | 所有権。診断用の平衡式も `!` の優先順位が不正 |
| `weight_union_find.cpp` | 追加の確定問題なし |
| `wordsize_tree_set.cpp` | 重要: 値域 0、範囲チェック、`fill` の不変条件 |
