/// https://github.com/titan-23/Library_cpp/blob/main/titan_cpplib/ahc/tsp/tsp.cpp
#pragma once
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>
#include <stdexcept>
#include <type_traits>
#include <utility>
#include <vector>
using namespace std;
namespace titan23 {
template<class EdgeCost>
class TspProblem;
template<class Cost>
class TspState;

/// @brief TspState::make_two_opt が作る未適用の 2-opt 操作 / デフォルト構築した値は無効な操作
template<class Cost>
class TspTwoOptMove {
public:
    /// @brief 無効な操作を作る / make_ 系のメソッドは、作成できない引数に対してこの値を返す
    TspTwoOptMove() = default;
    TspTwoOptMove(const TspTwoOptMove&) = default;
    TspTwoOptMove(TspTwoOptMove&&) = default;
    TspTwoOptMove& operator=(const TspTwoOptMove&) = default;
    TspTwoOptMove& operator=(TspTwoOptMove&&) = default;
    /// @brief 適用できる操作なら true を返す
    bool valid() const { return state_ != nullptr; }
    /// @brief 適用したときのコストの差分を返す / 無効な操作では 0
    Cost delta() const { return delta_; }
private:
    const void* problem_id_ = nullptr;
    const TspState<Cost>* state_ = nullptr;
    uint64_t revision_ = 0;
    int left_ = 0;
    int right_ = 0;
    Cost delta_{};

    TspTwoOptMove(const void* problem_id, const TspState<Cost>* state, uint64_t revision, int left, int right, Cost delta)
        : problem_id_(problem_id), state_(state), revision_(revision), left_(left), right_(right), delta_(move(delta)) {}
    template<class>
    friend class TspState;
};

/// @brief TspState::make_or_opt が作る未適用の Or-opt 操作 / デフォルト構築した値は無効な操作
template<class Cost>
class TspOrOptMove {
public:
    /// @brief 無効な操作を作る / make_ 系のメソッドは、作成できない引数に対してこの値を返す
    TspOrOptMove() = default;
    TspOrOptMove(const TspOrOptMove&) = default;
    TspOrOptMove(TspOrOptMove&&) = default;
    TspOrOptMove& operator=(const TspOrOptMove&) = default;
    TspOrOptMove& operator=(TspOrOptMove&&) = default;
    /// @brief 適用できる操作なら true を返す
    bool valid() const { return state_ != nullptr; }
    /// @brief 適用したときのコストの差分を返す / 無効な操作では 0
    Cost delta() const { return delta_; }
private:
    const void* problem_id_ = nullptr;
    const TspState<Cost>* state_ = nullptr;
    uint64_t revision_ = 0;
    int first_ = 0;
    int length_ = 0;
    int after_ = 0;
    bool reversed_ = false;
    Cost delta_{};

    TspOrOptMove(const void* problem_id, const TspState<Cost>* state, uint64_t revision, int first, int length, int after, bool reversed, Cost delta)
        : problem_id_(problem_id), state_(state), revision_(revision), first_(first), length_(length), after_(after), reversed_(reversed), delta_(move(delta)) {}
    template<class>
    friend class TspState;
};

/// @brief TspState::make_double_bridge が作る未適用の Double Bridge 操作 / デフォルト構築した値は無効な操作
template<class Cost>
class TspDoubleBridgeMove {
public:
    /// @brief 無効な操作を作る / make_ 系のメソッドは、作成できない引数に対してこの値を返す
    TspDoubleBridgeMove() = default;
    TspDoubleBridgeMove(const TspDoubleBridgeMove&) = default;
    TspDoubleBridgeMove(TspDoubleBridgeMove&&) = default;
    TspDoubleBridgeMove& operator=(const TspDoubleBridgeMove&) = default;
    TspDoubleBridgeMove& operator=(TspDoubleBridgeMove&&) = default;
    /// @brief 適用できる操作なら true を返す
    bool valid() const { return state_ != nullptr; }
    /// @brief 適用したときのコストの差分を返す / 無効な操作では 0
    Cost delta() const { return delta_; }
private:
    const void* problem_id_ = nullptr;
    const TspState<Cost>* state_ = nullptr;
    uint64_t revision_ = 0;
    int cut1_ = 0;
    int cut2_ = 0;
    int cut3_ = 0;
    int cut4_ = 0;
    Cost delta_{};

