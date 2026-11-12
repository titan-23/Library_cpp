# e-means

`emeans.cpp` は、数値座標のクラスタ内二乗誤差を小さくする実装である。既定では **K-means++で初期化 → EMAによるミニバッチ更新を3周 → Hamerly法で仕上げ** を行う。

`kmeans_hamerly` と同じ所属を速く求めるものではなく、別の局所解へ到達する手法である。外れ値を含む入力、重なりの大きい入力、クラスタ数が多い入力では費用が下がりやすい。よく分離した入力ではHamerly法が少ない反復で終わるので、そちらが速い。どちらを使うかは同じ時間枠での費用で決める。

## 使い方

```cpp
#include <array>
#include <vector>
#include "titan_cpplib/ahc/clustering/emeans.cpp"

std::vector<std::array<double, 2>> points = {
    {0, 0}, {1, 0}, {10, 0}, {11, 0}
};
auto result = titan23::emeans(points, 2, {.seed = 23});
// result.labels, centers, cluster_sizes, total_cost, iterations, converged

// 種 seed, seed+1, ... で複数回実行し、費用が最小の結果を返す。
auto best = titan23::emeans_best_of(points, 2, 8, {.seed = 23});

// 初期中心を指定する。
std::vector<std::array<double, 2>> centers = {{0, 0}, {10, 0}};
auto continued = titan23::emeans_from_centers(points, centers, {.seed = 23});
```

点と初期中心には、`size()` と添字アクセスを持ち、座標を `double` に変換できる型を使う。`vector<double>`、`array<double, D>`、整数座標の `array<int, D>` などに対応する。返す中心の型は、入力の型によらず `vector<double>` である。

返却型は `KMeansResult<vector<double>, double>` である。`total_cost` は、返された所属と中心に対する二乗距離の総和である。全クラスタを空でなくし、中心をその所属点の平均にそろえて返す。

`iterations` は、前処理のHamerly更新、EMAのバッチ更新、最後の平均計算1回、仕上げのHamerly更新の合計である。全点を走査した回数ではないため、`kmeans` の `iterations` と速度比較する指標には使えない。`converged` は、Hamerly法で固定点を確認できた場合だけ `true` になる。仕上げを無効にした場合は `false` である。

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
| `epochs` | 全点をシャッフルして走査する回数。1以上 |
| `batch_size` | 一度の中心更新に使う点数。0で自動設定。点数を超える指定は全点に切り詰める |
| `alpha0` | 最初のEMA更新率。0で自動設定。手動指定は `(0,1]` |
| `revival_threshold` | 小クラスタ再配置の閾値（平均EMA個数に対する比）。-1で自動設定、0で無効、手動指定は `[0,1]` |
| `kmeans_pp` | `true` でK-means++初期化、`false` で重複しない点番号から無作為に初期化。中心指定版では使わない |
| `warmup_iterations` | EMAの前に行うHamerly更新の上限。その間に収束したらそのまま返す。0で無効 |
| `polish_iterations` | EMAの後に行うHamerly更新の上限。0で無効 |
| `seed` | 初期化、シャッフル、再配置に使う種 |

- 初期化時間を減らすなら `kmeans_pp=false` にする。ただし、離れた小クラスタを初期中心で拾い損ねると、特に低次元で品質が悪化しやすい。
- `warmup_iterations=5` とすると、Hamerly法ですぐ収束する入力ではEMAを省ける。すべての入力で速くなるわけではない。
- `epochs=5, kmeans_pp=false, polish_iterations=0` は原論文に近い設定である。ただし倍精度、乱数生成器、空クラスタの修復などが公式実装と異なり、完全な再現ではない。

## 仕組み

### EMA更新

各クラスタについて、所属点数と座標和の指数移動平均（EMA）を持ち、その比を中心とする。点をシャッフルしてバッチに分け、バッチごとに各点を最も近い中心へ割り当てる。バッチ内でクラスタに入った点数を `m`、その平均を `b`、更新前のEMA個数と中心を `N`、`c` とすると、次のように更新する。

```text
N' = (1 - alpha) * N + alpha * m
c' = c + (alpha * m / N') * (b - c)   // m > 0 の場合
```

全点の所属が決まるまで待たずに中心を動かすため、初期の探索が速く進む。更新率 `alpha` は余弦スケジュールで `alpha0` から約 `alpha0/10` まで下げる。

### 自動設定

`batch_size` と `alpha0` は、公式実装の経験式で `n`、`k`、次元、周回数から決める。16次元以下ではバッチを約1/8にし、2次元以下では小クラスタの再配置を無効にする。これらはデータに対する最適性を保証しない。

### 小クラスタの再配置

5バッチごとに、EMA個数が閾値未満のクラスタを探す。見つかった場合は、EMA個数に比例した確率で別のクラスタを選んで二つに分ける。選んだクラスタの中心を互いに逆向きへ少しずらした二点にし、片方を閾値未満のクラスタの中心にする。EMA個数も半分ずつ分ける。これはクラスタの個数制約を満たすための処理ではない。

### 仕上げ

最後の周回で得た所属から中心を平均で計算し直す。空のクラスタがあれば `kmeans` と同じ方法で埋める。その後Hamerly法で残りを詰める。EMA更新は各更新で費用が減る保証がなく、仕上げた結果が `kmeans_hamerly` より良くなる保証もない。反復上限に達した場合も、返す所属・中心・費用は互いに整合する。

## 適用範囲

目的関数は二乗ユークリッド距離、中心は算術平均に限る。任意の `cost` / `center_of`、K-medians、点の重み、クラスタの個数制約は扱わない。それらには [K-means](kmeans.md) の汎用APIを使う。

個数制約付き問題では、得た中心を `kmeans_balanced_from_centers` の初期中心に使える。`labels` は個数条件を満たすとは限らないので、個数制約付きSAへそのまま渡してはいけない。個数自由のSAには初期所属として渡せる。

## 計算量

点数を `n`、次元を `d`、クラスタ数を `k`、周回数を `T`、バッチサイズを `B`、更新回数を `U = T⌈n/B⌉` とする。

- EMA部分: `O(Tnkd + Ukd)`。再配置するときは最大 `O(k log k + kd)` が加わる
- K-means++初期化: `O(nkd)`。無作為初期化は `O(n + kd)`
- 最後の所属からの平均計算: `O(nd + kd)`。空クラスタが `e` 個ある場合の修復は追加で `O(ne)`
- 前後のHamerly法: [K-meansの計算量](kmeans.md#計算量)が別に加わる
- 入力以外のメモリ: `O(n + kd)`。点群のコピーや `n×k` の距離行列は作らない

## 入力条件

- `1 <= k <= n`、`n <= INT_MAX`
- 次元は正で、全点・全初期中心で一致する
- 座標は有限な `double` に変換でき、座標和、二乗距離、費用の総和などが `double` に収まる
- 更新回数の上界が `INT_MAX` を超える設定は例外を投げる

## 参考

- [e-means: outperforming k-means (in one-tenth the epochs)](https://arxiv.org/abs/2611.09501)
- [公式Faiss実装](https://github.com/facebookresearch/faiss/blob/main/contrib/e_means/e_means.py)、[自動設定](https://github.com/facebookresearch/faiss/blob/main/contrib/e_means/base.py)
