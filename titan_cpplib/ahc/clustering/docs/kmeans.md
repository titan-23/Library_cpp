# K-means

## ファイル

- `kmeans.cpp`: Lloyd 法、K-means++、複数の初期値による試行を実装しています。
- `kmeans_hamerly.cpp`: Hamerly 法で距離計算を省略する版です。`kmeans.cpp` を読み込みます。
- `kmeans_balanced.cpp`: 各クラスタの点の個数に対する制約を最小費用流で扱う版です。`kmeans.cpp`、`cluster_size_range.cpp`、`atcoder/mincostflow` を読み込みます。

3 つとも、点と中心の型、コスト関数、中心の計算方法を利用側が渡す汎用的な実装です。数値座標の二乗誤差だけを扱う場合は [e-means](emeans.md) も候補になります。

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

- 接尾辞なし: K-means++ で初期中心を選択してから反復します。
- `_from_centers`: 渡した初期中心から反復します。
- `_best_of`: シード `seed, seed+1, ..., seed+trials-1` で接尾辞なしの版を実行し、`total_cost` が最小の結果を返します。コストが同じ場合は先に実行した試行の結果を返します。

## 設定と結果

```cpp
struct KMeansOptions {
    int max_iterations = 100;
    uint32_t seed = 0;
};
```

通常版と Hamerly 版は `KMeansResult` を返し、個数制約付き版は `KMeansBalancedResult` を返します。

```cpp
result.labels;          // 各点のクラスタ番号
result.centers;         // 各クラスタの中心
result.cluster_sizes;   // 各クラスタの点の個数
result.total_cost;      // cost の合計
result.iterations;      // 中心を更新した回数
result.converged;       // 返す状態が不動点なら true
result.total_flow_cost; // flow_cost の合計 / 個数制約付き版のみ
```

反復回数の上限に達した場合、返す中心は返す所属から計算した値ですが、所属はその中心に対して全点を割り当て直した結果とは限りません。このとき `converged == false` となります。

## 渡す関数

`cost(point, center)` は最小化の対象となる 0 以上の有限値を返します。戻り値の型は 0 からの構築、加算、大小比較、`long double` への変換に対応している必要があります。標準的な K-means では二乗ユークリッド距離を使用します。K-means++ で初期中心をランダムに選択するときも、この値をそのまま重みとして使用し、再度二乗しません。

`center_of(points, members)` は空でないクラスタに属する点から、そのクラスタの中心を返します。

```cpp
Center center_of(const vector<Point>& points, span<const int> members);
```

`Point` と `Center` は別の型でも構いません。`center_of` は同じ入力に対して同じ中心を返し、クラスタ内の `cost` の合計を増やさない必要があります。1 点だけから計算した中心では、その点の `cost` が 0 になることも前提とします。

重み付き K-means は、`cost` に点の重みを掛け、`center_of` で重み付き平均を返すことで実現できます。K-medians は、距離の絶対値和を `cost` とし、`center_of` で各座標の中央値を返すことで実現できます。

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

各反復では、全点を `cost` が最小の中心に割り当て、各クラスタの中心を `center_of` で計算し直します。所属が変化しなくなった時点で終了します。コストが同じ中心が複数ある場合、初回は番号が小さい中心を選択し、以降は現在の所属先もコストが最小であれば所属を維持します。

割り当ての後に空のクラスタが生じた場合は、2 点以上を含むクラスタの点のうち現在のコストが最大の点を移動して、空のクラスタを埋めます。そのため、返すクラスタは常に空ではありません。

## Hamerly 法

```cpp
#include "titan_cpplib/ahc/clustering/kmeans_hamerly.cpp"

double metric(const Point& a, const Point& b) {
    return hypot(a.first - b.first, a.second - b.second);
}

auto result = titan23::kmeans_hamerly(points, 2, cost, metric, center_of, {.max_iterations = 100, .seed = 23});
```

各点について、所属する中心までの距離の上界と、2 番目に近い中心までの距離の下界を保持します。中心の移動量で上界と下界を更新します。上界が、下界と「所属する中心から最も近い別の中心までの距離の半分」の大きい方以下であれば、その点の距離計算を省略します。通常版と同じ所属、中心、反復回数を返すため、解の質は通常版と同じです。

