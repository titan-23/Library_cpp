# K-means

## ファイル

- `kmeans.cpp`: Lloyd法、K-means++、複数初期値
- `kmeans_hamerly.cpp`: Hamerly法による距離計算の省略。`kmeans.cpp` を読み込む
- `kmeans_balanced.cpp`: クラスタの個数制約を最小費用流で扱う版。`kmeans.cpp`、`cluster_size_range.cpp`、`atcoder/mincostflow` を読み込む

三つとも、点と中心の型、費用、中心の計算方法を利用側が渡す汎用実装である。数値座標の二乗誤差だけを扱うなら [e-means](emeans.md) も候補になる。

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

- 接尾辞なし: K-means++で初期中心を選んでから反復する
- `_from_centers`: 渡した初期中心から反復する
- `_best_of`: 種 `seed, seed+1, ..., seed+trials-1` で接尾辞なし版を実行し、`total_cost` が最小の結果を返す。同じ費用なら先の試行を残す

## 設定と結果

```cpp
struct KMeansOptions {
    int max_iterations = 100;
    uint32_t seed = 0;
};
```

通常版とHamerly版は `KMeansResult`、個数制約付き版は `KMeansBalancedResult` を返す。

```cpp
result.labels;          // 各点のクラスタ番号
result.centers;         // 各クラスタの中心
result.cluster_sizes;   // 各クラスタの点数
result.total_cost;      // cost の合計
result.iterations;      // 中心を更新した回数
result.converged;       // 返却状態が固定点なら true
result.total_flow_cost; // 個数制約付き版のみ。flow_cost の合計
```

反復上限に達した場合、中心は返却された所属点から計算済みだが、その中心に対して全点を割り当て直した状態とは限らない。このとき `converged == false` である。

## 渡す関数

`cost(point, center)` は最小化する0以上の有限値を返す。戻り型には0からの構築、加算、大小比較、`long double` への変換が必要である。標準的なK-meansでは二乗ユークリッド距離を使う。K-means++もこの値をそのまま抽選の重みに使い、再度二乗しない。

`center_of(points, members)` は空でない所属点から中心を返す。

```cpp
Center center_of(const vector<Point>& points, span<const int> members);
```

`Point` と `Center` は別の型でもよい。`center_of` は同じ入力に同じ中心を返し、クラスタ内の `cost` の合計を増やさない必要がある。一点だけから作った中心では、その点の `cost` が0になることも前提とする。

重み付きK-meansは `cost` に点の重みを掛けて `center_of` で重み付き平均を返せばよく、K-mediansは距離の絶対値和を `cost`、各座標の中央値を `center_of` にすればよい。

## 通常版

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
auto result = titan23::kmeans(points, 2, cost, center_of, {.max_iterations = 100, .seed = 23});

vector<Point> initial_centers = {{0, 0}, {10, 0}};
auto from_centers = titan23::kmeans_from_centers(points, initial_centers, cost, center_of, 100);

auto best = titan23::kmeans_best_of(points, 2, 8, cost, center_of, {.max_iterations = 100, .seed = 23});
```

各反復では、全点を `cost` が最小の中心へ割り当て、各クラスタの中心を `center_of` で計算し直す。所属が変わらなくなったら止まる。費用が同じ中心が複数ある場合、初回は番号が小さい中心を選び、以後は現在の所属先も最小なら所属を維持する。

割当後に空のクラスタができた場合は、二点以上を持つクラスタから現在の費用が最大の点を移して埋める。そのため返却するクラスタは常に空でない。

## Hamerly法

```cpp
#include "titan_cpplib/ahc/clustering/kmeans_hamerly.cpp"

double metric(const Point& a, const Point& b) {
    return hypot(a.first - b.first, a.second - b.second);
}

