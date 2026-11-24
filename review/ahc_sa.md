# titan_cpplib/ahc/sa レビュー

2026-11-24 時点のコードを精査した結果の修正箇所です。実行はしていません。

対象は `titan_cpplib/ahc/sa/` の全ファイルと、使用例の `titan_cpplib/ahc/tsp/examples/sa_tsp.cpp` です。

重要度は次の 3 段階に分類しました。

- **[バグ]** 誤動作、未定義動作、コンパイル不能につながります。
- **[注意]** 特定の条件で問題になります。
- **[軽微]** 通常の使用では結果に影響しません。

## sa.cpp

### 3 つの関数に共通する修正

- **[注意] 引数と温度パラメータの範囲の確認を追加します。** 各関数の先頭で次の条件を `assert` で確認します。
  - `TIME_LIMIT` が有限であること。NaN の場合は `sa_run` と `sa_multi_run` のループが終了せず、`progress` から `temp_idx` への変換が未定義動作になります。正の無限大の場合は 3 つの関数とも終了しません。
  - `State::param.start_temp` が正の有限値であること。0 以下または NaN の場合は温度に 0 以下の値または NaN が含まれます。
  - `State::param.end_temp` が有限であること。NaN と正の無限大は `max(end_temp, 1e-9)` で置き換えられません。
  - `sa_multi_run` の `NUM_STARTS` が 1 以上であること。0 以下の場合は `init` を呼び出していない `State` と未初期化の `Result` で第 2 段階を実行します。
  - `sa_multi_run` の `FIRST_PHASE_RATIO` が 0 以上 1 以下であること。1 を超える場合は第 1 段階だけで `TIME_LIMIT` を超過し、NaN の場合は第 1 段階のループが終了しません。
  - `replica_run` の `NUM_REPLICAS` が 1 以上であること。0 の場合は空の `vector` に対して `states[0]` を参照し、負の場合は `vector` の確保で例外が発生します。
  - `replica_run` の `SWAP_ITER_INTERVAL` が 1 以上であること。0 以下の場合は `modify` を呼び出さずに `TIME_LIMIT` まで交換だけを繰り返します。
- **[注意] `replica_run` の温度の計算でも `end_temp` を `max(end_temp, 1e-9)` に置き換えます。** 現在は `sa_run` と `sa_multi_run` だけが置き換えています。`end_temp` が 0 以下の場合は温度に 0 以下の値または NaN が含まれ、交換の判定に使用する `delta` が不正な値になります。
- **[注意] 統計配列の添字に使用する `changed.type` の範囲を確認します。**
  - `sa_run` と `sa_multi_run` の統計配列の長さを、`replica_run` と同じ `max(1, TYPE_CNT)` にします。現在は `TYPE_CNT` が 0 の場合に必ず範囲外参照になります。
  - `modify` の呼び出しの直後に `0 <= changed.type < TYPE_CNT` を `assert` で確認します。`State::modify` が `changed.type` に代入せずに return した場合に、直前の反復の値または未初期化の値を添字に使用するためです。
- **[軽微] `ScoreType` が整数型の場合は、`threshold` を `floor` で丸めてから変換します。** 現在は 0 の方向に丸められます。丸める前の値が負で整数でない場合に、本来の閾値より 1 未満だけ大きいスコアまで受理します。0 以上の場合は結果が変化しません。
- **[軽微] `replica_run` が `State::modify` に渡す `iter` を 1 から始めます。** `sa_run` と `sa_multi_run` は 1 から、`replica_run` は 0 から始めています。`iter % 周期 == 0` で分岐する `State` は、`replica_run` でだけ最初の呼び出しがその分岐になります。
- **[軽微] 統計の更新を `verbose` が真の場合に限定します。** 対象は種類別の統計配列と `valid_cnt`, `upd_cnt`, `bst_cnt` です。`State::modify` に渡す `iter` は探索の入力であるため維持します。
- **[軽微] `LOG_TABLE` の定義に `inline` を付けます。** 現在は外部リンケージの定義であり、複数の翻訳単位から include するとリンクエラーになります。
- **[軽微] `<omp.h>` の include と `replica_run` の定義を `#ifdef _OPENMP` で囲みます。** 現在は `replica_run` を使用しない場合も `<omp.h>` を include します。
- **[軽微] `titan23::Timer` の `high_resolution_clock` を `steady_clock` に変更します。** libstdc++ では `high_resolution_clock` は `system_clock` の別名であり、単調増加は保証されません。実行中にシステムの時刻が戻ると `progress` が負になり、温度表の範囲外を参照します。修正するファイルは `titan_cpplib/ahc/timer.cpp` です。
- **[軽微] 受理確率が 1/4096 単位に丸められることを `LOG_TABLE` のコメントに記載します。** スコアの悪化量を `d`、温度を `T` とすると、受理確率は `exp(-d / T)` を 1/4096 の倍数に四捨五入した値になります。`d > T * log(8192)` の遷移は受理されません。`log(8192)` は約 9.01 です。
- **[軽微] 経過時間の取得を間引く設定を追加するかを決定します。** `sa_run` と `sa_multi_run` は経過時間を反復ごとに取得しており、`modify` の処理時間が短い問題では取得のコストが反復回数に影響します。`sa_run` には 32 反復ごとに取得するコードがコメントとして残っています。間引くと、取得しない反復では `progress` と温度が同じ値になり、`TIME_LIMIT` を超過する時間も増えます。採用しない場合はコメントのコードを削除します。

