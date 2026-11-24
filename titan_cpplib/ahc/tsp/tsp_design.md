# AHC 向け TSP ライブラリの設計

## 結論

ライブラリは、読み取り専用の問題データを保持する `TspProblem` と、現在の巡回路を保持する `TspState` を中心に構成します。

- `TspProblem`: 点の数と辺のコスト関数を保持します。
- `TspCandidates`: 近傍の候補表を保持します。
- `TspState`: 巡回順、各点の位置、現在のコスト、作業領域を保持します。
- 局所探索、反復局所探索、誘導局所探索、辺にペナルティを加える比較用の探索: 渡された `TspState` を直接更新します。
- 焼きなまし: 問題固有の `sa::State` が `TspState` と未適用の操作を保持します。

`sa_tsp.cpp` からは正しい差分計算と巡回路の更新だけを共通化します。`tsp2.py` の探索は、標準的な誘導局所探索とは別の比較対象として残します。

小規模な問題を厳密に解く `titan_cpplib/alg/traveling_salesman_problem.cpp` は役割が異なるため、そのまま残します。新しいライブラリは大規模な問題向けの近似解法だけを扱います。

## 対象とする問題

`TspState` と、名前が `tsp_` で始まる探索は対称 TSP だけを対象とします。

- 巡回路は対象とする各点を一度ずつ訪問する閉路です。
- `edge_cost(u, v) == edge_cost(v, u)`
- 辺のコストは有限かつ 0 以上です。
- 三角不等式を満たす必要はありません。

非対称 TSP では、2-opt で区間を反転すると区間内の全ての辺の向きが変化します。境界の辺だけを使用してコストの差分を定数時間で求めることができないため、`TspState` の近傍操作と対称版の探索は対称 TSP だけを対象とします。非対称 TSP には `atsp_iterated_local_search.cpp` の探索を使用します。

## ファイル構成

```text
titan_cpplib/ahc/tsp/
├── tsp.cpp
├── tsp_symmetric_moves.cpp
├── tsp_local_search.cpp
├── tsp_iterated_local_search.cpp
├── atsp_iterated_local_search.cpp
├── tsp_guided_local_search.cpp
├── tsp_edge_penalty_search.cpp
├── tsp_initial_state.cpp
├── multiple_tours.cpp
├── how_to_use_tsp.md
├── examples/
└── test/
```

- `tsp.cpp`: `TspProblem`、`TspCandidates`、`TspState`、最近傍法による初期解
- `tsp_symmetric_moves.cpp`: 2-opt、Or-opt、反転 Or-opt、Double Bridge
- `tsp_local_search.cpp`: 全探索または候補制限付きの 2-opt 局所探索と、探索関数が共有する内部クラス `TourSearch`
- `tsp_iterated_local_search.cpp`: 反復局所探索と、巡回順を 1 回の呼び出しで求める `solve_tsp`
- `atsp_iterated_local_search.cpp`: 非対称コストの反復局所探索と、`solve_atsp`、`solve_atsp_path`
- `tsp_guided_local_search.cpp`: 標準的な誘導局所探索
- `tsp_edge_penalty_search.cpp`: 現在の `tsp2.py` の処理順を再現した比較用の探索
- `tsp_initial_state.cpp`: SA の初期状態の改善方法を選択する補助関数
- `multiple_tours.cpp`: 固定デポがある複数巡回路の状態と巡回路間の操作
- `examples/`: 参照元の `sa_tsp.cpp`、`tsp2.py`、TSP の入力、可視化処理
- `test/`: コストの差分と不変条件の自動テスト

焼きなましの反復処理は既存の `ahc/sa/sa.cpp` と重複するため、`tsp_sa.cpp` は作成しません。

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

`TspProblem` は辺のコスト関数を元の型のまま値で保持し、`function` は使用しません。構築後は変更せず、複数の探索やレプリカから同時に読み取ることができるようにします。

状態と操作の確認に問題のアドレスを使用するため、コピーと移動を禁止してアドレスを固定します。問題の寿命は、その問題から作成した全ての状態と探索の寿命より長くする必要があります。

辺のコスト関数が距離表などを参照する場合は、参照先の寿命も問題より長くし、探索中に変更や再確保をしないようにします。大きな距離表はラムダ式へ値でキャプチャしないようにします。

## TspCandidates

候補表は問題の構築時に暗黙的には作成しません。必要な探索だけが明示的に作成します。

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

候補表は長さ `node_count * candidate_count` の 1 次元配列で保持します。実際の候補数は `min(requested_count, node_count-1)` とします。

