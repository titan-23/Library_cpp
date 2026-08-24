/// https://github.com/titan-23/Library_cpp/blob/main/titan_cpplib/ahc/tsp/tsp_edge_penalty_search.cpp
#pragma once
#include <cmath>
#include <cstdint>
#include <stdexcept>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>
#include "titan_cpplib/ahc/tsp/tsp_guided_local_search.cpp"
using namespace std;
namespace titan23 {
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

namespace tsp_edge_penalty_detail {
inline void check_options(const TspEdgePenaltySearchOptions& options) {
    if (options.max_penalty_rounds < -1) throw invalid_argument("tsp_edge_penalty_search: max_penalty_rounds must be -1 or nonnegative");
    if (options.max_evaluated_moves < -1) throw invalid_argument("tsp_edge_penalty_search: max_evaluated_moves must be -1 or nonnegative");
    if (!isfinite(options.time_limit_ms) || (options.time_limit_ms < 0 && options.time_limit_ms != -1)) throw invalid_argument("tsp_edge_penalty_search: time_limit_ms must be -1 or nonnegative");
    if (!isfinite(options.initial_penalty_ratio) || options.initial_penalty_ratio < 0) throw invalid_argument("tsp_edge_penalty_search: initial_penalty_ratio must be finite and nonnegative");
    if (!isfinite(options.improved_penalty_ratio) || options.improved_penalty_ratio < 0) throw invalid_argument("tsp_edge_penalty_search: improved_penalty_ratio must be finite and nonnegative");
    if (options.max_penalty_rounds == -1 && options.time_limit_ms == -1) {
        throw invalid_argument("tsp_edge_penalty_search: a time or penalty round limit must be enabled");
    }
}

template<class Cost>
int logical_node(const TspState<Cost>& state, int first_node, int direction, int index) {
    int n = state.size();
    int position = state.position(first_node);
    int offset = index % n;
    int target;
    if (direction == 1) target = offset < n - position ? position + offset : offset - (n - position);
    else target = offset <= position ? position - offset : n - (offset - position);
    return state.order()[target];
}

template<class Cost>
int logical_index(const TspState<Cost>& state, int first_node, int direction, int node) {
    int n = state.size();
    int first = state.position(first_node);
    int position = state.position(node);
    if (direction == 1) return position >= first ? position - first : n - (first - position);
    return position <= first ? first - position : n - (position - first);
}

template<class Cost>
int forward_edge_start(const TspState<Cost>& state, int u, int v) {
    int n = state.size();
    int pu = state.position(u), pv = state.position(v);
    if ((pu + 1) % n == pv) return pu;
    if ((pv + 1) % n == pu) return pv;
    throw logic_error("tsp_edge_penalty_search: selected nodes are not adjacent");
}
}

template<class EdgeCost>
auto tsp_edge_penalty_search(
    const TspProblem<EdgeCost>& problem,
    const TspCandidates& candidates,
    TspState<typename TspProblem<EdgeCost>::Cost>& state,
    TspEdgePenaltySearchOptions options
) {
    using Cost = typename TspProblem<EdgeCost>::Cost;
    static_assert(is_convertible_v<Cost, long double>, "tsp_edge_penalty_search: Cost must be convertible to long double");
    tsp_edge_penalty_detail::check_options(options);
    if (!candidates.belongs_to(problem)) throw invalid_argument("tsp_edge_penalty_search: candidates and problem do not match");
    if (!state.belongs_to(problem)) throw invalid_argument("tsp_edge_penalty_search: state and problem do not match");
    tsp_guided_detail::SearchTimer timer;
    int64_t evaluated_moves = 0, applied_moves = 0, penalty_rounds = 0, best_updates = 0;
    vector<int> best_order = state.order();
    Cost best_cost = state.total_cost();
    auto finish = [&](TspSearchStopReason reason) {
        if (state.order() != best_order) state.reset(problem, move(best_order));
        return TspEdgePenaltySearchResult{
            evaluated_moves,
            applied_moves,
            penalty_rounds,
            best_updates,
            timer.elapsed_ms(),
            reason,
        };
    };
    if (state.size() < 4 || candidates.candidate_count() == 0) return finish(TspSearchStopReason::completed);
    if (options.time_limit_ms == 0) return finish(TspSearchStopReason::time_limit);
    if (options.max_evaluated_moves == 0) return finish(TspSearchStopReason::evaluated_move_limit);
    if (options.max_penalty_rounds == 0) return finish(TspSearchStopReason::penalty_round_limit);
    unordered_map<uint64_t, int64_t> penalties;
    penalties.reserve((size_t)state.size() * 4 + 1);
    long double lambda = options.initial_penalty_ratio * (long double)best_cost / state.size();
    int logical_first = state.order()[0];
    int direction = 1;
    while (true) {
        if (options.time_limit_ms >= 0 && timer.elapsed_ms() >= options.time_limit_ms) return finish(TspSearchStopReason::time_limit);
        if (options.max_evaluated_moves >= 0 && evaluated_moves >= options.max_evaluated_moves) return finish(TspSearchStopReason::evaluated_move_limit);
        if (options.max_penalty_rounds >= 0 && penalty_rounds >= options.max_penalty_rounds) return finish(TspSearchStopReason::penalty_round_limit);
        int n = state.size();
        int selected_index = -1;
        long double maximum_utility = -1;
        for (int i = 0; i < n; ++i) {
            int u = tsp_edge_penalty_detail::logical_node(state, logical_first, direction, i);
            int v = tsp_edge_penalty_detail::logical_node(state, logical_first, direction, (i + 1) % n);
            int64_t penalty = tsp_guided_detail::get_penalty(penalties, u, v);
            long double utility = (long double)problem.edge_cost(u, v) / (1 + (long double)penalty);
            if (maximum_utility < utility) {
                maximum_utility = utility;
                selected_index = i;
            }
        }
        int index1 = selected_index;
        int index2 = (index1 + 1) % n;
        int city1 = tsp_edge_penalty_detail::logical_node(state, logical_first, direction, index1);
        int city2 = tsp_edge_penalty_detail::logical_node(state, logical_first, direction, index2);
        int64_t& selected_penalty = penalties[tsp_guided_detail::edge_key(city1, city2)];
        if (selected_penalty == numeric_limits<int64_t>::max()) throw overflow_error("tsp_edge_penalty_search: penalty overflow");
        ++selected_penalty;
        tsp_guided_detail::increment_counter(penalty_rounds, "tsp_edge_penalty_search: penalty round counter overflow");
        for (int city3 : candidates[city1]) {
            int state_position3 = state.position(city3);
            if (state_position3 < 0) continue;
            int index3 = tsp_edge_penalty_detail::logical_index(state, logical_first, direction, city3);
            int index4 = (index3 + 1) % n;
            if (index2 == index3 || index1 == index4) continue;
            if (options.max_evaluated_moves >= 0 && evaluated_moves >= options.max_evaluated_moves) return finish(TspSearchStopReason::evaluated_move_limit);
            int city4 = tsp_edge_penalty_detail::logical_node(state, logical_first, direction, index4);
            tsp_guided_detail::increment_counter(evaluated_moves, "tsp_edge_penalty_search: evaluated move counter overflow");
            int edge1 = tsp_edge_penalty_detail::forward_edge_start(state, city1, city2);
            int edge2 = tsp_edge_penalty_detail::forward_edge_start(state, city3, city4);
            int first_edge = min(edge1, edge2);
            int second_edge = max(edge1, edge2);
            auto move = state.make_two_opt(problem, first_edge + 1, second_edge);
            if (!move) continue;
            long double removed_penalty =
                (long double)tsp_guided_detail::get_penalty(penalties, city1, city2) +
                (long double)tsp_guided_detail::get_penalty(penalties, city3, city4);
            long double added_penalty =
                (long double)tsp_guided_detail::get_penalty(penalties, city1, city3) +
                (long double)tsp_guided_detail::get_penalty(penalties, city2, city4);
            long double augmented_delta =
                (long double)move->delta() +
                lambda * (added_penalty - removed_penalty);
            if (augmented_delta >= 0) continue;
            int left = index2, right = index3;
            if (left > right) {
                left = index3 + 1;
                right = index2 - 1;
            }
            int next_logical_first;
            int next_logical_second;
            if (left == 0) {
                next_logical_first = tsp_edge_penalty_detail::logical_node(state, logical_first, direction, right);
                next_logical_second = tsp_edge_penalty_detail::logical_node(state, logical_first, direction, right - 1);
            } else {
                next_logical_first = logical_first;
                int second_index = left == 1 ? right : 1;
                next_logical_second = tsp_edge_penalty_detail::logical_node(state, logical_first, direction, second_index);
            }
            state.apply(problem, *move);
            tsp_guided_detail::increment_counter(applied_moves, "tsp_edge_penalty_search: applied move counter overflow");
            int first_position = state.position(next_logical_first);
            int forward_node = state.order()[(first_position + 1) % n];
            int backward_node = state.order()[first_position == 0 ? n - 1 : first_position - 1];
            if (forward_node == next_logical_second) direction = 1;
            else if (backward_node == next_logical_second) direction = -1;
            else throw logic_error("tsp_edge_penalty_search: logical tour was not preserved");
            logical_first = next_logical_first;
            if (state.total_cost() < best_cost) {
                best_cost = state.total_cost();
                best_order = state.order();
                tsp_guided_detail::increment_counter(best_updates, "tsp_edge_penalty_search: best update counter overflow");
                lambda = options.improved_penalty_ratio * (long double)best_cost / state.size();
            }
            break;
        }
    }
}
}
