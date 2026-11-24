# e-means

`emeans.cpp` は、数値座標のクラスタ内二乗誤差を小さくする実装です。既定では次の順に処理を行います。

1. K-means++ による初期化
2. EMA によるミニバッチ更新を 3 周
3. Hamerly 法による最後の更新

e-means は、`kmeans_hamerly` と同じ所属を高速に求める手法ではなく、別の局所解へ到達する手法です。外れ値を含む入力、重なりの大きい入力、クラスタ数が多い入力では、コストが下がりやすくなります。よく分離した入力では Hamerly 法が少ない反復で終了するため、Hamerly 法の方が高速です。どちらを使用するかは、同じ時間制限で得られるコストを比較して決定してください。

## 使用方法

```cpp
#include <array>
#include <vector>
#include "titan_cpplib/ahc/clustering/emeans.cpp"

std::vector<std::array<double, 2>> points = {
    {0, 0}, {1, 0}, {10, 0}, {11, 0}
};
auto result = titan23::emeans(points, 2, {.seed = 23});
// result.labels, centers, cluster_sizes, total_cost, iterations, converged

// シード seed, seed+1, ... で複数回実行し、コストが最小の結果を返す
auto best = titan23::emeans_best_of(points, 2, 8, {.seed = 23});

// 初期中心を指定する
std::vector<std::array<double, 2>> centers = {{0, 0}, {10, 0}};
auto continued = titan23::emeans_from_centers(points, centers, {.seed = 23});
```

点と初期中心には、`size()` と添字アクセスがあり、座標を `double` に変換できる型を使用します。`vector<double>`、`array<double, D>`、整数座標の `array<int, D>` などに対応しています。返される中心の型は、入力の型によらず `vector<double>` です。

戻り値の型は `KMeansResult<vector<double>, double>` です。`total_cost` は、返された所属と中心に対する二乗距離の総和です。返す時点ではすべてのクラスタが空でなく、各中心はそのクラスタに所属する点の平均に一致します。

`iterations` は次の回数の合計です。

- EMA の前に行う Hamerly 法の更新
- EMA のバッチ更新
- 最後の平均の計算 (1 回)
- EMA の後に行う Hamerly 法の更新

`iterations` は全点を走査した回数ではないため、`kmeans` の `iterations` と速度を比較する指標には使用できません。`converged` は、Hamerly 法で固定点を確認できた場合だけ `true` になります。EMA の後に行う Hamerly 法の更新を無効にした場合は `false` です。

## 設定

```cpp
struct EMeansOptions {
    int epochs = 3;
    int batch_size = 0;
    double alpha0 = 0;
    double revival_threshold = -1;
    bool kmeans_pp = true;
    int warmup_iterations = 0;
    int polish_iterations = 100;
    uint32_t seed = 0;
};
```

| 項目 | 意味 |
|---|---|
| `epochs` | 全点をシャッフルして走査する回数。1 以上を指定する |
| `batch_size` | 一度の中心の更新に使用する点数。0 の場合は自動的に設定する。点の総数を超える値を指定した場合は点の総数に切り詰める |
| `alpha0` | 最初の EMA の更新率。0 の場合は自動的に設定する。手動で指定する場合は `(0,1]` の範囲にする |
| `revival_threshold` | 小さいクラスタを再配置する閾値。点数の EMA の平均に対する比で指定する。-1 の場合は自動的に設定し、0 の場合は再配置を行わない。手動で指定する場合は `[0,1]` の範囲にする |
| `kmeans_pp` | `true` の場合は K-means++ で初期化し、`false` の場合は重複しない点番号を無作為に選んで初期化する。初期中心を指定する場合は参照しない |
| `warmup_iterations` | EMA の前に行う Hamerly 法の更新回数の上限。その間に収束した場合はその結果を返す。0 の場合は行わない |
| `polish_iterations` | EMA の後に行う Hamerly 法の更新回数の上限。0 の場合は行わない |
| `seed` | 初期化、シャッフル、再配置に使用するシード |

- 初期化の時間を減らす場合は `kmeans_pp=false` にします。ただし、離れた位置にある小さいクラスタの中に初期中心が 1 つも選ばれない場合は、特に低次元で結果の品質が悪化しやすくなります。
- `warmup_iterations=5` とすると、Hamerly 法が少ない反復で収束する入力では EMA を省略できます。すべての入力で高速になるとは限りません。
- `epochs=5, kmeans_pp=false, polish_iterations=0` は原論文に近い設定です。ただし、倍精度の使用、乱数生成器、空のクラスタの修復などが公式実装と異なるため、完全には再現されません。

