#pragma once
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <stdexcept>
#include <type_traits>
#include <utility>
#include <vector>
#include "titan_cpplib/ahc/tsp/tsp.cpp"
using namespace std;
namespace titan23 {
template<class Cost>
class MultipleTspState;

template<class EdgeCost>
MultipleTspState<typename TspProblem<EdgeCost>::Cost> make_multiple_tsp_state(const TspProblem<EdgeCost>& problem, vector<int> depots, vector<vector<int>> routes);

template<class Cost>
class MultipleTspTwoOptMove {
public:
    MultipleTspTwoOptMove(const MultipleTspTwoOptMove&) = default;
    MultipleTspTwoOptMove(MultipleTspTwoOptMove&&) = default;
    MultipleTspTwoOptMove& operator=(const MultipleTspTwoOptMove&) = default;
    MultipleTspTwoOptMove& operator=(MultipleTspTwoOptMove&&) = default;
    int route() const { return route_; }
    Cost delta() const { return delta_; }
private:
    const void* problem_id_;
    const MultipleTspState<Cost>* state_;
    uint64_t revision_;
    int route_;
    int left_;
    int right_;
    Cost delta_;

    MultipleTspTwoOptMove(const void* problem_id, const MultipleTspState<Cost>* state, uint64_t revision, int route, int left, int right, Cost delta)
        : problem_id_(problem_id), state_(state), revision_(revision), route_(route), left_(left), right_(right), delta_(move(delta)) {}
    friend class MultipleTspState<Cost>;
};

template<class Cost>
class MultipleTspBlockShiftMove {
public:
    MultipleTspBlockShiftMove(const MultipleTspBlockShiftMove&) = default;
    MultipleTspBlockShiftMove(MultipleTspBlockShiftMove&&) = default;
    MultipleTspBlockShiftMove& operator=(const MultipleTspBlockShiftMove&) = default;
    MultipleTspBlockShiftMove& operator=(MultipleTspBlockShiftMove&&) = default;
    int source_route() const { return source_route_; }
    int target_route() const { return target_route_; }
    Cost source_delta() const { return source_delta_; }
    Cost target_delta() const { return target_delta_; }
private:
    const void* problem_id_;
    const MultipleTspState<Cost>* state_;
    uint64_t revision_;
    int source_route_;
    int first_;
    int length_;
    int target_route_;
    int after_;
    Cost source_delta_;
    Cost target_delta_;

    MultipleTspBlockShiftMove(const void* problem_id, const MultipleTspState<Cost>* state, uint64_t revision, int source_route, int first, int length, int target_route, int after, Cost source_delta, Cost target_delta)
        : problem_id_(problem_id), state_(state), revision_(revision), source_route_(source_route), first_(first), length_(length), target_route_(target_route), after_(after), source_delta_(move(source_delta)), target_delta_(move(target_delta)) {}
    friend class MultipleTspState<Cost>;
};

template<class Cost>
class MultipleTspBlockSwapMove {
public:
    MultipleTspBlockSwapMove(const MultipleTspBlockSwapMove&) = default;
    MultipleTspBlockSwapMove(MultipleTspBlockSwapMove&&) = default;
    MultipleTspBlockSwapMove& operator=(const MultipleTspBlockSwapMove&) = default;
    MultipleTspBlockSwapMove& operator=(MultipleTspBlockSwapMove&&) = default;
    int route1() const { return route1_; }
    int route2() const { return route2_; }
    Cost delta1() const { return delta1_; }
    Cost delta2() const { return delta2_; }
private:
    const void* problem_id_;
    const MultipleTspState<Cost>* state_;
    uint64_t revision_;
    int route1_;
    int first1_;
    int length1_;
    int route2_;
    int first2_;
    int length2_;
    Cost delta1_;
    Cost delta2_;

    MultipleTspBlockSwapMove(const void* problem_id, const MultipleTspState<Cost>* state, uint64_t revision, int route1, int first1, int length1, int route2, int first2, int length2, Cost delta1, Cost delta2)
        : problem_id_(problem_id), state_(state), revision_(revision), route1_(route1), first1_(first1), length1_(length1), route2_(route2), first2_(first2), length2_(length2), delta1_(move(delta1)), delta2_(move(delta2)) {}
    friend class MultipleTspState<Cost>;
};

template<class Cost>
class MultipleTspState {
public:
    using CostType = Cost;

