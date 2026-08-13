# クラスタリングのベンチマーク

## 目的

通常のK-means、Hamerly法、個数制約付きK-means、クラスタリングSAを同じ問題で比較する。

確認するのは次の四点である。

- 所属と個数条件が常に正しいか
- 既知の最良費用へどこまで近づけるか
- 同じ総時間なら、K-meansの複数初期値とSAのどちらがよいか
- SAの各近傍が、速度と解の質にどれだけ寄与するか

自動テストではない。自動テストは小さい入力で実装の正しさを確認し、ここでは複数の種と実行時間を使って性能を測る。

## ファイル

- `benchmark/clustering_benchmark.cpp`: 問題ファイルの読み書き、費用と調整ランド指数の再計算
- `benchmark/generate_cases.cpp`: 固定した人工問題の生成
- `benchmark/prepare_public_data.py`: 公開問題と一般的な数値表の変換
- `benchmark/run_benchmark.cpp`: 各手法の実行とCSV出力
- `benchmark/summarize.py`: 問題、手法ごとの集計
- [`public_data.md`](public_data.md): 公開問題の入手先と使い分け
- [`results_2026-08-13.md`](results_2026-08-13.md): 初回の実測結果と現時点の判断
- `benchmark/data/`: 変換後の問題を置く場所

## 問題ファイル

拡張子は `.tcb` とする。点群、クラスタ数、個数条件、既知の最良費用、正解ラベルを一つのファイルへ入れる。

```text
titan_clustering_benchmark_v1
name "sample"
source "example"
points 4
dimension 2
clusters 2
best_known_cost none
ranges 1 4 1 4
reference_labels present 0 0 1 1
data
0 0
1 0
10 0
11 0
```

`ranges` にはクラスタごとの下限と上限を順に書く。個数自由なら全クラスタを `[1,n]`、個数固定なら下限と上限を同じ値にする。SA版は空クラスタを扱わないため、下限は1以上にする。

`best_known_cost` が不明なら `none` とする。`reference_labels` がない場合も `none` とする。正解ラベルと二乗誤差の最適解は一致するとは限らず、分類ラベルの種類数が最適化で指定するクラスタ数と異なる場合もある。正解ラベルには個数条件を課さず、両者を分けて記録する。

## 構築

C++20を使う。個数制約付きK-meansを含むため、AtCoder Libraryを読み込める環境が必要である。

```bash
g++ -O3 -march=native -DNDEBUG -std=c++20 -I. \
  titan_cpplib/ahc/clustering/benchmark/generate_cases.cpp \
  -o clustering_generate_cases

g++ -O3 -march=native -DNDEBUG -std=c++20 -I. \
  titan_cpplib/ahc/clustering/benchmark/run_benchmark.cpp \
  -o clustering_benchmark
```

## 人工問題

```bash
./clustering_generate_cases titan_cpplib/ahc/clustering/benchmark/data
```

次の条件を固定した種で作る。

- 分離が明確
- 重なりが大きい
- 細長い分布
- クラスタの大きさが不均衡
- 外れ値を含む
- 同じ座標を重複して含む
- 高次元
- 個数自由、個数固定、上下限付き
- 点数、次元数、クラスタ数が大きい問題

人工問題の正解ラベルは生成に使った所属であり、二乗誤差を最小にする所属とは限らない。費用と調整ランド指数を別々に見る。

## 実行

次は一例である。実行時間は各試行1000ミリ秒、種を変えて30回測る。

```bash
./clustering_benchmark \
  --output clustering_raw.csv \
  --runs 30 \
  --time-ms 1000 \
  titan_cpplib/ahc/clustering/benchmark/data/*.tcb
```

集計は次で行う。

```bash
python3 titan_cpplib/ahc/clustering/benchmark/summarize.py \
  clustering_raw.csv clustering_summary.csv
```

途中で中断しても、それまでの行はCSVへ書き出されている。同じ出力ファイルを再指定すると上書きする。

## 比較する手法

