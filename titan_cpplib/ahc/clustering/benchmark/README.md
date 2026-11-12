# クラスタリングのベンチマーク

K-means、Hamerly法、e-means、個数制約付きK-means、クラスタリングSAを同じ問題で比較する。正しさの検証は `test/` で行い、ここでは複数の種と実行時間で性能を測る。

## ファイル

- `clustering_benchmark.cpp`: 問題ファイルの読み書き、費用と調整ランド指数の再計算
- `generate_cases.cpp`: 固定した人工問題の生成
- `prepare_public_data.py`: 公開問題と一般的な数値表の変換
- `run_benchmark.cpp`: 各手法の実行とCSV出力
- `summarize.py`: 問題・手法ごとの集計

## 構築と実行

リポジトリのルートで実行する。個数制約付きK-meansを含むため、AtCoder Libraryを読み込める必要がある。

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

`benchmark/data/` は `.gitignore` で除外している。公開データは配布元ごとに利用条件が異なるため、変換後もリポジトリに含めない。

途中で中断しても、それまでの行はCSVへ書き出されている。同じ出力ファイルを指定すると上書きする。

## 人工問題

`generate_cases.cpp` は、固定した種で次の問題を作る。

- 分離が明確、重なりが大きい、細長い分布、大きさが不均衡、外れ値を含む、同じ座標を含む
- 低次元から64次元まで
- 個数自由、個数固定、上下限付き
- 50から240クラスタの個数固定問題
- 一様分布で、少数の点や多数のクラスタを持つ個数自由問題

人工問題の正解ラベルは生成に使った所属であり、二乗誤差を最小にする所属とは限らない。費用と調整ランド指数は別々に見る。

## 手法

| 名前 | 内容 | 終了条件 |
|---|---|---|
| `kmeans` | Lloyd法、K-means++で初期化 | 収束または反復上限 |
| `hamerly` | Hamerly法、K-means++で初期化 | 収束または反復上限 |
| `emeans` | e-meansの既定設定 | 収束または反復上限 |
| `hamerly_repeated` | Hamerly法を種を変えて繰り返し、最良を残す | 総時間 |
| `emeans_repeated` | e-meansを種を変えて繰り返し、最良を残す | 総時間 |
| `hamerly_repaired` | Hamerly法の所属を個数条件へ補正した簡易初期解 | 一回 |
| `balanced` | 最小費用流で個数条件を守るK-means | 収束または反復上限 |
| `balanced_repeated` | `balanced` を種を変えて繰り返す | 総時間 |
| `sa` | `hamerly_repaired` の所属からSA | 初期解を含む総時間 |
| `sa_balanced` | `balanced` の所属からSA | 初期解を含む総時間 |
| `sa_no_relocate` など | 一点移動、二点交換、三点循環、再構築のどれかを外したSA | 初期解を含む総時間 |
| `sa_no_rebuild_three` | 三クラスタ再構築だけを外したSA | 初期解を含む総時間 |

個数条件を守らない `kmeans`、`hamerly`、`emeans`、`*_repeated` は個数自由の問題だけで実行する。`hamerly_repaired`、`balanced` 系、`sa_balanced` は個数条件のある問題だけで実行する。省いた組は `status=skipped` として出力する。

`hamerly_repaired` は、各クラスタで中心に近い点から目標の点数だけ残し、あふれた点を不足するクラスタへ近い順に入れる。安く有効な初期解を作るための処理で、割当の最適性は保証しない。

`balanced` の最小費用流は整数の辺費用を使うため、二乗距離に倍率を掛けて丸める。倍率は原則100万とし、AtCoder Libraryの上限を超えない範囲まで自動で下げる。実際の倍率は `flow_cost_scale` 列に出す。倍率が小さい問題では多くの辺費用が同じ整数に丸められる。

手法を絞る場合はカンマ区切りで指定する。

```bash
./clustering_benchmark --output raw.csv --methods hamerly_repeated,emeans_repeated,sa case1.tcb case2.tcb
```

## 引数

| 引数 | 既定値 | 意味 |
|---|---:|---|
| `--output` | 必須 | 出力CSV |
| `--tag` | 空文字 | 構築設定や実験名を区別する任意の文字列 |
| `--runs` | 30 | 問題・手法ごとの試行数 |
| `--seed` | 23 | 基準の種。試行 `r` は `seed + r * 1000003` を使う |
| `--time-ms` | 1000 | 総時間で比べる手法の一試行あたりの時間 |
| `--max-iterations` | 100 | K-means系の一回あたりの反復上限 |
| `--balanced-edge-limit` | 50000 | 最小費用流で使う、点とクラスタを結ぶ辺の本数の上限 |
| `--methods` | 全手法 | 実行する手法 |

手法の実行順は試行ごとに一つずつずらし、同じ手法が常に最初や最後にならないようにする。

## 時間の扱い

- 問題ファイルの読み込みは計測に含めない。
- SAの総時間には、SA用の問題データと初期解の作成を含める。初期解だけで時間を使い切った場合はその初期解を返す。
- K-meansの一回、最小費用流の一回、SA状態の構築は途中で止めないため、その分だけ上限を超えることがある。
- `kmeans`、`hamerly`、`emeans`、`balanced` は完了まで測り、一回の速さと基礎的な解の質を見る。

総時間を10、30、100、300、1000ミリ秒のように変えて別々に実行すると、短時間から長時間までの伸びを確認できる。比較では同じ計算機、構築設定、実行時間、問題順を使う。