## 仕組み

### EMA による更新

各クラスタについて、所属する点数と座標の和の指数移動平均 (EMA) を保持し、その比を中心とします。点をシャッフルしてバッチに分け、バッチごとに各点を最も近い中心へ割り当てます。バッチ内でクラスタに割り当てられた点数を `m`、それらの点の平均を `b`、更新前の点数の EMA を `N`、更新前の中心を `c` とすると、次のように更新します。

```text
N' = (1 - alpha) * N + alpha * m
c' = c + (alpha * m / N') * (b - c)   // m > 0 の場合
```

全点の所属が定まるのを待たずに中心を移動するため、初期の探索が速く進みます。更新率 `alpha` は、コサインスケジュールで `alpha0` から約 `alpha0/10` まで下げます。

### 自動設定

`batch_size` と `alpha0` は、公式実装の経験式で `n`、`k`、次元、周回数から決めます。16 次元以下ではバッチを約 1/8 にし、2 次元以下では小さいクラスタの再配置を無効にします。これらの値がデータに対して最適であるとは限りません。

### 小さいクラスタの再配置

5 バッチごとに、点数の EMA が閾値未満のクラスタを探します。見つかった場合は、点数の EMA に比例した確率で別のクラスタを選び、2 つに分割します。選んだクラスタの中心を互いに逆向きへ少しずらして 2 点を作り、一方を選んだクラスタの中心、もう一方を閾値未満のクラスタの中心にします。点数の EMA も半分ずつに分けます。この処理はクラスタの個数制約を満たすためのものではありません。

### EMA の後の処理

最後の周回で得た所属から、中心を平均として求め直します。空のクラスタがある場合は、`kmeans` と同じ方法で点を移動して修復します。その後、Hamerly 法で所属と中心を更新します。EMA による更新では各更新でコストが減るとは限らず、最終的な結果が `kmeans_hamerly` より良くなるとも限りません。反復回数が上限に達した場合も、返される所属、中心、コストは互いに整合します。

## 適用範囲

目的関数は二乗ユークリッド距離、中心は算術平均に限ります。任意の `cost` / `center_of`、K-medians、点の重み、クラスタの個数制約は扱いません。これらが必要な場合は [K-means](kmeans.md) の汎用 API を使用してください。

個数制約付きの問題では、得られた中心を `kmeans_balanced_from_centers` の初期中心として使用できます。`labels` は個数の条件を満たすとは限らないため、個数制約付きの SA へそのまま渡さないでください。個数に制約のない SA には、初期所属として渡すことができます。

## 計算量

点数を `n`、次元を `d`、クラスタ数を `k`、周回数を `T`、バッチサイズを `B`、更新回数を `U = T⌈n/B⌉` とします。

- EMA の部分: `O(Tnkd + Ukd)`。再配置を行う場合は最大で `O(k log k + kd)` が加わります。
- K-means++ による初期化: `O(nkd)`。無作為な初期化は `O(n + kd)` です。
- 最後の所属からの平均の計算: `O(nd + kd)`。空のクラスタが `e` 個ある場合は、修復に `O(ne)` が加わります。
- EMA の前後の Hamerly 法: [K-means の計算量](kmeans.md#計算量)が別に加わります。
- 入力以外のメモリ: `O(n + kd)`。点群のコピーや `n×k` の距離行列は作成しません。

## 入力条件

- `1 <= k <= n`、`n <= INT_MAX`
- 次元が正であり、すべての点と初期中心で等しい必要があります。
- 座標を有限の `double` に変換でき、座標の和、二乗距離、コストの総和などが `double` に収まる必要があります。
- 更新回数の上界が `INT_MAX` を超える設定では例外を投げます。

## 参考

- [e-means: outperforming k-means (in one-tenth the epochs)](https://arxiv.org/abs/2611.09501)
- [Faiss の公式実装](https://github.com/facebookresearch/faiss/blob/main/contrib/e_means/e_means.py)、[自動設定](https://github.com/facebookresearch/faiss/blob/main/contrib/e_means/base.py)
