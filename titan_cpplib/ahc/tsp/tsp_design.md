# AHC 向け TSP ライブラリの設計

## 問題、候補表、巡回路の状態

`TspProblem` は点数と辺のコスト関数を保持します。対称 TSP の探索では、同じ問題から作成した候補表 `TspCandidates` と巡回路の状態 `TspState` を使用します。`TspState` は巡回順、各点の位置、巡回路のコスト、近傍操作の作業領域を保持し、探索関数は結果をこの状態に反映します。

非対称 TSP の探索では、同じ `TspProblem` に対して入辺と出辺の候補表 `AtspCandidates` を作成し、巡回順を `vector<int>` で保持します。

このライブラリは時間や反復回数を制限して近似解を求めます。小規模な問題の厳密解には `titan_cpplib/alg/traveling_salesman_problem.cpp` を使用できます。

## 対象とする問題

巡回路は指定した各点を 1 回ずつ訪問し、最後に開始点に戻る閉路です。辺のコストは有限かつ 0 以上とします。三角不等式は不要です。

対称 TSP では `edge_cost(u, v) == edge_cost(v, u)` が成り立つため、区間を反転しても区間内のコストは一定です。`TspState` の 2-opt は区間の両端で交換する辺だけを評価します。非対称 TSP では区間内の辺も向きによってコストが変化するため、`atsp_iterated_local_search.cpp` の探索でその差分を評価します。

## ファイル構成

| ファイル | 主な役割 |
|---|---|
| `tsp.cpp` | 問題、対称 TSP の候補表、巡回路の状態、最近傍法による初期解 |
| `tsp_symmetric_moves.cpp` | 2-opt、Or-opt、反転 Or-opt、Double Bridge の差分計算と適用 |
| `tsp_local_search.cpp` | 2-opt 局所探索と対称 TSP の探索で共有する内部クラス `TourSearch` |
| `tsp_iterated_local_search.cpp` | 対称 TSP の反復局所探索と `solve_tsp` |
| `atsp_iterated_local_search.cpp` | 非対称 TSP の反復局所探索、`solve_atsp`、`solve_atsp_path` |
| `tsp_guided_local_search.cpp` | 誘導局所探索 |
| `tsp_initial_state.cpp` | 焼きなましに渡す初期巡回路の改善 |
| `multiple_tours.cpp` | 固定デポを備えた複数巡回路の状態と近傍操作 |
| `examples/` | 焼きなましの使用例 `sa_tsp.cpp`、TSPLIB の入力、可視化スクリプト |
| `test/` | 近傍操作、状態の不変条件、探索結果の確認 |

各関数の使用例は [利用ガイド](how_to_use_tsp.md) に記載しています。

## TspProblem

```cpp
template<class Cost>
class TspState;
template<class Cost>
class TspTwoOptMove;
template<class Cost>
class TspOrOptMove;
template<class Cost>
class TspDoubleBridgeMove;

template<class EdgeCost>
class TspProblem {
public:
    using Cost = remove_cvref_t<invoke_result_t<const EdgeCost&, int, int>>;

    TspProblem(int node_count, EdgeCost edge_cost);
    TspProblem(const TspProblem&) = delete;
    TspProblem& operator=(const TspProblem&) = delete;
    TspProblem(TspProblem&&) = delete;
    TspProblem& operator=(TspProblem&&) = delete;

    int node_count() const;
    Cost edge_cost(int u, int v) const;
    TspState<Cost> make_state(vector<int> order) const;
    TspState<Cost> make_nearest_neighbor_state(int start) const;
    TspState<Cost> make_nearest_neighbor_state(
        int start, span<const int> nodes_to_visit) const;
};
```

`TspProblem` は辺のコスト関数をテンプレート引数 `EdgeCost` の型のまま値で保持します。探索や焼きなましの複数のレプリカから、構築済みの問題を読み取り専用で共有できます。

状態、候補表、近傍操作は問題のアドレスで作成元を識別します。アドレスを固定するため `TspProblem` のコピーと移動を禁止しています。探索が終了し、その問題を参照する全ての状態と候補表を破棄した後に、問題を破棄します。

ラムダ式から大きな距離表を参照する場合は参照キャプチャを使用できます。参照先は問題より長く保持し、探索中は内容と参照先のアドレスを維持します。

## TspCandidates

候補表を使用する探索では `make_tsp_candidates` で事前に候補表を作成します。

```cpp
template<class EdgeCost>
TspCandidates make_tsp_candidates(
    const TspProblem<EdgeCost>& problem,
    int requested_count);

class TspCandidates {
public:
    int node_count() const;
    int candidate_count() const;
    span<const int> operator[](int node) const;
};
```

