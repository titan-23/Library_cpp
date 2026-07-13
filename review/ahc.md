# titan_cpplib/ahc レビュー(直下・pruner)

対象は以下の13ファイル。テスト実行はせず、コードを読んで精査した。
sa/ と beam_search/ は別ファイル(ahc_sa.md、ahc_beam_search.md)にまとめる。
old/、test/、gomi、ahclib_results、可視化用 py、メモ系 md は対象外。

- bitboard.cpp
- cpu_timer.cpp
- kmeans.cpp
- kmeans_new.cpp
- mcmc.cpp
- normal_distribution.cpp
- profiler.cpp
- state_pool.cpp
- template.cpp
- timer.cpp
- pruner/hoeffding_pruner.cpp
- pruner/successive_halving_pruner.cpp
- pruner/wilcoxon_pruner.cpp

重要度は次の3段階で付けた。

- **[バグ]** 誤動作・UB・コンパイル不能につながる
- **[注意]** 特定条件で問題になる。仕様として明記すれば許容できる
- **[軽微]** 動作に影響しない指摘

## ディレクトリ横断の指摘

- **[軽微] `<bits/stdc++.h>` の使用**。mcmc.cpp と profiler.cpp がライブラリファイルで使っており、CLAUDE.md に書いた規約と食い違う。
- **[軽微] 慣例の不統一**。HoeffdingPruner は `is_maximize=true` が既定、WilcoxonPruner は `is_minimize=true` が既定で、向きが逆。取り違えやすい。

## timer.cpp

- 問題なし。
- **[軽微]** elapsed() 内に旧実装のコメントアウトが残っている。

## cpu_timer.cpp

- 問題なし。Windows/Linux 両実装とも単位換算(100ns→ms、ns→ms)は正しい。
- wall clock との違い、取得コスト、マルチスレッド時の挙動をコメントで説明しており適切。

## state_pool.cpp

- **[注意]** `init(n)` は unused_idx に 0..n-1 を積むが、pool に既存要素がある状態で呼ぶと使用中の id を未使用として登録する。`pool.size()` を基準にすべき。現状は構築直後専用。
- **[注意]** `del()` が二重解放を検出しない。同じ id を2回 del すると、以後 `gen()` が同じ id を2回返し、状態が壊れる。デバッグ用に検出があると安全。
- **[軽微]** デストラクタがなく `new T` が解放されない。プロセス終了で回収されるためコンテスト用途では実害なし。
- **[軽微]** `assert(id < pool.size())` は int と size_t の比較で警告が出る。

## template.cpp

- **[注意]** `template <class T, class U> T min(const T&, const U&)` が第一引数の型を返す。`min(3, 2.5)` は u=2.5 を返す分岐で int に切り詰められ 2 になる。max も同様。同型どうしは std::min が優先されるため、混合型のときだけ静かに値が壊れる。
- それ以外はテンプレートとして妥当。

## normal_distribution.cpp

- **[バグ]** `log_pdf()` が未定義の `PI` を参照している。`M_PI` の誤り。非テンプレートクラスのためメンバ関数本体は include 時点でコンパイルされ、利用側が事前に PI を定義していない限りこのヘッダを include しただけで通らない。
- **[バグ]** `update_posterior()` が未定義の `EPSILON` を参照している。リポジトリ内に定義は見つからなかった。同上の理由で include しただけで通らない。
- **[軽微]** `M_PI` は C++ 標準にない。GCC では通るが移植性はない。
- ロジックは確認した。cdf(erf 版)、cdf_approx(Abramowitz–Stegun 7.1.26、絶対誤差 1.5e-7 程度)、erfinv_approx(Winitzki 近似)、ベイズ更新、KL ダイバージェンスのいずれも式は正しい。
- **[軽微]** erfinv_approx にニュートン法1回の精度向上がコメントアウトで残っている。inverse_cdf の精度が要る用途では有効化する価値がある。

## profiler.cpp

- **[注意]** `#define PROFILE` がライブラリファイル内で無条件に定義されている。`#else` の空マクロ分岐が到達不能で、計測を無効化するにはこのファイルを編集するしかない。利用側で定義する設計にすべき。
- **[軽微]** グローバル変数 `profiler` が非 inline でヘッダに定義されており、複数 TU から include するとリンクエラーになる。単一 TU 前提なら実害なし。
- **[軽微]** report のヘッダ行で `setw(10) << "Avg(ms)\n"` と改行込みの文字列に幅を当てており、列が1文字ずれる。
- start/stop の対応はスタックで取っており、ネスト計測は正しく動く。ただし親の計測時間に子の時間が含まれる。自己時間でないことはコメントに明記するとよい。

## bitboard.cpp

- 主要ロジックはすべて確認した。expand_into のビット演算、フロンティア方式の flood/bfs_dist、components/label/largest_component の走査、いずれも正しい。
- bfs_nearest のタイブレーク(同距離で始点番号最小)は、前層のセルがすべて確定済みであることと帰納法により成立している。コメントどおり。
- **[軽微]** bfs_nearest は新規セルごとに4近傍をスカラーで見るためビット並列でない。O(HW·4) で妥当だが、他メソッドとの速度差は意識しておくとよい。
- **[軽微]** ローカル変数 `dirs = 4` は固定値で、8近傍に切り替える際は DR/DC のコメント解除と併せて直す必要がある。罠になりやすい。
- **[軽微]** shift のコメント「dr は -1,0,1」は実装上任意の dr で正しく動くため、制約の記述が過剰。
- **[軽微]** expand のコメントから、出力に s 自身が含まれないことが読み取りにくい。s ∪ 近傍が欲しい場合は別途 ior が要る。
- 境界は resize の `assert(w <= word_bits())` と lowmask の分岐で守られており、シフト UB はない。

