# K-means

## ファイル

- `kmeans.cpp`: Lloyd 法、K-means++、複数初期値
- `kmeans_hamerly.cpp`: Hamerly 法による距離計算の省略
- `kmeans_balanced.cpp`: クラスタの個数制約を最小費用流で扱う版。`atcoder::mcf_graph` に依存

通常版は `kmeans.cpp` だけを読み込みます。Hamerly法と個数制約付き版は、それぞれのファイルが `kmeans.cpp` を読み込みます。

## 選び方

| 条件 | 関数 |
|---|---|
| 通常はこれ | `kmeans` |
| 初期中心を指定する | `kmeans_from_centers` |
| 複数の初期値を試す | `kmeans_best_of` |
| 距離計算が重く、三角不等式を使える | `kmeans_hamerly` |
| クラスタの個数に上下限がある | `kmeans_balanced` |
| 各クラスタの個数を固定する | `kmeans_balanced_exact` |

各方式には `_from_centers` と `_best_of` もあります。

## 公開関数

```cpp
kmeans(points, k, cost, center_of, options)
kmeans_from_centers(points, initial_centers, cost, center_of, max_iterations)
kmeans_best_of(points, k, trials, cost, center_of, options)

kmeans_hamerly(points, k, cost, metric, center_of, options)
kmeans_hamerly_from_centers(points, initial_centers, cost, metric, center_of, max_iterations)
kmeans_hamerly_best_of(points, k, trials, cost, metric, center_of, options)

kmeans_balanced(points, ranges, cost, flow_cost, center_of, options)
kmeans_balanced_from_centers(points, initial_centers, ranges, cost, flow_cost, center_of, max_iterations)
kmeans_balanced_best_of(points, ranges, trials, cost, flow_cost, center_of, options)

kmeans_balanced_exact(points, sizes, cost, flow_cost, center_of, options)
kmeans_balanced_exact_from_centers(points, initial_centers, sizes, cost, flow_cost, center_of, max_iterations)
kmeans_balanced_exact_best_of(points, sizes, trials, cost, flow_cost, center_of, options)
```

## 共通の設定と結果

```cpp
struct KMeansOptions {
    int max_iterations = 100;
    uint32_t seed = 0;
};
```

通常版とHamerly版は `KMeansResult`、個数制約付き版は `KMeansBalancedResult` を返します。

```cpp
result.labels;          // 各点のクラスタ番号
result.centers;         // 各クラスタの中心
result.cluster_sizes;   // 各クラスタの点数
result.total_cost;      // cost の合計
result.iterations;      // 中心を更新した回数
result.converged;       // 返却状態が固定点なら true
```

個数制約付き版には、返却状態を `flow_cost` で評価した `total_flow_cost` もあります。

個数制約付き版は、中心を固定したときの所属を最小費用流で決めます。この所属は、個数制約を満たすものの中で `flow_cost` の合計が最小です。その後で各クラスタの中心を更新し、同じ処理を繰り返します。したがって、固定した中心に対する所属は正確に求まりますが、中心の更新まで含めたクラスタリング全体の最適解を保証するものではありません。

反復上限に達した場合、中心は返却された所属点から計算済みですが、その中心に対して全点を再割当した状態とは限りません。この場合は `converged == false` です。

## 渡す関数

`cost(point, center)` は最小化する0以上の有限値を返します。戻り型には0からの構築、加算、大小比較、`long double` への変換が必要です。標準的なK-meansでは二乗ユークリッド距離です。K-means++もこの値をそのまま抽選重みに使い、再度二乗しません。

`center_of(points, members)` は非空の所属点から中心を返します。

```cpp
Center center_of(const vector<Point>& points, span<const int> members);
```

`Point` と `Center` は別の型でも構いません。`center_of` は同じ入力に同じ中心を返し、クラスタ内の `cost` 合計を増やさない必要があります。単点から作った中心では、その点の `cost` が0になることも前提です。

## 通常版の例

```cpp
#include <bits/stdc++.h>
#include "titan_cpplib/ahc/clustering/kmeans.cpp"
using namespace std;
using Point = pair<double, double>;

double cost(const Point& a, const Point& b) {
    double dx = a.first - b.first;
    double dy = a.second - b.second;
    return dx * dx + dy * dy;
}

Point center_of(const vector<Point>& points, span<const int> members) {
    Point center{};
    for (int i : members) {
        center.first += points[i].first;
        center.second += points[i].second;
    }
    center.first /= members.size();
    center.second /= members.size();
    return center;
}

vector<Point> points = {{0, 0}, {1, 0}, {10, 0}, {11, 0}};
auto result = titan23::kmeans(points, 2, cost, center_of, {.max_iterations=100, .seed=23});
```

初期中心を指定する場合は次の形です。

