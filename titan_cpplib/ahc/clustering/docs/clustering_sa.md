# クラスタリング SA

## 対象

`clustering_sa.cpp` は、各点から所属クラスタの平均までの二乗距離の総和を、焼きなましで小さくする実装です。クラスタ数は固定で、各クラスタの点数の条件は、制約なし、固定、上下限付きのいずれかから選択できます。

- 常に個数の条件を満たす状態だけを扱い、条件に違反する途中の状態は作成しません。
- 各クラスタには常に 1 点以上が所属します。下限が 0 の空のクラスタは扱いません。
- 点の重みと、点ごとの所属先の制限は扱いません。

個数に制約がない問題では、まず Hamerly 法を複数の初期値で実行することを推奨します。SA は、個数に制約がある場合や、K-means の局所解からさらに所属を入れ替えたい場合に使用します。

`clustering_sa.cpp` を include すると、`cluster_size_range.cpp`、`clustering_partition.cpp`、`ahc/sa/sa.cpp` も読み込まれます。

## 最小例

```cpp
#include <bits/stdc++.h>
#include "titan_cpplib/ahc/clustering/clustering_sa.cpp"
using namespace std;

vector<array<double, 2>> points = {
    {0, 0}, {1, 0}, {0, 1},
    {10, 10}, {11, 10}, {10, 11}
};
vector<int> initial_labels = {0, 0, 1, 0, 1, 1};

titan23::ClusteringSaProblem problem(points);
auto ranges = titan23::make_clustering_size_ranges(2, points.size());
auto result = titan23::clustering_sa_from_labels(
    problem,
    initial_labels,
    ranges,
    1000,   // 時間制限 [ms]
    {},     // ClusteringSaOptions
    23,     // seed
    false   // verbose
);
```

`result.labels` が最良の所属、`result.centers` が各クラスタの平均、`result.total_cost` がクラスタ内二乗誤差の総和です。

## 点の型

既定の構築方法では、各点に `size()` と `operator[]` が必要です。`vector<double>` と `array<double, D>` はそのまま使用できます。座標は構築時に `double` の 1 次元配列へコピーされます。

`pair` や独自の点の型を使用する場合は、次元と座標を取り出す関数を渡します。

```cpp
using Point = pair<double, double>;
vector<Point> points = {{0, 0}, {1, 0}, {10, 0}, {11, 0}};

titan23::ClusteringSaProblem problem(
    points,
    2,
    [](const Point& point, int axis) {
        return axis == 0 ? point.first : point.second;
    }
);
```

`ClusteringSaProblem` は構築後に変化せず、複数の状態から共有できます。ただし、状態より後に破棄してください。

## 個数の範囲

```cpp
// 個数に制約がない場合 / 各クラスタの範囲は [1, n]
auto ranges = titan23::make_clustering_size_ranges(k, points.size());

// 個数を固定する場合
auto ranges = titan23::make_exact_clustering_size_ranges({20, 20, 30});

// 上下限を個別に指定する場合
vector<titan23::ClusterSizeRange> ranges = {{10, 30}, {15, 35}, {20, 40}};
```

各範囲は `1 <= lower <= upper <= n` を満たし、点数は下限の合計以上かつ上限の合計以下である必要があります。`check_clustering_size_ranges(n, ranges)` で確認できます。

## 初期所属

初期所属は個数の条件を満たしている必要があります。

- 個数に制約がない場合は、`kmeans`、`kmeans_hamerly`、`emeans` の `labels` をそのまま渡すことができます。
- 個数に制約がある場合は、下限が 1 以上の範囲で `kmeans_balanced` を実行した結果の `labels` を渡すことができます。
- 個数の制約を満たす初期所属を短時間で作成する場合は、Hamerly 法の所属を、条件を満たすように補正します。各クラスタでは中心に近い点から順に残し、残らなかった点は、点数が不足するクラスタへそのクラスタの中心に近い順に割り当てます。実装例は `benchmark/run_benchmark.cpp` の `repair_labels_to_ranges` にあります。