    MultipleTspState(const MultipleTspState& other)
        : routes_(other.routes_), depots_(other.depots_), route_of_(other.route_of_), position_(other.position_), route_costs_(other.route_costs_), work_buffer1_(other.work_buffer1_), work_buffer2_(other.work_buffer2_), problem_id_(other.problem_id_), revision_(0) {}
    MultipleTspState(MultipleTspState&& other) noexcept
        : routes_(move(other.routes_)), depots_(move(other.depots_)), route_of_(move(other.route_of_)), position_(move(other.position_)), route_costs_(move(other.route_costs_)), work_buffer1_(move(other.work_buffer1_)), work_buffer2_(move(other.work_buffer2_)), problem_id_(other.problem_id_), revision_(0) {}
    MultipleTspState& operator=(const MultipleTspState& other) {
        if (this == &other) return *this;
        if (revision_ == numeric_limits<uint64_t>::max()) throw overflow_error("MultipleTspState::operator=: revision overflow");
        MultipleTspState replacement(other);
        routes_ = move(replacement.routes_);
        depots_ = move(replacement.depots_);
        route_of_ = move(replacement.route_of_);
        position_ = move(replacement.position_);
        route_costs_ = move(replacement.route_costs_);
        work_buffer1_ = move(replacement.work_buffer1_);
        work_buffer2_ = move(replacement.work_buffer2_);
        problem_id_ = replacement.problem_id_;
        ++revision_;
        return *this;
    }
    MultipleTspState& operator=(MultipleTspState&& other) {
        if (this == &other) return *this;
        if (revision_ == numeric_limits<uint64_t>::max()) throw overflow_error("MultipleTspState::operator=: revision overflow");
        routes_ = move(other.routes_);
        depots_ = move(other.depots_);
        route_of_ = move(other.route_of_);
        position_ = move(other.position_);
        route_costs_ = move(other.route_costs_);
        work_buffer1_ = move(other.work_buffer1_);
        work_buffer2_ = move(other.work_buffer2_);
        problem_id_ = other.problem_id_;
        ++revision_;
        return *this;
    }

    int node_count() const { return (int)route_of_.size(); }
    int route_count() const { return (int)routes_.size(); }
    const vector<int>& route(int route_id) const {
#ifdef TITAN_DEBUG
        if (route_id < 0 || route_id >= route_count()) throw out_of_range("MultipleTspState::route: route is out of range");
#endif
        return routes_[route_id];
    }
    int route_of(int node) const {
#ifdef TITAN_DEBUG
        if (node < 0 || node >= node_count()) throw out_of_range("MultipleTspState::route_of: node is out of range");
#endif
        return route_of_[node];
    }
    int position(int node) const {
#ifdef TITAN_DEBUG
        if (node < 0 || node >= node_count()) throw out_of_range("MultipleTspState::position: node is out of range");
#endif
        return position_[node];
    }
    Cost route_cost(int route_id) const {
#ifdef TITAN_DEBUG
        if (route_id < 0 || route_id >= route_count()) throw out_of_range("MultipleTspState::route_cost: route is out of range");
#endif
        return route_costs_[route_id];
    }

    template<class EdgeCost>
    optional<MultipleTspTwoOptMove<Cost>> make_two_opt(const TspProblem<EdgeCost>& problem, int route, int left, int right) const;
    template<class EdgeCost>
    optional<MultipleTspBlockShiftMove<Cost>> make_block_shift(const TspProblem<EdgeCost>& problem, int source_route, int first, int length, int target_route, int after) const;
    template<class EdgeCost>
    optional<MultipleTspBlockSwapMove<Cost>> make_block_swap(const TspProblem<EdgeCost>& problem, int route1, int first1, int length1, int route2, int first2, int length2) const;
    template<class EdgeCost, class Move>
    void apply(const TspProblem<EdgeCost>& problem, const Move& move);
private:
    vector<vector<int>> routes_;
    vector<int> depots_;
    vector<int> route_of_;
    vector<int> position_;
    vector<Cost> route_costs_;
    vector<int> work_buffer1_;
    vector<int> work_buffer2_;
    const void* problem_id_;
    uint64_t revision_;

