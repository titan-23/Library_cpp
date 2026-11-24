# クラスタリングのベンチマーク

K-means、Hamerly 法、e-means、個数制約付き K-means、クラスタリング SA を同じ問題で比較します。正しさの検証は `test/` で行い、ここでは複数のシードと実行時間で性能を計測します。

## ファイル

- `clustering_benchmark.cpp`: 問題ファイルの読み書き、コストと調整ランド指数の再計算
- `generate_cases.cpp`: 固定した人工問題の生成
- `prepare_public_data.py`: 公開問題と一般的な数値表の変換
- `run_benchmark.cpp`: 各手法の実行と CSV 出力
- `summarize.py`: 問題と手法の組ごとの集計

## ビルドと実行

リポジトリのルートで実行します。個数制約付き K-means を含むため、AtCoder Library をインクルードできる必要があります。

```bash
g++ -O3 -march=native -DNDEBUG -std=c++20 -I. \
  titan_cpplib/ahc/clustering/benchmark/generate_cases.cpp -o clustering_generate_cases
g++ -O3 -march=native -DNDEBUG -std=c++20 -I. -I/path/to/ac-library \
  titan_cpplib/ahc/clustering/benchmark/run_benchmark.cpp -o clustering_benchmark

./clustering_generate_cases titan_cpplib/ahc/clustering/benchmark/data
./clustering_benchmark --output raw.csv --runs 10 --time-ms 200 \
  titan_cpplib/ahc/clustering/benchmark/data/*.tcb
python3 titan_cpplib/ahc/clustering/benchmark/summarize.py raw.csv summary.csv
```

`benchmark/data/` は `.gitignore` で除外しています。公開データは配布元ごとに利用条件が異なるため、変換後もリポジトリに含めません。

途中で中断した場合も、それまでの行は CSV に出力されています。同じ出力ファイルを指定すると、既存のファイルの内容を置き換えます。

## 人工問題

`generate_cases.cpp` は、固定したシードで次の問題を生成します。

- 分離が明確、重なりが大きい、細長い分布、大きさが不均衡、外れ値を含む、同じ座標を含む
- 低次元から 64 次元まで
- 個数自由、個数固定、上下限付き
- 50 から 240 クラスタの個数固定問題
- 一様分布で、点が少ない、またはクラスタが多い個数自由問題

人工問題の正解ラベルは生成に使用した所属であり、二乗誤差を最小にする所属とは限りません。コストと調整ランド指数は別々に評価します。

## 手法

| 名前 | 内容 | 終了条件 |
|---|---|---|
| `kmeans` | Lloyd 法、K-means++ で初期化 | 収束または反復上限 |
| `hamerly` | Hamerly 法、K-means++ で初期化 | 収束または反復上限 |
| `emeans` | e-means の既定設定 | 収束または反復上限 |
| `hamerly_repeated` | シードを変更して Hamerly 法を繰り返し、最良の結果を残す | 総時間 |
| `emeans_repeated` | シードを変更して e-means を繰り返し、最良の結果を残す | 総時間 |
| `hamerly_repaired` | Hamerly 法の所属を個数条件に合わせて補正した簡易な初期解 | 1 回 |
| `balanced` | 最小費用流で個数条件を満たす K-means | 収束または反復上限 |
| `balanced_repeated` | シードを変更して `balanced` を繰り返す | 総時間 |
| `sa` | `hamerly_repaired` の所属を初期解とする SA | 初期解を含む総時間 |
| `sa_balanced` | `balanced` の所属を初期解とする SA | 初期解を含む総時間 |
| `sa_no_relocate` など | 1 点の移動、2 点の交換、3 点の循環、再構築のいずれかを除いた SA | 初期解を含む総時間 |
| `sa_no_rebuild_three` | 3 クラスタの再構築だけを除いた SA | 初期解を含む総時間 |

個数条件を考慮しない `kmeans`、`hamerly`、`emeans`、`*_repeated` は個数自由の問題だけで実行します。`hamerly_repaired`、`balanced` 系、`sa_balanced` は個数条件のある問題だけで実行します。実行しなかった問題と手法の組は `status=skipped` として出力します。