```cpp
auto initial = titan23::kmeans_hamerly(/* ... */);
titan23::ClusteringSaProblem problem(points);
auto ranges = titan23::make_clustering_size_ranges(initial.centers.size(), points.size());
auto result = titan23::clustering_sa_from_labels(problem, initial.labels, ranges, 500);
```

`clustering_cost_from_labels(problem, labels, ranges)` は、所属が条件を満たすかを確認し、同じ目的関数で求めたコストを返します。

### 複数の初期所属から開始する場合

```cpp
auto make_initial_labels = [&](uint32_t current_seed) {
    return make_initial_solution(current_seed).labels;  // vector<int>
};

auto result = titan23::clustering_sa_from_label_factory(
    problem, ranges,
    1000,   // 総時間 [ms]
    0.2,    // 初期所属の作成に使用する時間の割合
    make_initial_labels,
    options, 23, false
);
```

総時間の約 20% が経過するまで、シード `seed, seed+1, ...` で初期所属を作成し、コストが最小の初期所属から開始して、残りの時間で SA を実行します。初期所属を作成する 1 回の処理は途中で中断しないため、その分だけ割合を超える場合があります。初期所属の作成に割り当てる時間が 0 でも、初期所属は 1 つ作成します。作成した回数は `result.initial_trials` に格納されます。

個数を固定した中規模の問題では、時間の一部を初期所属の作成に使用すると結果が改善しやすくなります。一方、クラスタ数が多い問題では、SA の時間が減ることによる悪影響が大きくなります。割合は問題ごとに計測して決定してください。

## 設定

```cpp
struct ClusteringSaOptions {
    int nearby_cluster_count = 4;
    int cluster_samples = 4;
    int early_cluster_samples = -1;
    int point_samples = 8;
    int early_point_samples = 0;
    int swap_partner_samples = 12;
    int cycle_partner_samples = 8;
    int candidate_refresh_interval = 256;
    int rebuild_point_limit = 128;
    int rebuild_three_point_limit = 36;
    int rebuild_iterations = 4;
    bool refine_rebuild_seeds = false;
    double rebuild_three_probability = 0;
    int rebuild_three_same_state_threshold = -1;
    double score_scale = 0;
    double uniform_selection_probability = 0.05;
    double middle_phase_start = 0.30;
    double late_phase_start = 0.80;
    ClusteringNeighborhoodWeights early_weights{35, 35, 10, 20};
    ClusteringNeighborhoodWeights middle_weights{45, 40, 10, 5};
    ClusteringNeighborhoodWeights late_weights{50, 45, 5, 0};
};
```

