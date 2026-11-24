# AHC ライブラリ概要

`titan_cpplib/ahc/` には AHC の提出コードへ組み込む探索部品、問題ごとの雛形、実験用コード、解法設計の文書があります。
各 `.cpp` はほかの `titan_cpplib` と同様に提出コードから直接 `include` して使用します。

## ビームサーチ

`beam_search/` の中心は、状態を 1 つだけ保持し、`apply_op` と `rollback` で探索木を巡回する実装です。

- `beam_search.cpp`: 深さが固定の標準版です。ハッシュによる重複除去と上位 `beam_width` 件の保持を行います。
- `beam_search_turn.cpp`: 操作ごとに進む論理ターンが異なる問題向けの実装です。`Action::target_turn` ごとに候補を管理します。
- `naive_beam_search.cpp`: 各候補が `State` を複製する比較用の単純版です。
- `beam_search_compose.cpp`: 子が 1 つだけの親の操作を `Action::compose` で子と合成する派生版です。
- `beam_search_radix.cpp`: 合成した操作を辺に保持する圧縮木版です。
- `beam_param.cpp`: 最大ターン、ビーム幅、制限時間、重複除去の方法などの設定です。
- `candidates.cpp`: ハッシュで重複を除去し、上位の候補を保持する共通部品です。
- `beam_log.cpp`, `beam_history.cpp`: ログと探索履歴を出力します。
- `beam_search_state.cpp`, `beam_search_state_turn.cpp`: 問題固有の `Action` と `State` を実装するための雛形です。
- `how_to_use_beam_search.md`: 標準版と可変ターン版の利用方法を説明します。
- `dynamic_beam_width_proposal.md`: 現行の動的ビーム幅を置き換える未実装の設計案です。
- `old/`: 過去の実装です。
- `test/`: 問題への組み込み例と比較用コードです。

標準版では主に次の関数を問題側で用意します。

- `State::init`
- `State::enumerate_actions`
- `State::try_op`
- `State::apply_op`
- `State::rollback`

スコアは小さいほど良いものとして扱います。ハッシュが同じ状態の中では、スコアが最も小さい候補だけを残します。

## Chokudai Search

`chokudai_search/` は各世代の未展開の候補を保持して繰り返し探索する実装です。
既存のビームサーチと同じ `Action`、`State`、`BeamParam`、`BeamResult` を利用できます。

- `chokudai_search.cpp`: 毎世代型です。`ChokudaiSearch` が 1 操作で 1 世代進む状態を扱います。
- `chokudai_search_turn.cpp`: 世代をスキップする型です。`ChokudaiSearchTurn` が `Action::target_turn` ごとに候補を管理します。
- `chokudai_param.cpp`: 既存の `BeamParam` に巡回数の上限を加える設定です。
- `chokudai_search_state.cpp`, `chokudai_search_state_turn.cpp`: 問題固有の `Action` と `State` を実装するための雛形です。
- `how_to_use_chokudai_search.md`: 使用例、ビームサーチからの移行、設定、計算量とメモリを説明します。

`beam_width` は 1 回の巡回で各世代から展開する数であり、保存する候補数に上限はありません。
`time_limit` による打ち切りに加えて、`ChokudaiParam::max_sweeps` で巡回数を指定できます。
完成解を得た後も探索を続け、最良の完成解を返します。2 つの実装は同時に include できます。

## 焼きなまし

`sa/sa.cpp` が焼きなましの本体であり、問題固有の処理は `State` に分離されています。

- `sa_run`: 単一の状態で行う通常の焼きなましです。提出では主にこの関数を使用します。
- `sa_multi_run`: 複数の初期状態を短時間ずつ探索し、最良の状態から残りの時間で探索を続けます。
- `replica_run`: 複数の温度を OpenMP で並列に探索するレプリカ交換法です。主にローカルでの調査に使用します。
- `sa_state.cpp`: `modify`、`rollback`、`advance` などを実装するための雛形です。
- `how_to_use_sa.md`: 状態の更新手順、差分計算、採択判定、並列実行時の注意を説明します。
- `test/vis_replica.py`: レプリカ交換の記録を描画する補助処理です。

焼きなましでもスコアは小さいほど良いものとして扱います。`modify` で変更の候補を作成し、不採択の場合は `rollback`、採択の場合は `advance` を呼び出します。

引数のある状態を使用する場合は、seed から状態を作成する関数を `sa_run` へ渡すことができます。TSP の初期局所探索などの探索前の処理も、この関数の中に記述できます。

## 巡回セールスマン問題

`tsp/` は対称 TSP 向けの状態と近傍操作を提供します。

