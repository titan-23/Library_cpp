# AHC向けTSPライブラリ設計

## 結論

ライブラリの中心を、読み取り専用の問題データを持つ `TspProblem` と、現在の巡回路を持つ `TspState` にする。

- `TspProblem`: 点数と辺費用関数を保持する
- `TspCandidates`: 近傍候補表を保持する
- `TspState`: 巡回順、逆引き位置、現在費用、作業領域を保持する
- 局所探索、誘導局所探索、比較用の辺罰則探索: 渡された `TspState` を直接更新する
- 焼きなまし: 問題側の `sa::State` が `TspState` と未適用操作を持つ

`sa_tsp.cpp` からは正しい差分計算と巡回路更新だけを共通化する。`tsp2.py` の探索は、標準的な誘導局所探索とは別の比較対象として残す。

小規模問題の厳密解 `titan_cpplib/alg/traveling_salesman_problem.cpp` は役割が異なるため、そのまま残す。新しいライブラリは大規模問題向けの近似解法だけを扱う。

## 対象

初版は対称TSP専用とする。

- 各対象点を一度ずつ通る閉路
- `edge_cost(u, v) == edge_cost(v, u)`
- 辺費用は有限かつ0以上
- 三角不等式は不要

非対称TSPでは、2-optで区間を反転すると内部の全辺の向きが変わる。境界辺だけを使う定数時間差分が成立しないため、初版では対応しない。

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

- `tsp.cpp`: `TspProblem`、`TspCandidates`、`TspState`、最近傍初期解
- `tsp_symmetric_moves.cpp`: 2-opt、Or-opt、反転Or-opt、Double Bridge
- `tsp_local_search.cpp`: 全探索または候補制限付き2-opt
- `tsp_guided_local_search.cpp`: 標準的な誘導局所探索
- `tsp_edge_penalty_search.cpp`: 現在の `tsp2.py` の処理順を残す比較用探索
- `tsp_initial_state.cpp`: SA初期状態の改善方法を選ぶ補助関数
- `multiple_tours.cpp`: 固定デポを持つ複数巡回路の状態と巡回路間操作
- `examples/`: 参照元の `sa_tsp.cpp`、`tsp2.py`、TSP入力、可視化処理
- `test/`: 費用差分と不変条件の自動確認

焼きなましの反復処理は既存の `ahc/sa/sa.cpp` と重複するため、`tsp_sa.cpp` は作らない。

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

`TspProblem` は辺費用関数を型のまま値で保持し、`function` は使わない。構築後は変更せず、複数の探索やレプリカから同時に読み取れるようにする。

問題のアドレスを状態と操作の確認に使うため、コピーと移動を禁止してアドレスを固定する。問題は、それから作った全ての状態と探索より長く生存させる。

辺費用関数が距離表などを参照する場合、その参照先も問題より長く生存させ、探索中に変更・再確保しない。大きな距離表をラムダへ値で捕捉しない。

## TspCandidates

候補表は問題構築時に暗黙作成しない。必要な探索だけが明示的に作る。

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

候補表は長さ `node_count * candidate_count` の平坦な配列で持つ。実際の候補数は `min(requested_count, node_count-1)` とする。

各点について `(辺費用, 点番号)` を `nth_element` で絞り、選んだ部分だけを整列する。同じ費用なら点番号が小さい方を先にする。

`TspCandidates` は作成元の問題を識別し、探索関数へ別の問題と組み合わせて渡した場合は例外にする。候補数の異なる表を同じ問題から複数作ってよい。

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

次の状態を非公開で持つ。

```cpp
vector<int> order;
vector<int> position;
Cost total_cost;
vector<int> work_buffer;
const void* problem_id;
uint64_t revision;
```

不変条件は次のとおり。

- `order` は対象点を重複なく並べ、末尾に開始点を重ねない
- `position[order[i]] == i`
- 巡回路に含まれない点の `position` は `-1`
- 最後の辺は `order.back()` から `order.front()`
- `total_cost` は差分適用後の現在費用
- `order[0]` は開始点として固定する

`TspState` は1点以上を持つ。要素数1の費用は0とし、`edge_cost(v, v)` は呼ばない。要素数2では往路と復路の2辺を数える。

