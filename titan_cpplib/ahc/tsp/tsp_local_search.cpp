/// https://github.com/titan-23/Library_cpp/blob/main/titan_cpplib/ahc/tsp/tsp_local_search.cpp
#pragma once
#include <cstdint>
#include <optional>
#include <stdexcept>
#include <utility>
#include "titan_cpplib/ahc/tsp/tsp_symmetric_moves.cpp"
using namespace std;
namespace titan23 {
struct TspLocalSearchOptions {
    int64_t max_evaluated_moves = -1;
    bool first_improvement = true;
};

struct TspLocalSearchResult {
    int64_t evaluated_moves;
    int64_t applied_moves;
    bool locally_optimal;
};

namespace tsp_local_search_detail {
inline void check_options(const TspLocalSearchOptions& options) {
    if (options.max_evaluated_moves < -1) throw invalid_argument("tsp_two_opt_local_search: max_evaluated_moves must be -1 or nonnegative");
}

inline int previous_index(int index, int n) {
    return index == 0 ? n - 1 : index - 1;
}

inline bool make_two_opt_indices(int edge1, int edge2, int n, int& left, int& right) {
    if (edge1 == edge2 || (edge1 + 1) % n == edge2 || (edge2 + 1) % n == edge1) return false;
    if (edge1 > edge2) swap(edge1, edge2);
    left = edge1 + 1;
    right = edge2;
    return true;
}

template<class EdgeCost, class Enumerate>
TspLocalSearchResult run(
    const TspProblem<EdgeCost>& problem,
    TspState<typename TspProblem<EdgeCost>::Cost>& state,
    const TspLocalSearchOptions& options,
    Enumerate&& enumerate
) {
    using Cost = typename TspProblem<EdgeCost>::Cost;
    int64_t evaluated_moves = 0;
    int64_t applied_moves = 0;
    if (state.size() < 4) return {0, 0, true};
    while (true) {
        bool stopped_by_limit = false;
        bool applied = false;
        optional<TspTwoOptMove<Cost>> best_move;
        enumerate([&](int left, int right) {
            if (options.max_evaluated_moves >= 0 && evaluated_moves >= options.max_evaluated_moves) {
                stopped_by_limit = true;
                return false;
            }
            auto candidate_move = state.make_two_opt(problem, left, right);
            if (!candidate_move) return true;
            ++evaluated_moves;
            if (!(candidate_move->delta() < Cost{})) return true;
            if (options.first_improvement) {
                state.apply(problem, *candidate_move);
                ++applied_moves;
                applied = true;
                return false;
            }
            if (!best_move || candidate_move->delta() < best_move->delta()) best_move = move(candidate_move);
            return true;
        });
        if (stopped_by_limit) return {evaluated_moves, applied_moves, false};
        if (options.first_improvement) {
            if (!applied) return {evaluated_moves, applied_moves, true};
            continue;
        }
        if (!best_move) return {evaluated_moves, applied_moves, true};
        state.apply(problem, *best_move);
        ++applied_moves;
    }
}
}

template<class EdgeCost>
TspLocalSearchResult tsp_two_opt_local_search(
    const TspProblem<EdgeCost>& problem,
    TspState<typename TspProblem<EdgeCost>::Cost>& state,
    TspLocalSearchOptions options = {}
) {
    tsp_local_search_detail::check_options(options);
    if (!state.belongs_to(problem)) throw invalid_argument("tsp_two_opt_local_search: state and problem do not match");
    auto enumerate = [&](auto&& visit) {
        int n = state.size();
        for (int edge1 = 0; edge1 < n; ++edge1) {
            for (int edge2 = edge1 + 1; edge2 < n; ++edge2) {
                int left, right;
                if (!tsp_local_search_detail::make_two_opt_indices(edge1, edge2, n, left, right)) continue;
                if (!visit(left, right)) return;
            }
        }
    };
    return tsp_local_search_detail::run(problem, state, options, enumerate);
}

template<class EdgeCost>
TspLocalSearchResult tsp_two_opt_local_search(
    const TspProblem<EdgeCost>& problem,
    const TspCandidates& candidates,
    TspState<typename TspProblem<EdgeCost>::Cost>& state,
    TspLocalSearchOptions options = {}
) {
    tsp_local_search_detail::check_options(options);
    if (!candidates.belongs_to(problem)) throw invalid_argument("tsp_two_opt_local_search: candidates and problem do not match");
    if (!state.belongs_to(problem)) throw invalid_argument("tsp_two_opt_local_search: state and problem do not match");
    auto enumerate = [&](auto&& visit) {
        int n = state.size();
        const auto& order = state.order();
        for (int index = 0; index < n; ++index) {
            int node = order[index];
            for (int candidate : candidates[node]) {
                int other = state.position(candidate);
                if (other < 0 || other == index) continue;
                int edge_pairs[2][2] = {
                    {index, other},
                    {tsp_local_search_detail::previous_index(index, n), tsp_local_search_detail::previous_index(other, n)},
                };
                for (const auto& edge_pair : edge_pairs) {
                    int left, right;
                    if (!tsp_local_search_detail::make_two_opt_indices(edge_pair[0], edge_pair[1], n, left, right)) continue;
                    if (!visit(left, right)) return;
                }
            }
        }
    };
    return tsp_local_search_detail::run(problem, state, options, enumerate);
}
}