点ごとの候補列を長さ `node_count * candidate_count` の 1 次元配列に格納します。各点の候補数は `min(requested_count, node_count - 1)` です。

各点について、自身を除く全点を点番号順に走査します。辺のコストが小さい順に `candidate_count` 個まで保持し、コストが等しい場合は点番号の小さい方を優先します。`edge_cost` の呼び出し回数は `O(n²)` です。

候補数の異なる表も同じ問題から作成できます。探索関数は各表の作成元を確認し、別の問題の表を渡した場合は `invalid_argument` を送出します。

## TspState

```cpp
template<class Cost>
class TspState {
public:
    using CostType = Cost;

    int node_count() const;
    int size() const;
    const vector<int>& order() const;
    int position(int node) const;
    Cost total_cost() const;

    template<class EdgeCost>
    TspTwoOptMove<Cost> make_two_opt(
        const TspProblem<EdgeCost>& problem,
        int left, int right) const;
    template<class EdgeCost>
    TspOrOptMove<Cost> make_or_opt(
        const TspProblem<EdgeCost>& problem,
        int first, int length, int after, bool reversed) const;
    template<class EdgeCost>
    TspDoubleBridgeMove<Cost> make_double_bridge(
        const TspProblem<EdgeCost>& problem,
        int cut1, int cut2, int cut3, int cut4) const;

    template<class EdgeCost, class Move>
    void apply(const TspProblem<EdgeCost>& problem, const Move& move);
    template<class EdgeCost>
    void reset(
        const TspProblem<EdgeCost>& problem,
        vector<int> order);
};
```

`TspState` は巡回順とその逆引き用の配列、コスト、作業領域に加え、作成元の問題と状態の更新回数を保持します。

```cpp
vector<int> order;
vector<int> position;
Cost total_cost;
vector<int> work_buffer;
const void* problem_id;
uint64_t revision;
```

巡回路の状態は次の条件を満たします。

- `order` には巡回路上の各点が 1 回ずつ現れ、`order[0]` が開始点になります。各近傍操作は開始点を維持します。
- 各位置 `i` で `position[order[i]] == i` が成り立ちます。巡回路に含まれない点の位置は `-1` です。
- 2 点以上では `total_cost` に末尾から先頭に戻る辺を含む全辺のコストを計上します。2 点の場合は往復 2 辺、1 点の場合は自己ループを評価せずコストを 0 とします。

`make_state` は 1 点以上の巡回順を受け取り、各点の位置とコストを計算します。`reset` は指定した巡回順でこれらを再計算し、更新回数を 1 増やします。

状態をコピーすると巡回順、位置、コスト、作業領域が複製されます。コピーの時間は `O(n)` で、問題と候補表は共有できます。近傍操作は作成元の状態にのみ適用できます。適用までは作成元の状態を保持してください。代入または `reset` の前に作成した操作は無効になります。

## 最近傍法による初期解

```cpp
auto state = problem.make_nearest_neighbor_state(start);
auto partial_state =
    problem.make_nearest_neighbor_state(start, nodes_to_visit);
```

最近傍法では現在の点からの辺のコストが最小の未訪問点を順に選択します。コストが等しい場合は点番号の小さい方を優先します。

`start` だけを指定すると `[0, node_count)` の全点を巡回します。部分巡回路を作成する場合は `start` を除く訪問点を `nodes_to_visit` に指定します。部分巡回路は複数巡回路の初期化にも使用できます。

## 近傍操作

`make_two_opt`、`make_or_opt`、`make_double_bridge` は巡回路を変更せずにコストの差分を求めます。採択した操作を `apply` に渡すと巡回路とコストを更新します。

```cpp
auto two_opt = state.make_two_opt(problem, left, right);
if (two_opt.valid()) {
    auto next_cost = state.total_cost() + two_opt.delta();
    if (accept(next_cost)) state.apply(problem, two_opt);
}

auto or_opt = state.make_or_opt(
    problem, first, length, after, reversed);

auto double_bridge = state.make_double_bridge(
    problem, cut1, cut2, cut3, cut4);
```

近傍操作は作成元の問題と状態、作成時の更新回数 `revision`、変更前の添字、コストの差分を保持します。`delta()` は適用後のコストから適用前のコストを引いた値を返します。

添字や区間長が操作の条件を満たさない場合は無効な操作を返します。デフォルト構築した操作も無効です。`valid()` で確認できるのは作成時の有効性です。`apply` は作成元の問題、状態、更新回数を確認するため、作成後に状態を更新した場合は例外になります。