| 名前 | 内容 | 終了条件 |
|---|---|---|
| `kmeans` | Lloyd法、K-means++で初期化 | 収束または反復上限 |
| `hamerly` | Hamerly法、K-means++で初期化 | 収束または反復上限 |
| `hamerly_repeated` | Hamerly法を異なる初期値で繰り返す | 総時間 |
| `hamerly_repaired` | Hamerly法の所属を個数条件へ合わせる簡易初期解 | 一回 |
| `balanced` | 最小費用流で個数条件を守るK-means | 収束または反復上限 |
| `balanced_repeated` | 個数制約付きK-meansを異なる初期値で繰り返す | 総時間 |
| `sa` | Hamerly法または修正した所属からSA | 初期解を含む総時間 |
| `sa_balanced` | 個数制約付きK-meansからSA | 初期解を含む総時間 |
| `sa_no_relocate` | 一点移動を外したSA | 初期解を含む総時間 |
| `sa_no_swap` | 二点交換を外したSA | 初期解を含む総時間 |
| `sa_no_cycle` | 三点循環を外したSA | 初期解を含む総時間 |
| `sa_no_rebuild` | 二・三クラスタ再構築を外したSA | 初期解を含む総時間 |
| `sa_no_rebuild_three` | 三クラスタ再構築だけを外したSA | 初期解を含む総時間 |

実験用に `sa_multi_initial_10`、`20`、`30`、`40`、`70`、`80`、`90` も指定できる。末尾の数値は、総時間の何%までHamerly法による初期解作成を繰り返すかを表す。これらは既定の実行手法には含めない。

移動元クラスタの選び方を比較するため、`sa_cluster_samples_8`、`sa_cluster_samples_16`、`sa_early_cluster_samples_8`、`sa_early_cluster_samples_16`、`sa_no_early_cluster_boost` も指定できる。`cluster_samples` は全期間、`early_cluster_samples` は序盤だけの候補数を変える。`sa_no_early_cluster_boost` は、50クラスタ以上で序盤の候補数を自動的に16へ増やす処理を外す。

点候補数、再構築の重み、仮中心、反復数を比較する `sa_point_samples_16`、`sa_point_samples_32`、`sa_early_point_samples_16`、`sa_early_rebuild_40`、`sa_early_rebuild_60`、`sa_refined_rebuild_seeds`、`sa_rebuild_iterations_2`、`sa_rebuild_iterations_6` も指定できる。これらは既定の実行手法には含めない。

個数自由の問題では `balanced`、`balanced_repeated`、`sa_balanced` を省く。個数条件がある問題では、条件を守らない `kmeans`、`hamerly`、`hamerly_repeated` を省く。

`hamerly_repaired` は、各クラスタに残す点を現在の中心への距離で決め、余った点を不足するクラスタへ入れる簡易処理である。個数条件下の有効な初期解を安く作るための比較対象であり、その割当自体の最適性は保証しない。

実行する手法を絞る場合はカンマ区切りで指定する。

```bash
./clustering_benchmark \
  --output clustering_raw.csv \
  --methods hamerly_repeated,sa,sa_no_rebuild \
  case1.tcb case2.tcb
```

## 主な設定

| 引数 | 既定値 | 意味 |
|---|---:|---|
| `--output` | 必須 | 出力CSV |
| `--tag` | 空文字 | 構築設定や実験名を区別する任意の文字列 |
| `--runs` | 30 | 問題、手法ごとの試行数 |
| `--seed` | 23 | 基準の種 |
| `--time-ms` | 1000 | 総時間で比較する手法の一試行あたり時間 |
| `--max-iterations` | 100 | K-means系の一回あたり反復上限 |
| `--balanced-edge-limit` | 50000 | 最小費用流で使う、点とクラスタを結ぶ辺の本数の上限 |
| `--methods` | 全手法 | 実行する手法 |

試行番号ごとの種は十分離し、一試行内の複数初期値はそこから1ずつ増やす。手法の実行順は試行ごとに一つずつ回し、常に同じ手法が最初または最後にならないようにする。

## 時間の扱い

`hamerly_repeated`、`balanced_repeated`、各SAを時間比較の対象とする。