| 項目 | 意味 |
|---|---|
| `nearby_cluster_count` | 各クラスタについて保存する、中心が近いクラスタの数 `h`。0 の場合は近いクラスタと各点の次候補を求めない |
| `cluster_samples` | 移動元クラスタを選ぶときに比較する候補数 |
| `early_cluster_samples` | 序盤だけ使用する移動元クラスタの候補数。-1 の場合は、クラスタ数が 50 以上なら 16、50 未満なら `cluster_samples` と同じ。0 の場合は常に `cluster_samples` を使用する |
| `point_samples` | 移動する点を選ぶときに比較する候補数 |
| `early_point_samples` | 序盤だけ使用する点の候補数。0 の場合は `point_samples` と同じ |
| `swap_partner_samples` | 2 点の交換で、相手側のクラスタから選んで正確に評価する点数 |
| `cycle_partner_samples` | 3 点の循環で、最後のクラスタから選んで正確に評価する点数 |
| `candidate_refresh_interval` | この回数だけ採択するごとに候補の情報を更新する |
| `rebuild_point_limit` | 2 クラスタの再構築で取り出す点数の上限 |
| `rebuild_three_point_limit` | 3 クラスタの再構築で取り出す点数の上限。時間とメモリは取り出す点数の三乗で増加する |
| `rebuild_iterations` | 再構築の中で割り当てと中心の更新を繰り返す回数 |
| `refine_rebuild_seeds` | 再構築の仮の中心を選ぶとき、最遠点をもう一度たどる |
| `rebuild_three_probability` | 再構築を選んだとき、停滞とは関係なく 3 クラスタの再構築を使用する確率 |
| `rebuild_three_same_state_threshold` | 2 クラスタの再構築がこの回数だけ連続して同じ所属に戻った場合は、次の再構築を 3 クラスタの再構築にする。-1 の場合は、個数が固定または制約なしのとき 1、上下限付きのとき 3 とする。0 の場合はこの切り替えを行わない |
| `score_scale` | SA の内部でコストを割る正の数。0 の場合は初期のコストを点数で割った値を使用する |
| `uniform_selection_probability` | 候補の情報を使用せずにクラスタと点を一様に選ぶ割合 |
| `middle_phase_start`, `late_phase_start` | 近傍の重みを切り替える進行率 |
| `*_weights` | 序盤、中盤、終盤の近傍の重み。1 点の移動、2 点の交換、3 点の循環、再構築の順に指定する |

`score_scale` は、温度の尺度を問題によらずそろえるために使用します。返される `total_cost` は、この値で割る前の値です。異なる初期所属を `sa_multi_run` や `replica_run` で比較する場合は、すべての状態に同じ正の値を指定してください。

温度は `sa.cpp` と同じく `param` で設定します。既定値は開始温度が 1、終了温度が 0.001 です。

```cpp
titan23::ClusteringSaState::param.start_temp = 1.0;
titan23::ClusteringSaState::param.end_temp = 0.001;
```

## State を直接使用する場合

問題固有の初期化と組み合わせる場合や、`sa.cpp` の別の実行関数を使用する場合は、`ClusteringSaState` を構築して渡します。

```cpp
titan23::Timer timer;
titan23::ClusteringSaState state(problem, initial_labels, ranges, options, 23);
double remaining = max(0.0, 1000.0 - timer.elapsed());
auto result = sa::sa_run<titan23::ClusteringSaState>(remaining, state, false);
state.finalize_result(result);  // 探索全体の統計を格納する
```

状態を複製すると、所属、集計、候補の情報、作業領域も複製されます。

## 結果

```cpp
result.labels;          // 最良の所属
result.centers;         // 各クラスタの平均
result.cluster_sizes;   // 各クラスタの点数
result.total_cost;      // 二乗誤差の総和 (true_score と同じ)
result.score;           // total_cost / score_scale
result.score_scale;
result.statistics;      // 近傍ごとの統計
result.initial_trials;  // 初期所属を作成した回数
```

`statistics` には次の値が格納されます。

- 近傍ごとの試行数、有効な提案数、採択数、改善した採択数
- 元のコストの単位で表した、近傍ごとの改善量の合計
- 候補の情報の更新回数
- 同じ所属に戻った再構築の回数

3 クラスタの再構築だけを集計した値は `rebuild_three_*` に格納されます。所属とコストは最良解のものであり、統計は探索全体を集計したものです。

## 仕組み

### 状態とコスト

所属は `ClusteringPartition` で管理します。`ClusteringPartition` は各点の所属、各クラスタに所属する点の列、列の中での各点の位置を保持し、1 点の移動、2 点の交換、3 点の循環を `O(1)` で反映します。

各クラスタについて、点数 `count`、座標の和 `sum`、座標の二乗和の総和 `squared_sum` を保持します。クラスタのコストは次の式で求まります。

```text
cost = squared_sum - |sum|² / count
```

1 点の追加と削除では、集計を `O(d)` で更新できます。1 点の移動、2 点の交換、3 点の循環のコストの差は、影響を受ける 2 つまたは 3 つのクラスタについて、変更後の集計から正確に計算します。

