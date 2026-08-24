/// https://github.com/titan-23/Library_cpp/blob/main/titan_cpplib/ahc/tsp/tsp_initial_state.cpp
#pragma once
#include <cstdint>
#include "titan_cpplib/ahc/tsp/tsp_edge_penalty_search.cpp"
using namespace std;
namespace titan23 {
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

struct TspInitialStateResult {
    TspInitialSearch search;
    int64_t evaluated_moves;
    int64_t applied_moves;
    int64_t penalty_rounds;
    int64_t best_updates;
    double elapsed_ms;
    bool locally_optimal;
    TspSearchStopReason stop_reason;
};

template<class EdgeCost>
TspInitialStateResult improve_tsp_initial_state(
    const TspProblem<EdgeCost>& problem,
    const TspCandidates& candidates,
    TspState<typename TspProblem<EdgeCost>::Cost>& state,
    const TspInitialStateOptions& options
) {
    if (options.search == TspInitialSearch::none) {
        return {options.search, 0, 0, 0, 0, 0, false, TspSearchStopReason::completed};
    }
    if (options.search == TspInitialSearch::two_opt) {
        tsp_guided_detail::SearchTimer timer;
        TspLocalSearchResult result = tsp_two_opt_local_search(
            problem,
            candidates,
            state,
            options.local_search
        );
        TspSearchStopReason reason = result.locally_optimal
            ? TspSearchStopReason::completed
            : TspSearchStopReason::evaluated_move_limit;
        return {
            options.search,
            result.evaluated_moves,
            result.applied_moves,
            0,
            0,
            timer.elapsed_ms(),
            result.locally_optimal,
            reason,
        };
    }
    if (options.search == TspInitialSearch::guided_local_search) {
        TspGuidedLocalSearchResult result = tsp_guided_local_search(
            problem,
            candidates,
            state,
            options.guided_local_search
        );
        return {
            options.search,
            result.evaluated_moves,
            result.applied_moves,
            result.penalty_rounds,
            result.best_updates,
            result.elapsed_ms,
            false,
            result.stop_reason,
        };
    }
    if (options.search == TspInitialSearch::edge_penalty_search) {
        TspEdgePenaltySearchResult result = tsp_edge_penalty_search(
            problem,
            candidates,
            state,
            options.edge_penalty_search
        );
        return {
            options.search,
            result.evaluated_moves,
            result.applied_moves,
            result.penalty_rounds,
            result.best_updates,
            result.elapsed_ms,
            false,
            result.stop_reason,
        };
    }
    throw invalid_argument("improve_tsp_initial_state: invalid search type");
}
}
