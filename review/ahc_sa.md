# titan_cpplib/ahc/sa レビュー

対象は以下の5ファイル。テスト実行はせず、コードを読んで精査した。
a.py、tsp2.py、vis_kmeans.py、vis_replica.py は実験・可視化用スクリプトのため対象外。

- sa.cpp
- sa_state.cpp
- sa_tsp.cpp
- km.cpp
- how_to_use_sa.md
- kmeans.md

重要度は次の3段階で付けた。

- **[バグ]** 誤動作・UB・コンパイル不能につながる
- **[注意]** 特定条件で問題になる。仕様として明記すれば許容できる
- **[軽微]** 動作に影響しない指摘

## sa.cpp

### replica_run

- **[バグ] `iter` 配列の範囲外アクセス**。`vector<int64_t> iter(max_threads, 0)` と スレッド数でしか確保していないのに、`iter[r]`(r はレプリカ番号、0 ≤ r < NUM_REPLICAS)でアクセスしている。既定の NUM_REPLICAS=32 に対して物理スレッドが 32 未満の環境ではヒープ破壊になる。NUM_REPLICAS で確保すべき。
- **[注意] thread_local 乱数のシードが全スレッドで同一**。関数内 `thread_local titan23::Random sa_rnd` は各スレッドが既定シードで初期化されるため、閾値計算の乱数列がスレッド間で同一になる。State 側の sarnd はレプリカごとに異なるシード(1000+i)なので探索自体は分かれるが、受理判定が相関する。tid でシードをずらすのが安全。
- レプリカ交換の受理式は確認した。Δ = (E1−E2)(1/T1−1/T2)、受理確率 min(1, e^Δ)、`LOG_TABLE[..] < Δ` による判定は正しい。偶奇オフセットの隣接ペア交換も標準どおり。
- 並列化の競合は確認した。rep_idx は置換で、1 レプリカを 1 スレッドが担当するため states への競合はない。統計配列は tid 分離、swap は直列区間で行っており正しい。
- **[軽微]** シードを外から渡せず 1000+i 固定。関数に seed 引数がない。
- **[軽微]** `progress` がブロック開始時刻ベースで、ブロック内の SWAP_ITER_INTERVAL 回は同じ値になる。実害は小さい。

### sa_run

- Metropolis 判定は確認した。threshold = score − T·log(u)、受理条件 new ≤ threshold は exp(−Δ/T) ≥ u と等価で正しい。LOG_TABLE の (i+0.5)/N も log(0) を回避しており正しい。
- **[注意] TYPE_CNT=0 で範囲外アクセス**。統計配列を `state.changed.TYPE_CNT` で確保するため、雛形(sa_state.cpp)の既定値 0 のまま使うと `modify[state.changed.type]++` が範囲外になる。replica_run は `max(1, TYPE_CNT)` で防いでいるのに sa_run と sa_multi_run にはガードがない。
- **[軽微]** `now_time = sa_timer.elapsed()` を毎イテレーション呼ぶ。間引き版(`iter & 31`)がコメントアウトで残っており、軽量な modify では計時が定数倍に効く。
- **[軽微]** 線形冷却用の `TEMP_VAL` が未使用のまま残っている。
- **[軽微]** ScoreType が整数型のとき threshold への代入で小数部が切り捨てられ、受理がわずかに渋くなる。実害は小さい。
- **[軽微]** `LOG_TABLE` が非 static のグローバル定義で、複数 TU から include するとリンクエラー。単一 TU 前提なら実害なし。
- **[軽微]** `<omp.h>` を無条件 include している。sa_run しか使わない提出でも -fopenmp なし g++ で通るが、環境依存が増える。

### sa_multi_run

- **[注意] フェーズ間の温度スケジュールが整合しない**。第1フェーズは各スタートが progress 0→1 で START_TEMP から END_TEMP まで完全に冷却する。一方第2フェーズは「全体スケジュールの FIRST_PHASE_RATIO 時点の温度」から再開する。第1フェーズを途中温度(phase2_start_temp)までの冷却にするか、設計意図をコメントに書くべき。
- **[注意] State のコピーが発生する**。`best_local_state = state` がローカル最良更新のたびに走る。how_to_use_sa.md は「探索ループ内で State 自体のコピーは発生しません」と保証しており矛盾する。State が重いと性能も落ちる。
- **[注意]** sa_run と同じく TYPE_CNT=0 のガードがない。
- **[軽微]** how_to_use_sa.md に sa_multi_run の記載がない(後述)。
- **[軽微]** フェーズ2開始時の `score = state.get_score()` は正しい(best_local_state のスコアと一致する)。