各点について、コストの昇順に並べた上位 `candidate_count` 個を保持し、ほかの点を点番号の昇順に走査して挿入します。コストが同じ場合は点番号が小さい方を先にします。`edge_cost` の呼び出しは `O(n²)` 回で、上位に入る点だけを挿入します。

`TspCandidates` は作成元の問題を識別し、探索関数へ別の問題と組み合わせて渡した場合は例外を投げます。同じ問題から候補数の異なる表を複数作成することができます。

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

次のデータを非公開で保持します。

```cpp
vector<int> order;
vector<int> position;
Cost total_cost;
vector<int> work_buffer;
const void* problem_id;
uint64_t revision;
```

不変条件は次のとおりです。

- `order` は巡回路に含まれる点を重複なく並べたもので、末尾に開始点を再度含めません。
- `position[order[i]] == i`
- 巡回路に含まれない点の `position` は `-1` です。
- 最後の辺は `order.back()` から `order.front()` への辺です。
- `total_cost` は差分を適用した後の現在のコストです。
- `order[0]` は開始点として固定します。

`TspState` は 1 個以上の点を含みます。要素数が 1 の場合のコストは 0 とし、`edge_cost(v, v)` は呼び出しません。要素数が 2 の場合は往路と復路の 2 辺のコストを計上します。

状態は `TspProblem::make_state` からだけ作成します。巡回順、位置、コストは外部から個別に変更できないようにします。

`reset` は巡回順全体を置き換え、位置とコストを再計算します。探索関数が終了時に探索結果の巡回順を `state` へ反映する用途で使用し、それまでに作成した未適用の操作を全て無効にします。

状態のコピーは巡回順、位置、コスト、作業領域を複製するため `O(n)` です。問題と候補表は複製しません。コピー先は独立した新しい確定状態となり、コピー元で作成した操作は適用できません。代入の前に代入先から作成した操作も無効になります。未適用の操作を作成してから適用するまでの間は、作成元の状態を移動または破棄してはいけません。

## 最近傍法による初期解

```cpp
auto state = problem.make_nearest_neighbor_state(start);
auto partial_state =
    problem.make_nearest_neighbor_state(start, nodes_to_visit);
```

全ての点を対象とする版は、`start` を先頭にして `[0, node_count)` の全ての点を巡回します。

部分集合を対象とする版では、`nodes_to_visit` に開始点を含めず、開始点と指定した点だけからなる巡回路を作成します。複数巡回路の初期化にも利用できます。コストが同じ場合は点番号が小さい点を選択し、結果を決定的にします。

## 近傍操作

各操作は、状態を変更せずにコストの差分を求める処理と、採択後に適用する処理に分けます。

```cpp
auto move = state.make_two_opt(problem, left, right);
if (move.valid()) {
    auto next_cost = state.total_cost() + move.delta();
    state.apply(problem, move);
}

auto move = state.make_or_opt(
    problem, first, length, after, reversed);

auto move = state.make_double_bridge(
    problem, cut1, cut2, cut3, cut4);
```

操作の型は次の情報を非公開で保持し、コストの差分だけを公開します。

- 作成元の問題
- 作成元の状態
- 作成時の `revision`
- 変更前の添字
- コストの差分

操作の型はデフォルト構築することができ、その値は無効な操作です。`valid()` は適用できる操作の場合に true を返します。`optional` は使用しません。

適用時に問題、状態、`revision` を確認します。無効な操作、別の状態から作成した操作、古い操作、別の問題の操作は適用できません。

添字は全て、操作を作成した時点の巡回順に対するものとします。

- 2-opt: `[left, right]` を反転します。閉路の境界をまたぐ場合は、0 番目を含まない側を反転します。
- Or-opt: `[first, first+length)` を、変更前の `after` 番目の直後へ移動します。
- 反転 Or-opt: 区間を反転してから移動します。
- Double Bridge: `cut1 < cut2 < cut3 < cut4` を始点とする 4 辺を削除し、5 つの区間を `S0,S3,S2,S1,S4` の順に連結します。`cut4 == n-1` の場合は最後の区間を空とします。

移動する区間には 0 番目を含めません。無効な添字、隣接する辺だけを削除する操作、実質的に変化しない操作には、無効な操作を返します。

2-opt は追加の配列を使用せずに区間を反転し、Or-opt は `rotate` を使用します。Double Bridge は `TspState` の作業領域を使用し、反復中に配列を確保しません。各点の位置は、位置が変化する区間だけを更新します。

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

探索は渡された `state` を直接更新します。

- `first_improvement == true`: コストが下がる操作を最初に見つけた時点で適用します。
- `first_improvement == false`: 一巡のうちコストが最も下がる操作を適用します。
- `max_evaluated_moves == -1`: 候補の評価回数を制限しません。
- 上限に達して停止した場合は `locally_optimal == false` になります。
- 点の数が 4 未満の場合は、操作の回数が 0 のまま終了します。