    TspDoubleBridgeMove(const void* problem_id, const TspState<Cost>* state, uint64_t revision, int cut1, int cut2, int cut3, int cut4, Cost delta)
        : problem_id_(problem_id), state_(state), revision_(revision), cut1_(cut1), cut2_(cut2), cut3_(cut3), cut4_(cut4), delta_(move(delta)) {}
    template<class>
    friend class TspState;
};

/// @brief 対称 TSP の点数と辺のコストを返す関数を保持する / コピーと移動はできない
/// 辺のコストは有限かつ 0 以上で、対称とする
/// 問題は、そこから作った候補表と状態より後に破棄する
template<class EdgeCost>
class TspProblem {
public:
    using Cost = remove_cvref_t<invoke_result_t<const EdgeCost&, int, int>>;
    static_assert(is_arithmetic_v<Cost>, "TspProblem: Cost must be an arithmetic type");
    static_assert(numeric_limits<Cost>::is_signed, "TspProblem: Cost must be able to represent negative differences");

    /// @brief 点数 node_count と辺のコストを返す関数 edge_cost で構築する / node_count >= 1
    TspProblem(int node_count, EdgeCost edge_cost)
        : node_count_(node_count), edge_cost_(move(edge_cost)) {
        if (node_count_ <= 0) throw invalid_argument("TspProblem: node_count must be positive");
    }
    TspProblem(const TspProblem&) = delete;
    TspProblem& operator=(const TspProblem&) = delete;
    TspProblem(TspProblem&&) = delete;
    TspProblem& operator=(TspProblem&&) = delete;

    /// @brief 点数を返す
    int node_count() const { return node_count_; }
    /// @brief 辺 (u, v) のコストを返す
    Cost edge_cost(int u, int v) const {
#ifdef TITAN_DEBUG
        if (u < 0 || u >= node_count_ || v < 0 || v >= node_count_) throw out_of_range("TspProblem::edge_cost: node is out of range");
#endif
        return edge_cost_(u, v);
    }
    /// @brief 巡回順 order の状態を作る / order は重複しない 1 点以上の点の列 / O(n)
    TspState<Cost> make_state(vector<int> order) const;
    /// @brief start から最近傍法で全点を巡る状態を作る / 同じコストなら点番号の小さい点を選ぶ / O(n^2)
    TspState<Cost> make_nearest_neighbor_state(int start) const;
    /// @brief start と nodes_to_visit の点だけを最近傍法で巡る状態を作る / nodes_to_visit は重複せず start を含まない
    TspState<Cost> make_nearest_neighbor_state(int start, span<const int> nodes_to_visit) const;
private:
    int node_count_;
    EdgeCost edge_cost_;
};

/// @brief 各点の近傍候補を辺のコストの昇順に並べた表 / make_tsp_candidates で作る
class TspCandidates {
public:
    TspCandidates(const TspCandidates&) = default;
    TspCandidates(TspCandidates&& other) noexcept
        : node_count_(other.node_count_), candidate_count_(other.candidate_count_), candidates_(move(other.candidates_)), problem_id_(other.problem_id_) {
        other.node_count_ = 0;
        other.candidate_count_ = 0;
        other.problem_id_ = nullptr;
    }
    TspCandidates& operator=(const TspCandidates& other) {
        if (this == &other) return *this;
        vector<int> copied = other.candidates_;
        candidates_ = move(copied);
        node_count_ = other.node_count_;
        candidate_count_ = other.candidate_count_;
        problem_id_ = other.problem_id_;
        return *this;
    }
    TspCandidates& operator=(TspCandidates&& other) noexcept {
        if (this == &other) return *this;
        node_count_ = other.node_count_;
        candidate_count_ = other.candidate_count_;
        candidates_ = move(other.candidates_);
        problem_id_ = other.problem_id_;
        other.node_count_ = 0;
        other.candidate_count_ = 0;
        other.problem_id_ = nullptr;
        return *this;
    }