状態は `TspProblem::make_state` からだけ作る。巡回順、位置、費用は外部から個別に書き換えさせない。

`reset` は巡回順全体を置き換え、位置と費用を作り直す。誘導局所探索の終了時に保存済みの最良巡回順へ戻す用途で使い、それまでに作った未適用操作は全て無効にする。

状態のコピーは巡回順、位置、費用、作業領域を複製するため `O(n)` である。問題と候補表は複製しない。コピー先は独立した新しい確定状態となり、コピー元で作った操作は適用できない。代入前に代入先から作った操作も無効になる。未適用操作を作ってから適用するまでの間は、作成元状態を移動・破棄してはならない。

## 最近傍初期解

```cpp
auto state = problem.make_nearest_neighbor_state(start);
auto partial_state =
    problem.make_nearest_neighbor_state(start, nodes_to_visit);
```

全点版は `start` を先頭にして `[0, node_count)` の全点を巡る。

部分集合版では `nodes_to_visit` に開始点を含めず、開始点と指定点だけの巡回路を作る。複数巡回路の初期化にも利用できる。同じ費用なら点番号が小さい点を選び、結果を決定的にする。

## 近傍操作

各操作は、状態を変更せずに費用差を求める処理と、採択後に適用する処理へ分ける。

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

操作型は次の情報を非公開で持ち、費用差だけを公開する。

- 作成元の問題
- 作成元の状態
- 作成時の `revision`
- 変更前の添字
- 費用差

適用時に問題、状態、`revision` を確認する。別の状態から作った操作、古い操作、別問題の操作は適用できない。

添字は全て操作作成時の巡回順に対するものとする。

- 2-opt: `[left, right]` を反転する。閉路の境界をまたぐ場合は0番目を含まない側を反転する
- Or-opt: `[first, first+length)` を、変更前の `after` 番目の直後へ移す
- 反転Or-opt: 区間を反転してから移す
- Double Bridge: `cut1 < cut2 < cut3 < cut4` を始点とする4辺を切り、5区間を `S0,S3,S2,S1,S4` の順につなぐ。`cut4 == n-1` では最後の区間を空とする

移動区間は0番目を含めない。無効な添字、隣接辺だけを切る操作、実質的に変化しない操作には `nullopt` を返す。

2-optはその場で反転し、Or-optは `rotate` を使う。Double Bridgeは `TspState` の作業領域を使い、反復中に配列を確保しない。

| 操作 | 費用差 | 適用 |
|---|---:|---:|
| 2-opt | `O(1)` | 反転区間長に比例 |
| Or-opt | `O(1)` | `O(n)` |
| 反転Or-opt | `O(1)` | `O(n)` |
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

探索は渡された `state` を直接更新する。

- `first_improvement == true`: 最初に見つけた改善を適用する
- `first_improvement == false`: 一巡で費用差が最小の改善を適用する
- `max_evaluated_moves == -1`: 候補評価回数を制限しない
- 上限で止まった場合は `locally_optimal == false`
- 点数が4未満なら操作0回で終了する

候補表を渡さない全探索版も用意する。全探索版が改善なしで終われば2-optに関する局所最適、候補制限版では候補内の局所最適である。

候補制限版では、各候補点について巡回路上の前後2辺を調べる。候補数を `n-1` にすれば全ての非隣接辺対を調べる。

部分巡回路では、候補表に含まれていても巡回路に存在しない点を `position(node) == -1` で読み飛ばす。

## 誘導局所探索

標準版は、罰則更新のたびに罰則付き費用による局所探索を行う。

1. 真の辺費用で候補内の局所最適まで探索する
2. 最初の局所解から `lambda = penalty_ratio * total_cost / state.size()` を決める
3. 現在の巡回路上で `edge_cost(u,v) / (1+penalty(u,v))` が最大の辺を全て罰する
4. 罰則付き費用で候補内の局所最適まで探索する
5. 真の費用が最良なら巡回順を保存し、上限まで3～5を繰り返す
6. 終了時に渡された `state` を最良巡回路へ戻す

罰則付き費用は次式とする。