```cpp
vector<Point> initial_centers = {{0, 0}, {10, 0}};
auto result = titan23::kmeans_from_centers(points, initial_centers, cost, center_of, 100);
```

複数の初期値を試す場合、試行 `t` は `seed + t` を使います。同じ費用なら先の試行を残します。

```cpp
auto result = titan23::kmeans_best_of(points, 2, 8, cost, center_of, {.max_iterations=100, .seed=23});
```

## Hamerly法

```cpp
#include "titan_cpplib/ahc/clustering/kmeans_hamerly.cpp"

double metric(const Point& a, const Point& b) {
    return hypot(a.first - b.first, a.second - b.second);
}

auto result = titan23::kmeans_hamerly(points, 2, cost, metric, center_of, {.max_iterations=100, .seed=23});
```

`metric` は点と中心、中心同士の両方に対して呼べる必要があります。また、有限、非負、対称、三角不等式を満たし、中心の近さの大小と同値関係が `cost` と一致する必要があります。整数距離は内部の浮動小数型へ変換した後も比較結果を保てる範囲に収めます。標準例は `cost` が二乗ユークリッド距離、`metric` がユークリッド距離です。二乗距離を `metric` に渡してはいけません。

費用が同じ中心が複数ある場合、初回は番号が小さい中心を選び、以後は現在の所属先も最小なら所属を維持します。通常版とHamerly版で同じ規則です。

## 個数制約付き版

```cpp
#include "titan_cpplib/ahc/clustering/kmeans_balanced.cpp"

long long flow_cost(const Point& a, const Point& b) {
    return llround(cost(a, b) * 1000000);
}

vector<titan23::ClusterSizeRange> ranges = {{1, 3}, {1, 3}};
auto result = titan23::kmeans_balanced(points, ranges, cost, flow_cost, center_of, {.max_iterations=30, .seed=23});
```

各範囲は `0 <= lower <= upper` を満たし、下限合計以下かつ上限合計以上に点数が収まる必要があります。個数を固定する場合は次の形です。

```cpp
vector<int> sizes = {2, 2};
auto result = titan23::kmeans_balanced_exact(points, sizes, cost, flow_cost, center_of, {.max_iterations=30, .seed=23});
```

`flow_cost` は0以上で、`long long` 以下の幅の符号付き整数を返します。全点分の合計を `long long` に収め、最大値と `n+k+3` の積を AtCoder Library の上限 `8*10^18+1000` 以下にします。浮動小数からの丸め方は問題ごとに異なるため、ライブラリ内では変換しません。`cost` と `flow_cost` の順序が一致しない場合は費用の単調減少を保証できないため、途中で得た実行可能な状態のうち `total_cost` が最小のものを返します。

下限0のクラスタは空でも正当です。空クラスタの中心は前回値を維持し、最小費用流で決めた所属を別の処理で変更しません。

## 計算量

点数を `n`、クラスタ数を `k`、中心更新回数を `I`、複数初期値の試行数を `r` とします。Hamerly法について、反復 `i` で全中心との距離を調べ直す点数を `q_i`、空クラスタ数を `e_i` とします。`cost`、`metric`、1点の中心計算を定数時間とみなします。

| 処理 | 時間 | 追加メモリ |
|---|---:|---:|
| K-means++ | `O(nk)` | `O(n+k)` |
| Lloyd 法 | `O(Ink)` | `O(n+k)` |
| Hamerly法 | `O(nk + Σ(k²+n+q_i k+ne_i))` | `O(n+k)` |
| 個数制約付き版 | `O(nk + In(nk+n+k)log(n+k))` | `O(nk)` |
| 複数初期値 | 各方式の時間の `r` 倍 | 各方式と同じ |

Hamerly法では `0 <= q_i <= n`、`0 <= e_i < k` です。単純な最悪上界は `O(nk+I(k²+nk))` ですが、クラスタが分離すると `q_i` が小さくなり、多くの距離計算を省けます。

個数制約付き版は、頂点数 `O(n+k)`、辺数 `O(nk)`、流量 `n` の最小費用流を各反復で解く場合の上界です。実際の時間も通常版よりかなり大きくなります。

中心計算自体が重い場合は、その計算量を各反復分加えてください。

## 入力条件

- 自動初期化では `points` が非空で `1 <= k <= n`
- 初期中心指定の通常版とHamerly版でも `1 <= k <= n`
- 個数制約付きの初期中心指定版は、範囲が実現可能なら `k > n` も許容
- `max_iterations >= 1`
- 複数初期値では `trials >= 1`
- `n+k+3 <= INT_MAX`
- 費用、距離、中心計算の各関数は同じ入力に同じ結果を返し、点と中心を変更しない
- `cost` の各値と全点分の合計は、戻り型で有限、非負かつ桁あふれしない