    /// @brief 点数を返す
    int node_count() const { return node_count_; }
    /// @brief 1 点あたりの候補数を返す
    int candidate_count() const { return candidate_count_; }
    /// @brief node の候補を返す / コストが同じなら点番号の小さい順
    span<const int> operator[](int node) const {
#ifdef TITAN_DEBUG
        if (node < 0 || node >= node_count_) throw out_of_range("TspCandidates::operator[]: node is out of range");
#endif
        if (candidate_count_ == 0) return {};
        size_t offset = (size_t)node * (size_t)candidate_count_;
        return span<const int>(candidates_.data() + offset, (size_t)candidate_count_);
    }
    /// @brief problem から作った表なら true を返す
    template<class EdgeCost>
    bool belongs_to(const TspProblem<EdgeCost>& problem) const {
        return problem_id_ == static_cast<const void*>(&problem);
    }
private:
    int node_count_;
    int candidate_count_;
    vector<int> candidates_;
    const void* problem_id_;

    TspCandidates(int node_count, int candidate_count, vector<int> candidates, const void* problem_id)
        : node_count_(node_count), candidate_count_(candidate_count), candidates_(move(candidates)), problem_id_(problem_id) {}
    template<class EdgeCost>
    friend TspCandidates make_tsp_candidates(const TspProblem<EdgeCost>& problem, int requested_count);
};

/// @brief 各点の近傍候補を c = min(requested_count, n - 1) 個ずつ求める / O(n^2) 回の edge_cost の呼び出しと、最悪 O(n^2 c) の挿入
template<class EdgeCost>
TspCandidates make_tsp_candidates(const TspProblem<EdgeCost>& problem, int requested_count) {
    using Cost = typename TspProblem<EdgeCost>::Cost;
    if (requested_count < 0) throw invalid_argument("make_tsp_candidates: requested_count must be nonnegative");
    int n = problem.node_count();
    int candidate_count = min(requested_count, n - 1);
    vector<int> candidates;
    if (candidate_count == 0) return TspCandidates(n, 0, move(candidates), static_cast<const void*>(&problem));
    if ((size_t)n > candidates.max_size() / (size_t)candidate_count) throw length_error("make_tsp_candidates: candidate table is too large");
    candidates.resize((size_t)n * (size_t)candidate_count);
    // コストの昇順に並べた上位 candidate_count 個を保持し、点番号の昇順に走査して挿入する
    // コストが同じ点は後から挿入しないため、点番号の小さい順になる
    vector<Cost> nearest_cost((size_t)candidate_count);
    for (int u = 0; u < n; ++u) {
        int* nearest = candidates.data() + (size_t)u * (size_t)candidate_count;
        int count = 0;
        for (int v = 0; v < n; ++v) {
            if (u == v) continue;
            Cost cost = problem.edge_cost(u, v);
            if (count == candidate_count && !(cost < nearest_cost[count - 1])) continue;
            int i = count < candidate_count ? count++ : count - 1;
            while (i > 0 && cost < nearest_cost[i - 1]) {
                nearest_cost[i] = nearest_cost[i - 1];
                nearest[i] = nearest[i - 1];
                --i;
            }
            nearest_cost[i] = cost;
            nearest[i] = v;
        }
    }
    return TspCandidates(n, candidate_count, move(candidates), static_cast<const void*>(&problem));
}

/// @brief 対称 TSP の巡回順、各点の位置、巡回路のコストを保持する状態 / order()[0] は操作で変化しない
/// 操作は作成元の状態にだけ適用できる
/// 状態を変更、代入、reset すると、それ以前に作った操作は無効になる
template<class Cost>
class TspState {
public:
    using CostType = Cost;