`hamerly_repaired` は、各クラスタで中心に近い点から目標の点数だけ残し、目標を超えた分の点を、点数が不足するクラスタに近い順に割り当てます。少ない計算量で有効な初期解を作成するための処理であり、割り当ての最適性は保証しません。

`balanced` の最小費用流は整数の辺のコストを使用するため、二乗距離に倍率を掛けて丸めます。倍率は 100 万を基本とし、AtCoder Library の上限を超えない範囲まで自動的に小さくします。実際の倍率は `flow_cost_scale` 列に出力します。倍率が小さい問題では、多くの辺のコストが同じ整数に丸められます。

実行する手法を限定する場合は、カンマ区切りで指定します。

```bash
./clustering_benchmark --output raw.csv --methods hamerly_repeated,emeans_repeated,sa case1.tcb case2.tcb
```

## 引数

| 引数 | 既定値 | 意味 |
|---|---:|---|
| `--output` | 必須 | 出力 CSV |
| `--tag` | 空文字 | ビルド設定や実験名を区別する任意の文字列 |
| `--runs` | 30 | 問題と手法の組ごとの試行数 |
| `--seed` | 23 | 基準のシードで、試行 `r` では `seed + r * 1000003` を使用する |
| `--time-ms` | 1000 | 総時間で比較する手法の 1 試行あたりの時間 |
| `--max-iterations` | 100 | K-means 系の 1 回あたりの反復上限 |
| `--balanced-edge-limit` | 50000 | 最小費用流で使用する、点とクラスタを結ぶ辺の本数の上限 |
| `--methods` | 全手法 | 実行する手法 |

手法の実行順は試行ごとに 1 つずつずらし、同じ手法が常に最初または最後に実行されないようにします。

## 時間の扱い

- 問題ファイルの読み込みは計測に含めません。
- SA の総時間には、SA 用の問題データと初期解の作成を含めます。初期解の作成だけで総時間に達した場合は、その初期解を返します。
- K-means の 1 回の実行、最小費用流の 1 回の計算、SA の状態の構築は途中で中断しないため、その分だけ上限を超える場合があります。
- `kmeans`、`hamerly`、`emeans`、`balanced` は完了まで実行して時間を計測し、1 回の実行の速さと基本的な解の質を評価します。

総時間を 10、30、100、300、1000 ミリ秒のように変更して別々に実行すると、実行時間を延ばしたときの解の改善を確認できます。比較する場合は、同じ計算機、ビルド設定、実行時間、問題順を使用します。

## 出力 CSV

主な列は次のとおりです。

- `status`: `ok`、`skipped`、`error` のいずれかです。所属が個数条件を満たさない場合と、コストが有限の非負値でない場合も `error` になります。
- `elapsed_ms`: 実測時間です。SA では `initialization_ms` と `search_ms` に分けた値も出力します。
- `reported_cost`, `recalculated_cost`: ライブラリが返したコストと、所属から long double の精度で再計算したコスト
- `difference_from_best_percent`: 既知の最良コストからの差の割合
- `adjusted_rand_index`: 正解ラベルとの調整ランド指数
- `valid`: 所属が個数条件を満たすかどうか
- `trials`: 時間内に完了した試行の数
- `initial_cost`, `improvement_from_initial_percent`: SA の初期解のコストと、そこからの改善率
- `iterations`, `converged`: K-means の反復回数と収束状態です。SA の行では、初期解を作成した K-means の値を出力します。
- `label_hash`: 同じ所属を再現したかを確認するための値
- `attempts_*`, `valid_*`, `accepted_*`, `improvements_*`, `rebuild_three_*`: SA の近傍ごとの統計

`reported_cost` と `recalculated_cost` が大きく異なる場合は、コストの差分更新か数値範囲に原因があると考えられます。`valid` が 0 の行は比較の対象外とします。

`summarize.py` は、問題と手法の組ごとに次の値を出力します。

- コストと実行時間の最良値、中央値、悪い側の 90 パーセンタイル
- 調整ランド指数
- 近傍ごとの試行速度、有効率、採択率、改善量

最良の 1 回だけでなく、中央値と悪い側の値も確認します。

## 問題ファイル

