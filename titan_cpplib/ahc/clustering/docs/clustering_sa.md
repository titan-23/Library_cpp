# クラスタリングSA

## 対象

`clustering_sa.cpp` は、各点から所属クラスタの平均までの二乗距離の総和を、焼きなましで小さくする実装である。クラスタ数は固定し、各クラスタの点数は自由、固定、上下限付きのいずれかにできる。

- 常に個数条件を満たす状態だけを扱い、条件を破る途中状態を作らない
- 各クラスタは常に一点以上を持つ。下限0の空クラスタは扱わない
- 点の重さ、点ごとの所属先の制限は扱わない

個数自由の問題では、まずHamerly法を複数初期値で回す方がよい。SAは、個数制約がある場合や、K-meansの局所解から所属をさらに入れ替えたい場合に使う。

`clustering_sa.cpp` を読み込めば、`cluster_size_range.cpp`、`clustering_partition.cpp`、`ahc/sa/sa.cpp` も読み込まれる。

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

`result.labels` が最良の所属、`result.centers` が各クラスタの平均、`result.total_cost` がクラスタ内二乗誤差の総和である。

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

`ClusteringSaProblem` は構築後に変化せず、複数の状態から共有できる。状態より長く生存させる。

## 個数範囲

```cpp
// 個数自由。各クラスタは [1, n]
auto ranges = titan23::make_clustering_size_ranges(k, points.size());

// 個数固定
auto ranges = titan23::make_exact_clustering_size_ranges({20, 20, 30});

// 上下限を個別に指定
vector<titan23::ClusterSizeRange> ranges = {{10, 30}, {15, 35}, {20, 40}};
```

各範囲は `1 <= lower <= upper <= n` を満たし、点数は下限の合計以上かつ上限の合計以下でなければならない。`check_clustering_size_ranges(n, ranges)` で検査できる。

## 初期所属

初期所属は個数条件を満たしている必要がある。

- 個数自由なら、`kmeans`、`kmeans_hamerly`、`emeans` の `labels` をそのまま渡せる。
- 個数制約付きなら、下限1以上の範囲で `kmeans_balanced` を回した `labels` を渡せる。
- 短時間で個数制約付きの初期所属が欲しい場合は、Hamerly法の所属を条件に合うよう補正する。各クラスタで中心に近い点から残し、あふれた点を不足するクラスタへ近い順に入れる。実装例は `benchmark/run_benchmark.cpp` の `repair_labels_to_ranges` にある。

```cpp
auto initial = titan23::kmeans_hamerly(/* ... */);
titan23::ClusteringSaProblem problem(points);
auto ranges = titan23::make_clustering_size_ranges(initial.centers.size(), points.size());
auto result = titan23::clustering_sa_from_labels(problem, initial.labels, ranges, 500);
```

`clustering_cost_from_labels(problem, labels, ranges)` は、所属が条件を満たすか検査し、同じ目的関数の費用を返す。

### 複数の初期所属から始める

```cpp
auto make_initial_labels = [&](uint32_t current_seed) {
    return make_initial_solution(current_seed).labels;  // vector<int>
};

auto result = titan23::clustering_sa_from_label_factory(
    problem, ranges,
    1000,   // 総時間 [ms]
    0.2,    // 初期所属の作成に使う時間の割合
    make_initial_labels,
    options, 23, false
);
```

総時間の約20%まで、種 `seed, seed+1, ...` で初期所属を作り、費用が最小のものから残り時間でSAを回す。初期所属を作る一回の処理は途中で止めないため、その分だけ割合を超えることがある。時間が0でも一つは作る。作った回数は `result.initial_trials` に入る。

個数固定の中規模問題では、時間の一部を初期所属に使うと改善しやすい。一方、クラスタ数が多い問題ではSAの時間が減る悪影響が大きい。割合は問題ごとに測って決める。

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
| `nearby_cluster_count` | 各クラスタについて保存する中心の近いクラスタ数 `h`。0なら近いクラスタと各点の次候補を作らない |
| `cluster_samples` | 移動元クラスタを選ぶときに比べる候補数 |
| `early_cluster_samples` | 序盤だけ使う移動元クラスタの候補数。-1なら50クラスタ以上で16、それ未満で `cluster_samples` と同じ。0なら常に `cluster_samples` |
| `point_samples` | 動かす点を選ぶときに比べる候補数 |
| `early_point_samples` | 序盤だけ使う点の候補数。0なら `point_samples` と同じ |
| `swap_partner_samples` | 二点交換で相手側から正確に評価する点数 |
| `cycle_partner_samples` | 三点循環で最後のクラスタから正確に評価する点数 |
| `candidate_refresh_interval` | この回数の採択ごとに候補情報を更新する |
| `rebuild_point_limit` | 二クラスタ再構築で取り出す点数の上限 |
| `rebuild_three_point_limit` | 三クラスタ再構築で取り出す点数の上限。時間とメモリが三乗で増える |
| `rebuild_iterations` | 再構築内で割当と中心更新を繰り返す回数 |
| `refine_rebuild_seeds` | 再構築の仮中心を選ぶとき、最遠点をもう一度たどる |
| `rebuild_three_probability` | 再構築を選んだとき、停滞と無関係に三クラスタ版を使う確率 |
| `rebuild_three_same_state_threshold` | 二クラスタ再構築がこの回数連続で同じ所属に戻ったら、次の再構築を三クラスタ版にする。-1なら個数固定と個数自由で1、上下限付きで3。0ならこの切替えを使わない |
| `score_scale` | SA内部の費用を割る正数。0なら初期費用を点数で割った値 |
| `uniform_selection_probability` | クラスタと点を候補情報を使わず一様に選ぶ割合 |
| `middle_phase_start`, `late_phase_start` | 近傍の重みを切り替える進行率 |
| `*_weights` | 序盤・中盤・終盤の近傍の重み。順に一点移動、二点交換、三点循環、再構築 |