## 出力CSV

主な列は次のとおりである。

- `status`: `ok`、`skipped`、`error`。所属が個数条件を破る場合と、費用が有限な非負値でない場合も `error`
- `elapsed_ms`: 実測時間。SAでは `initialization_ms` と `search_ms` に分けても出す
- `reported_cost`, `recalculated_cost`: ライブラリが返した費用と、所属から長倍精度で再計算した費用
- `difference_from_best_percent`: 既知の最良費用からの差の割合
- `adjusted_rand_index`: 正解ラベルとの調整ランド指数
- `valid`: 所属が個数条件を満たすか
- `trials`: 時間内に完了した試行の数
- `initial_cost`, `improvement_from_initial_percent`: SAの初期費用と、そこからの改善率
- `iterations`, `converged`: K-meansの反復回数と収束状態。SAの行では初期解を作ったK-meansの値
- `label_hash`: 同じ所属を再現したかを確かめる値
- `attempts_*`, `valid_*`, `accepted_*`, `improvements_*`, `rebuild_three_*`: SAの近傍ごとの統計

`reported_cost` と `recalculated_cost` が大きく違う場合は、費用の差分更新か数値範囲を疑う。`valid` が0の行は比較に使わない。

`summarize.py` は、費用と実行時間の最良値、中央値、悪い側の90パーセンタイル、調整ランド指数、近傍ごとの試行速度・有効率・採択率・改善量を問題と手法ごとに出す。最良の一回だけでなく、中央値と悪い側も見る。

## 問題ファイル

拡張子は `.tcb` とする。点群、クラスタ数、個数条件、既知の最良費用、正解ラベルを一つのファイルに入れる。

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

`ranges` にはクラスタごとの下限と上限を順に書く。個数自由なら全クラスタを `[1,n]`、個数固定なら下限と上限を同じ値にする。SAは空クラスタを扱わないため、下限は1以上にする。`best_known_cost` と `reference_labels` が不明なら `none` とする。正解ラベルは二乗誤差の最適解と一致するとは限らず、種類数がクラスタ数と異なってもよい。

## 公開問題

### SOS-SDPの結果がある小規模問題

[Ground truth clustering is not the optimum clustering](https://github.com/shudianzhao/GroundTruth_VS_OptimalCluster) には、二乗誤差のクラスタリングをSOS-SDPで解いた13種類の実データがある。次のコマンドは固定したコミットを取得し、`データ名 × クラスタ数` ごとに `.tcb` を作る。公開された所属から二乗誤差を再計算して `best_known_cost` に入れる。

```bash
python3 titan_cpplib/ahc/clustering/benchmark/prepare_public_data.py \
  download-sos-sdp-reference titan_cpplib/ahc/clustering/benchmark/data
```

取得URL、コミット、ZIPのSHA-256は `sos_sdp_reference_source.txt` に保存される。展開済みの配布物があれば通信せずに変換できる。

```bash
python3 titan_cpplib/ahc/clustering/benchmark/prepare_public_data.py \
  sos-sdp-reference /path/to/GroundTruth_VS_OptimalCluster titan_cpplib/ahc/clustering/benchmark/data
```

配布元はGPLv3である。`best_known_cost` は公開された所属の費用であり、最適性の証明済みの値としては扱わない。`iono, k=4` ではHamerly法の複数初期値がこれを約0.03%下回った。`difference_from_best_percent` が負なら、所属と費用を再確認したうえでより良い解として扱う。

### 中規模・大規模問題

[Clustering basic benchmark](https://cs.uef.fi/sipu/datasets/) には、S1-S4（5000点、15クラスタ）、A1-A3（3000〜7500点、20〜50クラスタ）、Birch（100000点、100クラスタ）、Unbalance、DIM、G2などがある。[Clustering Benchmarks](https://clustering-benchmarks.gagolewski.com/) は複数の問題集を版付きでまとめている。正解ラベルは比較用の情報で、二乗誤差の最適解ではない。

ヘッダのない空白区切りの座標表は `convert-table` で変換する。

```bash
python3 titan_cpplib/ahc/clustering/benchmark/prepare_public_data.py \
  convert-table s1.txt s1.tcb --name s1 --source "SIPU clustering basic benchmark" \
  --dimension 2 --clusters 15 --labels s1-label.pa --label-base 1
```

### 個数固定の問題

[Cardinality-constrained minimum sum-of-squares clustering](https://link.springer.com/article/10.1007/s10107-023-02021-8) には、個数を固定した29個の実データ問題の最適値がある（実装は [cc-sos-sdp](https://github.com/antoniosudoso/cc-sos-sdp)）。数値表を `点数 次元数` の行に続けて座標を並べた形にすれば、一般の変換を使える。

```bash
python3 titan_cpplib/ahc/clustering/benchmark/prepare_public_data.py \
  convert input.txt output.tcb --name problem_name --source "cc-sos-sdp benchmark" \
  --clusters 3 --sizes 20,30,50 --best-known 123.456
```

論文の値を転記するときは、座標の前処理、クラスタ数、個数の順序、費用の定義が同じかを確認する。同じ名前のデータでも、標準化や列の除去で二乗誤差は変わる。

### 独立した実装との比較

[University of Eastern Finlandのクラスタリングソフトウェア](https://cs.uef.fi/ml/software/) にはRandom Swapなどの実装がある。`run_benchmark.cpp` は外部実装を呼ばないため、比較するときは入出力を変換する小さなプログラムを別に用意する。