- `tsp.cpp`: 問題、候補表、単一の巡回路の状態、最近傍法による初期解
- `tsp_symmetric_moves.cpp`: 2-opt、Or-opt、反転 Or-opt、Double Bridge
- `tsp_local_search.cpp`: 全探索または候補制限付きの 2-opt 局所探索
- `tsp_guided_local_search.cpp`: 標準的な誘導局所探索
- `tsp_edge_penalty_search.cpp`: `tsp/examples/tsp2.py` と同じ処理順の比較用探索
- `tsp_initial_state.cpp`: SA の初期状態を改善する方法の切り替え
- `multiple_tours.cpp`: 固定デポがある複数の巡回路での区間の移動と交換
- `how_to_use_tsp.md`: 使用例、利用条件、計算量
- `test/`: 差分、不変条件、比較用探索、SA 連携の自動確認
- `examples/`: 参照元の大きな SA の例、`tsp2.py`、TSPLIB 入力、可視化

巡回順、点から位置への逆引き、巡回路のコストは `TspState` がまとめて管理します。近傍操作では先にコストの差分だけを計算し、採択後に状態へ適用できます。このため、焼きなましで操作の適用を採択後まで遅延させる場合に使用できます。

## グリッド

- `grid/`: Bitboard による盤面処理、鏡で光を反射させる盤面を扱う MirrorGrid、一般的な Grid 実装の設計案があります。選び方と文書一覧は `grid/README.md` を参照してください。

## その他の共通部品

- `timer.cpp`: 実時間をミリ秒で計測します。
- `cpu_timer.cpp`: プロセスの CPU 時間をミリ秒で計測します。
- `profiler.cpp`: 名前を付けた処理区間の回数、合計時間、平均時間を集計します。
- `state_pool.cpp`: 状態を番号で確保して再利用する簡易的な領域管理です。
- `pruner/hoeffding_pruner.cpp`: 同じ入力で得たスコア差を Hoeffding の不等式で判定します。
- `pruner/wilcoxon_pruner.cpp`: 同じ入力で得たスコア差を Wilcoxon 符号順位検定で判定します。
- `pruner/successive_halving_pruner.cpp`: 評価の途中で候補数を段階的に減らします。
- `clustering/`: K-means、EMA 更新を使用する e-means、個数制約付き K-means、クラスタリング SA、階層型クラスタリングがあります。選び方と文書一覧は `clustering/README.md` を参照してください。
- `tsp/`: 対称 TSP の状態、近傍、局所探索、誘導局所探索、複数巡回路があります。
- `normal_distribution.cpp`: 正規分布の確率、密度、逆累積確率などを計算します。
- `mcmc.cpp`: 線形な正規分布モデルに特化した Gibbs sampler です。

AHC 専用ではありませんが、空間的な近傍の候補を作成する部品として次のファイルも利用できます。

- `geometry/delaunay_triangulation.cpp`: 整数座標の点のドロネー三角形分割と隣接点
- `geometry/voronoi_diagram.cpp`: 指定した長方形で切り取ったボロノイ領域
- `geometry/kd_tree.cpp`: 多次元の点の最近点、上位 k 点、半径内の点を検索する k-d tree
- `geometry/kd_tree_2d.cpp`: 検索の定数倍とメモリを抑えた静的な 2 次元 k-d tree
- `geometry/incremental_kd_tree_2d.cpp`: 点番号を維持したまま点を追加できる 2 次元 k-d tree
- `geometry/how_to_use_delaunay_voronoi.md`: 重複点と退化入力の扱い、使用例、計算量
- `geometry/how_to_use_kd_tree.md`: 点の型、所有関係、検索方法、計算量
- `geometry/how_to_use_kd_tree_2d.md`: 静的版と追加に対応した版の選び方、所有関係、追加、計算量
- `geometry/benchmark/kd_tree_benchmark.cpp`: 各 k-d tree の構築、検索、追加の定数倍を比較する
  ベンチマーク

## 解法設計の文書

- `典型.md`: 問題の言い換え、分割、探索法の選択、評価関数、構築法などの総合的なメモです。後半には AI 向けの質問文も含みます。
- `貪欲.md`: 貪欲法の派生と評価関数の設計方法を説明します。
- `過去改変.md`: 現在の操作を改善するために、過去の操作列に挿入、置換、削除を行う構築法を整理した文書です。

## ファイルの位置付け

- 提出用の主な候補: `beam_search.cpp`、`beam_search_turn.cpp`、`sa.cpp`、Bitboard、各種タイマー
- 問題ごとに変更するファイル: `beam_search_state*.cpp`、`sa_state.cpp`
- 比較用と高度な派生版: 状態複製版、操作合成版、圧縮木版、レプリカ交換版
- 個別の実験: `beam_search/test/`、`sa/test/`、`clustering/test/`、`clustering/benchmark/`、`tsp/examples/`
- 設計だけの文書: `dynamic_beam_width_proposal.md`、日本語の解法文書
- 過去の実装: `beam_search/old/`

`beam_search.cpp`、`beam_search_compose.cpp`、`beam_search_turn.cpp` は同名のクラスを定義するため、同時には使用せず、目的に応じて 1 つを選択します。