`score_scale` は温度の尺度を問題によらずそろえるためのもので、返す `total_cost` は割る前の値である。異なる初期所属を `sa_multi_run` や `replica_run` で比べる場合は、全状態に同じ正の値を指定する。

温度は `sa.cpp` と同じく `param` で設定する。既定値は開始1、終了0.001である。

```cpp
titan23::ClusteringSaState::param.start_temp = 1.0;
titan23::ClusteringSaState::param.end_temp = 0.001;
```

## Stateを直接使う

問題固有の初期化と組み合わせる場合や、`sa.cpp` の別の実行関数を使う場合は、`ClusteringSaState` を構築して渡す。

```cpp
titan23::Timer timer;
titan23::ClusteringSaState state(problem, initial_labels, ranges, options, 23);
double remaining = max(0.0, 1000.0 - timer.elapsed());
auto result = sa::sa_run<titan23::ClusteringSaState>(remaining, state, false);
state.finalize_result(result);  // 探索全体の統計を入れる
```

状態の複製には、所属、集計、候補情報、作業領域の複製が伴う。

## 結果

```cpp
result.labels;          // 最良の所属
result.centers;         // 各クラスタの平均
result.cluster_sizes;   // 各クラスタの点数
result.total_cost;      // 二乗誤差の総和（true_score と同じ）
result.score;           // total_cost / score_scale
result.score_scale;
result.statistics;      // 近傍ごとの統計
result.initial_trials;  // 初期所属を作った回数
```

`statistics` には、近傍ごとの試行数、有効な提案数、採択数、改善した採択数、改善量の合計（元の費用の単位）、候補情報の更新回数、同じ所属に戻った再構築の回数が入る。三クラスタ再構築だけの値は `rebuild_three_*` に入る。所属と費用は最良解のもので、統計は探索全体のものである。

## 仕組み

### 状態と費用

所属は `ClusteringPartition` が持つ。各点の所属、各クラスタの所属点の列、列内の位置を保ち、一点移動・二点交換・三点循環を `O(1)` で反映する。

各クラスタについて、点数 `count`、座標和 `sum`、座標二乗和の総和 `squared_sum` を持つ。クラスタの費用は次で求まる。

```text
cost = squared_sum - |sum|² / count
```

一点の追加・削除で集計は `O(d)` で更新できる。一点移動、二点交換、三点循環の費用差は、影響する二、三クラスタについて変更後の集計から正確に計算する。

二乗距離では、点 `x` を点数 `a` のクラスタAから点数 `b` のクラスタBへ移す費用差は次になる（`a >= 2`）。

```text
- a / (a - 1) * |x - center_a|² + b / (b + 1) * |x - center_b|²
```

移動による中心の変化も含むため、Lloyd法が収束した後でも改善する一点移動が残っていることがある。

### 候補情報

候補を選ぶために、次の情報を持つ。

- 各クラスタについて、中心の近い `h` 個のクラスタ
- 各点について、現在の中心までの二乗距離と、近い `h` 個のクラスタのうち最も近いもの（次候補）
- 各点の所属の余裕: 次候補までの二乗距離 − 現在の中心までの二乗距離

所属の余裕が小さい点はクラスタの境界にあり、移動や交換の候補として有力である。現在の中心から遠い点は、誤ったクラスタに残された点の候補になる。ただし点群全体の外側にある点は、中心から遠くても移る先がないことが多いため、所属の余裕も合わせて使う。

候補情報は、`candidate_refresh_interval` 回の採択ごとと再構築の採択後に全体を作り直す。それ以外では古いまま使うが、採択の判定には常に正確な費用差を使うため、古さは探索効率にしか影響しない。

### 近傍の選択

進行率が `middle_phase_start` 未満なら `early_weights`、`late_phase_start` 未満なら `middle_weights`、それ以降は `late_weights` の重みで近傍を選ぶ。重い再構築は序盤に多く使い、終盤では使わない。個数がすべて固定のときは一点移動を選ばない。

各提案では、`uniform_selection_probability` の確率でクラスタと点を一様に選ぶ。それ以外は次のように候補を絞る。