添字は操作を作成した時点の巡回順に対応します。

- **2-opt** は区間 `[left, right]` を反転します。閉路の先頭をまたぐ区間を反転したい場合は、開始点を含まない側の区間を指定します。
- **Or-opt** は区間 `[first, first + length)` を、変更前の位置 `after` の直後に移動します。反転 Or-opt では移動する区間の向きも反転します。
- **Double Bridge** は位置 `cut1 < cut2 < cut3 < cut4` から出る 4 辺を削除します。配列の先頭から順に区間を `S0, S1, S2, S3, S4` とすると、操作後の順序は `S0, S3, S2, S1, S4` です。`cut4 == n - 1` の場合は `S4` が空になります。

いずれの操作でも移動する範囲は開始点を含みません。2-opt は区間を配列内で反転し、Or-opt は `rotate` で区間を移動します。Double Bridge は状態に確保した作業領域を再利用します。各操作は位置が変化した区間についてのみ `position` を更新します。

| 操作 | コストの差分 | 適用 |
|---|---:|---:|
| 2-opt | `O(1)` | 反転する区間の長さに比例 |
| Or-opt | `O(1)` | 移動する区間と移動先の間の長さに比例 |
| 反転 Or-opt | `O(1)` | 移動する区間と移動先の間の長さに比例 |
| Double Bridge | `O(1)` | `cut4 - cut1` に比例 |

## 局所探索

```cpp
struct TspLocalSearchOptions {
    int64_t max_evaluated_moves = -1;
    bool first_improvement = true;
};

struct TspLocalSearchResult {
    int64_t evaluated_moves;
    int64_t applied_moves;
    bool locally_optimal;
};

auto result = tsp_two_opt_local_search(
    problem, candidates, state, options);
```

`tsp_two_opt_local_search` は引数 `state` の巡回路を直接更新します。

- `first_improvement == true` の場合は、コストが減少する最初の操作を適用します。
- `first_improvement == false` の場合は、一巡のうちコストの減少量が最大の操作を適用します。
- `max_evaluated_moves` は操作の評価回数の上限です。`-1` は上限なしを意味します。上限で探索を打ち切ると `locally_optimal` は `false` になります。
- 4 点未満の巡回路では評価回数と適用回数がともに 0 になります。

候補表を省略すると全ての 2-opt が評価対象です。候補表を指定した場合は、各点 `u` とその候補点 `v` を隣接させる 2-opt が対象です。どちらも、指定した評価対象に改善操作がないことを確認すると `locally_optimal` が `true` になります。

候補表を使用する場合は、`u` と `v` のそれぞれから次の点に向かう 2 辺を削除する操作と、前の点に向かう 2 辺を削除する操作を評価します。全点の巡回路で候補数を `n - 1` にすると、端点を共有しない全ての辺の組が対象になります。部分巡回路では、その巡回路に含まれる候補点だけを評価します。

全探索で `first_improvement == true` の場合は、改善操作を適用した後もその位置から一巡を続けます。一巡中に改善操作がなければ探索を終了します。

候補表を指定し `first_improvement == true` とした場合は、内部クラス `TourSearch` が次のように探索します。

1. 巡回路を配列に格納し、全ての点を再評価用のキューに追加します。キューから取り出した点について、削除する辺よりもコストが小さい候補辺を順に評価します。候補はコストの昇順なので、この条件を満たさなくなった時点で走査を終了できます。
2. 改善操作を適用するときは、反転する区間とその補集合のうち短い方を反転し、隣接関係が変化した辺の端点をキューに追加します。対称 TSP ではどちらの区間を反転しても、巡回路を構成する無向辺の集合は同じです。
3. キューが空になったら全点を再度キューに追加し、候補辺のコストによる打ち切りを解除します。改善操作を適用しない一巡が完了するまで繰り返します。
4. 元の開始点が先頭になるように巡回順を回転し、結果を `state` に反映します。

## 反復局所探索

```cpp
struct TspIteratedLocalSearchOptions {
    double time_limit_ms = -1;
    int64_t max_iterations = -1;
    int max_kick_length = 50;
    double temperature_ratio = 0.5;
    uint32_t seed = 23;
    bool use_three_opt = true;
};

struct TspIteratedLocalSearchResult {
    int64_t iterations;
    int64_t accepted_iterations;
    int64_t applied_moves;
    double elapsed_ms;
    TspSearchStopReason stop_reason;
};

auto result = tsp_iterated_local_search(
    problem, candidates, state, options);
```