auto result = titan23::kmeans_hamerly(points, 2, cost, metric, center_of, {.max_iterations = 100, .seed = 23});
```

各点について、所属中心までの距離の上界と、二番目に近い中心までの距離の下界を持つ。中心の移動量で界を更新し、上界が下界と「最も近い別中心までの距離の半分」の大きい方以下なら、その点の距離計算を省く。通常版と同じ所属・中心・反復回数を返し、解の質は変わらない。

`metric` は点と中心、中心同士の両方に対して呼べる必要がある。また、有限、非負、対称で三角不等式を満たし、中心の近さの大小と同値関係が `cost` と一致しなければならない。標準例は `cost` が二乗ユークリッド距離、`metric` がユークリッド距離である。二乗距離を `metric` に渡してはいけない。整数の距離は内部の浮動小数型へ変換した後も比較結果を保てる範囲に収める。

## 個数制約付き版

```cpp
#include "titan_cpplib/ahc/clustering/kmeans_balanced.cpp"

long long flow_cost(const Point& a, const Point& b) {
    return llround(cost(a, b) * 1000000);
}

vector<titan23::ClusterSizeRange> ranges = {{1, 3}, {1, 3}};
auto result = titan23::kmeans_balanced(points, ranges, cost, flow_cost, center_of, {.max_iterations = 30, .seed = 23});

vector<int> sizes = {2, 2};
auto exact = titan23::kmeans_balanced_exact(points, sizes, cost, flow_cost, center_of, {.max_iterations = 30, .seed = 23});
```

中心を固定し、個数条件を満たす所属のうち `flow_cost` の合計が最小のものを最小費用流で求める。その後に中心を更新し、これを繰り返す。固定した中心に対する所属は正確に求まるが、中心の更新まで含めた全体の最適解は保証しない。`flow_cost` が `cost` を丸めた値なら、正確なのは丸めた費用に対してである。

`cost` と `flow_cost` の大小が一致しない場合は費用が単調に減るとは限らないため、途中で得た状態のうち `total_cost` が最小のものを返す。

各範囲は `0 <= lower <= upper` を満たし、下限の合計以上かつ上限の合計以下に点数が収まる必要がある。下限0のクラスタは空でもよい。空クラスタの中心は前回の値を維持し、最小費用流で決めた所属は変更しない。

`flow_cost` は0以上で、`long long` 以下の幅の符号付き整数を返す。全点分の合計を `long long` に収め、最大値と `n+k+3` の積をAtCoder Libraryの上限 `8*10^18+1000` 以下にする。浮動小数からの丸め方は問題ごとに異なるため、ライブラリ内では変換しない。

## 計算量

点数を `n`、クラスタ数を `k`、中心更新回数を `I`、複数初期値の試行数を `r` とする。Hamerly法について、反復 `i` で全中心との距離を調べ直す点数を `q_i`、空クラスタ数を `e_i` とする。`cost`、`metric`、一点の中心計算は定数時間とみなす。

| 処理 | 時間 | 追加メモリ |
|---|---:|---:|
| K-means++ | `O(nk)` | `O(n+k)` |
| Lloyd法 | `O(Ink)` | `O(n+k)` |
| Hamerly法 | `O(nk + Σ(k²+n+q_i k+ne_i))` | `O(n+k)` |
| 個数制約付き版 | `O(nk + In(nk+n+k)log(n+k))` | `O(nk)` |
| 複数初期値 | 各方式の時間の `r` 倍 | 各方式と同じ |

Hamerly法では `0 <= q_i <= n`、`0 <= e_i < k` である。単純な最悪上界は `O(nk+I(k²+nk))` だが、クラスタが分離していると `q_i` が小さくなり、多くの距離計算を省ける。

個数制約付き版の上界は、頂点数 `O(n+k)`、辺数 `O(nk)`、流量 `n` の最小費用流を各反復で解く場合のものである。実際の時間も通常版よりかなり大きい。

中心の計算自体が重い場合は、その計算量を各反復分加える。

## 入力条件

- 自動初期化では `points` が空でなく `1 <= k <= n`
- 初期中心を指定する通常版とHamerly版でも `1 <= k <= n`
- 初期中心を指定する個数制約付き版は、範囲が実現可能なら `k > n` も許す
- `max_iterations >= 1`、複数初期値では `trials >= 1`
- 個数制約付き版では `n+k+3 <= INT_MAX`
- 費用、距離、中心計算の各関数は同じ入力に同じ結果を返し、点と中心を変更しない
- `cost` の各値と全点分の合計は、戻り型で有限、非負かつ桁あふれしない
