# titan_cpplib/alg レビュー

対象は以下の12ファイル。テスト実行はせず、コードを読んで精査した。

- double_sigma.cpp
- doubling.cpp
- doubling_monoid.cpp
- itertools.cpp
- lis.cpp
- mo.cpp
- random.cpp
- random_mt.cpp
- random_tree.cpp
- traveling_salesman_problem.cpp
- tree_generator.cpp
- zaatsu.cpp

重要度は次の3段階で付けた。

- **[バグ]** 誤動作・UB につながる
- **[注意]** 特定条件で問題になる。仕様として明記すれば許容できる
- **[軽微]** 動作に影響しない指摘

## double_sigma.cpp

- ロジックは全メソッド正しい。`MultisetSum::index(a)` が「a 未満の個数」、`sum(a)` が「a 未満の総和」を返すことを確認済みで、online 系の計算と整合している。
- **[注意]** `T=int` で使うと `MultisetSum` 内部の `data`(型 T)と `ans` が容易にオーバーフローする。long long 前提であることをコメントに明記した方がよい。
- **[軽微]** `sigma_abs_online` の変数 `n` が未使用。
- **[軽微]** テスト関数は `sigma_abs` 等のソート版のみ検証し、online 版を検証していない。

## doubling.cpp

- 構築 O(n log LIM)、クエリ O(log LIM) で正しい。
- **[注意]** `kth` に `k <= LIM` の assert がない。`k > LIM` だと上位ビットが黙って無視され、誤った位置を返す。
- **[軽微]** `db` を log+1 行確保し `db[log]` も計算するが、`kth` は `db[log-1]` までしか使わない。1 行分の構築時間とメモリが無駄。
- start が -1 になった直後に break するため、`db[i][-1]` へのアクセスは起きない。この点は正しい。

## doubling_monoid.cpp

- doubling.cpp と同じ構造で、モノイド積の合成順(`op(res, db[i][start].second)`)も左から右で正しい。
- **[注意]** 同じく `k <= LIM` の assert がない。
- **[軽微]** 同じく `db[log]` が未使用。

## itertools.cpp

- 列挙系(combinations、combinations_bit、submasks、partitions、grouping_pair、product、permutations、combinations_with_replacement)はいずれも境界条件(r=0、repeat=0、空集合)を含め正しい。
- **[注意]** `nCr` は途中の `res * (n - r + i)` でオーバーフローし得る。`combinations` はこれを reserve 目的で呼ぶため、n が大きいと UB を経由する。nCr の適用範囲をコメントに書くか、reserve 用途では上限クリップした方が安全。
- **[軽微]** `permutations(n, r)` の計算量表記は dfs ノード数を含めると O(n · n!/(n-r)!) 相当がより正確。

## lis.cpp

- LIS、LIS_vec とも O(n log n) で正しい。strict/非 strict の lower_bound/upper_bound の使い分けも正しい。

## mo.cpp

- Hilbert order による並べ替え、区間伸縮の順序(add してから del)とも正しい。
- **[注意]** n がちょうど 2^25(初期 max_n)のとき、r = n = max_n が Hilbert 座標の範囲 [0, max_n) を 1 はみ出す。結果は正しいが順序が乱れ性能が落ちる。`while (max_n < n)` を `while (max_n <= n)` にすれば解消する。
- **[軽微]** 計算量コメントは `O(q√n)` だが、Hilbert order では O(n√q) が正確。
- **[軽微]** run_light / run / run_range で eval 計算とソートのコードが3重に重複している。

## random.cpp

- xorshift128 の実装、Fisher-Yates、rand_pair の一様性はいずれも正しい。
- **[注意]** `randint(end)` は `end = INT_MAX` で `end+1` が int オーバーフロー(UB)。実用上は問題になりにくい。
- **[軽微]** `rand_u64` のコメントは「[0, u64_MAX)」だが実際は inclusive。
- **[軽微]** `choice(a, w, normal)` は累積和の末尾を 1.0 に強制していない。random_mt.cpp 版はしている。二分探索が末尾に丸めるため実害はないが、挙動を揃えた方がよい。
- **[軽微]** `normal == false` 固定の assert という API は不自然。引数を削除できる。

## random_mt.cpp

- 全メソッド正しい。分布は std のものを使っており偏りもない。
- **[軽微]** `rand_u64` のコメントが inclusive/exclusive で実装と不一致(random.cpp と同じ)。
- **[軽微]** `choices` の `assert(a.size() >= k)` は符号なし比較で、k < 0 のとき意図せず通る。
- **[軽微]** `choices` は全体を shuffle する O(n)。Random 版は先頭 k 要素だけの部分 shuffle で同じ結果を得ており、そちらの方式に揃えられる。

## random_tree.cpp

- **[バグ] `gen_path` の分岐が無効**。

  ```cpp
  if (random.randint(1)) {
      edges[i] = {p[i], p[i+1]};
  } else {}
      edges[i] = {p[i+1], p[i]};
  ```

  `else {}` が空ブロックのため、最後の代入が常に実行され、向きは必ず `{p[i+1], p[i]}` になる。木としては正しいので出力は壊れないが、向きのランダム化が働いていない。
- **[バグ]** `gen_path` は n = 0 のとき `edges(n-1)` が `vector(-1)` となり、巨大サイズの確保でクラッシュする。n <= 1 のガードがない。
- **[軽微]** `gen_random` が tree_generator.cpp 内の同名関数と重複実装。
- gen_random 自体(Prufer 列からの復元)は正しい。

## traveling_salesman_problem.cpp

- bitDP の遷移、O(2^n n^2) の計算量とも正しい。
- **[注意]** `dp[s][v] == INF` のまま `dist[v][u]` を加算する。T = int、INF = INT_MAX だと符号付きオーバーフロー(UB)。`if (dp[s][v] == INF) continue;` を入れるべき。到達不能状態は結果の min には影響しないため、現状の誤りはオーバーフローのみ。
- **[軽微]** bit 0 を含まない状態(到達不能)もループしており、約2倍の無駄がある。

## tree_generator.cpp

- 各生成器(Prufer 一様ランダム、ウニ、スター、ムカデ、完全二分木、long-path killer、全列挙、AHU による同型除去)のロジックは正しい。境界(N=1、N=2)も確認した。
- **[注意]** `TreeType` と `TreeGenerator` が `namespace titan23` の外にある。ライブラリ規約に反する。
- **[軽微]** `gen_lobster` はコメントの通りパラメータの根拠が不明。D > 2 になり得るため、厳密にはロブスター木(全頂点が主鎖から距離 2 以内)の定義を満たさない場合がある。
- **[軽微]** 内部 UnionFind は tree_generator 専用の重複実装。ds/union_find.cpp を使えば減らせるが、生成器の独立性を優先するなら現状でよい。

## zaatsu.cpp

- 圧縮・復元のロジックは正しい。
- **[注意]** `to_zaatsu` は未登録の値でも黙って挿入位置を返す。誤用検出のため `assert(res < n && a[res] == v)` を入れるか、この挙動を仕様としてコメントに明記した方がよい。
- **[軽微]** `add` 後に `build` を忘れると `to_zaatsu` が壊れるが、検出手段がない。build 済みフラグを持つ手もある。
