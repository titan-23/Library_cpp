# AHC 向け TSP ライブラリの設計

## 結論

ライブラリは、読み取り専用の問題データを保持する `TspProblem` と、現在の巡回路を保持する `TspState` を中心に構成します。

- `TspProblem`: 点の数と辺のコスト関数を保持します。
- `TspCandidates`: 近傍の候補表を保持します。
- `TspState`: 巡回順、各点の位置、現在のコスト、作業領域を保持します。
- 局所探索、誘導局所探索、辺にペナルティを加える比較用の探索: 渡された `TspState` を直接更新します。
- 焼きなまし: 問題固有の `sa::State` が `TspState` と未適用の操作を保持します。

`sa_tsp.cpp` からは正しい差分計算と巡回路の更新だけを共通化します。`tsp2.py` の探索は、標準的な誘導局所探索とは別の比較対象として残します。

小規模な問題を厳密に解く `titan_cpplib/alg/traveling_salesman_problem.cpp` は役割が異なるため、そのまま残します。新しいライブラリは大規模な問題向けの近似解法だけを扱います。

## 対象とする問題

初版は対称 TSP だけを対象とします。

- 巡回路は対象とする各点を一度ずつ訪問する閉路です。
- `edge_cost(u, v) == edge_cost(v, u)`
- 辺のコストは有限かつ 0 以上です。
- 三角不等式を満たす必要はありません。

非対称 TSP では、2-opt で区間を反転すると区間内の全ての辺の向きが変化します。境界の辺だけを使用してコストの差分を定数時間で求めることができないため、初版では対応しません。

## ファイル構成

```text
titan_cpplib/ahc/tsp/
├── tsp.cpp
├── tsp_symmetric_moves.cpp
├── tsp_local_search.cpp
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
- `tsp_local_search.cpp`: 全探索または候補制限付きの 2-opt 局所探索
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

各点について `(辺費用, 点番号)` の組を `nth_element` で絞り込み、選択した部分だけをソートします。コストが同じ場合は点番号が小さい方を先にします。

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
    optional<TspTwoOptMove<Cost>> make_two_opt(
        const TspProblem<EdgeCost>& problem,
        int left, int right) const;
    template<class EdgeCost>
    optional<TspOrOptMove<Cost>> make_or_opt(
        const TspProblem<EdgeCost>& problem,
        int first, int length, int after, bool reversed) const;
    template<class EdgeCost>
    optional<TspDoubleBridgeMove<Cost>> make_double_bridge(
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

`reset` は巡回順全体を置き換え、位置とコストを再計算します。誘導局所探索の終了時に保存済みの最良の巡回順へ戻す用途で使用し、それまでに作成した未適用の操作を全て無効にします。

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
if (move) {
    auto next_cost = state.total_cost() + move->delta();
    state.apply(problem, *move);
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

適用時に問題、状態、`revision` を確認します。別の状態から作成した操作、古い操作、別の問題の操作は適用できません。

添字は全て、操作を作成した時点の巡回順に対するものとします。

- 2-opt: `[left, right]` を反転します。閉路の境界をまたぐ場合は、0 番目を含まない側を反転します。
- Or-opt: `[first, first+length)` を、変更前の `after` 番目の直後へ移動します。
- 反転 Or-opt: 区間を反転してから移動します。
- Double Bridge: `cut1 < cut2 < cut3 < cut4` を始点とする 4 辺を削除し、5 つの区間を `S0,S3,S2,S1,S4` の順に連結します。`cut4 == n-1` の場合は最後の区間を空とします。

移動する区間には 0 番目を含めません。無効な添字、隣接する辺だけを削除する操作、実質的に変化しない操作には `nullopt` を返します。

2-opt は追加の配列を使用せずに区間を反転し、Or-opt は `rotate` を使用します。Double Bridge は `TspState` の作業領域を使用し、反復中に配列を確保しません。

| 操作 | コストの差分 | 適用 |
|---|---:|---:|
| 2-opt | `O(1)` | 反転する区間の長さに比例 |
| Or-opt | `O(1)` | `O(n)` |
| 反転 Or-opt | `O(1)` | `O(n)` |
| Double Bridge | `O(1)` | `O(n)` |

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

候補制限版では、各候補点について巡回路上の前後の 2 辺を評価します。候補数を `n-1` にすると、隣接していない全ての辺の組を評価します。

部分巡回路では、候補表に含まれていても巡回路に存在しない点を `position(node) == -1` で判定してスキップします。

## 誘導局所探索

標準版は、ペナルティを更新するたびに、ペナルティを加えたコストで局所探索を行います。

1. 真の辺のコストで候補内の局所最適解まで探索します。
2. 最初の局所最適解から `lambda = penalty_ratio * total_cost / state.size()` を定めます。
3. 現在の巡回路の辺のうち、`edge_cost(u,v) / (1+penalty(u,v))` が最大となる全ての辺にペナルティを加えます。
4. ペナルティを加えたコストで候補内の局所最適解まで探索します。
5. 真のコストが最良であれば巡回順を保存します。上限に達するまで 3～5 を繰り返します。
6. 終了時に、渡された `state` を最良の巡回路へ戻します。

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
    int time_check_interval = 256;
    long double penalty_ratio = 0.3L;
};

enum class TspSearchStopReason {
    completed,
    time_limit,
    evaluated_move_limit,
    penalty_round_limit,
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

ペナルティは各状態に `n*n` の表として保持させず、実際にペナルティを加えた無向辺だけを疎な表へ保存します。ペナルティは 1 回の探索の中だけで使用するため、探索の後も存続する `TspState` のメンバにはしません。

時間、候補の評価回数、ペナルティの更新回数の上限のうち、最初に達した上限で停止します。少なくとも 1 つの上限を明示的に有効にします。`time_limit_ms == 0` または `max_evaluated_moves == 0` の場合は、入力状態を変更せずに返します。`max_penalty_rounds == 0` の場合は真のコストによる最初の局所探索だけを行い、ペナルティは更新しません。

複数の上限に同時に達した場合は、時間、候補の評価回数、ペナルティの更新回数の順に停止理由を定めます。開始時に時間の上限または候補の評価回数の上限が 0 であれば、ペナルティの更新回数の上限に関係なく何も行いません。点の数が 4 未満または候補数が 0 の場合も、辺のコストを評価せずに入力状態をそのまま返します。

時間の上限は、誘導局所探索の関数を呼び出した時点から計測します。問題、候補表、初期状態の構築時間は含めません。時間を確認する間隔と、実行中の 1 回の辺の走査、巡回路の保存、2-opt の適用の残り時間の分だけ、上限を超える可能性があります。

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
};

struct TspInitialStateOptions {
    TspInitialSearch search = TspInitialSearch::none;
    TspLocalSearchOptions local_search;
    TspGuidedLocalSearchOptions guided_local_search;
    TspEdgePenaltySearchOptions edge_penalty_search;
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
}
return State(&problem, &candidates, move(tsp), seed);
```