`metric` は点と中心の組、中心同士の組の両方を引数として受け取る必要があります。また、値が有限かつ非負で、対称性と三角不等式を満たす必要があります。中心までの近さを `metric` で比較した結果は、大小と等しさのどちらについても `cost` で比較した結果と一致しなければなりません。標準的な例は、`cost` が二乗ユークリッド距離、`metric` がユークリッド距離の組み合わせです。二乗距離を `metric` に渡してはいけません。整数の距離は、内部で浮動小数点型に変換した後も比較結果が変化しない範囲に収める必要があります。

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

中心を固定し、個数の条件を満たす所属のうち `flow_cost` の合計が最小のものを最小費用流で求めます。その後に中心を更新し、これを繰り返します。固定した中心に対しては最適な所属を厳密に求めますが、中心の更新まで含めた全体の最適解は保証しません。`flow_cost` が `cost` を丸めた値の場合、所属の最適性は丸めた後のコストについて成り立ちます。

`cost` と `flow_cost` の大小関係が一致しない場合はコストが単調に減少するとは限らないため、途中で得た状態のうち `total_cost` が最小のものを返します。

各範囲が `0 <= lower <= upper` を満たし、点の個数が下限の合計以上かつ上限の合計以下である必要があります。下限が 0 のクラスタは空でも構いません。空のクラスタの中心は前回の値を維持し、最小費用流で求めた所属は変更しません。

`flow_cost` は 0 以上の値を `long long` 以下の幅の符号付き整数型で返します。全点分の合計が `long long` に収まり、返す値の最大値と `n+k+3` の積が AtCoder Library の上限 `8*10^18+1000` 以下である必要があります。浮動小数点数からの丸め方は問題ごとに異なるため、ライブラリ内では変換しません。

## 計算量

点の個数を `n`、クラスタ数を `k`、中心の更新回数を `I`、複数の初期値での試行回数を `r` とします。Hamerly 法については、反復 `i` で全中心との距離を計算し直す点の個数を `q_i`、空のクラスタの個数を `e_i` とします。`cost`、`metric`、1 点あたりの中心計算は定数時間とみなします。

| 処理 | 時間 | 追加メモリ |
|---|---:|---:|
| K-means++ | `O(nk)` | `O(n+k)` |
| Lloyd 法 | `O(Ink)` | `O(n+k)` |
| Hamerly 法 | `O(nk + Σ(k²+n+q_i k+ne_i))` | `O(n+k)` |
| 個数制約付き版 | `O(nk + In(nk+n+k)log(n+k))` | `O(nk)` |
| 複数の初期値 | 各方式の時間の `r` 倍 | 各方式と同じ |

Hamerly 法では `0 <= q_i <= n`、`0 <= e_i < k` です。最悪計算量の単純な上界は `O(nk+I(k²+nk))` ですが、クラスタが分離している場合は `q_i` が小さくなりやすく、多くの距離計算を省略できます。

個数制約付き版の上界は、頂点数 `O(n+k)`、辺数 `O(nk)`、流量 `n` の最小費用流を各反復で解く場合の値です。実際の実行時間も通常版よりかなり長くなります。

中心の計算自体に時間がかかる場合は、その計算量を反復の回数分加えます。

## 入力条件

- 初期中心を自動的に選択する場合は、`points` が空でなく、`1 <= k <= n` である必要があります。
- 初期中心を指定する通常版と Hamerly 版でも、`1 <= k <= n` が必要です。
- 初期中心を指定する個数制約付き版では、範囲が実現可能であれば `k > n` も許容します。
- `max_iterations >= 1` が必要です。複数の初期値で実行する場合は `trials >= 1` も必要です。
- 個数制約付き版では `n+k+3 <= INT_MAX` が必要です。
- コスト、距離、中心計算の各関数は、同じ入力に対して同じ結果を返し、点と中心を変更しない必要があります。
- `cost` の各値と全点分の合計は、戻り値の型で有限かつ非負であり、桁あふれしない必要があります。