- SAの総時間には、問題データの準備と初期解の作成を含める
- 問題ファイルの読み込みは全手法に共通なので、各手法の計測前に行う
- 初期解だけで時間を使い切った場合は、その初期解を返す
- 問題データ、初期解、SA状態の構築や、最小費用流とK-meansの一回を途中で止めないため、その処理の残り時間だけ上限を超える場合がある
- CSVへの書き出しは計測後に行う
- 完了まで測る `kmeans`、`hamerly`、`balanced` は、一回の速さと基礎的な解の質を見るための別枠とする

10、30、100、300、1000ミリ秒のように総時間を変えて別々に実行すると、短時間から長時間までの伸びを確認できる。比較時は同じ計算機、構築設定、実行時間、問題順を使う。

## 個数制約付きK-meansの費用

`kmeans_balanced.cpp` の最小費用流は整数の辺費用を使う。ベンチマークでは二乗距離に倍率を掛けて整数へ丸める。倍率は原則100万とし、AtCoder Libraryの上限を超えない範囲まで自動的に下げる。実際の倍率は `flow_cost_scale` に出す。

固定した中心に対する割当は、この丸めた費用と個数条件の下で最小になる。ただし、中心更新を含む反復全体の大域的な最適性は保証しない。`best_known_cost` との差は、丸める前の二乗誤差を再計算して求める。

倍率が小さくなる問題では、多くの辺費用が同じ整数へ丸められ、割当の区別が弱くなる。`flow_cost_scale` も結果と一緒に確認する。

## CSV

重要な列は次のとおりである。

- `status`: `ok`、`skipped`、`error`。所属が個数条件を破る場合と、費用が有限な非負値でない場合も `error`
- `experiment_tag`, `compiler`: 実験名とコンパイラ
- `max_iterations`, `balanced_edge_limit`: 実行時の主要設定
- `elapsed_ms`: 実測時間
- `initialization_ms`, `search_ms`: SA用問題データと初期解の作成時間、SA状態の構築を含む探索時間
- `reported_cost`: ライブラリが返した費用
- `recalculated_cost`: 所属から長倍精度で再計算した費用
- `difference_from_best_percent`: 既知の最良費用からの差の割合
- `adjusted_rand_index`: 正解ラベルとの調整ランド指数
- `valid`: 所属と個数条件を満たすか
- `trials`: 時間内に完了した初期値の数
- `initial_cost`, `improvement_from_initial_percent`: SAの初期費用と初期解からの改善率
- `iterations`, `converged`: K-meansの反復回数と収束状態。SAの行では初期解を作ったK-meansについての値
- `label_hash`: 同じラベル列を再現したか確認する値
- `attempts_*`, `valid_*`, `accepted_*`, `improvements_*`: SA近傍ごとの探索終了までの統計
- `rebuild_three_*`: 三クラスタ再構築だけを抜き出した試行数、有効数、採択数、改善数、改善量、同じ所属へ戻った回数

`reported_cost` と `recalculated_cost` が大きく違う場合は、費用の差分更新か数値範囲を疑う。`valid` が0の結果は性能比較に使わない。

集計CSVには、費用と実行時間の最良値、中央値、悪い側の90パーセンタイル、調整ランド指数、近傍ごとの試行速度、有効率、採択率、改善率、改善量を出す。候補情報の更新頻度と、再構築が同じ所属へ戻った割合も集計する。単位時間の値は `search_ms` を分母にする。最良の一回だけでなく、中央値と悪い側も重視する。

## 評価の進め方

1. 小さい人工問題で `valid=1` と費用の再計算結果を確認する
2. 最良費用が分かる小規模問題で、最良値との差と30試行の安定性を見る
3. 中規模、大規模問題で `hamerly_repeated` と `sa` を同じ総時間で比較する
4. 個数固定問題で `balanced_repeated`、`sa_balanced`、`sa` を比較する
5. SA近傍を一つずつ外し、費用だけでなく試行速度、有効率、採択率も見る
6. 総時間を複数段階に変え、どの時点からSAが有利になるかを見る
7. 最後に独立した実装とも比較する

上位を狙えるかは一つの問題の最良値では判断しない。公開問題とAHCを想定した大規模人工問題の両方で、中央値と悪い側が既存手法を上回り、近傍を外した比較から改善理由を説明できる状態を目標にする。