拡張子は `.tcb` とします。点群、クラスタ数、個数条件、既知の最良コスト、正解ラベルを 1 つのファイルに格納します。

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

`ranges` にはクラスタごとの下限と上限を順に記載します。個数自由の場合は全クラスタを `[1,n]` とし、個数固定の場合は下限と上限を同じ値にします。SA は空のクラスタを扱わないため、下限は 1 以上にします。`best_known_cost` と `reference_labels` が不明な場合は `none` とします。正解ラベルは二乗誤差の最適解と一致するとは限らず、ラベルの種類数がクラスタ数と異なっていてもかまいません。

## 公開問題

### SOS-SDP の結果がある小規模問題

[Ground truth clustering is not the optimum clustering](https://github.com/shudianzhao/GroundTruth_VS_OptimalCluster) には、二乗誤差のクラスタリングを SOS-SDP で解いた 13 種類の実データがあります。次のコマンドは固定したコミットを取得し、`データ名 × クラスタ数` ごとに `.tcb` を作成します。公開された所属から二乗誤差を再計算し、`best_known_cost` に設定します。

```bash
python3 titan_cpplib/ahc/clustering/benchmark/prepare_public_data.py \
  download-sos-sdp-reference titan_cpplib/ahc/clustering/benchmark/data
```

取得 URL、コミット、ZIP の SHA-256 は `sos_sdp_reference_source.txt` に保存されます。展開済みの配布物がある場合は、通信せずに変換できます。

```bash
python3 titan_cpplib/ahc/clustering/benchmark/prepare_public_data.py \
  sos-sdp-reference /path/to/GroundTruth_VS_OptimalCluster titan_cpplib/ahc/clustering/benchmark/data
```

配布元のライセンスは GPLv3 です。`best_known_cost` は公開された所属のコストであり、最適性が証明された値としては扱いません。`iono, k=4` では、複数の初期値から実行した Hamerly 法の結果がこの値を約 0.03% 下回りました。`difference_from_best_percent` が負の場合は、所属とコストを再確認したうえで、より良い解として扱います。

### 中規模・大規模問題

[Clustering basic benchmark](https://cs.uef.fi/sipu/datasets/) には次の問題があります。

- S1-S4: 5000 点、15 クラスタ
- A1-A3: 3000〜7500 点、20〜50 クラスタ
- Birch: 100000 点、100 クラスタ
- Unbalance、DIM、G2 など

[Clustering Benchmarks](https://clustering-benchmarks.gagolewski.com/) は、複数の問題集をバージョン付きでまとめています。これらの正解ラベルは比較用の情報であり、二乗誤差の最適解ではありません。

ヘッダのない空白区切りの座標の表は `convert-table` で変換します。

```bash
python3 titan_cpplib/ahc/clustering/benchmark/prepare_public_data.py \
  convert-table s1.txt s1.tcb --name s1 --source "SIPU clustering basic benchmark" \
  --dimension 2 --clusters 15 --labels s1-label.pa --label-base 1
```

### 個数固定の問題

[Cardinality-constrained minimum sum-of-squares clustering](https://link.springer.com/article/10.1007/s10107-023-02021-8) には、個数を固定した 29 個の実データ問題の最適値があります。実装は [cc-sos-sdp](https://github.com/antoniosudoso/cc-sos-sdp) で公開されています。数値表を、`点数 次元数` の行の後に座標を並べた形式にすると、汎用の変換コマンドを使用できます。

```bash
python3 titan_cpplib/ahc/clustering/benchmark/prepare_public_data.py \
  convert input.txt output.tcb --name problem_name --source "cc-sos-sdp benchmark" \
  --clusters 3 --sizes 20,30,50 --best-known 123.456
```

論文の値を転記する場合は、座標の前処理、クラスタ数、個数の順序、コストの定義が同じかを確認します。同じ名前のデータでも、標準化や列の除去によって二乗誤差は変化します。

### 独立した実装との比較

[University of Eastern Finland のクラスタリングソフトウェア](https://cs.uef.fi/ml/software/) には Random Swap などの実装があります。`run_benchmark.cpp` は外部の実装を呼び出さないため、比較する場合は入出力を変換する小さなプログラムを別に用意します。