### sa_multi_run の修正

- **[注意] 第 1 段階の終了温度を `phase2_start_temp` にします。** 現在の第 1 段階は各初期状態を終了温度まで冷却し、第 2 段階は `phase2_start_temp` から冷却を再開するため、第 2 段階の開始時に温度が上がります。`State::modify` に渡す `progress` も、初期状態ごとと第 2 段階でそれぞれ 0 から 1 まで変化します。再加熱を意図している場合はコードを変更せず、関数のコメントにその仕様を記載します。
- **[注意] 各初期状態の開始時刻 `start_time` を `init` の前に取得します。** 現在は `init` と `State` のコピーの後に取得するため、第 1 段階の全体の時間が `NUM_STARTS` 回分の `init` とコピーの時間だけ長くなり、その分だけ第 2 段階が短くなります。合計が `TIME_LIMIT` を超える場合は、`TIME_LIMIT` を超過してから return します。
- **[注意] `TIME_LIMIT` が 0 以下の場合の早期 return を追加します。** 現在は `init` を `NUM_STARTS` 回実行してから return します。

### replica_run の修正

- **[軽微] `progress` を探索の開始時刻からの割合にします。** 現在は `now_time / TIME_LIMIT` であり、`init` の時間を含むため、`sa_run` と異なり探索の開始時に 0 になりません。
- **[軽微] スレッドごとの統計を `alignas(64)` の構造体にまとめます。** 統計配列は反復ごとに更新され、配列が短いため、複数のスレッドの配列が同じキャッシュラインに配置される可能性があります。受理の判定用の乱数は `PaddedRandom` で同じ対策をしています。

## sa_state.cpp

- **[注意] `modify`, `rollback`, `advance` のコメントの行末のバックスラッシュを削除します。** `//` のコメントの行末にバックスラッシュがあると、次の行もコメントの続きになります。現在は各コメントの最後の行にバックスラッシュがないため、関数の定義はコメントになっていません。最後の行にバックスラッシュを付けた場合や、最後の行だけを削除した場合は、直後の関数の定義の 1 行目がコメントになります。g++ は `-Wall` を指定すると、該当する 3 箇所で `multi-line comment` の警告を出力します。
- **[注意] `rollback` の先頭の `if (!is_valid) { return; }` に前提のコメントを追加します。** この早期 return は、`modify` が盤面を変更する前に `is_valid = false` にする実装でだけ正しく動作します。`how_to_use_sa.md` は、`modify` の中で盤面を部分的に変更した場合はその変更も `rollback` で元に戻すように求めており、その実装方法では早期 return を削除する必要があります。
- **[軽微] `sa_init()` を削除します。** ローカル変数の `Random` を作成して破棄するだけであり、効果がありません。残す場合は `inline` を付けます。

## how_to_use_sa.md

- **[注意] `sa_multi_run` の説明を追加します。** 追加する箇所は、Section 0 の提供する関数の説明、Section 0.5 のファイルの表、シグネチャの一覧、関数の表です。`titan_cpplib/ahc/README.md` は `sa_multi_run` も挙げています。
- **[注意] Section 0 の「探索ループ内で `State` 自体のコピーは発生しません」を `sa_run` と `replica_run` に限定します。** `sa_multi_run` は、各初期状態の探索の開始時、最良値を更新するたび、第 2 段階の開始時に `State` をコピーします。
- **[軽微] `State::print()` の説明を削除します。** 該当する箇所は「ビルド時の注意」と Section 2 の `State::print` の節です。テンプレートの `State` に `print` はなく、ライブラリも呼び出しません。
- **[軽微] seed を渡す `sa_run` の第 2 引数が `seed` であることを関数の表に記載します。** `sa_run<State>(T, false)` のように `verbose` のつもりで `bool` を渡すと、`seed` に 0 または 1 が渡され、`verbose` は真のままになります。
- **[軽微] `replica_run` が `TIME_LIMIT` を確認する間隔を記載します。** 確認は交換の間隔で行うため、最大で各レプリカの `SWAP_ITER_INTERVAL` 回の反復の時間だけ超過します。

## sa_tsp.cpp

近傍操作の種類は `changed.type` の値で示します。

- **[軽微] `P` を type 6 で処理する巡回路の大きさに合わせて確保します。** 現在は `N * N` 要素の `double` の配列であり、`N` が 4461 の場合は `State` 1 つにつき約 159 MB になります。`replica_run` では `NUM_REPLICAS` 個分を確保します。
- **[軽微] type 6 の `route`, `best_route`, `route_pos` を `State` のメンバにして再利用します。** 現在は呼び出しのたびに確保します。頻度は 100 万反復に 1 回であるため、影響は小さいです。

## vis_replica.py

- **[軽微] 読み込みの箇所のコメントの「各レプリカのスコア」を「各温度のスコア」に修正します。** `replica_run` は、温度の番号 `i` を担当している `State` のスコアを `i` 番目の列に記録します。