既定値は `none` とし、初期解の改善を使用する場合だけ、時間、候補の評価回数、ペナルティの更新回数の上限を与えます。まず `none`、通常の 2-opt、標準版、`tsp2.py` の方式の 4 通りを同じ初期巡回路から比較します。

計算量は、専用の時間の上限だけでなく候補の評価回数の上限でも制限することができます。実行時間に依存しない比較には候補の評価回数またはペナルティの更新回数を使用し、本番では初期解用の時間の上限も併用します。候補表は一度だけ作成し、初期解の改善と焼きなましで共有します。

誘導局所探索を初期解に使用する方針は妥当ですが、常に有利とは限りません。初期解のコストは下がりますが、焼きなましの反復に使用できる時間と複数の初期解の多様性が減少するためです。制限時間が短い場合は、まず通常の 2-opt を基準にし、誘導局所探索へ渡す上限を小さい値から増やして、最終的なコストと焼きなましの反復回数を合わせて比較します。

`sa_run` は、seed から状態を返す状態生成関数を受け取ることができます。タイマーは状態生成関数を呼び出す前に開始するため、その中で行う初期解の改善は焼きなましの制限時間に含まれます。`sa_run` の前に問題と候補表を作成する時間は含まれないため、プログラム全体の制限時間からその分を減算して `sa_run` へ渡します。

同じ初期巡回路を使用する複数のレプリカでは、決定的な初期解の改善を各レプリカで繰り返さないようにします。一度改善した確定状態を `O(n)` で複製し、焼きなましの乱数だけを変更します。異なる初期巡回路を試す場合だけ、各状態に独立した上限を与えて初期解の改善を行います。

## 焼きなましとの組合せ

問題固有の `sa::State` が、確定済みの `TspState` と未適用の操作を保持します。

```cpp
class State {
    const Problem* problem;
    const TspCandidates* candidates;
    TspState<Cost> tsp;
    variant<monostate, TwoOptMove, OrOptMove, DoubleBridgeMove>
        pending_move;
    ScoreType score;
};
```

1 回の反復の処理は次のとおりです。

