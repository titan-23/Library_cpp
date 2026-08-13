# クラスタリングSA

## 対象

`clustering_sa.cpp` は、二乗ユークリッド距離の合計を焼きなましで改善する実装である。クラスタ数は固定し、各クラスタの点数を自由、固定、上下限付きのいずれかにできる。

次の近傍を使う。

- 一点を別のクラスタへ移す
- 異なるクラスタの二点を交換する
- 三クラスタの点を一つずつ循環移動する
- 二クラスタから点を一部取り出し、個数を保って分け直す
- 二クラスタ再構築が同じ所属へ戻った後、三クラスタをまとめて分け直す

候補選びには中心間距離や各点の次候補を使うが、採択判定の費用差は現在のクラスタ集計から計算する。候補情報が少し古くても費用差は変わらない。

各クラスタは常に一点以上を持つ。下限0の空クラスタ、点の重さ、点ごとの所属先制限は未対応である。

クラスタが一つだけなら変更の余地がないため、`clustering_sa_from_labels` は状態を構築してそのまま返す。

## ファイル

- `clustering_partition.cpp`: 所属点、所属内の位置、一点移動、二点交換、三点循環を管理
- `clustering_sa.cpp`: 二乗ユークリッド距離用の問題、状態、近傍、実行関数
- [`sa_neighborhood_design.md`](sa_neighborhood_design.md): 近傍を選んだ理由と今後の候補
- [`benchmark.md`](benchmark.md): 公開問題と人工問題による性能比較

`clustering_sa.cpp` を読み込めば、所属管理と既存の `sa.cpp` も読み込まれる。

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
    1000,
    {},
    23,
    false
);
```

`result.labels` が最良の所属、`result.centers` が各座標の平均、`result.total_cost` がクラスタ内二乗誤差の合計である。

## 点の型

既定の構築方法では、各点に `size()` と `operator[]` が必要である。`vector<double>` と `array<double, D>` をそのまま使える。座標は構築時に `double` の平坦な配列へコピーされる。

`pair` や独自の点型には、次元と座標を取り出す関数を渡す。

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

点数と次元は正にし、座標は有限な値にする。各点の座標二乗和、点や中心の間の二乗距離、クラスタの座標和と費用も `double` で有限になる範囲に収める。

## 個数制約

個数を自由にする場合でも、空クラスタは許さない。

```cpp
auto ranges = titan23::make_clustering_size_ranges(k, points.size());
```

個数を固定する場合は各クラスタの点数を渡す。

```cpp
auto ranges = titan23::make_exact_clustering_size_ranges({20, 20, 30});
```

上下限を個別に指定することもできる。

```cpp
vector<titan23::ClusteringSizeRange> ranges = {
    {10, 30},
    {15, 35},
    {20, 40}
};
```

初期所属はすでに制約を満たす必要がある。一点移動は移動後も上下限を守れる場合だけ提案する。二点交換、三点循環、二・三クラスタ再構築は各クラスタの点数を変えない。

## K-meansの結果から始める

通常版または個数制約付き版の `labels` をそのまま初期所属にできる。

```cpp
auto initial = titan23::kmeans(/* ... */);
titan23::ClusteringSaProblem problem(points);
auto ranges = titan23::make_clustering_size_ranges(initial.centers.size(), points.size());
auto result = titan23::clustering_sa_from_labels(
    problem,
    initial.labels,
    ranges,
    500,
    {},
    23,
    false
);
```

個数制約付きK-meansから始める場合は、その制約と同じ範囲を渡す。ただし、空クラスタを含む初期解は現在のSA版へ渡せない。

## 複数の初期解から始める

初期所属を作る関数を渡し、総時間の一部で複数の初期解を比べられる。

```cpp
auto make_initial_labels = [&](uint32_t current_seed) {
    auto initial = make_initial_solution(current_seed);
    return initial.labels;
};