    template<class EdgeCost>
    MultipleTspState(const TspProblem<EdgeCost>& problem, vector<int> depots, vector<vector<int>> routes)
        : routes_(move(routes)), depots_(move(depots)), route_of_((size_t)problem.node_count(), -1), position_((size_t)problem.node_count(), -1), route_costs_(routes_.size(), Cost{}), problem_id_(static_cast<const void*>(&problem)), revision_(0) {
        static_assert(is_same_v<Cost, typename TspProblem<EdgeCost>::Cost>);
        if (routes_.empty()) throw invalid_argument("MultipleTspState: routes must not be empty");
        if (routes_.size() != depots_.size()) throw invalid_argument("MultipleTspState: depots and routes must have the same size");
        if (routes_.size() > route_of_.size()) throw invalid_argument("MultipleTspState: too many routes");
        vector<int> depot_route(route_of_.size(), -1);
        for (int route = 0; route < route_count(); ++route) {
            int depot = depots_[route];
            if (depot < 0 || depot >= node_count()) throw invalid_argument("MultipleTspState: depot is out of range");
            if (depot_route[depot] != -1) throw invalid_argument("MultipleTspState: depots must be distinct");
            depot_route[depot] = route;
        }
        for (int route = 0; route < route_count(); ++route) {
            if (routes_[route].empty()) throw invalid_argument("MultipleTspState: route must contain its depot");
            if (routes_[route].size() > route_of_.size()) throw invalid_argument("MultipleTspState: route contains too many nodes");
            if (routes_[route][0] != depots_[route]) throw invalid_argument("MultipleTspState: route must start at its depot");
            for (int position = 0; position < (int)routes_[route].size(); ++position) {
                int node = routes_[route][position];
                if (node < 0 || node >= node_count()) throw invalid_argument("MultipleTspState: route contains an out-of-range node");
                if (route_of_[node] != -1) throw invalid_argument("MultipleTspState: routes contain a duplicate node");
                if (depot_route[node] != -1 && (position != 0 || depot_route[node] != route)) throw invalid_argument("MultipleTspState: depot belongs only to its own route");
                route_of_[node] = route;
                position_[node] = position;
            }
        }
        for (int node = 0; node < node_count(); ++node) if (route_of_[node] == -1) throw invalid_argument("MultipleTspState: every node must belong to a route");
        for (int route = 0; route < route_count(); ++route) {
            int size = (int)routes_[route].size();
            if (size < 2) continue;
            for (int i = 0; i < size; ++i) route_costs_[route] += problem.edge_cost(routes_[route][i], routes_[route][(i + 1) % size]);
        }
    }
    template<class EdgeCost>
    friend MultipleTspState<typename TspProblem<EdgeCost>::Cost> make_multiple_tsp_state(const TspProblem<EdgeCost>& problem, vector<int> depots, vector<vector<int>> routes);
    friend class MultipleTspTwoOptMove<Cost>;
    friend class MultipleTspBlockShiftMove<Cost>;
    friend class MultipleTspBlockSwapMove<Cost>;
};

template<class EdgeCost>
MultipleTspState<typename TspProblem<EdgeCost>::Cost> make_multiple_tsp_state(const TspProblem<EdgeCost>& problem, vector<int> depots, vector<vector<int>> routes) {
    using Cost = typename TspProblem<EdgeCost>::Cost;
    return MultipleTspState<Cost>(problem, move(depots), move(routes));
}

template<class Cost>
template<class EdgeCost>
optional<MultipleTspTwoOptMove<Cost>> MultipleTspState<Cost>::make_two_opt(const TspProblem<EdgeCost>& problem, int route, int left, int right) const {
    static_assert(is_same_v<Cost, typename TspProblem<EdgeCost>::Cost>);
    if (problem_id_ != static_cast<const void*>(&problem)) throw invalid_argument("MultipleTspState::make_two_opt: state and problem do not match");
    if (route < 0 || route >= route_count()) return nullopt;
    int size = (int)routes_[route].size();
    if (size < 4 || left < 1 || left >= right || right >= size) return nullopt;
    if (left == 1 && right == size - 1) return nullopt;
    int before = routes_[route][left - 1];
    int first = routes_[route][left];
    int last = routes_[route][right];
    int after = routes_[route][(right + 1) % size];
    Cost delta{};
    delta += problem.edge_cost(before, last);
    delta += problem.edge_cost(first, after);
    delta -= problem.edge_cost(before, first);
    delta -= problem.edge_cost(last, after);
    return MultipleTspTwoOptMove<Cost>(problem_id_, this, revision_, route, left, right, move(delta));
}

template<class Cost>
template<class EdgeCost>
optional<MultipleTspBlockShiftMove<Cost>> MultipleTspState<Cost>::make_block_shift(const TspProblem<EdgeCost>& problem, int source_route, int first, int length, int target_route, int after) const {
    static_assert(is_same_v<Cost, typename TspProblem<EdgeCost>::Cost>);
    if (problem_id_ != static_cast<const void*>(&problem)) throw invalid_argument("MultipleTspState::make_block_shift: state and problem do not match");
    if (source_route < 0 || source_route >= route_count() || target_route < 0 || target_route >= route_count() || source_route == target_route) return nullopt;
    int source_size = (int)routes_[source_route].size();
    int target_size = (int)routes_[target_route].size();
    if (first < 1 || first >= source_size || length <= 0 || length > source_size - first || after < 0 || after >= target_size) return nullopt;
    int past_last = first + length;
    int block_first = routes_[source_route][first];
    int block_last = routes_[source_route][past_last - 1];
    Cost inside{};
    for (int i = first; i + 1 < past_last; ++i) inside += problem.edge_cost(routes_[source_route][i], routes_[source_route][i + 1]);
    Cost source_delta{};
    if (source_size == length + 1) {
        source_delta -= route_costs_[source_route];
    } else {
        int before = routes_[source_route][first - 1];
        int after_block = routes_[source_route][past_last % source_size];
        source_delta += problem.edge_cost(before, after_block);
        source_delta -= problem.edge_cost(before, block_first);
        source_delta -= inside;
        source_delta -= problem.edge_cost(block_last, after_block);
    }
    int destination = routes_[target_route][after];
    Cost target_delta{};
    if (target_size == 1) {
        target_delta += problem.edge_cost(destination, block_first);
        target_delta += inside;
        target_delta += problem.edge_cost(block_last, destination);
    } else {
        int after_destination = routes_[target_route][(after + 1) % target_size];
        target_delta += problem.edge_cost(destination, block_first);
        target_delta += inside;
        target_delta += problem.edge_cost(block_last, after_destination);
        target_delta -= problem.edge_cost(destination, after_destination);
    }
    return MultipleTspBlockShiftMove<Cost>(problem_id_, this, revision_, source_route, first, length, target_route, after, move(source_delta), move(target_delta));
}

template<class Cost>
template<class EdgeCost>
optional<MultipleTspBlockSwapMove<Cost>> MultipleTspState<Cost>::make_block_swap(const TspProblem<EdgeCost>& problem, int route1, int first1, int length1, int route2, int first2, int length2) const {
    static_assert(is_same_v<Cost, typename TspProblem<EdgeCost>::Cost>);
    if (problem_id_ != static_cast<const void*>(&problem)) throw invalid_argument("MultipleTspState::make_block_swap: state and problem do not match");
    if (route1 < 0 || route1 >= route_count() || route2 < 0 || route2 >= route_count() || route1 == route2) return nullopt;
    int size1 = (int)routes_[route1].size();
    int size2 = (int)routes_[route2].size();
    if (first1 < 1 || first1 >= size1 || length1 <= 0 || length1 > size1 - first1) return nullopt;
    if (first2 < 1 || first2 >= size2 || length2 <= 0 || length2 > size2 - first2) return nullopt;
    int past_last1 = first1 + length1;
    int past_last2 = first2 + length2;
    int before1 = routes_[route1][first1 - 1];
    int block_first1 = routes_[route1][first1];
    int block_last1 = routes_[route1][past_last1 - 1];
    int after1 = routes_[route1][past_last1 % size1];
    int before2 = routes_[route2][first2 - 1];
    int block_first2 = routes_[route2][first2];
    int block_last2 = routes_[route2][past_last2 - 1];
    int after2 = routes_[route2][past_last2 % size2];
    Cost inside1{};
    for (int i = first1; i + 1 < past_last1; ++i) inside1 += problem.edge_cost(routes_[route1][i], routes_[route1][i + 1]);
    Cost inside2{};
    for (int i = first2; i + 1 < past_last2; ++i) inside2 += problem.edge_cost(routes_[route2][i], routes_[route2][i + 1]);
    Cost delta1{};
    delta1 += problem.edge_cost(before1, block_first2);
    delta1 += inside2;
    delta1 += problem.edge_cost(block_last2, after1);
    delta1 -= problem.edge_cost(before1, block_first1);
    delta1 -= inside1;
    delta1 -= problem.edge_cost(block_last1, after1);
    Cost delta2{};
    delta2 += problem.edge_cost(before2, block_first1);
    delta2 += inside1;
    delta2 += problem.edge_cost(block_last1, after2);
    delta2 -= problem.edge_cost(before2, block_first2);
    delta2 -= inside2;
    delta2 -= problem.edge_cost(block_last2, after2);
    return MultipleTspBlockSwapMove<Cost>(problem_id_, this, revision_, route1, first1, length1, route2, first2, length2, move(delta1), move(delta2));
}

template<class Cost>
template<class EdgeCost, class Move>
void MultipleTspState<Cost>::apply(const TspProblem<EdgeCost>& problem, const Move& move) {
    static_assert(is_same_v<Cost, typename TspProblem<EdgeCost>::Cost>);
    using MoveType = remove_cvref_t<Move>;
    static_assert(is_same_v<MoveType, MultipleTspTwoOptMove<Cost>> || is_same_v<MoveType, MultipleTspBlockShiftMove<Cost>> || is_same_v<MoveType, MultipleTspBlockSwapMove<Cost>>);
    const void* problem_id = static_cast<const void*>(&problem);
    if (problem_id_ != problem_id || move.problem_id_ != problem_id) throw invalid_argument("MultipleTspState::apply: state, move, and problem do not match");
    if (move.state_ != this) throw logic_error("MultipleTspState::apply: move was created from another state");
    if (move.revision_ != revision_) throw logic_error("MultipleTspState::apply: move is stale");
    if (revision_ == numeric_limits<uint64_t>::max()) throw overflow_error("MultipleTspState::apply: revision overflow");
    if constexpr (is_same_v<MoveType, MultipleTspTwoOptMove<Cost>>) {
        vector<int>& route = routes_[move.route_];
        reverse(route.begin() + move.left_, route.begin() + move.right_ + 1);
        for (int i = move.left_; i <= move.right_; ++i) position_[route[i]] = i;
        route_costs_[move.route_] += move.delta_;
    } else if constexpr (is_same_v<MoveType, MultipleTspBlockShiftMove<Cost>>) {
        vector<int>& source = routes_[move.source_route_];
        vector<int>& target = routes_[move.target_route_];
        size_t needed = target.size() + (size_t)move.length_;
        if (target.capacity() < needed) {
            size_t grown = target.capacity() + target.capacity() / 2 + 1;
            target.reserve(max(needed, grown));
        }
        target.insert(target.begin() + move.after_ + 1, source.begin() + move.first_, source.begin() + move.first_ + move.length_);
        source.erase(source.begin() + move.first_, source.begin() + move.first_ + move.length_);
        for (int i = move.first_; i < (int)source.size(); ++i) {
            route_of_[source[i]] = move.source_route_;
            position_[source[i]] = i;
        }
        for (int i = move.after_ + 1; i < (int)target.size(); ++i) {
            route_of_[target[i]] = move.target_route_;
            position_[target[i]] = i;
        }
        route_costs_[move.source_route_] += move.source_delta_;
        route_costs_[move.target_route_] += move.target_delta_;
    } else {
        vector<int>& route1 = routes_[move.route1_];
        vector<int>& route2 = routes_[move.route2_];
        work_buffer1_.assign(route1.begin() + move.first1_, route1.begin() + move.first1_ + move.length1_);
        work_buffer2_.assign(route2.begin() + move.first2_, route2.begin() + move.first2_ + move.length2_);
        size_t new_size1 = route1.size() - work_buffer1_.size() + work_buffer2_.size();
        size_t new_size2 = route2.size() - work_buffer2_.size() + work_buffer1_.size();
        if (route1.capacity() < new_size1) {
            size_t grown = route1.capacity() + route1.capacity() / 2 + 1;
            route1.reserve(max(new_size1, grown));
        }
        if (route2.capacity() < new_size2) {
            size_t grown = route2.capacity() + route2.capacity() / 2 + 1;
            route2.reserve(max(new_size2, grown));
        }
        route1.erase(route1.begin() + move.first1_, route1.begin() + move.first1_ + move.length1_);
        route2.erase(route2.begin() + move.first2_, route2.begin() + move.first2_ + move.length2_);
        route1.insert(route1.begin() + move.first1_, work_buffer2_.begin(), work_buffer2_.end());
        route2.insert(route2.begin() + move.first2_, work_buffer1_.begin(), work_buffer1_.end());
        for (int i = move.first1_; i < (int)route1.size(); ++i) {
            route_of_[route1[i]] = move.route1_;
            position_[route1[i]] = i;
        }
        for (int i = move.first2_; i < (int)route2.size(); ++i) {
            route_of_[route2[i]] = move.route2_;
            position_[route2[i]] = i;
        }
        route_costs_[move.route1_] += move.delta1_;
        route_costs_[move.route2_] += move.delta2_;
    }
    ++revision_;
}
}