二乗距離では、点 `x` を点数 `a` のクラスタ A から点数 `b` のクラスタ B へ移動するときのコストの差は、`a >= 2` のとき次の式になります。

```text
- a / (a - 1) * |x - center_a|² + b / (b + 1) * |x - center_b|²
```

この式は移動による中心の変化も含むため、Lloyd 法が収束した後でも、コストを改善する 1 点の移動が残っている場合があります。

### 候補の情報

候補を選ぶために次の情報を保持します。

- 各クラスタについて、中心が近い `h` 個のクラスタ
- 各点について、現在の中心までの二乗距離と、所属クラスタに近い `h` 個のクラスタのうち中心が最も近いクラスタ (次候補)
- 各点の所属の余裕: 次候補までの二乗距離 − 現在の中心までの二乗距離

所属の余裕が小さい点はクラスタの境界にあり、移動や交換の候補として有力です。現在の中心から遠い点は、誤ったクラスタに残された点である可能性があります。ただし、点群全体の外側にある点は中心から遠くても移動先がない場合が多いため、所属の余裕も合わせて使用します。

候補の情報は、`candidate_refresh_interval` 回の採択ごとと再構築の採択後に、すべて求め直します。それ以外のときは古い情報をそのまま使用します。採択の判定には常に正確なコストの差を使用するため、情報が古くても影響を受けるのは探索の効率だけです。

### 近傍の選択

進行率が `middle_phase_start` 未満の場合は `early_weights`、`late_phase_start` 未満の場合は `middle_weights`、それ以降は `late_weights` の重みで近傍を選びます。計算量の大きい再構築は序盤に多く使用し、終盤では使用しません。すべてのクラスタの個数が固定の場合は、1 点の移動を選びません。

各提案では、`uniform_selection_probability` の確率でクラスタと点を一様に選びます。それ以外の場合は次のように候補を絞り込みます。

- 移動元クラスタ: 無作為に選んだ `cluster_samples` 個のクラスタのうち、1 点当たりのコストが最大のクラスタを選択します。クラスタ数が 50 以上の場合は序盤だけ 16 個を比較し、初期所属に残ったコストの大きいクラスタを早い段階で改善します。
- 移動する点: 移動元クラスタの点から選んだ `point_samples` 個のうち、所属の余裕が最小の点と現在のコストが最大の点のどちらかを等確率で選択します。
- 移動先クラスタ: 点の次候補、移動元に近いクラスタ、残りのクラスタの順に、個数の条件を満たすクラスタを選択します。

### 小さい近傍

| 近傍 | 内容 |
|---|---|
| 1 点の移動 | 1 点を別のクラスタへ移動する。移動元の点数が下限にある場合と、移動先の点数が上限にある場合は選ばない |
| 2 点の交換 | 移動元の 1 点と移動先の 1 点を交換する。移動先からは `swap_partner_samples` 点を選び、コストの差が最小になる組を提案する |
| 3 点の循環 | クラスタ A → B → C をたどり、A の点を B へ、B の点を C へ、C の点を A へ移動する。C からは `cycle_partner_samples` 点を試す |

2 点の交換と 3 点の循環は各クラスタの点数を変更しないため、個数を固定した問題でも使用できます。

### 2 クラスタの再構築

選んだ 2 つのクラスタから、合計で最大 `rebuild_point_limit` 点を取り出します。各クラスタから取り出す点数はクラスタの点数に比例させ、その半分を相手のクラスタとの境界に近い点、残りを現在の中心から遠い点とします。

取り出した点から仮の中心を 2 つ選びます。1 つは無作為に選んだ点、もう 1 つはその点から最も遠い点です。仮の中心を固定したとき、各クラスタの元の点数を維持する最良の割り当ては、2 つの中心への二乗距離の差が小さい点から順に一方のクラスタへ割り当てると求まります。この割り当てと中心の更新を最大 `rebuild_iterations` 回繰り返します。

