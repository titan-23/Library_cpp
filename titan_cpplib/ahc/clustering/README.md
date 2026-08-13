# クラスタリング

AHC向けのクラスタリング実装をまとめたディレクトリです。各ファイルは必要なものを直接 `#include` して使います。

## 選び方

| 用途 | 実装 | 詳細 |
|---|---|---|
| 通常のK-means | `kmeans.cpp` | [K-means](docs/kmeans.md) |
| 距離計算を減らしたK-means | `kmeans_hamerly.cpp` | [K-means](docs/kmeans.md) |
| クラスタごとに個数制約がある | `kmeans_balanced.cpp` | [K-means](docs/kmeans.md) |
| K-meansの解をSAで改善する | `clustering_sa.cpp` | [クラスタリングSA](docs/clustering_sa.md) |
| 階層を作って後から分割数を決める | `hierarchical_clustering.cpp` | [階層型クラスタリング](docs/hierarchical_clustering.md) |
| 二次元整数座標で単連結法だけ使う | `euclidean_single_linkage.cpp` | [階層型クラスタリング](docs/hierarchical_clustering.md) |

## 構成

- `docs/`: 利用法、設計、ベンチマーク結果
- `benchmark/`: 性能比較用のコードとデータ置き場
- `test/`: 正しさの検証と可視化

設計の背景は [全体設計](docs/design.md) と [SAの近傍設計](docs/sa_neighborhood_design.md)、性能評価は [ベンチマーク](docs/benchmark.md) を参照してください。