```text
true_cost + lambda * Σ penalty(edge)
```

この形は、Voudouris と Tsang の[誘導局所探索によるTSP](https://doi.org/10.1016/S0377-2217(98)00099-X)に合わせる。

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

最良巡回路は探索中の作業領域へ保存し、結果型へ巡回路を重複保持しない。終了後は `state.order()` と `state.total_cost()` から結果を得る。

罰則は `n*n` の表を各状態に持たせず、実際に罰した無向辺だけを疎な表へ保存する。罰則は一回の探索だけで使うため、恒久的な `TspState` のメンバにはしない。

時間上限、候補評価数、罰則周回数のうち、先に達した上限で止める。少なくとも一つの上限を明示的に有効にする。`time_limit_ms == 0` または `max_evaluated_moves == 0` なら、入力状態を変更せず返す。`max_penalty_rounds == 0` では最初の真の費用による局所探索だけを行い、罰則は更新しない。

複数の上限へ同時に達した場合は、時間、候補評価数、罰則周回数の順に停止理由を決める。開始時に時間上限または候補評価数上限が0なら、罰則周回数上限に関係なく何も行わない。点数が4未満または候補数が0の場合も、辺費用を評価せず入力状態をそのまま返す。

時間上限は誘導局所探索関数へ入ってから測る。問題、候補表、初期状態の構築時間は含めない。時間確認の間隔と、一回の辺走査・巡回路保存・2-opt適用の残り時間だけ上限を超える可能性がある。

## 現在のtsp2.py方式

性能比較のため、現在の `tsp2.py` の処理順を残した `tsp_edge_penalty_search` も実装する。これは標準的な誘導局所探索ではないため、`tsp_guided_local_search` の設定では切り替えない。

一周の処理は次のとおり。

1. 初期状態をそのまま最良解とし、最初の局所探索は行わない
2. `lambda = initial_penalty_ratio * best_cost / state.size()` とする
3. 現在の巡回路を先頭から調べ、`edge_cost(u,v) / (1+penalty(u,v))` が最大の最初の1辺を選ぶ
4. 選んだ無向辺の罰則を1増やす
5. その辺の始点 `u` の候補を近い順に調べ、候補点から出る後続辺だけを2-optの相手にする
6. `true_delta + lambda * penalty_delta < 0` を最初に満たす1手だけを適用する。該当する手がなくても次の周へ進む
7. 真の費用が最良なら巡回順を保存し、`lambda = improved_penalty_ratio * best_cost / state.size()` とする

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

`tsp2.py` の密な罰則表は移植しない。未登録辺の罰則増加回数を0とする疎な表を使えば、元の初期値1は分母の `1+penalty` として同じ意味になる。全辺に共通する初期値は2-optの罰則差では相殺される。

終了時には、探索途中の現在解ではなく、真の費用が最小だった巡回順へ `state` を戻す。`TspState::total_cost()` には常に真の辺費用だけを保持し、罰則付き費用は入れない。

時間上限、候補評価数、罰則周回数のうち、先に達した上限で止める。いずれかの上限が0なら入力状態を変更せず返し、同時に達した場合の停止理由は時間、候補評価数、罰則周回数の順にする。時間は各周の先頭で確認するため、一周の辺走査、候補評価、2-opt適用の分だけ上限を超える可能性がある。点数が4未満または候補数が0の場合も、入力状態を変更せず返す。

元のコードは2-optによって配列の0番目を動かす場合がある。比較版では、Python版の配列0番目に相当する論理上の走査開始点を探索中だけ保持する。Python版と同じ区間を反転した後、巡回方向を変えずに配列を循環させて `TspState` の固定開始点を0番目へ戻す。次の辺走査は論理上の開始点から行う。これにより、固定開始点の不変条件を守りながら、辺の向き、候補を調べる始点、走査順を元の処理に合わせる。

標準版との比較には、同じ問題、候補表、初期状態のコピーを使う。時間だけでなく、候補評価数または罰則周回数もそろえ、真の最良費用、評価数、適用数、周回数、経過時間を記録する。

## 焼きなましの初期状態

誘導局所探索は、焼きなまし本体へ組み込まず、状態を作る処理で任意に適用する。

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

状態生成の流れは次のようにする。

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

既定は `none` とし、初期解改善を使う場合だけ時間・候補評価数・罰則周回数の上限を与える。まず `none`、通常の2-opt、標準版、`tsp2.py` 方式の4通りを同じ初期巡回路から比較する。

計算量が大きくなり過ぎることは、専用の時間上限だけでなく候補評価数でも抑えられる。実行時間によらない比較には候補評価数または罰則周回数を使い、本番では初期解用の時間上限も併用する。候補表は一度だけ作り、初期解改善と焼きなましで共有する。

誘導局所探索を初期解に使う方針自体は妥当だが、常に有利とは限らない。初期費用は下がる一方、焼きなましの反復時間と複数初期値の多様性を減らすためである。短い制限時間では、まず通常の2-optを基準にし、誘導局所探索へ渡す上限を小さい値から増やして、最終費用と焼きなましの反復回数を合わせて比較する。

`sa_run` は、seedから状態を返す状態生成関数を受け取れる。タイマーは状態生成関数を呼ぶ前に開始するため、その中の初期解改善は焼きなましの制限時間に含まれる。問題と候補表を `sa_run` の前で作る時間は含まれないため、プログラム全体の制限時間からその分を引いて `sa_run` へ渡す。

同じ初期巡回路を使う複数レプリカでは、決定的な初期解改善を各レプリカで繰り返さない。一度改善した確定状態を `O(n)` で複製し、焼きなましの乱数だけを変える。異なる初期巡回路を試す場合だけ、各状態へ独立した上限を与えて初期解改善を行う。

## 焼きなましとの組合せ

問題固有の `sa::State` が、確定済みの `TspState` と未適用操作を持つ。

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

一反復の流れは次のとおり。

1. `modify` の先頭で有効な操作番号を設定する
2. `TspState` から操作を作るが、まだ適用しない
3. 操作を `pending_move` へ保存し、`score` だけを次の費用へ更新する
4. `advance` で操作を適用し、`pending_move` を空にする
5. 棄却時は `pending_move` を捨て、巡回路には触れない

単一TSPなら提案後の費用は `tsp.total_cost() + move.delta()` である。

`sa_run`、`sa_multi_run`、`replica_run` の状態生成関数版を使えば、`State` の既定構築は不要である。従来の `State::init(seed)` 版も残す。

問題と候補表は全レプリカで共有し、`TspState`、乱数、作業領域、未適用操作はレプリカごとに持つ。大域変数で問題データを渡さない。

`sa::State` を保存・複製するのは `pending_move` が空の反復境界だけとする。複製先の `pending_move` は空にし、新しい `TspState` から操作を作り直す。

## 複数巡回路

複数巡回路は、単一の `TspState` を巡回路数だけ並べず、全巡回路を一つの状態で管理する。

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

不変条件は次のとおり。

- 各巡回路の0番目は、その巡回路だけに属する固定デポ
- デポを含む全点は、全巡回路を通じてちょうど一度だけ現れる
- `route_of` と `position` は全点の位置を指す
- 顧客0点でデポだけの巡回路も許す

初版の巡回路間操作は区間移動と区間交換とする。

- 区間移動: 移動元の `[first, first+length)` を、異なる移動先巡回路の `after` 番目の直後へ入れる
- 区間交換: 異なる2巡回路の二区間を交換する

各区間はデポを含めない。操作は各巡回路の費用差 `delta1`、`delta2` を返す。

総距離と距離の二乗和は `O(1)` で更新できる。最大距離は単純には `O(k)`、巡回路費用の集合を持つなら `O(log k)` で更新できる。目的関数は利用側が選び、ライブラリ内へ固定しない。

K-meansによる初期分割も固定しない。K-means、個数制約付きK-means、単純な最近傍割当を問題に応じて組み合わせる。

## 現行コードから持ち込まないもの

- 固定ファイル名、固定点数、TSPLIB入力、描画処理
- 大域変数の距離表、点数、巡回路数、デポ
- 各状態が持つ `n*n` の罰則表
- K-means初期化、距離の二乗和、温度、近傍比率の固定
- 局所改善を焼きなましの一操作として直接確定する処理

現行 `sa_tsp.cpp` には、乱数へ引数の種を設定していない、罰則付き2-optがデポを先頭から動かす、実行部では巡回路数が1固定、各状態が `n*n` の罰則表を持つ、という問題がある。正しい近傍の数式だけを移し、状態管理は新しく作る。

## 入力条件

- `node_count >= 1`
- 点番号は `[0, node_count)`
- 初期巡回順と `nodes_to_visit` に重複がない
- `start` は有効な点で、`nodes_to_visit` には含めない
- `Cost` は符号付きの算術型
- 合計、差分、`total_cost + delta` の各中間値が `Cost` の範囲内
- 辺費用関数は `const` 呼出し可能で、有限、0以上、対称
- 候補数は0以上
- 各上限は `-1` または0以上
- `time_check_interval > 0`
- `penalty_ratio`、`initial_penalty_ratio`、`improved_penalty_ratio` は有限かつ0以上
- 罰則探索では時間上限か罰則周回数上限の少なくとも一方を有効にし、候補評価数上限は補助上限として使う

構築時に一度確認できる違反は `invalid_argument` とする。反復中の無効な近傍候補は `nullopt` とし、例外を使わない。辺費用の条件と算術範囲は利用側の前提とし、反復中には検査しない。

## 計算量

点数を `n`、実際の候補数を `c`、適用操作数を `A`、反転・移動区間長を `s_i` とする。

| 処理 | 時間 | 追加メモリ |
|---|---:|---:|
| `TspState` の作成 | `O(n)` | `O(n)` |
| `TspState` のコピー | `O(n)` | `O(n)` |
| 最近傍初期解 | `O(n²)` | `O(n)` |
| 候補表 | 平均 `O(n² + nc log(c+1))` | `O(nc+n)` |
| 2-optの評価 | `O(1)` | `O(1)` |
| 2-optの適用 | `O(s_i)` | `O(1)` |
| Or-optの評価 | `O(1)` | `O(1)` |
| Or-optの適用 | `O(n)` | `O(1)` |
| Double Bridgeの評価 | `O(1)` | `O(1)` |
| Double Bridgeの適用 | `O(n)` | State内の `O(n)` 作業領域 |
| 全2-optの一巡 | `O(n²)` | `O(1)` |
| 候補制限付き一巡 | `O(n(c+1))` | `O(1)` |

候補制限付き局所探索の単純な上界は `O((A+1)n(c+1) + Σs_i)`、全探索版は `O((A+1)n² + Σs_i)` となる。

誘導局所探索について、有効な2-optの評価数を `Q`、候補表の参照数を `H`、罰則周回数を `R`、最良更新回数を `B`、罰した異なる辺数を `P` とする。候補表作成を除く時間は平均 `O(H + Q + Σs_i + Rn + (B+1)n)`、追加メモリは `O(n+P)` である。

`tsp2.py` 方式は、候補表作成を除いて平均 `O(Rn + H + Q + Σs_i + (B+1)n)`、追加メモリ `O(n+P)` である。全点を含む巡回路では `H=O(Rc)` となる。

同じ確定初期状態を `r` 個の焼きなまし状態へ複製する時間とメモリは `O(rn)` である。各状態で独立に誘導局所探索を行う場合は、探索時間と作業メモリも状態数倍になる。

距離行列を利用側で作る場合は、別に時間・メモリ `O(n²)` が必要となる。

複数巡回路の長さ `L` の区間移動・交換は、評価 `O(L)`、適用は関係する巡回路長に比例する。長さを1～3に制限する場合、評価は定数時間になる。

## 実装済みの確認

`test/tsp_test.cpp` では、小さい入力について各操作の費用差と全費用計算を照合する。操作後の逆引き位置、固定開始点、各点の一意性、複数巡回路の所属も確認する。比較用探索は、`tsp2.py` と同じ処理を直接書いた参照実装と複数の初期巡回順で照合する。

`test/tsp_sa_test.cpp` では、既定構築できないSA状態を先に構築してから `sa_run` へ渡し、初期2-optと焼きなましの遅延適用を組み合わせる。
