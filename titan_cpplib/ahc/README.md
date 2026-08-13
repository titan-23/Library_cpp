# AHC ライブラリ概要

`titan_cpplib/ahc/` には、AHC の提出コードへ組み込む探索部品、問題ごとの雛形、実験用コード、解法設計の文書が置かれている。
各 `.cpp` は、ほかの `titan_cpplib` と同様に提出コードから直接 `include` して使う。

## ビームサーチ

`beam_search/` の中心は、状態を一つだけ持ち、`apply_op` と `rollback` で探索木を巡回する実装である。

- `beam_search.cpp`: 固定深さの標準版。ハッシュによる重複除去と上位 `beam_width` 件の保持を行う。
- `beam_search_turn.cpp`: 操作ごとに進む論理ターンが異なる問題向け。`Action::target_turn` ごとに候補を管理する。
- `naive_beam_search.cpp`: 各候補が `State` を複製する比較用の単純版。
- `beam_search_compose.cpp`: 一本道の操作を `Action::compose` で合成する派生版。
- `beam_search_radix.cpp`: 合成した操作を辺に持つ圧縮木版。
- `beam_param.cpp`: 最大ターン、ビーム幅、時間予算、重複除去方法などの設定。
- `candidates.cpp`: ハッシュ重複除去付きで上位候補を保持する共通部品。
- `beam_log.cpp`, `beam_history.cpp`: ログと探索履歴の出力。
- `beam_search_state.cpp`, `beam_search_state_turn.cpp`: 問題固有の `Action` と `State` を埋める雛形。
- `how_to_use_beam_search.md`: 標準版と可変ターン版の利用方法。
- `dynamic_beam_width_proposal.md`: 現行の動的ビーム幅を置き換える未実装の設計案。
- `old/`: 過去の実装。
- `test/`: 問題への組み込み例と比較用コード。

標準版では、主に次の関数を問題側で用意する。

- `State::init`
- `State::enumerate_actions`
- `State::try_op`
- `State::apply_op`
- `State::rollback`

スコアは小さいほど良いものとして扱う。同じハッシュの状態は、スコアが最も小さい候補だけを残す。

## 焼きなまし

`sa/sa.cpp` が焼きなましの本体で、問題固有の処理は `State` に分離されている。

- `sa_run`: 単一状態で行う通常の焼きなまし。提出用の中心関数。
- `sa_multi_run`: 複数の初期状態を短く探索し、最良状態から残り時間を探索する。
- `replica_run`: 複数温度を OpenMP で並列探索するレプリカ交換法。主にローカル調査用。
- `sa_state.cpp`: `modify`、`rollback`、`advance` などを埋める雛形。
- `how_to_use_sa.md`: 状態の更新手順、差分計算、採択判定、並列実行時の注意をまとめた説明。
- `test/vis_replica.py`: レプリカ交換の記録を描画する補助処理。

焼きなましもスコアを小さいほど良いものとして扱う。`modify` で変更候補を作り、不採択なら `rollback`、採択なら `advance` を呼ぶ。

引数のある状態を使う場合は、seedから状態を作る関数を `sa_run` へ渡せる。TSPの初期局所探索など、探索前の処理もこの関数内で組み立てられる。

## 巡回セールスマン問題

`tsp/` は対称TSP向けの状態と近傍操作を提供する。

- `tsp.cpp`: 問題、候補表、単一巡回路の状態、最近傍初期解。
- `tsp_symmetric_moves.cpp`: 2-opt、Or-opt、反転Or-opt、Double Bridge。
- `tsp_local_search.cpp`: 全探索または候補制限付き2-opt局所探索。
- `tsp_guided_local_search.cpp`: 標準的な誘導局所探索。
- `tsp_edge_penalty_search.cpp`: `tsp/examples/tsp2.py`の処理順を保った比較用探索。
- `tsp_initial_state.cpp`: SAの初期状態を改善する方法の切り替え。
- `multiple_tours.cpp`: 固定デポを持つ複数巡回路の区間移動・交換。
- `how_to_use_tsp.md`: 使用例、利用条件、計算量。
- `test/`: 差分、不変条件、比較用探索、SA連携の自動確認。
- `examples/`: 参照元の大きなSA例、`tsp2.py`、TSPLIB入力、可視化。

巡回順、点から位置への逆引き、巡回費用は`TspState`が一体で管理する。近傍は先に費用差だけを作り、採択後に状態へ適用できるため、焼きなましの遅延適用に使える。

## グリッド

- `grid/`: Bitboardによる盤面処理と、一般的なGrid実装の設計案。選び方と文書一覧は `grid/README.md` を参照。

## その他の共通部品

- `timer.cpp`: 実時間をミリ秒で測る。
- `cpu_timer.cpp`: プロセスの CPU 時間をミリ秒で測る。
- `profiler.cpp`: 名前を付けた処理区間の回数、合計時間、平均時間を集計する。
- `state_pool.cpp`: 状態を番号で確保・再利用する簡易領域管理。
- `pruner/hoeffding_pruner.cpp`: 同じ入力で得たスコア差を Hoeffding の不等式で判定する。
- `pruner/wilcoxon_pruner.cpp`: 同じ入力で得たスコア差を Wilcoxon 符号順位検定で判定する。
- `pruner/successive_halving_pruner.cpp`: 評価の途中で候補数を段階的に減らす。
- `clustering/`: K-means、個数制約付きK-means、クラスタリングSA、階層型クラスタリング。選び方と文書一覧は `clustering/README.md` を参照。
- `tsp/`: 対称TSPの状態、近傍、局所探索、誘導局所探索、複数巡回路。
- `normal_distribution.cpp`: 正規分布の確率、密度、逆累積確率などの計算。
- `mcmc.cpp`: 線形な正規分布モデルに特化した Gibbs sampler。

AHC専用ではないが、空間的な近傍候補を作る部品として次も利用できる。

- `geometry/delaunay_triangulation.cpp`: 整数座標点のドロネー三角形分割と隣接点。
- `geometry/voronoi_diagram.cpp`: 指定長方形内へ切り取ったボロノイ領域。
- `geometry/kd_tree.cpp`: 多次元点の最近点、上位k点、半径内の点を探すk-d tree。
- `geometry/how_to_use_delaunay_voronoi.md`: 重複点と退化入力の扱い、使用例、計算量。
- `geometry/how_to_use_kd_tree.md`: 点型、所有関係、検索方法、計算量。

## 解法設計の文書

- `典型.md`: 問題の言い換え、分割、探索法の選択、評価関数、構築法などの総合メモ。後半には AI 向けの質問文も含む。
- `貪欲.md`: 貪欲法の派生と評価関数の設計方法。
- `過去改変.md`: 現在の操作を改善するために、過去の操作列を挿入・置換・削除する構築法の整理。

## ファイルの位置付け

- 提出用の中心候補: `beam_search.cpp`、`beam_search_turn.cpp`、`sa.cpp`、Bitboard、各種タイマー。
- 問題ごとに書き換えるもの: `beam_search_state*.cpp`、`sa_state.cpp`。
- 比較・高度な派生: 状態複製版、操作合成版、圧縮木版、レプリカ交換版。
- 個別実験: `beam_search/test/`、`sa/test/`、`clustering/test/`、`clustering/benchmark/`、`tsp/examples/`。
- 設計だけのもの: `dynamic_beam_width_proposal.md`、日本語の解法文書。
- 過去のもの: `beam_search/old/`。

`beam_search.cpp`、`beam_search_compose.cpp`、`beam_search_turn.cpp` は同名のクラスを定義するため、同時に組み合わせるのではなく、目的に応じて一つを選んで使う。