候補表を渡さない全探索版も用意します。コストが下がる操作がなくなって全探索版が終了した場合、結果は 2-opt に関する局所最適解です。候補制限版の場合は候補内の局所最適解です。

候補制限版では、点 `u` とその候補点 `v` を隣接させる 2-opt を評価します。`u` と `v` それぞれの次の点への辺を削除する操作と、前の点への辺を削除する操作の 2 通りです。候補数を `n-1` にすると、隣接していない全ての辺の組を評価します。

部分巡回路では、候補表に含まれていても巡回路に存在しない点を探索の準備の時点で除きます。

全探索版の `first_improvement == true` では、改善する操作を適用した後も続きの組を評価し、改善する操作のない一巡で終了します。

候補制限版の `first_improvement == true` は、内部クラス `TourSearch` で探索します。

- 巡回路を配列で保持し、向きと開始位置を区別しません。区間の反転では、反転する区間とその補集合のうち短い方を反転します。終了時に、開始点が 0 番目になるように巡回順を回転して `state` へ反映します。
- 再評価する点をキューで管理します。最初に全ての点をキューに追加し、操作を適用するたびに、辺が変化した 4 点をキューに追加します。
- キューから取り出した点 `t1` について、`t1` と候補点 `t3` を結ぶ辺が、削除する `t1` の辺より短い操作だけを評価します。候補はコストの昇順に並んでいるため、長い候補に達した時点で打ち切ります。
- キューが空になった後、打ち切りを行わずに全ての候補を評価する一巡を、操作を 1 つも適用しなくなるまで繰り返します。この一巡により、終了時の結果は候補内の局所最適解になります。

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

`TourSearch` で 2-opt、Or-opt、3-opt の局所探索を行い、次の反復を上限に達するまで繰り返します。

1. 巡回路上の位置を無作為に選び、その直後に続く 3 つの区間を逆の順に並べ替える Double Bridge を適用します。各区間の長さは 1 以上 `max_kick_length` 以下から無作為に選びます。
2. Double Bridge で辺が変化した 8 点だけをキューに追加し、キューが空になるまで局所探索を行います。
3. コストが増加しなければ採択します。増加した場合は、温度 `T` に対して確率 `exp(-増加量 / T)` で採択します。
4. 採択しない場合は、反復の中で適用した反転を逆順に適用し、Double Bridge を元に戻します。

Or-opt は、キューから取り出した点 `t1` から始まる長さ 1 から 3 の区間を、`t1` の候補点とその隣の点の間へ移動します。区間の向きを維持する移動と反転する移動の両方を評価します。区間の移動は 2 回または 3 回の区間の反転として適用します。

3-opt は、キューから取り出した点 `a` について、辺 `{a,b}`, `{c,d}`, `{e,f}` を削除し、辺 `{a,c}`, `{d,e}`, `{f,b}` を追加する操作です。`use_three_opt == true` の場合に評価します。

- `b` は `a` の隣の点、`c` は `a` の候補点、`d` は `c` の隣の点、`e` は `d` の候補点、`f` は `e` の隣の点です。
- `cost(a,b) - cost(a,c)` が正で、それに `cost(c,d) - cost(d,e)` を加えた値も正である組合せだけを評価します。候補はコストの昇順に並んでいるため、正でなくなった時点で打ち切ります。
- `d` が `c` の先の点である場合は、辺 `{a,c}`, `{b,d}` を追加する 2-opt の後に、辺 `{b,d}` と `{e,f}` を交換する 2-opt を続ける操作になります。
- `d` が `c` の手前の点である場合は、`b` から `d` までの区間を `e` と `f` の間へ移動する操作になります。区間の長さに上限はなく、向きを維持する移動と反転する移動の両方を評価します。

計測では、3-opt を加えると反復 1 回あたりの時間が約 2.5 倍になりますが、同じ制限時間での結果は 1000 点を超える入力とクラスタ状の入力で改善しました。11 入力の平均の超過は、100 ms で 0.67% から 0.54%、1000 ms で 0.27% から 0.19% になりました。超過は、同じ探索を 20 秒以上実行して得たコストまたは最適解のコストに対する割合です。

温度の初期値は `temperature_ratio` と巡回路の辺の平均コストの積とし、時間または反復の回数の進行度に対して線形に 0 まで下げます。悪化を採択するため、最良の巡回路から初めて悪化する時点で巡回順を保存し、終了時に `state` を最良の巡回路へ戻します。