## kmeans.cpp

- **[注意]** `fit()` の反復中にクラスタが空になると `mean(空vector)` を呼ぶ。ユーザーの mean 実装次第で 0 除算や UB になる。fit_flow には空クラスタの分岐があるのに fit にはない。
- **[注意]** k-means++ 風初期化の重みが `p_f[i] = min_d + 1` で、型が `vector<int>`。問題が2つある。
  - 距離の2乗でなく距離+1 を重みにしており、k-means++ の性質(遠い点を強く選ぶ)が弱い。
  - DistType が double のとき int へ切り捨てられる。距離がすべて 1 未満だと全点の重みが 1 になり一様抽選になる。
- **[注意]** X の相異なる値の数が k 未満だと `assert(flag)` で落ちる。重複の多い入力では起こりうる。
- **[軽微]** fit(X) と fit_flow(X, target_sizes) の初期化コードが丸ごと重複している。
- **[軽微]** fit_flow の空クラスタ処理が直前の中心でなく `init_centers[j]` に戻す。cluster_centers.clear() で前の値を失っているため。
- **[軽微]** `if (!changed && _ > 0)` の `_ > 0` は labels の初期値が -1 で初回は必ず changed になるため冗長。
- 計算量は fit が O(max_iter·n·k)、fit_flow が毎反復 n·k 辺の MCF 構築で意図どおり。
- 上記の問題は kmeans_new.cpp で解消されている。両方を残すなら、こちらが旧版であることをファイル冒頭に明記するとよい。

## kmeans_new.cpp

- kmeans.cpp の問題点(空クラスタ、初期化の重み、収束判定)をすべて解消している。ロジックの誤りは見つからなかった。
- kmeans_pp は D² 重み・累積和による正しい k-means++。全重み 0(残りが全部重複点)のフォールバックもある。
- Hamerly 法の u/l/s/drift の更新則を確認した。正しい。
  - 空クラスタ補填でラベルを手動変更した際にバウンドを無効化(u=max, l=0)しており、正しい。
  - `iter > 0` ガードの理由コメントも適切。
- fit_flow_impl の下限付きフロー→標準フロー変換を検証した。SS→S cap n、T→TT cap n−sum_lo、j→TT cap lo_j、target=n は、SS→T cap sum_lo と T→TT cap n を sum_lo だけ相殺した形と一致し、正しい。コメントの導出説明も適切。
- **[注意]** kmeans_pp は `d_sq = d²` を重みにするため、dist が既に2乗距離を返す流儀だと D⁴ 重みになる。Hamerly には「三角不等式を満たすこと」の注意書きがあるので、kmeans_pp 側にも dist はメトリックである前提を書いておくとよい。
- **[軽微]** `new_centers.assign(k_, ElmType{})` が ElmType のデフォルト構築を要求する。制約として軽く明記するとよい。

## mcmc.cpp

- ギブスサンプラーの数式を確認した。条件付き事後分布の精度(1/prior_var + Σc²/var_i)、平均分子(prior_mu/prior_var + Σc·r/var_i)、残差の差分更新、いずれも正しい。
- prepare_buffers を毎回呼ぶ設計も、観測追加後の estimate_step で正しく効く。計算量コメント O(iterations·nnz + N + K) も正しい。
- **[軽微]** `variance = E[X²] − E[X]²` は数値誤差で僅かに負になりうる。利用側で sqrt すると NaN になるため、max(0.0, ·) で丸めると安全。
- **[軽微]** `<bits/stdc++.h>` を使っている。

## pruner/hoeffding_pruner.cpp

- 差分更新(再 report 時の current_sum・diff_sum の補正)は正しい。
- **[注意]** `score_range` の定義が曖昧。Hoeffding の ε は差 diff の取りうる幅を使う。diff = best − current は各スコアの幅を D とすると [−D, D] で幅 2D になる。コメントの「スコア差の最大幅(理論上の上限−下限)」を 2D と読めば実装は正しいが、「1シードのスコアの幅 D」と読むと ε が半分になり枝刈りが過剰になる。どちらを渡すのか一意に読める記述にすべき。
- **[軽微]** report の seed に範囲 assert がない。max_seeds 以上を渡すと範囲外アクセスになる。
- **[軽微]** next_param のベスト更新は共通シードでなく各自の評価済みシード全体の平均で比較する。シード集合が違うと公平でない。枝刈り用途の簡略化としては許容範囲。

## pruner/successive_halving_pruner.cpp

- nth_element による選抜、is_active の落とし方、clear の再初期化は正しい。O(生存者数) で妥当。
- **[注意]** 未評価候補(eval_counts==0)の平均を 0 とみなす。スコアが負になりうる最大化問題では、未評価候補が評価済み候補より上位に残る。未評価は最下位として扱うべき。
- **[軽微]** report の id に範囲 assert がない。

## pruner/wilcoxon_pruner.cpp

- 符号順位検定の実装を確認した。順位付け、同順位の平均順位、同順位補正 Σ(t³−t)/48、分散 n(n+1)(2n+1)/24、z 値、p = P(Z > z)、いずれも正しい。劣位のときに prune する向きも正しい。
- **[軽微]** 連続性補正がない。valid_n が min_samples=5 付近だと正規近似は粗いが、枝刈り用途では許容範囲。
- **[軽微]** report の seed に範囲 assert がない。
- **[軽微]** next_param のベスト更新が共通シードでない点は hoeffding と同じ。
