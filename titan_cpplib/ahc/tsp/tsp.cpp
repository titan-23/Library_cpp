/// https://github.com/titan-23/Library_cpp/blob/main/titan_cpplib/ahc/tsp/tsp.cpp
#pragma once
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
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

/// @brief TspState::make_two_opt が作る未適用の 2-opt 操作
template<class Cost>
class TspTwoOptMove {
public:
    TspTwoOptMove(const TspTwoOptMove&) = default;
    TspTwoOptMove(TspTwoOptMove&&) = default;
    TspTwoOptMove& operator=(const TspTwoOptMove&) = default;
    TspTwoOptMove& operator=(TspTwoOptMove&&) = default;
    /// @brief 適用したときの費用の差分を返す
    Cost delta() const { return delta_; }
private:
    const void* problem_id_;
    const TspState<Cost>* state_;
    uint64_t revision_;
    int left_;
    int right_;
    Cost delta_;

    TspTwoOptMove(const void* problem_id, const TspState<Cost>* state, uint64_t revision, int left, int right, Cost delta)
        : problem_id_(problem_id), state_(state), revision_(revision), left_(left), right_(right), delta_(move(delta)) {}
    template<class>
    friend class TspState;
};

/// @brief TspState::make_or_opt が作る未適用の Or-opt 操作
template<class Cost>
class TspOrOptMove {
public:
    TspOrOptMove(const TspOrOptMove&) = default;
    TspOrOptMove(TspOrOptMove&&) = default;
    TspOrOptMove& operator=(const TspOrOptMove&) = default;
    TspOrOptMove& operator=(TspOrOptMove&&) = default;
    /// @brief 適用したときの費用の差分を返す
    Cost delta() const { return delta_; }
private:
    const void* problem_id_;
    const TspState<Cost>* state_;
    uint64_t revision_;
    int first_;
    int length_;
    int after_;
    bool reversed_;
    Cost delta_;

    TspOrOptMove(const void* problem_id, const TspState<Cost>* state, uint64_t revision, int first, int length, int after, bool reversed, Cost delta)
        : problem_id_(problem_id), state_(state), revision_(revision), first_(first), length_(length), after_(after), reversed_(reversed), delta_(move(delta)) {}
    template<class>
    friend class TspState;
};

/// @brief TspState::make_double_bridge が作る未適用の Double Bridge 操作
template<class Cost>
class TspDoubleBridgeMove {
public:
    TspDoubleBridgeMove(const TspDoubleBridgeMove&) = default;
    TspDoubleBridgeMove(TspDoubleBridgeMove&&) = default;
    TspDoubleBridgeMove& operator=(const TspDoubleBridgeMove&) = default;
    TspDoubleBridgeMove& operator=(TspDoubleBridgeMove&&) = default;
    /// @brief 適用したときの費用の差分を返す
    Cost delta() const { return delta_; }
private:
    const void* problem_id_;
    const TspState<Cost>* state_;
    uint64_t revision_;
    int cut1_;
    int cut2_;
    int cut3_;
    int cut4_;
    Cost delta_;

    TspDoubleBridgeMove(const void* problem_id, const TspState<Cost>* state, uint64_t revision, int cut1, int cut2, int cut3, int cut4, Cost delta)
        : problem_id_(problem_id), state_(state), revision_(revision), cut1_(cut1), cut2_(cut2), cut3_(cut3), cut4_(cut4), delta_(move(delta)) {}
    template<class>
    friend class TspState;
};

/// @brief 対称 TSP の点数と辺費用関数を保持する / コピーと移動はできない
/// 辺費用は有限かつ 0 以上で、対称とする。問題は、そこから作った候補表と状態より後に破棄する
template<class EdgeCost>
class TspProblem {
public:
    using Cost = remove_cvref_t<invoke_result_t<const EdgeCost&, int, int>>;
    static_assert(is_arithmetic_v<Cost>, "TspProblem: Cost must be an arithmetic type");
    static_assert(numeric_limits<Cost>::is_signed, "TspProblem: Cost must be able to represent negative differences");

    /// @brief 点数 node_count と辺費用関数 edge_cost で構築する / node_count >= 1
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
    /// @brief 辺 (u, v) の費用を返す
    Cost edge_cost(int u, int v) const {
#ifdef TITAN_DEBUG
        if (u < 0 || u >= node_count_ || v < 0 || v >= node_count_) throw out_of_range("TspProblem::edge_cost: node is out of range");
#endif
        return edge_cost_(u, v);
    }
    /// @brief 巡回順 order の状態を作る / order は重複しない 1 点以上の点の列 / O(n)
    TspState<Cost> make_state(vector<int> order) const;
    /// @brief start から最近傍法で全点を巡る状態を作る / O(n^2)
    TspState<Cost> make_nearest_neighbor_state(int start) const;
    /// @brief start と nodes_to_visit の点だけを最近傍法で巡る状態を作る / nodes_to_visit は重複せず start を含まない
    TspState<Cost> make_nearest_neighbor_state(int start, span<const int> nodes_to_visit) const;
private:
    int node_count_;
    EdgeCost edge_cost_;
};