時間と反復の回数の少なくとも一方の上限を有効にします。`max_iterations == 0` の場合は最初の局所探索だけを行います。点の数が 4 未満または候補数が 0 の場合は、入力状態を変更せずに返します。点の数が 8 未満の場合は Double Bridge を適用せず、最初の局所探索だけを行います。

`solve_tsp` は、問題、候補数 10 の候補表、最近傍法の初期解を作成して反復局所探索を呼び出し、巡回順を返します。

## 非対称コストの探索

`atsp_iterated_local_search.cpp` は、`edge_cost(u, v)` と `edge_cost(v, u)` が異なる問題のための反復局所探索です。`TspProblem` をそのまま使用し、巡回順は `vector<int>` で受け渡します。`TspState` の近傍操作は対称なコストを前提にしているため使用しません。

```cpp
struct AtspCandidates {
    int node_count;
    int candidate_count;
    vector<int> out;   // 出る辺のコストが小さい点
    vector<int> in;    // 入る辺のコストが小さい点
    const void* problem_id;
};

AtspCandidates make_atsp_candidates(problem, requested_count);
auto result = atsp_iterated_local_search(problem, candidates, order, options);
vector<int> order = solve_atsp(node_count, edge_cost, time_limit_ms, start, seed);
vector<int> path = solve_atsp_path(node_count, edge_cost, time_limit_ms, start, goal, seed);
```

候補表は、全ての辺を 1 回走査して、出る辺の上位と入る辺の上位を同時に求めます。

内部クラス `atsp_detail::TourSearch` は、向きのある巡回路を配列で保持します。反復の構成、採択の規則、最良解の保存は対称版と同じです。局所探索の操作は次のとおりです。

- 隣接する 2 区間の入れ替え: 巡回路上でこの順に現れる辺 `a→b`, `c→d`, `e→f` を削除し、辺 `a→d`, `e→b`, `c→f` を追加します。区間 `[b..c]` と区間 `[d..e]` が入れ替わり、どの辺の向きも変化しません。`a` から出る辺の候補と `c` から出る辺の候補をたどる探索と、`b` に入る辺の候補と `f` に入る辺の候補をたどる探索の両方を行います。
- Or-opt: 長さ 1 から 3 の区間を別の位置へ移動します。区間の先頭に入る辺の候補と、区間の末尾から出る辺の候補から移動先を選びます。これも 2 区間の入れ替えとして適用します。
- 2-opt: 区間 `[b..c]` を反転します。区間内の辺の向きが逆になるため、差分には区間内の各辺の `cost(v, u) - cost(u, v)` の総和を加えます。
- 反転 Or-opt: 長さ 2 または 3 の区間を反転して移動します。

2 区間の入れ替えでは、巡回路を 3 区間とみなし、最も長い区間を動かさずに残りの 2 区間を入れ替えます。巡回路としては同じ結果になり、時間は短い 2 区間の長さの和に比例します。

区間の反転のために、各点 `u` について、`u` から次の点 `v` への辺の `cost(v, u) - cost(u, v)` を保持します。区間を反転したときの区間内のコストの変化は、この値を区間に沿って合計して求めます。合計に区間の長さに比例する時間がかかるため、反転する区間の長さを `max_reverse_length` 以下に制限します。区間を反転すると、区間内の辺は向きが逆になるだけであるため、保持する値は辺のコストを参照せずに符号の反転で更新します。

計測では、区間を反転する操作を加えると、対称な 1000 点の入力の 1000 ms での超過が 0.82% から 0.16% になりました。開始位置と終了位置がある入力と乱数の行列では、明確な効果はありませんでした。長さの上限は 200 と 400 が同程度で、1000 では悪化しました。

`solve_atsp` の初期解は最近傍法で作成します。2000 点以下で、ハンガリアン法の見積もり時間が残り時間の半分以下の場合は、割当問題の最適解から作った巡回順も作成し、コストの小さい方を使用します。

- 割当問題の最適解は、各点から出る辺と入る辺を 1 本ずつ選んだときのコストの総和が最小の辺集合で、いくつかの閉路になります。最大の閉路に、ほかの閉路を大きい順に 1 つずつ併合します。併合では、2 つの閉路から 1 辺ずつ選んで行き先を入れ替え、コストの増加が最小の組を選びます。
- 互いに最も近い 2 点の組に含まれる点が全体の 1/4 を超える場合と、閉路の数が点数の 1/8 を超える場合は、この初期解を使用しません。対称に近い問題では長さ 2 の閉路が多くなり、併合によるコストの増加が大きいためです。
- 乱数の行列の 1000 点では、最近傍法の初期解のコストが 62043、割当問題による初期解が 17022 で、割当問題の最適値は 15918 でした。