最初に `TourSearch` で 2-opt、Or-opt、3-opt の局所探索を行います。以降の各反復では次の操作を行います。

1. 巡回路上の位置をランダムに選択し、その直後に連続する 3 区間を Double Bridge で逆順に並べ替えます。各区間の長さは 1 以上で、`max_kick_length` と `(n - 1) / 3` の整数部分の小さい方を上限とします。
2. 交換した 4 辺の端点をキューに追加し、キューが空になるまで局所探索を行います。
3. 反復前よりコストが減少した場合と等しい場合は、その巡回路を採択します。増加量が `Δ` で温度 `T` が正の場合は確率 `exp(-Δ / T)` で採択します。
4. 棄却時はこの反復で記録した区間の反転を逆順に適用し、最後に Double Bridge を取り消して反復前の巡回路に戻します。

Or-opt はキューから取り出した点を先頭とする長さ 1 から 3 の区間を、その点の候補点と隣の点の間に移動します。区間の向きを維持する場合と反転する場合の両方を評価します。移動には 2 回または 3 回の区間反転を使用します。

`use_three_opt == true` の場合は 3-opt も評価します。キューから取り出した点 `a` を起点に辺 `{a, b}`, `{c, d}`, `{e, f}` を削除し、辺 `{a, c}`, `{d, e}`, `{f, b}` を追加します。

- `b` は `a` の隣の点、`c` は `a` の候補点、`d` は `c` の隣の点、`e` は `d` の候補点、`f` は `e` の隣の点です。
- `cost(a, b) - cost(a, c)` が正で、さらに `cost(c, d) - cost(d, e)` を加えた値も正になる組だけを評価します。候補はコストの昇順なので、この条件を満たさなくなった時点で候補の走査を終了します。
- 点 `a` から `b` へ進む向きで `d` が `c` の直後にある場合は、まず辺 `{a, c}`, `{b, d}` を追加する 2-opt を適用します。続く 2-opt で辺 `{b, d}`, `{e, f}` を削除します。
- 同じ向きで `d` が `c` の直前にある場合は、`b` から `d` までの区間を `e` と `f` の間に移動します。区間長に上限はなく、区間の向きを維持する場合と反転する場合の両方を評価します。

計測時には 3-opt を有効にすると反復 1 回あたりの時間が約 2.5 倍になり、同じ制限時間では 1000 点を超える入力とクラスタ状の入力でコストが減少しました。基準には既知の最適値または同じ探索を 20 秒以上実行して得たコストを使用しました。11 入力の平均超過率は 100 ms で 0.67% から 0.54% に、1000 ms で 0.27% から 0.19% に低下しました。

初期温度は `temperature_ratio` と初期巡回路の辺の平均コストの積です。温度は時間または反復回数の上限まで線形に下げます。最良解より高コストの巡回路を初めて採択する際に、それまでの最良の巡回順を保存します。終了時には最良の巡回路を `state` に反映します。

実行時間と反復回数の少なくとも一方に上限を指定します。巡回路が 4 点未満の場合と候補数が 0 の場合は入力状態をそのまま返します。4 点以上でも 8 点未満の場合、または `max_iterations == 0` の場合は初回の局所探索で終了します。

`solve_tsp` は各点につき最大 10 点の候補表と最近傍法の初期解を作成し、残り時間で反復局所探索を行います。

## 非対称 TSP の反復局所探索

`atsp_iterated_local_search` は辺の向きごとにコストを評価します。入力の問題を `TspProblem`、候補表を `AtspCandidates` で表し、引数 `order` の巡回順を直接更新します。

```cpp
struct AtspCandidates {
    int node_count;
    int candidate_count;
    vector<int> out;   // 各点から出る辺の行き先をコストの昇順に格納
    vector<int> in;    // 各点に入る辺の始点をコストの昇順に格納
    const void* problem_id;
};

TspProblem problem(node_count, edge_cost);
auto candidates = make_atsp_candidates(problem, requested_count);
vector<int> order = problem.make_nearest_neighbor_state(start).order();
AtspIteratedLocalSearchOptions options;
options.time_limit_ms = time_limit_ms;
auto result = atsp_iterated_local_search(problem, candidates, order, options);
```

各点について、自身を除く全点への辺を 1 回ずつ評価します。辺 `u → v` のコストを点 `u` の出辺と点 `v` の入辺の両方に使用し、それぞれコストの小さい順に候補を保持します。

