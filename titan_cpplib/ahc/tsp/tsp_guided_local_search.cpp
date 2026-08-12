#pragma once
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <stdexcept>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>
#include "titan_cpplib/ahc/tsp/tsp_local_search.cpp"
using namespace std;
namespace titan23 {
enum class TspSearchStopReason {
    completed,
    time_limit,
    evaluated_move_limit,
    penalty_round_limit,
};

struct TspGuidedLocalSearchOptions {
    int64_t max_penalty_rounds = -1;
    int64_t max_evaluated_moves = -1;
    double time_limit_ms = -1;
    int time_check_interval = 256;
    long double penalty_ratio = 0.3L;
};

struct TspGuidedLocalSearchResult {
    int64_t evaluated_moves;
    int64_t applied_moves;
    int64_t penalty_rounds;
    int64_t best_updates;
    double elapsed_ms;
    TspSearchStopReason stop_reason;
};

namespace tsp_guided_detail {
class SearchTimer {
    chrono::steady_clock::time_point start_;

public:
    SearchTimer() : start_(chrono::steady_clock::now()) {}

    double elapsed_ms() const {
        return chrono::duration<double, milli>(chrono::steady_clock::now() - start_).count();
    }
};

inline uint64_t edge_key(int u, int v) {
    if (u > v) swap(u, v);
    return (uint64_t)(uint32_t)u << 32 | (uint32_t)v;
}

inline int64_t get_penalty(const unordered_map<uint64_t, int64_t>& penalties, int u, int v) {
    auto it = penalties.find(edge_key(u, v));
    return it == penalties.end() ? 0 : it->second;
}

inline void increment_counter(int64_t& counter, const char* message) {
    if (counter == numeric_limits<int64_t>::max()) throw overflow_error(message);
    ++counter;
}

inline void check_options(const TspGuidedLocalSearchOptions& options) {
    if (options.max_penalty_rounds < -1) throw invalid_argument("tsp_guided_local_search: max_penalty_rounds must be -1 or nonnegative");
    if (options.max_evaluated_moves < -1) throw invalid_argument("tsp_guided_local_search: max_evaluated_moves must be -1 or nonnegative");
    if (!isfinite(options.time_limit_ms) || (options.time_limit_ms < 0 && options.time_limit_ms != -1)) throw invalid_argument("tsp_guided_local_search: time_limit_ms must be -1 or nonnegative");
    if (options.time_check_interval <= 0) throw invalid_argument("tsp_guided_local_search: time_check_interval must be positive");
    if (!isfinite(options.penalty_ratio) || options.penalty_ratio < 0) throw invalid_argument("tsp_guided_local_search: penalty_ratio must be finite and nonnegative");
    if (options.max_penalty_rounds == -1 && options.time_limit_ms == -1) {
        throw invalid_argument("tsp_guided_local_search: a time or penalty round limit must be enabled");
    }
}

inline int previous_index(int index, int n) {
    return index == 0 ? n - 1 : index - 1;
}

inline bool adjacent_edges(int edge1, int edge2, int n) {
    if (edge1 == edge2) return true;
    return (edge1 + 1) % n == edge2 || (edge2 + 1) % n == edge1;
}

template<class EdgeCost, class Cost>
long double penalized_delta(
    const TspProblem<EdgeCost>&,
    const TspState<Cost>& state,
    int left,
    int right,
    Cost true_delta,
    long double lambda,
    const unordered_map<uint64_t, int64_t>& penalties
) {
    const vector<int>& order = state.order();
    int n = order.size();
    int a = order[left - 1];
    int b = order[left];
    int c = order[right];
    int d = order[(right + 1) % n];
    long double removed = (long double)get_penalty(penalties, a, b) + (long double)get_penalty(penalties, c, d);
    long double added = (long double)get_penalty(penalties, a, c) + (long double)get_penalty(penalties, b, d);
    return (long double)true_delta + lambda * (added - removed);
}

template<class EdgeCost, class Cost, class AppliedFn>
TspSearchStopReason descend(
    const TspProblem<EdgeCost>& problem,
    const TspCandidates& candidates,
    TspState<Cost>& state,
    long double lambda,
    const unordered_map<uint64_t, int64_t>& penalties,
    const TspGuidedLocalSearchOptions& options,
    const SearchTimer& timer,
    int64_t& evaluated_moves,
    int64_t& applied_moves,
    AppliedFn&& on_applied
) {
    int n = state.size();
    while (true) {
        if (options.time_limit_ms >= 0 && timer.elapsed_ms() >= options.time_limit_ms) return TspSearchStopReason::time_limit;
        bool applied = false;
        const vector<int>& order = state.order();
        for (int index = 0; index < n && !applied; ++index) {
            int u = order[index];
            for (int v : candidates[u]) {
                int other = state.position(v);
                if (other < 0 || other == index) continue;
                int edge_pairs[2][2] = {
                    {index, other},
                    {previous_index(index, n), previous_index(other, n)},
                };
                for (auto& edge_pair : edge_pairs) {
                    int edge1 = edge_pair[0], edge2 = edge_pair[1];
                    if (adjacent_edges(edge1, edge2, n)) continue;
                    if (options.time_limit_ms >= 0 && evaluated_moves % options.time_check_interval == 0 && timer.elapsed_ms() >= options.time_limit_ms) {
                        return TspSearchStopReason::time_limit;
                    }
                    if (options.max_evaluated_moves >= 0 && evaluated_moves >= options.max_evaluated_moves) {
                        return TspSearchStopReason::evaluated_move_limit;
                    }
                    int first = min(edge1, edge2);
                    int second = max(edge1, edge2);
                    int left = first + 1;
                    int right = second;
                    auto move = state.make_two_opt(problem, left, right);
                    if (!move) continue;
                    increment_counter(evaluated_moves, "tsp_guided_local_search: evaluated move counter overflow");
                    long double delta = penalized_delta(problem, state, left, right, move->delta(), lambda, penalties);
                    if (delta < 0) {
                        state.apply(problem, *move);
                        increment_counter(applied_moves, "tsp_guided_local_search: applied move counter overflow");
                        on_applied();
                        applied = true;
                        break;
                    }
                }
                if (applied) break;
            }
        }
        if (!applied) return TspSearchStopReason::completed;
    }
}
}

template<class EdgeCost>
auto tsp_guided_local_search(
    const TspProblem<EdgeCost>& problem,
    const TspCandidates& candidates,
    TspState<typename TspProblem<EdgeCost>::Cost>& state,
    TspGuidedLocalSearchOptions options
) {
    using Cost = typename TspProblem<EdgeCost>::Cost;
    static_assert(is_convertible_v<Cost, long double>, "tsp_guided_local_search: Cost must be convertible to long double");
    tsp_guided_detail::check_options(options);
    if (!candidates.belongs_to(problem)) throw invalid_argument("tsp_guided_local_search: candidates and problem do not match");
    if (!state.belongs_to(problem)) throw invalid_argument("tsp_guided_local_search: state and problem do not match");
    tsp_guided_detail::SearchTimer timer;
    int64_t evaluated_moves = 0, applied_moves = 0, penalty_rounds = 0, best_updates = 0;
    auto finish = [&](TspSearchStopReason reason, vector<int>& best_order) {
        if (state.order() != best_order) state.reset(problem, move(best_order));
        return TspGuidedLocalSearchResult{
            evaluated_moves,
            applied_moves,
            penalty_rounds,
            best_updates,
            timer.elapsed_ms(),
            reason,
        };
    };
    vector<int> best_order = state.order();
    Cost best_cost = state.total_cost();
    if (state.size() < 4 || candidates.candidate_count() == 0) return finish(TspSearchStopReason::completed, best_order);
    if (options.time_limit_ms == 0) return finish(TspSearchStopReason::time_limit, best_order);
    if (options.max_evaluated_moves == 0) return finish(TspSearchStopReason::evaluated_move_limit, best_order);
    unordered_map<uint64_t, int64_t> penalties;
    penalties.reserve((size_t)state.size() * 4 + 1);
    auto count_initial_improvement = [&] {
        tsp_guided_detail::increment_counter(best_updates, "tsp_guided_local_search: best update counter overflow");
    };
    TspSearchStopReason reason = tsp_guided_detail::descend(
        problem,
        candidates,
        state,
        0,
        penalties,
        options,
        timer,
        evaluated_moves,
        applied_moves,
        count_initial_improvement
    );
    if (state.total_cost() < best_cost) {
        best_cost = state.total_cost();
        best_order = state.order();
    }
    if (reason != TspSearchStopReason::completed) return finish(reason, best_order);
    if (options.max_penalty_rounds == 0) return finish(TspSearchStopReason::penalty_round_limit, best_order);
    long double lambda = options.penalty_ratio * (long double)best_cost / state.size();
    vector<uint64_t> selected_edges;
    selected_edges.reserve(state.order().size());
    while (true) {
        if (options.time_limit_ms >= 0 && timer.elapsed_ms() >= options.time_limit_ms) return finish(TspSearchStopReason::time_limit, best_order);
        if (options.max_evaluated_moves >= 0 && evaluated_moves >= options.max_evaluated_moves) return finish(TspSearchStopReason::evaluated_move_limit, best_order);
        if (options.max_penalty_rounds >= 0 && penalty_rounds >= options.max_penalty_rounds) return finish(TspSearchStopReason::penalty_round_limit, best_order);
        const vector<int>& order = state.order();
        long double maximum_utility = -1;
        selected_edges.clear();
        for (int i = 0; i < (int)order.size(); ++i) {
            int u = order[i], v = order[(i + 1) % order.size()];
            uint64_t key = tsp_guided_detail::edge_key(u, v);
            int64_t penalty = tsp_guided_detail::get_penalty(penalties, u, v);
            long double utility = (long double)problem.edge_cost(u, v) / (1 + (long double)penalty);
            if (maximum_utility < utility) {
                maximum_utility = utility;
                selected_edges.clear();
                selected_edges.push_back(key);
            } else if (maximum_utility == utility) {
                selected_edges.push_back(key);
            }
        }
        for (uint64_t key : selected_edges) {
            int64_t& penalty = penalties[key];
            if (penalty == numeric_limits<int64_t>::max()) throw overflow_error("tsp_guided_local_search: penalty overflow");
            ++penalty;
        }
        tsp_guided_detail::increment_counter(penalty_rounds, "tsp_guided_local_search: penalty round counter overflow");
        auto save_best = [&] {
            if (state.total_cost() < best_cost) {
                best_cost = state.total_cost();
                best_order = state.order();
                tsp_guided_detail::increment_counter(best_updates, "tsp_guided_local_search: best update counter overflow");
            }
        };
        reason = tsp_guided_detail::descend(
            problem,
            candidates,
            state,
            lambda,
            penalties,
            options,
            timer,
            evaluated_moves,
            applied_moves,
            save_best
        );
        if (reason != TspSearchStopReason::completed) return finish(reason, best_order);
    }
}
}