`solve_atsp_path` は、番号 `node_count` の点を追加して巡回路の問題に変換します。追加した点から出る辺は始点への辺だけ、追加した点に入る辺は終点からの辺だけをコスト 0 とし、それ以外の辺はどの経路のコストよりも大きい値にします。終点を固定しない場合は、全ての点から追加した点への辺をコスト 0 にします。初期解は禁止した辺を含まないように作成し、探索はコストが最小の巡回順を返すため、結果も禁止した辺を含みません。

## 誘導局所探索

標準版は、ペナルティを更新するたびに、ペナルティを加えたコストで局所探索を行います。探索には `TourSearch` の 2-opt を使用します。

1. 真の辺のコストで、キューが空になるまで探索します。
2. 最初の局所最適解から `lambda = penalty_ratio * total_cost / state.size()` を定めます。
3. 現在の巡回路の辺のうち、`edge_cost(u,v) / (1+penalty(u,v))` が最大となる全ての辺にペナルティを加え、その辺の端点をキューに追加します。
4. ペナルティを加えたコストで、キューが空になるまで探索します。
5. 真のコストが最良であれば記録します。上限に達するまで 3～5 を繰り返します。
6. 終了時に、渡された `state` を最良の巡回路へ戻します。

ペナルティの変化で差分が変化する 2-opt は、ペナルティを加えた辺を削除する操作だけです。その操作をすべて辺の端点から評価するために、点 `u` の近傍には、`u` の候補点に加えて `u` を候補に含む点も含めます。ペナルティを加えたコストでは、候補の長さによる打ち切りを行いません。

真のコストが最良であるかは、手順 1 と 4 の探索が終了した時点で判定します。巡回順は、最良の巡回路に次のペナルティを加える直前に保存します。

ペナルティを加えたコストは次の式とします。

```text
true_cost + lambda * Σ penalty(edge)
```