現在の中心から再び割り当てを行っても同じ所属に戻るだけであるため、仮の中心を選び直して中心の配置を大きく変更します。仮の中心を固定したときの割り当ては正確に求まりますが、中心の更新を含めた全体は局所的な近似です。

### 3 クラスタの再構築

2 クラスタの再構築が `rebuild_three_same_state_threshold` 回続けて同じ所属に戻った後に使用します。3 つのクラスタを A → B → C とたどって選び、合計で最大 `rebuild_three_point_limit` 点を取り出します。各クラスタから取り出す点数と、そのクラスタへ戻す点数は等しくします。

仮の中心は、無作為に選んだ点、その点から最も遠い点、その 2 点のどちらからも遠い点の 3 つです。仮の中心を固定したときの割り当ては、先頭の 2 つのクラスタへ割り当てた点数を状態とする動的計画法で正確に求めます。この動的計画法には取り出す点数の三乗の時間がかかるため、点数の上限は 2 クラスタの再構築より小さくしています。

### 提案と反映

`modify` は近傍を選び、変更後のコストと差分だけを作業領域に格納します。所属と集計は更新しません。`advance` で所属、集計、コストを確定します。`rollback` は作業領域を破棄するだけであり、棄却時に元の状態へ戻す処理はありません。

## 計算量

点数を `n`、クラスタ数を `k`、次元を `d`、近いクラスタの数を `h`、移動元クラスタの候補数を `c`、交換相手の候補数を `s` とします。また、再構築で選んだクラスタの合計点数を `m`、取り出す点数を `b`、反復回数を `J` とします。

| 処理 | 平均時間 |
|---|---:|
| 問題の構築 | `O(nd)` |
| 状態の構築 | `O(nd+kd)` と候補の情報の作成 |
| 候補の情報の作成 | `O(nhd+k²d)` |
| 1 点の移動の提案 | 通常は `O(h+d)`、探索に失敗した場合は `O(k+d)` |
| 2 点の交換の提案 | 通常は `O(sd+h)`、探索に失敗した場合は `O(sd+k)` |
| 3 点の循環の提案 | 通常は `O(sd+h)`、探索に失敗した場合は `O(sd+k)` |
| 2 クラスタの再構築の提案 | `O(md+b log b+Jbd)` |
| 3 クラスタの再構築の提案 | `O(md+J(bd+b³))` |
| 小さい近傍の反映 | `O(d)` |
| 再構築の反映 | `O(b+d)` |

各提案では、移動元を選ぶために `O(c)` が加わります。

問題は座標に `O(nd)`、状態は所属と候補の情報に `O(n+kh)`、クラスタの集計に `O(kd)` のメモリを使用します。3 クラスタの再構築の動的計画法では、割り当ての復元に最悪の場合 `O(b³)` の作業領域が必要です。既定の `b=36` では作業領域は小さいですが、上限を大きくすると時間とメモリが急激に増加します。

## 注意

- 1 点の移動、2 点の交換、3 点の循環のコストの差は、現在の状態に対して `double` の範囲で正確に求めます。再構築でも、最後に得た所属のコストを集計から求めて採択を判定します。
- コストは `squared_sum - |sum|²/count` の形で、値の近い大きな数どうしの差として求めます。座標に共通の大きな平行移動がある入力では桁落ちが生じやすいため、座標の平均を引いてから渡してください。
- 座標、各点の座標の二乗和、点や中心の間の二乗距離、クラスタの座標の和とコストは、`double` で有限の値に収まる範囲にしてください。
- 候補を絞り込む近似解法であるため、全体の最適解は保証しません。
- クラスタが 1 つだけの場合は所属を変更できないため、`clustering_sa_from_labels` は状態を構築し、探索せずに結果を返します。
- `clustering_sa_from_labels` の時間制限には、0 以上の有限の値を指定してください。
