# クラスタリング

AHC 向けのクラスタリングの実装をまとめたディレクトリです。各 `.cpp` は提出コードから直接 `#include` して使用します。

## 選び方

| 目的 | 使用する関数 | 文書 |
|---|---|---|
| クラスタの点数に制約がなく、二乗誤差の総和を小さくする | `kmeans_hamerly_best_of`、`emeans_best_of` | [K-means](docs/kmeans.md)、[e-means](docs/emeans.md) |
| 任意のコストと中心 (K-medians、重み付きなど) | `kmeans` | [K-means](docs/kmeans.md) |
| クラスタごとの点数に上下限がある (小規模) | `kmeans_balanced` | [K-means](docs/kmeans.md) |
| 点数に上下限がある (短時間、大規模)、または K-means の解を改善する | `clustering_sa_from_labels` | [クラスタリング SA](docs/clustering_sa.md) |
| 統合木を作成し、後からクラスタ数や距離を指定して分割する | `hierarchical_clustering` | [階層型クラスタリング](docs/hierarchical_clustering.md) |
| 二次元整数座標の多数の点に単連結法を適用する | `euclidean_single_linkage_2d` | [階層型クラスタリング](docs/hierarchical_clustering.md) |

人工問題と公開問題での比較から、次の使い分けを基本とします。

- 点数に制約のない二乗誤差の最小化では、Hamerly 法を制限時間まで複数の初期値で実行する方法が第一候補です。同じ時間では SA はこれを上回りませんでした。
- e-means は、外れ値を含む入力、重なりの大きい入力、クラスタ数が多い入力でコストが下がる傾向があります。よく分離した入力では Hamerly 法のほうが短時間で終了します。どちらを使用するかは、同じ制限時間で得られるコストを比較して判断します。
- 点数に制約がある場合、最小費用流による実装は各反復で `n×k` 本の辺を張るため、2000 点、10 クラスタで数秒かかります。短時間で解く場合は、Hamerly 法で得た所属を、点数の条件を満たすように補正し、その所属から SA を実行します。
- e-means と Hamerly 法で得た所属は点数の条件を満たすとは限りません。点数の上下限を指定した SA へ渡す前に補正します。

## ファイルと依存関係

| ファイル | 内容 | include するファイル |
|---|---|---|
| `kmeans.cpp` | Lloyd 法、K-means++、複数の初期値からの実行 | `alg/random.cpp` |
| `kmeans_hamerly.cpp` | Hamerly 法で距離計算を省略する K-means | `kmeans.cpp` |
| `emeans.cpp` | EMA によるミニバッチ更新と Hamerly 法を組み合わせた二乗誤差専用の実装 | `kmeans_hamerly.cpp` |
| `cluster_size_range.cpp` | 点数の範囲 `ClusterSizeRange` と、範囲の作成と確認を行う関数 | なし |
| `kmeans_balanced.cpp` | 点数の上下限を満たす K-means (最小費用流) | `kmeans.cpp`、`cluster_size_range.cpp`、`atcoder/mincostflow` |
| `clustering_partition.cpp` | 所属の管理。1 点の移動、2 点の交換、3 点の循環を `O(1)` で行う | なし |
| `clustering_sa.cpp` | 二乗誤差の焼きなまし | `cluster_size_range.cpp`、`clustering_partition.cpp`、`ahc/sa/sa.cpp` |
| `hierarchical_clustering.cpp` | 単連結、完全連結、群平均、Ward 法の階層型クラスタリング | なし |
| `euclidean_single_linkage.cpp` | 二次元整数座標の単連結法 | `hierarchical_clustering.cpp`、`geometry/delaunay_triangulation.cpp` (Boost.Polygon) |

- `docs/`: 各実装の使用方法と仕組み
- `test/`: 正しさの検証
- `benchmark/`: 手法の性能比較。使用方法は [benchmark/README.md](benchmark/README.md)

## 共通の仕様

- 所属 `labels[i]` は `0` 以上クラスタ数未満の整数です。
- コストは最小化します。K-means 系の `total_cost` は `cost(point, center)` の総和、e-means と SA では中心までの二乗距離の総和です。
- `kmeans`、`kmeans_hamerly`、`emeans`、SA は空のクラスタを返しません。`kmeans_balanced` では下限が 0 のクラスタだけが空になる可能性があります。
- 点数の範囲 `ClusterSizeRange{lower, upper}` は両端を含みます。`ClusteringSizeRange` は同じ型の別名です。
- 同じ入力と同じ `seed` からは同じ結果を返します。ただし SA は時間で打ち切るため、実行ごとに結果が異なる場合があります。

## テスト

リポジトリのルートで実行します。

```bash
for t in kmeans emeans clustering_sa hierarchical_clustering; do
  g++ -std=c++20 -O2 -I. titan_cpplib/ahc/clustering/test/${t}_test.cpp -o /tmp/${t}_test && /tmp/${t}_test
done
# AtCoder Library が必要
g++ -std=c++20 -O2 -I. -I/path/to/ac-library titan_cpplib/ahc/clustering/test/kmeans_balanced_test.cpp -o /tmp/kmeans_balanced_test && /tmp/kmeans_balanced_test
# Boost が必要
g++ -std=c++20 -O2 -I. titan_cpplib/ahc/clustering/test/euclidean_single_linkage_test.cpp -o /tmp/euclidean_single_linkage_test && /tmp/euclidean_single_linkage_test
```

各テストは失敗すると `assert` で停止します。