この式は、Voudouris と Tsang の [誘導局所探索による TSP](https://doi.org/10.1016/S0377-2217(98)00099-X) に合わせています。

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

最良の巡回路は探索中の作業領域へ保存し、結果の型には巡回路を重複して保持しません。終了後は `state.order()` と `state.total_cost()` から結果を取得します。

ペナルティは各状態に `n*n` の表として保持させず、実際にペナルティを加えた無向辺だけをハッシュ表へ保存します。ペナルティは 1 回の探索の中だけで使用するため、探索の後も存続する `TspState` のメンバにはしません。

時間、候補の評価回数、ペナルティの更新回数の上限のうち、最初に達した上限で停止します。少なくとも 1 つの上限を明示的に有効にします。`time_limit_ms == 0` または `max_evaluated_moves == 0` の場合は、入力状態を変更せずに返します。`max_penalty_rounds == 0` の場合は真のコストによる最初の局所探索だけを行い、ペナルティは更新しません。

複数の上限に同時に達した場合は、時間、候補の評価回数、ペナルティの更新回数の順に停止理由を定めます。開始時に時間の上限または候補の評価回数の上限が 0 であれば、ペナルティの更新回数の上限に関係なく何も行いません。点の数が 4 未満または候補数が 0 の場合も、辺のコストを評価せずに入力状態をそのまま返します。

時間の上限は、誘導局所探索の関数を呼び出した時点から計測します。問題、候補表、初期状態の構築時間は含めません。時間は、ペナルティを更新するたびと、局所探索で 128 点を処理するたびに確認します。その間隔の分だけ、上限を超える可能性があります。

## 現在の tsp2.py の方式

性能を比較するため、現在の `tsp2.py` の処理順を再現した `tsp_edge_penalty_search` も実装します。これは標準的な誘導局所探索ではないため、`tsp_guided_local_search` の設定では切り替えないようにします。

1 回の反復の処理は次のとおりです。

1. 初期状態をそのまま最良解とし、最初の局所探索は行いません。
2. `lambda = initial_penalty_ratio * best_cost / state.size()` とします。
3. 現在の巡回路を先頭から走査し、`edge_cost(u,v) / (1+penalty(u,v))` が最大の辺のうち最初の 1 辺を選択します。
4. 選択した無向辺のペナルティを 1 増やします。
5. その辺の始点 `u` の候補点を近い順に走査し、選択した辺と 2-opt で組み合わせる辺は、候補点から次の点への辺だけとします。
6. `true_delta + lambda * penalty_delta < 0` を最初に満たす操作を 1 つだけ適用します。該当する操作がない場合も次の反復へ進みます。
7. 真のコストが最良であれば巡回順を保存し、`lambda = improved_penalty_ratio * best_cost / state.size()` とします。

```cpp
struct TspEdgePenaltySearchOptions {
    int64_t max_penalty_rounds = -1;
    int64_t max_evaluated_moves = -1;
    double time_limit_ms = -1;
    long double initial_penalty_ratio = 0.3L;
    long double improved_penalty_ratio = 0.1L;
};

struct TspEdgePenaltySearchResult {
    int64_t evaluated_moves;
    int64_t applied_moves;
    int64_t penalty_rounds;
    int64_t best_updates;
    double elapsed_ms;
    TspSearchStopReason stop_reason;
};

auto result = tsp_edge_penalty_search(
    problem, candidates, state, options);
```

`tsp2.py` の密なペナルティの表は移植しません。表に登録されていない辺のペナルティの増加回数を 0 とする疎な表を使用すれば、元の初期値 1 は、分母を `1+penalty` とすることで同じ意味になります。全ての辺に共通する初期値は、2-opt のペナルティの差分では相殺されます。

終了時には、探索途中の現在の解ではなく、真のコストが最小だった巡回順へ `state` を戻します。`TspState::total_cost()` には常に真の辺のコストだけを保持し、ペナルティを加えたコストは含めません。

時間、候補の評価回数、ペナルティの更新回数の上限のうち、最初に達した上限で停止します。いずれかの上限が 0 であれば入力状態を変更せずに返します。同時に達した場合の停止理由は、時間、候補の評価回数、ペナルティの更新回数の順に定めます。時間は各反復の先頭で確認するため、1 回の反復での辺の走査、候補の評価、2-opt の適用の分だけ上限を超える可能性があります。点の数が 4 未満または候補数が 0 の場合も、入力状態を変更せずに返します。

元のコードでは、2-opt によって配列の 0 番目が移動する場合があります。比較版では、Python 版の配列の 0 番目に相当する論理上の走査開始点を探索中だけ保持します。Python 版と同じ区間を反転した後、巡回方向を変更せずに配列を巡回シフトして、`TspState` の固定開始点を 0 番目へ戻します。次の辺の走査は論理上の開始点から行います。これにより、固定開始点の不変条件を維持しながら、辺の向き、候補を走査する始点、走査順を元の処理に一致させます。

標準版との比較には、同じ問題、候補表、初期状態のコピーを使用します。時間だけでなく候補の評価回数またはペナルティの更新回数もそろえ、真のコストの最良値、評価回数、適用回数、ペナルティの更新回数、経過時間を記録します。

## 焼きなましの初期状態

誘導局所探索は焼きなましの本体へ組み込まず、状態を作成する処理の中で必要に応じて適用します。

```cpp
enum class TspInitialSearch {
    none,
    two_opt,
    guided_local_search,
    edge_penalty_search,
    iterated_local_search,
};

struct TspInitialStateOptions {
    TspInitialSearch search = TspInitialSearch::none;
    TspLocalSearchOptions local_search;
    TspGuidedLocalSearchOptions guided_local_search;
    TspEdgePenaltySearchOptions edge_penalty_search;
    TspIteratedLocalSearchOptions iterated_local_search;
};
```

状態を生成する処理は次のように実装します。

```cpp
auto tsp = problem.make_nearest_neighbor_state(start);
switch (initial_options.search) {
case TspInitialSearch::none:
    break;
case TspInitialSearch::two_opt:
    tsp_two_opt_local_search(
        problem, candidates, tsp, initial_options.local_search);
    break;
case TspInitialSearch::guided_local_search:
    tsp_guided_local_search(
        problem, candidates, tsp,
        initial_options.guided_local_search);
    break;
case TspInitialSearch::edge_penalty_search:
    tsp_edge_penalty_search(
        problem, candidates, tsp,
        initial_options.edge_penalty_search);
    break;
case TspInitialSearch::iterated_local_search:
    tsp_iterated_local_search(
        problem, candidates, tsp,
        initial_options.iterated_local_search);
    break;
}
return State(&problem, &candidates, move(tsp), seed);
```

既定値は `none` とし、初期解の改善を使用する場合だけ、時間、候補の評価回数、ペナルティの更新回数、反復の回数の上限を与えます。

計算量は、専用の時間の上限だけでなく候補の評価回数の上限でも制限することができます。実行時間に依存しない比較には候補の評価回数またはペナルティの更新回数を使用し、本番では初期解用の時間の上限も併用します。候補表は一度だけ作成し、初期解の改善と焼きなましで共有します。

初期解の改善は常に有利とは限りません。初期解のコストは下がりますが、焼きなましの反復に使用できる時間と複数の初期解の多様性が減少するためです。制限時間が短い場合は、まず通常の 2-opt を基準にし、反復局所探索へ渡す上限を小さい値から増やして、最終的なコストと焼きなましの反復回数を合わせて比較します。

`sa_run` には初期化済みの状態を渡します。`sa_run` の `TIME_LIMIT` は呼び出してからの時間であるため、問題と候補表の作成、初期解の改善、状態の構築にかかった時間をプログラム全体の制限時間から減算して渡します。

同じ初期巡回路を使用する複数のレプリカでは、決定的な初期解の改善を各レプリカで繰り返さないようにします。一度改善した確定状態を `O(n)` で複製し、焼きなましの乱数だけを変更します。異なる初期巡回路を試す場合だけ、各状態に独立した上限を与えて初期解の改善を行います。

## 焼きなましとの組合せ

問題固有の `sa::State` が、確定済みの `TspState` と未適用の操作を保持します。

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

操作の種類ごとにメンバを 1 つ用意し、`changed.type` で使用するメンバを選択します。

1 回の反復の処理は次のとおりです。

1. `modify` の先頭で有効な操作番号を設定します。
2. `TspState` から操作を作成しますが、この時点では適用しません。
3. 操作をメンバへ保存し、`score` だけを提案後のコストへ更新します。作成できなかった場合は `is_valid = false` にします。
4. `advance` で、保存した操作を適用します。
5. 棄却した場合は何もしません。巡回路は変更していません。

単一の TSP では、提案後のコストは `tsp.total_cost() + move.delta()` です。

初期化済みの状態を受け取る `sa_run` を使用すれば、`State` のデフォルト構築は不要です。`sa_multi_run` と `replica_run` は、デフォルト構築の後に `State::init(seed)` を呼び出します。

問題と候補表は全レプリカで共有し、`TspState`、乱数、作業領域、未適用の操作はレプリカごとに保持します。問題のデータは大域変数で渡さないようにします。

`sa::State` を複製すると、保存した操作は複製元の `TspState` から作成した操作のままになり、複製先では適用できません。複製先では、`modify` で新しい `TspState` から操作を再作成してから `advance` を呼び出します。

## 複数巡回路

複数巡回路は、単一の `TspState` を巡回路の数だけ並べるのではなく、全ての巡回路を 1 つの状態で管理します。

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

不変条件は次のとおりです。

- 各巡回路の 0 番目は、その巡回路だけに属する固定デポです。
- デポを含む全ての点は、全巡回路を通じてちょうど一度だけ現れます。
- `route_of` と `position` は全ての点の位置を示します。
- 顧客の点が 0 個で、デポだけからなる巡回路も許容します。

初版の巡回路間の操作は、区間の移動と区間の交換とします。

- 区間の移動: 移動元の巡回路の `[first, first+length)` を、移動元とは異なる移動先の巡回路の `after` 番目の直後へ挿入します。
- 区間の交換: 異なる 2 つの巡回路にある 2 つの区間を交換します。

各区間にはデポを含めません。操作は各巡回路のコストの差分 `delta1`、`delta2` を返します。

総距離と距離の二乗和は `O(1)` で更新できます。最大距離は単純な方法では `O(k)` で、巡回路のコストの集合を保持する場合は `O(log k)` で更新できます。目的関数は利用側が選択し、ライブラリ内では固定しません。

K-means による初期分割も固定しません。K-means、個数制約付きの K-means、単純な最近傍への割り当てを問題に応じて組み合わせます。

## 現行コードから移植しない部分

- 固定のファイル名、固定の点の数、TSPLIB の入力、描画処理
- 大域変数の距離表、点の数、巡回路の数、デポ
- 各状態が保持する `n*n` のペナルティの表
- K-means による初期化、距離の二乗和、温度、近傍操作の比率の固定
- 局所的な改善を焼きなましの 1 つの操作として直接確定する処理

現行の `sa_tsp.cpp` には次の問題があります。

- 引数のシードを乱数に設定していません。
- ペナルティ付きの 2-opt がデポを先頭から移動させます。
- 実行部では巡回路の数が 1 に固定されています。
- 各状態が `n*n` のペナルティの表を保持します。

近傍の正しい数式だけを移植し、状態の管理は新しく作成します。

## 入力条件

- `node_count >= 1`
- 点番号は `[0, node_count)` の範囲です。
- 初期巡回順と `nodes_to_visit` には重複がないものとします。
- `start` は有効な点とし、`nodes_to_visit` には含めません。
- `Cost` は符号付きの算術型です。
- 合計、差分、`total_cost + delta` の各中間値は `Cost` の範囲内に収まるものとします。
- 辺のコスト関数は `const` で呼び出すことができ、有限かつ 0 以上の値を返す対称な関数です。
- 候補数は 0 以上です。
- 各上限は `-1` または 0 以上です。
- `penalty_ratio`、`initial_penalty_ratio`、`improved_penalty_ratio`、`temperature_ratio` は有限かつ 0 以上です。
- `max_kick_length > 0`
- ペナルティを使用する探索では、時間の上限とペナルティの更新回数の上限の少なくとも一方を有効にします。候補の評価回数の上限は補助的な上限として使用します。
- 反復局所探索では、時間の上限と反復の回数の上限の少なくとも一方を有効にします。

構築時に一度確認できる違反には `invalid_argument` を投げます。反復中の無効な近傍の候補には無効な操作を返し、例外は使用しません。辺のコストの条件と算術演算の範囲は利用側の前提とし、反復中にはチェックしません。

## 計算量

点の数を `n`、実際の候補数を `c`、適用した操作の数を `A`、反転または移動する区間の長さを `s_i` とします。

| 処理 | 時間 | 追加メモリ |
|---|---:|---:|
| `TspState` の作成 | `O(n)` | `O(n)` |
| `TspState` のコピー | `O(n)` | `O(n)` |
| 最近傍法による初期解 | `O(n²)` | `O(n)` |
| 候補表 | `O(n²)` 回の `edge_cost` の呼び出しと、最悪 `O(n²c)` の挿入 | `O(nc)` |
| 2-opt の評価 | `O(1)` | `O(1)` |
| 2-opt の適用 | `O(s_i)` | `O(1)` |
| Or-opt の評価 | `O(1)` | `O(1)` |
| Or-opt の適用 | `O(s_i)` | `O(1)` |
| Double Bridge の評価 | `O(1)` | `O(1)` |
| Double Bridge の適用 | `O(s_i)` | State 内の `O(n)` の作業領域 |
| 全ての 2-opt の一巡 | `O(n²)` | `O(1)` |
| `TourSearch` の構築 | `O(nc²)` | `O(nc)` |
| `TourSearch` での 1 点の再評価 | `O(c)` | `O(1)` |
| `TourSearch` での区間の反転 | 区間とその補集合のうち短い方の長さに比例 | `O(1)` |

全探索版の時間は、一巡の数を `W` として `O(Wn² + Σs_i)` です。候補を制限した局所探索は、キューから取り出した点の数を `V` として `O(Vc + Σs_i)` と、最後の確認の一巡ごとの `O(nc)` です。

反復局所探索は、1 回の反復につき、Double Bridge の `O(max_kick_length)` と、その後の局所探索の `O(Vc + Σs_i)` です。採択しない場合は、同じ `Σs_i` の時間で元に戻します。最良の巡回順の保存は 1 回につき `O(n)` です。

誘導局所探索について、ペナルティの更新回数を `R`、最良解の更新回数を `B`、ペナルティを加えた異なる辺の数を `P` とします。候補表の作成を除く時間は平均 `O(Vc + Σs_i + Rn + (B+1)n)`、追加メモリは `O(nc+P)` です。`Rn` は、ペナルティを加える辺を選ぶための巡回路の走査です。

`tsp2.py` の方式では、候補表の作成を除く時間が平均 `O(Rn + H + Q + Σs_i + (B+1)n)`、追加メモリが `O(n+P)` です。全ての点を含む巡回路では `H=O(Rc)` となります。

同じ確定済みの初期状態を `r` 個の焼きなましの状態へ複製する時間とメモリは `O(rn)` です。各状態で独立に誘導局所探索を行う場合は、探索時間と作業メモリも状態の数に比例して増加します。

利用側で距離行列を作成する場合は、別に時間とメモリが `O(n²)` 必要です。

複数巡回路での長さ `L` の区間の移動と交換は、評価が `O(L)` で、適用は関係する巡回路の長さに比例します。長さを 1～3 に制限する場合は、評価が定数時間になります。

## 実装済みのテスト

`test/tsp_test.cpp` では、小さい入力について、各操作のコストの差分をコスト全体の再計算の結果と照合します。操作後の各点の位置、固定開始点、各点の一意性、複数巡回路での所属も確認します。比較用の探索は、複数の初期巡回順について、`tsp2.py` と同じ処理を直接記述した参照実装と照合します。反復局所探索と `solve_tsp` は、全ての巡回順を列挙して求めた最小のコストと照合します。

`test/tsp_sa_test.cpp` では、デフォルト構築できない SA の状態を先に構築してから `sa_run` へ渡し、初期解に対する 2-opt と、焼きなましでの操作の遅延適用を組み合わせます。
