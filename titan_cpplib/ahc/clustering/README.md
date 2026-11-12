# クラスタリング

AHC向けのクラスタリング実装をまとめたディレクトリである。各 `.cpp` は提出コードから直接 `#include` して使う。

## 選び方

| やりたいこと | 使うもの | 文書 |
|---|---|---|
| 個数自由で二乗誤差の総和を小さくする | `kmeans_hamerly_best_of`、`emeans_best_of` | [K-means](docs/kmeans.md)、[e-means](docs/emeans.md) |
| 任意の費用と中心（K-medians、重み付きなど） | `kmeans` | [K-means](docs/kmeans.md) |
| クラスタごとの点数に上下限がある（小規模） | `kmeans_balanced` | [K-means](docs/kmeans.md) |
| 点数に上下限がある（短時間・大規模）、K-meansの解を改善する | `clustering_sa_from_labels` | [クラスタリングSA](docs/clustering_sa.md) |
| 統合木を作り、後からクラスタ数や距離で切る | `hierarchical_clustering` | [階層型クラスタリング](docs/hierarchical_clustering.md) |
| 二次元整数座標の単連結法を大きな点数で行う | `euclidean_single_linkage_2d` | [階層型クラスタリング](docs/hierarchical_clustering.md) |

人工問題と公開問題での比較から、次の使い分けを基本とする。

- 個数自由の二乗誤差では、Hamerly法を時間いっぱい複数初期値で回すのが第一候補である。同じ時間ではSAはこれを上回らなかった。
- e-meansは、外れ値を含む入力、重なりの大きい入力、クラスタ数が多い入力で費用が下がりやすい。よく分離した入力ではHamerly法の方が短時間で終わる。どちらを使うかは同じ時間枠の費用で決める。
- 個数制約付きでは、最小費用流版は各反復で `n×k` 本の辺を張るため重い（2000点・10クラスタで数秒）。短時間なら、Hamerly法の所属を個数条件へ補正し、そこからSAを回す。
- e-meansとHamerly法の所属は個数条件を満たさない。個数制約付きSAへ渡す前に補正する。

## ファイルと依存関係

| ファイル | 内容 | 読み込むもの |
|---|---|---|
| `kmeans.cpp` | Lloyd法、K-means++、複数初期値 | `alg/random.cpp` |
| `kmeans_hamerly.cpp` | Hamerly法で距離計算を省くK-means | `kmeans.cpp` |
| `emeans.cpp` | EMAミニバッチ更新とHamerly法を組み合わせた二乗誤差専用版 | `kmeans_hamerly.cpp` |
| `cluster_size_range.cpp` | 個数範囲 `ClusterSizeRange` と作成・検査関数 | なし |
| `kmeans_balanced.cpp` | 個数制約付きK-means（最小費用流） | `kmeans.cpp`、`cluster_size_range.cpp`、`atcoder/mincostflow` |
| `clustering_partition.cpp` | 所属の管理。一点移動、二点交換、三点循環を `O(1)` で行う | なし |
| `clustering_sa.cpp` | 二乗誤差の焼きなまし | `cluster_size_range.cpp`、`clustering_partition.cpp`、`ahc/sa/sa.cpp` |
| `hierarchical_clustering.cpp` | 単連結、完全連結、群平均、Ward法の階層型クラスタリング | なし |
| `euclidean_single_linkage.cpp` | 二次元整数座標の単連結法 | `hierarchical_clustering.cpp`、`geometry/delaunay_triangulation.cpp`（Boost.Polygon） |

- `docs/`: 各実装の使い方と仕組み
- `test/`: 正しさの検証
- `benchmark/`: 手法の性能比較。使い方は [benchmark/README.md](benchmark/README.md)

## 共通の約束

- 所属 `labels[i]` は `0` 以上クラスタ数未満の整数である。
- 費用は最小化する。K-means系の `total_cost` は `cost(point, center)` の総和、e-meansとSAでは中心までの二乗距離の総和である。
- `kmeans`、`kmeans_hamerly`、`emeans`、SAは空のクラスタを返さない。`kmeans_balanced` では下限0のクラスタだけが空になりうる。
- 個数範囲 `ClusterSizeRange{lower, upper}` は両端を含む。`ClusteringSizeRange` は同じ型の別名である。
- 同じ入力と同じ `seed` からは同じ結果を返す。ただしSAは時間で打ち切るため、実行ごとに結果が変わる。

## テスト

リポジトリのルートで実行する。

```bash
for t in kmeans emeans clustering_sa hierarchical_clustering; do
  g++ -std=c++20 -O2 -I. titan_cpplib/ahc/clustering/test/${t}_test.cpp -o /tmp/${t}_test && /tmp/${t}_test
done
# AtCoder Library が必要
g++ -std=c++20 -O2 -I. -I/path/to/ac-library titan_cpplib/ahc/clustering/test/kmeans_balanced_test.cpp -o /tmp/kmeans_balanced_test && /tmp/kmeans_balanced_test
# Boost が必要
g++ -std=c++20 -O2 -I. titan_cpplib/ahc/clustering/test/euclidean_single_linkage_test.cpp -o /tmp/euclidean_single_linkage_test && /tmp/euclidean_single_linkage_test
```

各テストは失敗すると `assert` で停止する。