1. `modify` の先頭で有効な操作番号を設定します。
2. `TspState` から操作を作成しますが、この時点では適用しません。
3. 操作を `pending_move` へ保存し、`score` だけを提案後のコストへ更新します。
4. `advance` で操作を適用し、`pending_move` を空にします。
5. 棄却した場合は `pending_move` を破棄し、巡回路は変更しません。

単一の TSP では、提案後のコストは `tsp.total_cost() + move.delta()` です。

`sa_run`、`sa_multi_run`、`replica_run` の状態生成関数を受け取る版を使用すれば、`State` のデフォルト構築は不要です。従来の `State::init(seed)` を使用する版も残します。

問題と候補表は全レプリカで共有し、`TspState`、乱数、作業領域、未適用の操作はレプリカごとに保持します。問題のデータは大域変数で渡さないようにします。

`sa::State` を保存または複製するのは、`pending_move` が空である反復の境界だけとします。複製先の `pending_move` は空にし、新しい `TspState` から操作を再作成します。

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
- `time_check_interval > 0`
- `penalty_ratio`、`initial_penalty_ratio`、`improved_penalty_ratio` は有限かつ 0 以上です。
- ペナルティを使用する探索では、時間の上限とペナルティの更新回数の上限の少なくとも一方を有効にします。候補の評価回数の上限は補助的な上限として使用します。

構築時に一度確認できる違反には `invalid_argument` を投げます。反復中の無効な近傍の候補は `nullopt` とし、例外は使用しません。辺のコストの条件と算術演算の範囲は利用側の前提とし、反復中にはチェックしません。

## 計算量

点の数を `n`、実際の候補数を `c`、適用した操作の数を `A`、反転または移動する区間の長さを `s_i` とします。

| 処理 | 時間 | 追加メモリ |
|---|---:|---:|
| `TspState` の作成 | `O(n)` | `O(n)` |
| `TspState` のコピー | `O(n)` | `O(n)` |
| 最近傍法による初期解 | `O(n²)` | `O(n)` |
| 候補表 | 平均 `O(n² + nc log(c+1))` | `O(nc+n)` |
| 2-opt の評価 | `O(1)` | `O(1)` |
| 2-opt の適用 | `O(s_i)` | `O(1)` |
| Or-opt の評価 | `O(1)` | `O(1)` |
| Or-opt の適用 | `O(n)` | `O(1)` |
| Double Bridge の評価 | `O(1)` | `O(1)` |
| Double Bridge の適用 | `O(n)` | State 内の `O(n)` の作業領域 |
| 全ての 2-opt の一巡 | `O(n²)` | `O(1)` |
| 候補を制限した一巡 | `O(n(c+1))` | `O(1)` |

候補を制限した局所探索の単純な上界は `O((A+1)n(c+1) + Σs_i)`、全探索版は `O((A+1)n² + Σs_i)` です。

誘導局所探索について、有効な 2-opt の評価回数を `Q`、候補表の参照回数を `H`、ペナルティの更新回数を `R`、最良解の更新回数を `B`、ペナルティを加えた異なる辺の数を `P` とします。候補表の作成を除く時間は平均 `O(H + Q + Σs_i + Rn + (B+1)n)`、追加メモリは `O(n+P)` です。

`tsp2.py` の方式では、候補表の作成を除く時間が平均 `O(Rn + H + Q + Σs_i + (B+1)n)`、追加メモリが `O(n+P)` です。全ての点を含む巡回路では `H=O(Rc)` となります。

同じ確定済みの初期状態を `r` 個の焼きなましの状態へ複製する時間とメモリは `O(rn)` です。各状態で独立に誘導局所探索を行う場合は、探索時間と作業メモリも状態の数に比例して増加します。

利用側で距離行列を作成する場合は、別に時間とメモリが `O(n²)` 必要です。

複数巡回路での長さ `L` の区間の移動と交換は、評価が `O(L)` で、適用は関係する巡回路の長さに比例します。長さを 1～3 に制限する場合は、評価が定数時間になります。

## 実装済みのテスト

`test/tsp_test.cpp` では、小さい入力について、各操作のコストの差分をコスト全体の再計算の結果と照合します。操作後の各点の位置、固定開始点、各点の一意性、複数巡回路での所属も確認します。比較用の探索は、複数の初期巡回順について、`tsp2.py` と同じ処理を直接記述した参照実装と照合します。

`test/tsp_sa_test.cpp` では、デフォルト構築できない SA の状態を先に構築してから `sa_run` へ渡し、初期解に対する 2-opt と、焼きなましでの操作の遅延適用を組み合わせます。