- 移動元クラスタ: 無作為に `cluster_samples` 個を取り、一点当たり費用が最大のものを選ぶ。50クラスタ以上では序盤だけ16個を比べ、初期所属に残った悪いクラスタを早く直す
- 動かす点: 所属点から `point_samples` 個を取り、所属の余裕が最小の点か現在の費用が最大の点を半々の確率で選ぶ
- 移動先クラスタ: 点の次候補、移動元に近いクラスタ、残りのクラスタの順に、個数条件を満たすものを選ぶ

### 小さい近傍

| 近傍 | 内容 |
|---|---|
| 一点移動 | 一点を別のクラスタへ移す。移動元が下限、移動先が上限にある場合は選ばない |
| 二点交換 | 移動元の一点と、移動先から `swap_partner_samples` 点を取り、費用差が最小の組を提案する |
| 三点循環 | クラスタA→B→Cをたどり、Aの点をBへ、Bの点をCへ、Cの点をAへ移す。Cからは `cycle_partner_samples` 点を試す |

二点交換と三点循環は各クラスタの点数を変えないため、個数固定の問題でも使える。

### 二クラスタ再構築

選んだ二クラスタから、合わせて最大 `rebuild_point_limit` 点を取り出す。各クラスタの取り出し数は点数に比例させ、その半分を相手クラスタとの境界に近い点、残りを現在の中心から遠い点とする。

取り出した点から仮中心を二つ選ぶ。一つは無作為な点、もう一つはそこから最も遠い点である。仮中心を固定したとき、各クラスタの元の点数を保つ最良の割当は、二中心への二乗距離の差が小さい順に一方へ入れれば求まる。この割当と中心の更新を `rebuild_iterations` 回まで繰り返す。

現在の中心から割当をやり直しても同じ所属に戻るだけなので、仮中心を選び直して中心配置を崩すのが目的である。仮中心を固定した割当は正確だが、中心の更新を含めた全体は局所的な近似である。

### 三クラスタ再構築

二クラスタ再構築が `rebuild_three_same_state_threshold` 回続けて同じ所属に戻った後に使う。三クラスタA→B→Cをたどって選び、合わせて最大 `rebuild_three_point_limit` 点を取り出す。各クラスタから取り出す点数と戻す点数は同じにする。

仮中心は、無作為な点、そこから最も遠い点、その二点のどちらからも遠い点の三つである。仮中心を固定した割当は、先頭二クラスタへ入れた点数を状態とする動的計画法で正確に求める。三乗の時間がかかるため、対象点数の上限は二クラスタ版より小さい。

### 提案と反映

`modify` は近傍を選んで変更後の費用と差分だけを作業領域に作り、所属と集計は書き換えない。`advance` で所属・集計・費用を確定し、`rollback` は作業領域を捨てるだけである。棄却時に元へ戻す処理はない。

## 計算量

点数を `n`、クラスタ数を `k`、次元を `d`、近いクラスタ数を `h`、移動元クラスタの調査数を `c`、交換相手の調査数を `s` とする。再構築で選んだクラスタの合計点数を `m`、取り出す点数を `b`、反復数を `J` とする。

| 処理 | 平均時間 |
|---|---:|
| 問題の構築 | `O(nd)` |
| 状態の構築 | `O(nd+kd)` と候補情報の作成 |
| 候補情報の作成 | `O(nhd+k²d)` |
| 一点移動の提案 | 通常 `O(h+d)`、探索失敗時は `O(k+d)` |
| 二点交換の提案 | 通常 `O(sd+h)`、探索失敗時は `O(sd+k)` |
| 三点循環の提案 | 通常 `O(sd+h)`、探索失敗時は `O(sd+k)` |
| 二クラスタ再構築の提案 | `O(md+b log b+Jbd)` |
| 三クラスタ再構築の提案 | `O(md+J(bd+b³))` |
| 小さい近傍の反映 | `O(d)` |
| 再構築の反映 | `O(b+d)` |

各提案では移動元を選ぶために `O(c)` が加わる。

問題は座標に `O(nd)`、状態は所属と候補情報に `O(n+kh)`、クラスタの集計に `O(kd)` のメモリを使う。三クラスタ再構築の動的計画法は、割当の復元に最悪 `O(b³)` の作業領域を使う。既定の `b=36` では小さいが、上限を大きくすると時間とメモリが急増する。

## 注意

- 一点移動、二点交換、三点循環の費用差は、現在の状態に対して `double` の範囲で正確に求める。再構築も、最後に得た所属の費用を集計から求めて採択を判定する。
- 費用は `squared_sum - |sum|²/count` の形で、近い大きな値の差として求める。座標に大きな共通の平行移動がある入力では桁落ちしやすいので、座標の平均を引いてから渡す。
- 座標、各点の座標二乗和、点や中心の間の二乗距離、クラスタの座標和と費用は、`double` で有限に収まる範囲にする。
- 候補を絞った近似解法なので、全体の最適解は保証しない。
- クラスタが一つだけなら変更の余地がないため、`clustering_sa_from_labels` は状態を作ってそのまま返す。
- `clustering_sa_from_labels` の時間制限は有限で0以上にする。