`atsp_detail::TourSearch` は向きのある巡回路を配列に格納します。反復の手順、採択条件、最良解の保存方法は対称 TSP の反復局所探索と共通です。局所探索では次の近傍操作を評価します。

- **隣接する 2 区間の交換**では、巡回順に現れる辺 `a → b`, `c → d`, `e → f` を削除し、辺 `a → d`, `e → b`, `c → f` を追加します。点 `b` から `c` までの区間と点 `d` から `e` までの区間は、内部の辺の向きを維持したまま入れ替わります。点 `a`, `c` の出辺候補から調べる場合と、点 `b`, `f` の入辺候補から調べる場合の両方を評価します。
- **Or-opt** は長さ 1 から 3 の区間を移動します。移動先は区間の先頭に入る辺の候補と末尾から出る辺の候補から選択し、隣接する 2 区間の交換として適用します。
- **2-opt** は 2 辺を交換して、その間の区間を反転します。コストの差分には、区間内の各辺 `u → v` について逆向きとの差 `cost(v, u) - cost(u, v)` を加えます。
- **反転 Or-opt** は長さ 2 または 3 の区間を反転して移動します。

区間の交換では、削除する 3 辺で巡回路を 3 区間に分割します。最長の区間の配列上の位置を維持して残りの 2 区間を交換すると、適用時間は短い 2 区間の長さの和に比例します。

区間の反転に備え、各点について次の点への辺とその逆向きの辺のコスト差を保持します。2-opt の評価ではこの差を反転区間内で合計するため、区間長を `max_reverse_length` 以下に制限します。反転後は、区間内部の各辺について保持していた差の符号を反転します。

対称な 1000 点の入力を 1000 ms で探索した計測では、反転を含む操作を加えるとコストの超過率が 0.82% から 0.16% に低下しました。各点に開始位置と終了位置を指定した入力と乱数で生成したコスト行列では、明確な差はありませんでした。反転区間の上限が 200 と 400 の場合は同程度で、1000 の場合は超過率が上昇しました。

### 割当問題による初期解

`solve_atsp` は最近傍法で初期解を作成します。点数 `n` が 8 以上 2000 以下で、ハンガリアン法の推定実行時間が残り時間の半分以下の場合は、次の条件で別の初期解も求めます。

1. 互いに最近傍となる組に含まれる点の数を求めます。この点数が全体の 1/4 を超える場合は最近傍法の初期解を使用します。対称に近い問題では長さ 2 の閉路が多くなり、併合時のコストの増加が大きくなる場合があるためです。
2. 各点の入辺と出辺を 1 本ずつ選ぶ割当問題を解き、辺のコストの総和が最小となる閉路の集合を求めます。閉路数が点数の 1/8 を超える場合は最近傍法の初期解を使用します。
3. 最大の閉路に残りの閉路を点数の多い順に併合します。併合時には 2 つの閉路から 1 辺ずつ選び、行き先を交換したときのコストの増加が最小となる組を採用します。完成した巡回路を最近傍法の初期解と比較し、コストの小さい方から探索します。

乱数で生成した 1000 点のコスト行列では、最近傍法の初期解のコストは 62043、割当問題から得た巡回路のコストは 17022 でした。この入力での割当問題の最適値は 15918 です。

### 始点に戻らない経路

`solve_atsp_path` は番号 `node_count` のダミー点を追加し、始点と終点の条件を辺のコストで表します。

- ダミー点から出る辺は、指定した始点に向かう辺のコストを 0 とします。
- 終点を指定した場合は、その終点からダミー点に向かう辺のコストを 0 とします。終点が任意の場合は、全ての元の点からダミー点に向かう辺のコストを 0 とします。
- ダミー点に接続する残りの辺には、元の点だけを通るどの経路のコストよりも大きな値を設定します。元の点同士の辺には元のコストを使用します。

初期巡回路は始点と終点の条件を満たします。探索はこの初期解以下のコストの巡回路を返すため、最良解も条件を満たします。ダミー点の直後を先頭として巡回順を回転し、ダミー点を除いた列を経路として返します。

## 誘導局所探索

`tsp_guided_local_search` は `TourSearch` で 2-opt を評価します。ペナルティは辺の選択と操作の採択に使用し、最良解の比較には元の辺のコストを使用します。

1. 元の辺のコストで局所探索を行い、最初の巡回路の改善を終えた時点で `lambda = penalty_ratio * total_cost / state.size()` を求めます。
2. 巡回路の各辺について `edge_cost(u, v) / (1 + penalty(u, v))` を計算します。値が最大の全ての辺についてペナルティを 1 増やし、その端点をキューに追加します。
3. ペナルティ付きのコストで、キューが空になるか実行時間または評価回数の上限に達するまで局所探索を行います。各回の局所探索が終了した段階で元のコストを比較し、最良解を更新します。
4. 上限に達するまで手順 2 と 3 を繰り返します。終了時には最良の巡回路を `state` に反映します。