    TspState(const TspState& other)
        : order_(other.order_), position_(other.position_), total_cost_(other.total_cost_), work_buffer_(other.work_buffer_), problem_id_(other.problem_id_), revision_(0) {}
    TspState(TspState&& other) noexcept
        : order_(move(other.order_)), position_(move(other.position_)), total_cost_(move(other.total_cost_)), work_buffer_(move(other.work_buffer_)), problem_id_(other.problem_id_), revision_(0) {}
    TspState& operator=(const TspState& other) {
        if (this == &other) return *this;
        if (revision_ == numeric_limits<uint64_t>::max()) throw overflow_error("TspState::operator=: revision overflow");
        TspState replacement(other);
        order_ = move(replacement.order_);
        position_ = move(replacement.position_);
        total_cost_ = replacement.total_cost_;
        work_buffer_ = move(replacement.work_buffer_);
        problem_id_ = replacement.problem_id_;
        ++revision_;
        return *this;
    }
    TspState& operator=(TspState&& other) {
        if (this == &other) return *this;
        if (revision_ == numeric_limits<uint64_t>::max()) throw overflow_error("TspState::operator=: revision overflow");
        order_ = move(other.order_);
        position_ = move(other.position_);
        total_cost_ = move(other.total_cost_);
        work_buffer_ = move(other.work_buffer_);
        problem_id_ = other.problem_id_;
        ++revision_;
        return *this;
    }

    /// @brief 問題の点数を返す
    int node_count() const { return (int)position_.size(); }
    /// @brief 巡回路に含まれる点数を返す
    int size() const { return (int)order_.size(); }
    /// @brief 巡回順を返す / 末尾に開始点を再び含めない
    const vector<int>& order() const { return order_; }
    /// @brief node の巡回路内の位置を返す / 巡回路に含まれない点は -1
    int position(int node) const {
#ifdef TITAN_DEBUG
        if (node < 0 || node >= node_count()) throw out_of_range("TspState::position: node is out of range");
#endif
        return position_[node];
    }
    /// @brief 巡回路のコストを返す
    Cost total_cost() const { return total_cost_; }
    /// @brief problem から作った状態なら true を返す
    template<class EdgeCost>
    bool belongs_to(const TspProblem<EdgeCost>& problem) const {
        return problem_id_ == static_cast<const void*>(&problem);
    }

    /// @brief 区間 [left, right] を反転する 2-opt を作る / 作成できない引数なら valid() が false の操作を返す / O(1)
    template<class EdgeCost>
    TspTwoOptMove<Cost> make_two_opt(const TspProblem<EdgeCost>& problem, int left, int right) const;
    /// @brief 区間 [first, first + length) を変更前の位置 after の直後へ移動する Or-opt を作る / reversed なら区間を反転して移動する / O(1)
    template<class EdgeCost>
    TspOrOptMove<Cost> make_or_opt(const TspProblem<EdgeCost>& problem, int first, int length, int after, bool reversed) const;
    /// @brief 位置 cut1 < cut2 < cut3 < cut4 を始点とする 4 辺を切る Double Bridge を作る / O(1)
    template<class EdgeCost>
    TspDoubleBridgeMove<Cost> make_double_bridge(const TspProblem<EdgeCost>& problem, int cut1, int cut2, int cut3, int cut4) const;
    /// @brief 操作 move を適用する / O(位置が変化する区間の長さ)
    template<class EdgeCost, class Move>
    void apply(const TspProblem<EdgeCost>& problem, const Move& move);
    /// @brief 巡回順全体を order に置き換える / O(n)
    template<class EdgeCost>
    void reset(const TspProblem<EdgeCost>& problem, vector<int> order) {
        static_assert(is_same_v<Cost, typename TspProblem<EdgeCost>::Cost>);
        if (problem_id_ != static_cast<const void*>(&problem)) throw invalid_argument("TspState::reset: state and problem do not match");
        if (revision_ == numeric_limits<uint64_t>::max()) throw overflow_error("TspState::reset: revision overflow");
        TspState replacement(problem, move(order));
        order_ = move(replacement.order_);
        position_ = move(replacement.position_);
        total_cost_ = move(replacement.total_cost_);
        work_buffer_ = move(replacement.work_buffer_);
        ++revision_;
    }
private:
    vector<int> order_;
    vector<int> position_;
    Cost total_cost_;
    vector<int> work_buffer_;
    const void* problem_id_;
    uint64_t revision_;