/// @brief 各点の近傍候補を辺費用の昇順に並べた表 / make_tsp_candidates で作る
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
    /// @brief node の候補を返す / 費用が同じなら点番号の小さい順
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

/// @brief 各点の近傍候補を c = min(requested_count, n - 1) 個ずつ求める / 平均 O(n^2 + nc log(c+1))
template<class EdgeCost>
TspCandidates make_tsp_candidates(const TspProblem<EdgeCost>& problem, int requested_count) {
    using Cost = typename TspProblem<EdgeCost>::Cost;
    if (requested_count < 0) throw invalid_argument("make_tsp_candidates: requested_count must be nonnegative");
    int n = problem.node_count();
    int candidate_count = min(requested_count, n - 1);
    vector<int> candidates;
    if (candidate_count == 0) return TspCandidates(n, 0, move(candidates), static_cast<const void*>(&problem));
    if ((size_t)n > candidates.max_size() / (size_t)candidate_count) throw length_error("make_tsp_candidates: candidate table is too large");
    candidates.reserve((size_t)n * (size_t)candidate_count);
    vector<pair<Cost, int>> nearest;
    nearest.reserve((size_t)n - 1);
    auto less_neighbor = [](const pair<Cost, int>& a, const pair<Cost, int>& b) {
        if (a.first < b.first) return true;
        if (b.first < a.first) return false;
        return a.second < b.second;
    };
    for (int u = 0; u < n; ++u) {
        nearest.clear();
        for (int v = 0; v < n; ++v) if (u != v) nearest.emplace_back(problem.edge_cost(u, v), v);
        if (candidate_count < n - 1) nth_element(nearest.begin(), nearest.begin() + candidate_count, nearest.end(), less_neighbor);
        sort(nearest.begin(), nearest.begin() + candidate_count, less_neighbor);
        for (int i = 0; i < candidate_count; ++i) candidates.push_back(nearest[i].second);
    }
    return TspCandidates(n, candidate_count, move(candidates), static_cast<const void*>(&problem));
}

/// @brief 対称 TSP の巡回順、各点の位置、巡回路の費用を持つ状態 / order()[0] は操作で変わらない
/// 操作は作成元の状態にだけ適用できる。状態を変更、代入、reset すると、それ以前に作った操作は無効になる
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
    /// @brief 巡回順を返す / 末尾に開始点を重ねない
    const vector<int>& order() const { return order_; }
    /// @brief node の巡回路内の位置を返す / 巡回路に含まれない点は -1
    int position(int node) const {
#ifdef TITAN_DEBUG
        if (node < 0 || node >= node_count()) throw out_of_range("TspState::position: node is out of range");
#endif
        return position_[node];
    }
    /// @brief 巡回路の費用を返す
    Cost total_cost() const { return total_cost_; }
    /// @brief problem から作った状態なら true を返す
    template<class EdgeCost>
    bool belongs_to(const TspProblem<EdgeCost>& problem) const {
        return problem_id_ == static_cast<const void*>(&problem);
    }

    /// @brief 区間 [left, right] を反転する 2-opt を作る / 作れない引数なら nullopt / O(1)
    template<class EdgeCost>
    optional<TspTwoOptMove<Cost>> make_two_opt(const TspProblem<EdgeCost>& problem, int left, int right) const;
    /// @brief 区間 [first, first + length) を変更前の位置 after の直後へ移す Or-opt を作る / reversed なら区間を反転して移す / O(1)
    template<class EdgeCost>
    optional<TspOrOptMove<Cost>> make_or_opt(const TspProblem<EdgeCost>& problem, int first, int length, int after, bool reversed) const;
    /// @brief 位置 cut1 < cut2 < cut3 < cut4 を始点とする 4 辺を切る Double Bridge を作る / O(1)
    template<class EdgeCost>
    optional<TspDoubleBridgeMove<Cost>> make_double_bridge(const TspProblem<EdgeCost>& problem, int cut1, int cut2, int cut3, int cut4) const;
    /// @brief 操作 move を適用する / 2-opt は O(反転区間の長さ)、Or-opt と Double Bridge は O(n)
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
    fill(visited.begin(), visited.end(), 0);
    visited[start] = 1;
    vector<int> order;
    order.reserve(nodes_to_visit.size() + 1);
    order.push_back(start);
    for (size_t step = 0; step < nodes_to_visit.size(); ++step) {
        int current = order.back();
        int best_node = -1;
        Cost best_cost{};
        for (int node : nodes_to_visit) if (!visited[node]) {
            Cost candidate_cost = edge_cost(current, node);
            if (best_node == -1 || candidate_cost < best_cost || (!(best_cost < candidate_cost) && node < best_node)) {
                best_node = node;
                best_cost = move(candidate_cost);
            }
        }
        if (best_node == -1) throw logic_error("TspProblem::make_nearest_neighbor_state: failed to select a node");
        visited[best_node] = 1;
        order.push_back(best_node);
    }
    return make_state(move(order));
}
}
