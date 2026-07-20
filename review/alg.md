# titan_cpplib/alg レビュー

対象は以下の12ファイル。テスト実行はせず、コードを読んで精査した。

- double_sigma.cpp **[完了]**
- doubling.cpp **[完了]**
- doubling_monoid.cpp **[完了]**
- itertools.cpp **[完了]**
- lis.cpp **[完了]**
- mo.cpp **[完了]**
- random.cpp
- random_mt.cpp
- random_tree.cpp
- traveling_salesman_problem.cpp **[完了]**
- tree_generator.cpp

重要度は次の3段階で付けた。

- **[バグ]** 誤動作・UB につながる
- **[注意]** 特定条件で問題になる。仕様として明記すれば許容できる
- **[軽微]** 動作に影響しない指摘

全指摘に対応し、残りがないファイルは見出しとファイル一覧に **[完了]** を付ける。

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

## traveling_salesman_problem.cpp **[完了]**

- bitDP の遷移、O(2^n n^2) の計算量とも正しい。
- 遷移と末尾ループに `dp == INF` のガードを入れ、`INT_MAX + dist` の符号付きオーバーフローを解消した。末尾も同じ不具合だったので合わせて直した。
- bit 0 を含まない到達不能状態を `if ((s & 1) == 0) continue;` で飛ばし、反復を約半分にした。

## tree_generator.cpp

- 各生成器(Prufer 一様ランダム、ウニ、スター、ムカデ、完全二分木、long-path killer、全列挙、AHU による同型除去)のロジックは正しい。境界(N=1、N=2)も確認した。
- **[注意]** `TreeType` と `TreeGenerator` が `namespace titan23` の外にある。ライブラリ規約に反する。
- **[軽微]** `gen_lobster` はコメントの通りパラメータの根拠が不明。D > 2 になり得るため、厳密にはロブスター木(全頂点が主鎖から距離 2 以内)の定義を満たさない場合がある。
- **[軽微]** 内部 UnionFind は tree_generator 専用の重複実装。ds/union_find.cpp を使えば減らせるが、生成器の独立性を優先するなら現状でよい。