ペナルティを増やした辺を削除する操作は、変更前より採択されやすくなります。この操作を辺の両端から評価できるように、点 `u` の近傍には、`u` の候補点と `u` を候補に含む点の両方を登録します。ペナルティ付きのコストで評価する間は、候補辺の長さによる打ち切りを解除します。

評価に使用するコストは、元の巡回路のコスト `true_cost` に、巡回路上の各辺のペナルティの総和と係数 `lambda` の積を加えた値です。[Voudouris と Tsang による誘導局所探索](https://doi.org/10.1016/S0377-2217(98)00099-X)

```text
true_cost + lambda * Σ penalty(edge)
```

```cpp
struct TspGuidedLocalSearchOptions {
    int64_t max_penalty_rounds = -1;
    int64_t max_evaluated_moves = -1;
    double time_limit_ms = -1;
    long double penalty_ratio = 0.3L;
};

enum class TspSearchStopReason {
    completed,
    time_limit,
    evaluated_move_limit,
    penalty_round_limit,
    iteration_limit,
};

struct TspGuidedLocalSearchResult {
    int64_t evaluated_moves;
    int64_t applied_moves;
    int64_t penalty_rounds;
    int64_t best_updates;
    double elapsed_ms;
    TspSearchStopReason stop_reason;
};

auto result = tsp_guided_local_search(
    problem, candidates, state, options);
```

最良解を保持する必要が生じた時点で、巡回順を探索用の作業領域に保存します。終了後の巡回順は `state.order()`、コストは `state.total_cost()` から取得できます。辺のペナルティは探索ごとにハッシュ表で管理し、更新した無向辺だけを登録します。

実行時間とペナルティの更新回数の少なくとも一方に上限が必要です。候補の評価回数にも上限を設定できます。

- `time_limit_ms == 0` または `max_evaluated_moves == 0` の場合は入力状態をそのまま返します。両方が 0 の場合、停止理由は時間制限です。
- `max_penalty_rounds == 0` の場合は元のコストによる初回の局所探索で終了します。
- 巡回路が 4 点未満の場合と候補数が 0 の場合は、辺を評価せずに終了します。

時間は探索関数内で計測します。問題、候補表、初期状態の準備に要した時間は対象外です。各ペナルティ更新の開始時と局所探索で 128 点を処理するごとに経過時間を確認するため、次の確認までに制限時間を超える場合があります。

## 焼きなましの初期状態

`improve_tsp_initial_state` は `TspInitialStateOptions::search` に対応する探索を初期巡回路に適用します。

```cpp
enum class TspInitialSearch {
    none,
    two_opt,
    guided_local_search,
    iterated_local_search = 4,
};

struct TspInitialStateOptions {
    TspInitialSearch search = TspInitialSearch::none;
    TspLocalSearchOptions local_search;
    TspGuidedLocalSearchOptions guided_local_search;
    TspIteratedLocalSearchOptions iterated_local_search;
};
```

最近傍法による巡回路を改善してから、焼きなましの `State` を構築します。

```cpp
auto tsp = problem.make_nearest_neighbor_state(start);
improve_tsp_initial_state(problem, candidates, tsp, initial_options);
return State(&problem, &candidates, move(tsp), seed);
```

既定値の `none` では作成した巡回路をそのまま使用します。探索を指定した場合は、選択した手法に応じて時間、候補の評価回数、反復回数、ペナルティの更新回数の上限を設定できます。候補表は初期解の改善と焼きなましで共有します。

実行時間に依存せずに比較する場合は、候補の評価回数やペナルティの更新回数を指定します。全体に制限時間がある場合は、初期解の改善に割く時間が長いほど焼きなましに使用できる時間が減ります。初期解用の上限を変更するときは、2-opt を基準に、焼きなまし終了時のコストと反復回数を比較します。

`sa_run` には初期化済みの状態と残り時間を渡します。残り時間は、プログラム全体の制限時間から問題と候補表の作成、初期解の改善、状態の構築に要した時間を引いた値です。

複数のレプリカで同じ初期巡回路を使用する場合は、一度改善した状態を `O(n)` で複製し、焼きなましの乱数シードをレプリカごとに指定します。異なる初期巡回路を使用する場合は各状態に初期解用の上限を設定できます。初期解を個別に改善すると、レプリカ間で巡回順が似る場合があります。

## 焼きなましでの操作の採択

問題固有の `State` に確定済みの `TspState` と採択前の近傍操作を保持します。

```cpp
class State {
    const Problem* problem;
    const TspCandidates* candidates;
    TspState<Cost> tsp;
    TspTwoOptMove<Cost> two_opt;
    TspOrOptMove<Cost> or_opt;
    TspDoubleBridgeMove<Cost> double_bridge;
    ScoreType score;
};
```

`changed.type` は作成する操作の種類を示します。各反復では次の順序で状態を更新します。

1. `modify` の開始時に操作の種類を設定し、確定済みの `TspState` から近傍操作を作成します。無効な操作の場合は `is_valid = false` を設定します。
2. 有効な操作をメンバに保存し、提案後のスコアを計算します。単一巡回路のコストを最小化する場合は `tsp.total_cost() + move.delta()` が提案後のスコアです。
3. 採択時は `advance` で保存した操作を適用します。棄却時には巡回路が提案前の状態にあるため、`rollback` で巡回順を復元する処理は不要です。

初期化済みの状態を渡す `sa_run` では `State` のデフォルト構築は不要です。`sa_multi_run` と `replica_run` は状態をデフォルト構築した後に `State::init(seed)` を呼び出します。

問題と候補表は全レプリカで共有し、`TspState`、乱数生成器、作業領域、採択前の操作はレプリカごとに保持します。状態を複製した後は、複製先の `TspState` から `modify` で操作を作成してから `advance` で適用します。

## 複数巡回路

`MultipleTspState` は各点の所属と位置、巡回路ごとの訪問順とコストを 1 つの状態で管理します。

```cpp
template<class Cost>
class MultipleTspState {
public:
    int node_count() const;
    int route_count() const;
    const vector<int>& route(int route_id) const;
    int route_of(int node) const;
    int position(int node) const;
    Cost route_cost(int route_id) const;
};
```

- 各巡回路の先頭は、その巡回路に属する固定デポです。
- デポを含む全ての点は、いずれかの巡回路に 1 回ずつ現れます。デポだけの巡回路も指定できます。
- `route_of(node)` は点の所属する巡回路を、`position(node)` はその巡回路内の位置を返します。

巡回路間の操作ではデポを除く区間を指定します。`make_block_shift` は移動元の区間 `[first, first + length)` を、別の巡回路の位置 `after` の直後に挿入します。`make_block_swap` は異なる巡回路から 1 区間ずつ選んで交換します。どちらも操作前後のコスト差を対象の巡回路ごとに取得できます。

目的関数は利用側で選択します。全巡回路の総距離と各巡回路の距離の二乗和は、変化した 2 つの巡回路のコストから `O(1)` で更新できます。巡回路数を `k` とすると、最長の巡回路の距離は全巡回路を走査すれば `O(k)` で求まります。利用側でコストを多重集合に保持する場合は `O(log k)` で更新できます。

各点の初期割り当てには、問題に応じて K-means、個数制約付きの K-means、最近傍のデポへの割り当てなどを使用できます。

`examples/sa_tsp.cpp` は TSPLIB の入力を使用する焼きなましの例で、実行時の巡回路数は 1 です。各状態が全点対のペナルティ表を保持し、巡回路の距離の二乗和を最小化します。ライブラリを使用する際は、入力形式、目的関数、温度、近傍操作の選択確率を問題に応じて設定できます。

## 入力条件

- 点数 `node_count` は 1 以上で、点番号は `[0, node_count)` の範囲に含まれます。`solve_atsp_path` では 2 点以上が必要です。
- 初期巡回順と `nodes_to_visit` にはそれぞれ重複のない点列を指定します。`start` は有効な点番号とし、`nodes_to_visit` から除きます。
- 辺のコスト関数は `const` で呼び出すことができ、有限かつ 0 以上の値を返す必要があります。対称 TSP の探索と `TspState` の近傍操作では両方向のコストが等しいことも条件です。
- `Cost` には、総コスト、差分、`total_cost + delta` の各中間値を表現できる符号付きの算術型を使用します。
- 候補数は 0 以上です。時間と回数の上限には `-1` または 0 以上の値を指定し、`-1` はその上限を無効とします。
- `penalty_ratio` と `temperature_ratio` は有限かつ 0 以上、`max_kick_length` は正の整数とします。
- 誘導局所探索では、実行時間とペナルティの更新回数の少なくとも一方に上限を指定します。候補の評価回数は追加の上限として指定できます。
- 反復局所探索では、実行時間と反復回数の少なくとも一方に上限を指定します。

点数、巡回順、探索設定などの引数が条件を満たさない場合は `invalid_argument` を送出します。近傍操作の添字や区間長が無効な場合は、繰り返し候補を評価できるように `valid()` が `false` の操作を返します。辺のコストと算術演算の範囲は呼び出し側で保証します。

## 計算量

全点を含む対称 TSP の巡回路について、点数を `n`、各点の候補数を `c` とします。`s_i` は適用した操作ごとに、反転または移動により位置を更新する範囲の長さを表します。

| 処理 | 時間 | 追加メモリ |
|---|---:|---:|
| `TspState` の作成 | `O(n)` | `O(n)` |
| `TspState` のコピー | `O(n)` | `O(n)` |
| 最近傍法による初期解 | `O(n²)` | `O(n)` |
| 候補表の作成 | `edge_cost` の呼び出しが `O(n²)` 回、候補の挿入を含めると最悪 `O(n²c)` | `O(nc)` |
| 2-opt の評価 | `O(1)` | `O(1)` |
| 2-opt の適用 | `O(s_i)` | `O(1)` |
| Or-opt の評価 | `O(1)` | `O(1)` |
| Or-opt の適用 | `O(s_i)` | `O(1)` |
| Double Bridge の評価 | `O(1)` | `O(1)` |
| Double Bridge の適用 | `O(s_i)` | `TspState` 内の `O(n)` の作業領域 |
| 全ての 2-opt の一巡 | `O(n²)` | `O(1)` |
| 各点の候補表を使用する `TourSearch` の構築 | `O(nc²)` | `O(nc)` |
| `TourSearch` での 1 点の候補評価 | 2-opt と Or-opt は `O(c)`、3-opt を含む場合は `O(c²)` | `O(1)` |
| `TourSearch` での区間の反転 | 区間とその補集合のうち短い方の長さに比例 | `O(1)` |

全探索版の 2-opt は走査回数を `W` とすると `O(Wn² + Σs_i)` 時間です。候補表とキューを使用する 2-opt は、キューから取り出した点の総数を `V` とすると `O(Vc + Σs_i)` 時間です。`V` には最終確認の走査で処理した点も含めます。

反復局所探索では各反復の Double Bridge に `O(max_kick_length)` 時間かかります。その反復でキューから `V` 点を取り出した場合、続く局所探索の時間は 3-opt 有効時に `O(Vc² + Σs_i)`、無効時に `O(Vc + Σs_i)` です。棄却時には区間の反転と Double Bridge を逆順に適用します。最良の巡回順の保存には 1 回あたり `O(n)` 時間かかります。

誘導局所探索では候補を相互に登録するため、1 点の候補数が `c` を超える場合があります。局所探索で調べた候補の総数を `H`、ペナルティの更新回数を `R`、最良解の保存回数を `B`、ペナルティを加えた異なる辺の数を `P` とします。探索の準備後は、ハッシュ表の操作を平均 `O(1)` として `O(V + H + Σs_i + Rn + (B + 1)n)` 時間、追加メモリは `O(nc + P)` です。`Rn` は各反復でペナルティを加える辺を選択するための走査に対応します。

同じ確定状態を `r` 個のレプリカに複製する時間とメモリは `O(rn)` です。各レプリカが独立に誘導局所探索を実行する場合は、それぞれの探索時間と作業領域が必要です。距離行列を事前に作成する場合は別途 `O(n²)` の時間とメモリを使用します。

複数巡回路で長さ `L` の区間を移動する操作の評価時間は `O(L)` です。区間を交換する場合は 2 区間の長さの和に比例し、各区間を長さ 1 から 3 に制限すれば定数時間です。どちらも適用時間は対象の巡回路の長さの和に比例します。

## テストの対象

- `test/tsp_test.cpp` は各操作のコストの差分を巡回路全体の再計算と照合し、各点の位置、開始点、点の一意性、複数巡回路での所属を確認します。反復局所探索と `solve_tsp` の結果は小規模な入力の全列挙と照合します。
- `test/atsp_test.cpp` は入辺候補と出辺候補の順序、部分巡回路、初期解を確認します。小規模な入力では、巡回路のコストを全列挙の結果と照合します。始点に戻らない経路についても、終点を固定する場合と任意とする場合の両方で同じ照合を行います。
- `test/tsp_sa_test.cpp` は初期化済みの状態を `sa_run` に渡し、初期巡回路の 2-opt と焼きなましでの近傍操作の遅延適用を確認します。