auto result = titan23::clustering_sa_from_label_factory(
    problem,
    ranges,
    1000,
    0.2,
    make_initial_labels,
    options,
    23,
    false
);
```

この例では、総時間1000ミリ秒のうち約20%まで初期所属を作り、費用が最も小さい所属から残り時間のSAを始める。初期所属を作る一回の処理は途中で止めないため、その処理の残り時間だけ割合を超えることがある。時間が0でも初期所属は一つ作る。

初期所属を作る関数は `uint32_t` の種を受け、`vector<int>` を返す。同じ種には同じ所属を返し、返す所属は個数条件を満たす必要がある。試した初期解の数は `result.initial_trials` に入る。

実測では、個数固定の中規模問題で20%を初期解へ使うと改善した。ただし100クラスタ問題ではSAへ残す時間が減る悪影響が大きかった。個数自由では時間の大半を初期解へ使っても、Hamerly法を時間いっぱい複数初期値で回す方法を安定して上回らなかった。割合は問題ごとに測って決める。

`clustering_cost_from_labels(problem, labels, ranges)` を使うと、所属が条件を満たすか検査し、同じ目的関数の費用を計算できる。

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

- `nearby_cluster_count`: 各クラスタについて保存する近いクラスタ数。0なら近いクラスタと各点の次候補を作らない
- `cluster_samples`: 移動元クラスタを選ぶときに比べる候補数
- `early_cluster_samples`: 序盤だけ使う移動元クラスタの候補数。-1なら50クラスタ以上で16、それ未満で `cluster_samples` と同じ。0ならクラスタ数に関係なく `cluster_samples` と同じ。正の値ならその値を使う
- `point_samples`: 移動する点を選ぶときに比べる候補数
- `early_point_samples`: 序盤だけ使う点の候補数。0なら `point_samples` と同じ
- `swap_partner_samples`: 二点交換で相手側から正確に調べる点数
- `cycle_partner_samples`: 三点循環で最後のクラスタから正確に調べる点数
- `candidate_refresh_interval`: この回数の採択ごとに全点の次候補を更新
- `rebuild_point_limit`: 二クラスタ再構築で取り出す点数の上限
- `rebuild_three_point_limit`: 三クラスタ再構築で取り出す点数の上限
- `rebuild_iterations`: 取り出した点の分け直しと中心更新の回数
- `refine_rebuild_seeds`: 仮中心を選ぶ際、遠い点を追加でもう一度たどるか。既定では使わない
- `rebuild_three_probability`: 再構築を選んだとき、停滞状態に関係なく三クラスタ版を使う確率
- `rebuild_three_same_state_threshold`: 二クラスタ再構築がこの回数連続で同じ所属へ戻った後、次の再構築を三クラスタ版にする。-1なら個数固定と個数自由で1、上下限付きで3を使う。0ならこの切替えを使わない
- `uniform_selection_probability`: 候補の偏りを避ける一様選択の割合
- `middle_phase_start`, `late_phase_start`: 近傍割合を切り替える進行率

`ClusteringNeighborhoodWeights` の順は、一点移動、二点交換、三点循環、再構築である。再構築の中で二クラスタ版と三クラスタ版を切り替える。重い再構築は既定で終盤に使わない。

`early_cluster_samples` を増やすと、初期所属に大きく悪いクラスタが残った場合に見つけやすくなる。一方、全期間で増やすと同じクラスタへ探索が偏りやすい。既定では50クラスタ以上に限り、進行率 `middle_phase_start` まで16個を比べ、その後は `cluster_samples` へ戻す。どちらもクラスタ数を上限とする。点の候補数も同様に序盤だけ変えられるが、実測では16へ増やすと悪い側の結果が不安定になったため、既定では切り替えない。

`score_scale` は焼きなまし内部の費用を割る正数である。0なら初期費用を点数で割った値を使う。返却する `total_cost` は割る前の費用なので、最小化する対象は変わらない。異なる初期所属を `sa_multi_run` や `replica_run` で比較する場合は、全状態に同じ正の値を指定する。

温度は既存SAと同じ設定を使う。

```cpp
titan23::ClusteringSaState::param.start_temp = 1.0;
titan23::ClusteringSaState::param.end_temp = 0.001;
```

## Stateを直接使う

問題固有の初期化を組み合わせる場合は、`ClusteringSaState` を構築して既存の `sa_run` へ渡す。

```cpp
titan23::Timer timer;
auto state = titan23::ClusteringSaState(
    problem,
    initial_labels,
    ranges,
    options,
    23
);
double remaining = max(0.0, 1000.0 - timer.elapsed());
auto result = sa::sa_run<titan23::ClusteringSaState>(
    remaining,
    state,
    false
);
```

`ClusteringSaProblem` は状態より長く生存させる。問題は構築後に変化せず、複数状態から共有できる。状態の複製には所属、集計、候補、作業領域の複製が伴う。

## 結果

```cpp
result.labels;
result.centers;
result.cluster_sizes;
result.total_cost;
result.true_score;
result.score;
result.score_scale;
result.statistics;
result.initial_trials;
```

`total_cost` と `true_score` は元の単位のクラスタ内二乗誤差である。`score` は費用を `score_scale` で割った焼きなまし内部の値である。

`statistics` には近傍ごとの試行数、有効な提案数、採択数、改善した採択数、元の費用単位での改善量、候補情報の更新回数、同じ所属へ戻った再構築数が入る。`sa_run` から返す最良解には、探索終了までの統計を入れる。所属と費用は最良解のものであり、統計だけは探索全体を表す。`initial_trials` は初期所属を作った回数で、`clustering_sa_from_labels` では1である。

## 二・三クラスタ再構築

### 二クラスタ版

選んだ二クラスタから、次の点を合わせて最大 `rebuild_point_limit` 点取り出す。

- 相手クラスタへ移りやすい境界付近の点
- 現在の中心から遠い点

取り出した点から仮中心を二つ選び、各クラスタの元の点数を保つように分け直す。仮中心を固定した一回の割当では、各点の二中心への費用差が小さい順に必要数を選ぶ。この割当は二クラスタの個数を固定した条件で正確である。中心更新まで繰り返した結果は局所的な近似であり、二クラスタ全体の最適解を保証しない。

### 三クラスタ版

三クラスタから合計最大 `rebuild_three_point_limit` 点を取り出す。各クラスタから取り出す点数と、分け直した後に戻す点数は同じにする。

仮中心を固定した一回の割当は、先頭二クラスタへ割り当てた点数を状態とする動的計画法で求める。三つ目の点数は、処理済み点数から先頭二つの点数を引けば決まる。このため、選んだ点と個数の条件に対する割当は正確である。仮中心の更新まで含めた全体は局所的な近似である。

既定では三クラスタ版を一定確率で混ぜない。二クラスタ版が規定回数だけ同じ所属へ戻った後に使う。二クラスタ版がまだ有効な問題でその試行回数を奪わず、二クラスタ単位で止まった状態だけを大きく変更するためである。

## 計算量

点数を `n`、クラスタ数を `k`、次元を `d`、近いクラスタ数を `h`、移動元クラスタの調査数を `c`、交換相手の調査数を `s` とする。再構築で選んだクラスタの合計点数を `m`、取り出す点数を `b`、反復数を `J` とする。

| 処理 | 平均時間 |
|---|---:|
| 問題の構築 | `O(nd)` |
| 状態の集計 | `O(nd+kd)` |
| 候補情報の全更新 | `O(nhd+k²d)` |
| 一点移動の提案 | 通常 `O(h+d)`、探索失敗時は `O(k+d)` |
| 二点交換の提案 | 通常 `O(sd+h)`、探索失敗時は `O(sd+k)` |
| 三点循環の提案 | 通常 `O(sd+h)`、探索失敗時は `O(sd+k)` |
| 二クラスタ再構築の提案 | `O(md+b log b+Jbd)` |
| 三クラスタ再構築の提案 | `O(md+J(bd+b³))` |
| 小さい近傍の適用 | `O(d)` |
| 二クラスタ再構築の適用 | `O(b+d)` |
| 三クラスタ再構築の適用 | `O(b+d)` |

各提案では移動元を選ぶため `O(c)` が加わる。既定の `c` は序盤の高クラスタ数でも最大16である。点と相手クラスタの候補を少数だけ調べる時間も加わるが、各設定値を定数とみなせば表の範囲に含まれる。`nth_element` を使う箇所の時間は平均である。

問題が持つ座標に `O(nd)`、状態ごとに所属と候補へ `O(n+kh)`、クラスタ集計へ `O(kd)` の追加メモリを使う。再構築候補を選ぶため、状態ごとに点数 `n` の作業配列を一つ持つ。三クラスタ版の動的計画法は、選んだ点の割当を復元するため最悪 `O(b³)` の作業領域を使う。既定の `b=36` では小さいが、上限を大きくすると時間とメモリが急増する。

候補情報は初期化時と、`candidate_refresh_interval` 回の採択ごと、再構築の採択後に更新する。全体時間は、小さい近傍の提案回数、再構築の対象点数、候補情報の更新回数に依存するため、時間上限で管理する。

## 保証と使い分け

- 一点移動、二点交換、三点循環の費用差は、現在状態に対して `double` の計算範囲内で求める。
- 再構築でも、最後に得た所属の費用をクラスタ集計から求めて採択する。
- どの操作も個数制約を破る途中状態を本体へ反映しない。
- 候補制限と焼きなましを使う近似解法なので、全体の最適解は保証しない。

通常のK-meansを短時間で解くなら、まずK-means++と複数初期値を使う。個数制約付きK-meansは、中心を固定した所属を最小費用流で正確に求める。SA版はその結果を初期解にし、中心更新も含む一点移動や複数点の変更で局所解から抜ける用途に向く。