    template<class EdgeCost>
    TspState(const TspProblem<EdgeCost>& problem, vector<int> order)
        : order_(move(order)), position_((size_t)problem.node_count(), -1), total_cost_{}, work_buffer_(order_.size()), problem_id_(static_cast<const void*>(&problem)), revision_(0) {
        static_assert(is_same_v<Cost, typename TspProblem<EdgeCost>::Cost>);
        if (order_.empty()) throw invalid_argument("TspState: order must not be empty");
        if (order_.size() > position_.size()) throw invalid_argument("TspState: order contains too many nodes");
        for (int i = 0; i < (int)order_.size(); ++i) {
            int node = order_[i];
            if (node < 0 || node >= (int)position_.size()) throw invalid_argument("TspState: order contains an out-of-range node");
            if (position_[node] != -1) throw invalid_argument("TspState: order contains a duplicate node");
            position_[node] = i;
        }
        if (order_.size() >= 2) {
            for (int i = 0; i < (int)order_.size(); ++i) total_cost_ += problem.edge_cost(order_[i], order_[(i + 1) % order_.size()]);
        }
    }
    template<class>
    friend class TspProblem;
    friend class TspTwoOptMove<Cost>;
    friend class TspOrOptMove<Cost>;
    friend class TspDoubleBridgeMove<Cost>;
};

template<class EdgeCost>
TspState<typename TspProblem<EdgeCost>::Cost> TspProblem<EdgeCost>::make_state(vector<int> order) const {
    return TspState<Cost>(*this, move(order));
}

template<class EdgeCost>
TspState<typename TspProblem<EdgeCost>::Cost> TspProblem<EdgeCost>::make_nearest_neighbor_state(int start) const {
    if (start < 0 || start >= node_count_) throw invalid_argument("TspProblem::make_nearest_neighbor_state: start is out of range");
    vector<int> nodes;
    nodes.reserve((size_t)node_count_ - 1);
    for (int node = 0; node < node_count_; ++node) if (node != start) nodes.push_back(node);
    return make_nearest_neighbor_state(start, nodes);
}

template<class EdgeCost>
TspState<typename TspProblem<EdgeCost>::Cost> TspProblem<EdgeCost>::make_nearest_neighbor_state(int start, span<const int> nodes_to_visit) const {
    if (start < 0 || start >= node_count_) throw invalid_argument("TspProblem::make_nearest_neighbor_state: start is out of range");
    if (nodes_to_visit.size() > (size_t)node_count_ - 1) throw invalid_argument("TspProblem::make_nearest_neighbor_state: too many nodes to visit");
    vector<unsigned char> visited((size_t)node_count_, 0);
    visited[start] = 1;
    for (int node : nodes_to_visit) {
        if (node < 0 || node >= node_count_) throw invalid_argument("TspProblem::make_nearest_neighbor_state: node is out of range");
        if (visited[node]) throw invalid_argument("TspProblem::make_nearest_neighbor_state: nodes must be distinct and exclude start");
        visited[node] = 1;
    }
    // 未訪問の点だけを remaining に保持し、選択した点は末尾の点と入れ替えて取り除く
    vector<int> remaining(nodes_to_visit.begin(), nodes_to_visit.end());
    vector<int> order;
    order.reserve(nodes_to_visit.size() + 1);
    order.push_back(start);
    while (!remaining.empty()) {
        int current = order.back();
        size_t best_index = 0;
        int best_node = -1;
        Cost best_cost{};
        for (size_t i = 0; i < remaining.size(); ++i) {
            int node = remaining[i];
            Cost candidate_cost = edge_cost(current, node);
            if (best_node == -1 || candidate_cost < best_cost || (!(best_cost < candidate_cost) && node < best_node)) {
                best_index = i;
                best_node = node;
                best_cost = move(candidate_cost);
            }
        }
        remaining[best_index] = remaining.back();
        remaining.pop_back();
        order.push_back(best_node);
    }
    return make_state(move(order));
}
}