## sa_state.cpp

- **[注意] `TYPE_CNT = 0` が既定値**。このまま sa_run に渡すと統計配列が空になり範囲外アクセスになる。雛形の既定は 1 にすべき(how_to_use_sa.md 内のテンプレートは 1 になっており不一致)。
- **[軽微]** `sa_init()` はローカルに Random を作って捨てるだけで意味がない。
- **[軽微]** `changed.type` が未初期化。TODO のまま modify を空実装で動かすと未定義値で配列を引く。雛形として 0 初期化しておくと安全。
- コメント(threshold の等号時遷移、rollback で score 復元不要、advance で遅延書き込み確定)はライブラリ実装と一致しており適切。

## sa_tsp.cpp(使用例)

mTSP の実装例。近傍の差分計算はすべて検証した。

- 2-opt(type 0)の辺差分と advance の reverse、Or-opt/反転 Or-opt(type 1,2)の rotate と辺差分、Double Bridge(type 3)のセグメント再構成、Block Shift/Swap(type 4,5)の erase/insert と pos 更新、いずれも整合している。遅延評価(盤面書き込みは advance のみ)も仕様どおり。
- **[注意] `init(seed)` が `sarnd.set_seed(seed)` を呼んでいない**。ガイドは必須としている。replica_run で使うと全レプリカの sarnd が同一シードになる。
- **[注意] `sa_run<sa::State>(100000, true)` の第2引数は seed**。verbose のつもりの true が seed=1 として渡っている。
- **[注意] type 6(周期的な 2-opt 局所探索)は modify 内で盤面を直接書き換える**。改善時のみ適用するため threshold 判定(改善なら必ず受理)と辻褄が合っており動くが、rollback 不能な遷移であることはコメントに明記した方がよい。
- **[軽微]** type 6 内の `int diff_d` は ll の DIST 差を int で受けており、座標が大きいとオーバーフローしうる。
- **[軽微]** `P` が N×N の double で、大きい N(例 fnl4461)では約 160MB になる。
- **[軽微]** type 6 内で route、best_route、route_pos(サイズ N)を毎回確保する。頻度が 1e6 イテレーションに1回なので実害はない。
- **[軽微]** グローバル変数(DIST 等)は init 後読み取り専用なので replica_run でも競合しない。ガイドの禁止事項とは形式上矛盾するが安全。

## km.cpp(使用例)

- kmeans_new.cpp の使用例として妥当。fit のみ使用しており問題ない。
- **[軽微]** calc_dist が2乗距離を返す。fit の割当・重心計算は2乗ユークリッドの k-means として正しいが、この dist で fit_hamerly を呼ぶと三角不等式を満たさず誤動作する。例に一言あるとよい。
- **[軽微]** template.cpp と同じ min/max テンプレートを再定義している(混合型で切り捨ての罠、ahc.md 参照)。

## how_to_use_sa.md

- ライフサイクル、threshold の等号時受理、score の上書き復元、is_valid の扱い、Changed の作業領域化。いずれも sa.cpp・sa_state.cpp の実装と一致しており正確。
- **[注意] sa_multi_run が記載されていない**。「提供関数は sa_run と replica_run」と明言しているため、sa_multi_run を公開関数として残すならドキュメントに追加すべき。
- **[注意] 「探索ループ内で State 自体のコピーは発生しません」は sa_multi_run では成り立たない**。前項とあわせて要修正。
- **[軽微]** 最小テンプレートの `TYPE_CNT = 1` と sa_state.cpp の `TYPE_CNT = 0` が食い違う。

## kmeans.md

- 旧版 kmeans.cpp の仕様書。記載内容(テンプレート引数、fit/fit_flow の仕様)は実装と一致している。
- **[軽微]** ahc/clustering の文書が sa/ 配下にあり、置き場所が分かりにくい。kmeans_new.cpp の記載もない。
