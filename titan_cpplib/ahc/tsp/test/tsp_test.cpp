/// https://github.com/titan-23/Library_cpp/blob/main/titan_cpplib/ahc/tsp/test/tsp_test.cpp
#include <algorithm>
#include <cassert>
#include <iostream>
#include <numeric>
#include <stdexcept>
#include <vector>
#include "titan_cpplib/ahc/tsp/tsp_initial_state.cpp"
#include "titan_cpplib/ahc/tsp/multiple_tours.cpp"
using namespace std;
using namespace titan23;

using Cost = long long;

struct MatrixCost {
    const vector<vector<Cost>>* distance;

    Cost operator()(int u, int v) const {
        return (*distance)[u][v];
    }
};

Cost calculate_cost(const vector<int>& order, const vector<vector<Cost>>& distance) {
    if (order.size() < 2) return 0;
    Cost result = 0;
    for (int i = 0; i < (int)order.size(); ++i) result += distance[order[i]][order[(i + 1) % order.size()]];
    return result;
}

template<class State>
void validate_state(const State& state, const vector<vector<Cost>>& distance, int fixed_start) {
    assert(!state.order().empty());
    assert(state.order()[0] == fixed_start);
    vector<int> seen(state.node_count());
    for (int i = 0; i < state.size(); ++i) {
        int node = state.order()[i];
        assert(!seen[node]);
        seen[node] = true;
        assert(state.position(node) == i);
    }
    for (int node = 0; node < state.node_count(); ++node) {
        if (!seen[node]) assert(state.position(node) == -1);
    }
    assert(state.total_cost() == calculate_cost(state.order(), distance));
}

// 全ての巡回順を列挙して最小のコストを求める
Cost calculate_optimal_cost(const vector<vector<Cost>>& distance) {
    vector<int> order(distance.size());
    iota(order.begin(), order.end(), 0);
    Cost best = calculate_cost(order, distance);
    do {
        best = min(best, calculate_cost(order, distance));
    } while (next_permutation(order.begin() + 1, order.end()));
    return best;
}

vector<vector<Cost>> make_distance(int n) {
    vector<vector<Cost>> distance(n, vector<Cost>(n));
    for (int u = 0; u < n; ++u) for (int v = u + 1; v < n; ++v) {
        Cost value = 1 + ((u + 3) * (v + 5) * 17 + (u + v) * 11) % 101;
        distance[u][v] = distance[v][u] = value;
    }
    return distance;
}

template<class Function>
void expect_exception(Function&& function) {
    bool thrown = false;
    try {
        function();
    } catch (const exception&) {
        thrown = true;
    }
    assert(thrown);
}

template<class Problem>
void test_moves(const Problem& problem, const vector<vector<Cost>>& distance, const vector<int>& order) {
    int n = order.size();
    for (int left = 0; left <= n; ++left) for (int right = left; right <= n; ++right) {
        auto state = problem.make_state(order);
        auto move = state.make_two_opt(problem, left, right);
        if (!move.valid()) continue;
        Cost expected = state.total_cost() + move.delta();
        state.apply(problem, move);
        validate_state(state, distance, order[0]);
        assert(state.total_cost() == expected);
    }
    for (int first = 0; first <= n; ++first) for (int length = 0; length <= n; ++length) {
        for (int after = 0; after < n; ++after) for (bool reversed : {false, true}) {
            auto state = problem.make_state(order);
            auto move = state.make_or_opt(problem, first, length, after, reversed);
            if (!move.valid()) continue;
            Cost expected = state.total_cost() + move.delta();
            state.apply(problem, move);
            validate_state(state, distance, order[0]);
            assert(state.total_cost() == expected);
        }
    }
    for (int cut1 = 0; cut1 < n; ++cut1) for (int cut2 = cut1 + 1; cut2 < n; ++cut2) {
        for (int cut3 = cut2 + 1; cut3 < n; ++cut3) for (int cut4 = cut3 + 1; cut4 < n; ++cut4) {
            auto state = problem.make_state(order);
            auto move = state.make_double_bridge(problem, cut1, cut2, cut3, cut4);
            if (!move.valid()) continue;
            Cost expected = state.total_cost() + move.delta();
            state.apply(problem, move);
            validate_state(state, distance, order[0]);
            assert(state.total_cost() == expected);
        }
    }
}

template<class State>
void validate_multiple_state(
    const State& state,
    const vector<int>& depots,
    const vector<vector<Cost>>& distance
) {
    vector<int> seen(state.node_count());
    for (int route = 0; route < state.route_count(); ++route) {
        const vector<int>& order = state.route(route);
        assert(!order.empty());
        assert(order[0] == depots[route]);
        for (int i = 0; i < (int)order.size(); ++i) {
            int node = order[i];
            assert(!seen[node]);
            seen[node] = true;
            assert(state.route_of(node) == route);
            assert(state.position(node) == i);
        }
        assert(state.route_cost(route) == calculate_cost(order, distance));
    }
    for (int value : seen) assert(value == 1);
}

template<class Problem>
void test_multiple_moves(const Problem& problem, const vector<vector<Cost>>& distance) {
    vector<int> depots = {0, 1, 2};
    vector<vector<int>> routes = {{0, 3, 4, 5}, {1, 6, 7}, {2, 8}};
    auto initial = make_multiple_tsp_state(problem, depots, routes);
    validate_multiple_state(initial, depots, distance);
    for (int route = 0; route < (int)routes.size(); ++route) {
        int size = routes[route].size();
        for (int left = 0; left <= size; ++left) for (int right = left; right <= size; ++right) {
            auto state = make_multiple_tsp_state(problem, depots, routes);
            auto move = state.make_two_opt(problem, route, left, right);
            if (!move.valid()) continue;
            Cost expected = state.route_cost(route) + move.delta();
            state.apply(problem, move);
            validate_multiple_state(state, depots, distance);
            assert(state.route_cost(route) == expected);
        }
    }
    for (int source = 0; source < (int)routes.size(); ++source) for (int target = 0; target < (int)routes.size(); ++target) {
        for (int first = 0; first <= (int)routes[source].size(); ++first) {
            for (int length = 0; length <= (int)routes[source].size(); ++length) {
                for (int after = 0; after < (int)routes[target].size(); ++after) {
                    auto state = make_multiple_tsp_state(problem, depots, routes);
                    auto move = state.make_block_shift(problem, source, first, length, target, after);
                    if (!move.valid()) continue;
                    Cost expected_source = state.route_cost(source) + move.source_delta();
                    Cost expected_target = state.route_cost(target) + move.target_delta();
                    state.apply(problem, move);
                    validate_multiple_state(state, depots, distance);
                    assert(state.route_cost(source) == expected_source);
                    assert(state.route_cost(target) == expected_target);
                }
            }
        }
    }
    for (int route1 = 0; route1 < (int)routes.size(); ++route1) for (int route2 = route1 + 1; route2 < (int)routes.size(); ++route2) {
        for (int first1 = 0; first1 <= (int)routes[route1].size(); ++first1) {
            for (int length1 = 0; length1 <= (int)routes[route1].size(); ++length1) {
                for (int first2 = 0; first2 <= (int)routes[route2].size(); ++first2) {
                    for (int length2 = 0; length2 <= (int)routes[route2].size(); ++length2) {
                        auto state = make_multiple_tsp_state(problem, depots, routes);
                        auto move = state.make_block_swap(problem, route1, first1, length1, route2, first2, length2);
                        if (!move.valid()) continue;
                        Cost expected1 = state.route_cost(route1) + move.delta1();
                        Cost expected2 = state.route_cost(route2) + move.delta2();
                        state.apply(problem, move);
                        validate_multiple_state(state, depots, distance);
                        assert(state.route_cost(route1) == expected1);
                        assert(state.route_cost(route2) == expected2);
                    }
                }
            }
        }
    }
    auto repeated = make_multiple_tsp_state(problem, depots, routes);
    auto first_swap = repeated.make_block_swap(problem, 0, 1, 2, 1, 1, 1);
    assert(first_swap.valid());
    repeated.apply(problem, first_swap);
    auto second_swap = repeated.make_block_swap(problem, 0, 1, 1, 2, 1, 1);
    assert(second_swap.valid());
    repeated.apply(problem, second_swap);
    validate_multiple_state(repeated, depots, distance);
    vector<vector<int>> depot_only_routes = {{0, 3, 4, 5, 8}, {1, 6, 7}, {2}};
    auto depot_only = make_multiple_tsp_state(problem, depots, depot_only_routes);
    auto into_depot_only = depot_only.make_block_shift(problem, 0, 1, 2, 2, 0);
    assert(into_depot_only.valid());
    depot_only.apply(problem, into_depot_only);
    validate_multiple_state(depot_only, depots, distance);
}

int main() {
    int n = 9;
    vector<vector<Cost>> distance = make_distance(n);
    MatrixCost edge_cost{&distance};
    TspProblem problem(n, edge_cost);
    vector<int> order(n);
    iota(order.begin(), order.end(), 0);
    rotate(order.begin() + 1, order.begin() + 3, order.end());
    auto state = problem.make_state(order);
    validate_state(state, distance, order[0]);
    auto candidates = make_tsp_candidates(problem, n - 1);
    assert(candidates.belongs_to(problem));
    assert(candidates.node_count() == n);
    assert(candidates.candidate_count() == n - 1);
    auto movable_candidates = make_tsp_candidates(problem, 3);
    TspCandidates moved_candidates = move(movable_candidates);
    assert(movable_candidates.node_count() == 0);
    assert(movable_candidates.candidate_count() == 0);
    assert(moved_candidates.belongs_to(problem));
    auto nearest = problem.make_nearest_neighbor_state(0);
    validate_state(nearest, distance, 0);
    vector<int> subset = {2, 4, 6};
    auto partial = problem.make_nearest_neighbor_state(1, subset);
    validate_state(partial, distance, 1);
    auto assigned = problem.make_state(order);
    auto stale_move = assigned.make_two_opt(problem, 1, 3);
    assert(stale_move.valid());
    assigned = nearest;
    validate_state(assigned, distance, nearest.order()[0]);
    expect_exception([&] { assigned.apply(problem, stale_move); });
    assigned = problem.make_state(order);
    validate_state(assigned, distance, order[0]);
    expect_exception([&] { assigned.apply(problem, stale_move); });
    TspTwoOptMove<Cost> invalid_move;
    assert(!invalid_move.valid());
    assert(!assigned.make_two_opt(problem, 0, 3).valid());
    expect_exception([&] { assigned.apply(problem, invalid_move); });
    vector<vector<Cost>> other_distance = distance;
    TspProblem other_problem(n, MatrixCost{&other_distance});
    auto other_candidates = make_tsp_candidates(other_problem, n - 1);
    auto other_state = other_problem.make_state(order);
    expect_exception([&] { tsp_two_opt_local_search(problem, other_state); });
    expect_exception([&] {
        TspGuidedLocalSearchOptions invalid_options;
        invalid_options.time_limit_ms = -0.5;
        tsp_guided_local_search(problem, candidates, state, invalid_options);
    });
    expect_exception([&] {
        TspGuidedLocalSearchOptions valid_options;
        valid_options.max_penalty_rounds = 1;
        tsp_guided_local_search(problem, other_candidates, state, valid_options);
    });
    test_moves(problem, distance, order);
    auto full_local = problem.make_state(order);
    Cost initial_cost = full_local.total_cost();
    auto full_result = tsp_two_opt_local_search(problem, full_local);
    validate_state(full_local, distance, order[0]);
    assert(full_result.locally_optimal);
    assert(full_local.total_cost() <= initial_cost);
    auto candidate_local = problem.make_state(order);
    auto candidate_result = tsp_two_opt_local_search(problem, candidates, candidate_local);
    validate_state(candidate_local, distance, order[0]);
    assert(candidate_result.locally_optimal);
    auto guided = problem.make_state(order);
    TspGuidedLocalSearchOptions guided_options;
    guided_options.max_penalty_rounds = 8;
    guided_options.max_evaluated_moves = 20000;
    auto guided_result = tsp_guided_local_search(problem, candidates, guided, guided_options);
    validate_state(guided, distance, order[0]);
    assert(guided_result.penalty_rounds <= guided_options.max_penalty_rounds);
    auto iterated = problem.make_state(order);
    TspIteratedLocalSearchOptions iterated_options;
    iterated_options.max_iterations = 2000;
    auto iterated_result = tsp_iterated_local_search(problem, candidates, iterated, iterated_options);
    validate_state(iterated, distance, order[0]);
    assert(iterated_result.iterations == iterated_options.max_iterations);
    assert(iterated_result.stop_reason == TspSearchStopReason::iteration_limit);
    assert(iterated.total_cost() == calculate_optimal_cost(distance));
    expect_exception([&] {
        TspIteratedLocalSearchOptions no_limit;
        tsp_iterated_local_search(problem, candidates, iterated, no_limit);
    });
    vector<int> solved = solve_tsp(n, edge_cost, 10.0, 4);
    assert((int)solved.size() == n && solved[0] == 4);
    assert(calculate_cost(solved, distance) == calculate_optimal_cost(distance));
    vector<int> partial_order = {1, 2, 4, 6, 8};
    auto partial_iterated = problem.make_state(partial_order);
    tsp_iterated_local_search(problem, candidates, partial_iterated, iterated_options);
    validate_state(partial_iterated, distance, 1);
    assert(partial_iterated.size() == (int)partial_order.size());
    auto initial_state = problem.make_state(order);
    TspInitialStateOptions initial_options;
    initial_options.search = TspInitialSearch::two_opt;
    initial_options.local_search.max_evaluated_moves = 20000;
    auto initial_result = improve_tsp_initial_state(problem, candidates, initial_state, initial_options);
    validate_state(initial_state, distance, order[0]);
    assert(initial_result.search == TspInitialSearch::two_opt);
    vector<int> depots = {0, 1, 2};
    vector<vector<int>> routes = {{0, 3, 4, 5}, {1, 6, 7}, {2, 8}};
    auto assigned_multiple = make_multiple_tsp_state(problem, depots, routes);
    auto stale_multiple_move = assigned_multiple.make_block_shift(problem, 0, 1, 1, 1, 0);
    assert(stale_multiple_move.valid());
    auto replacement_multiple = make_multiple_tsp_state(problem, depots, {{0, 5, 4, 3}, {1, 7, 6}, {2, 8}});
    assigned_multiple = replacement_multiple;
    validate_multiple_state(assigned_multiple, depots, distance);
    expect_exception([&] { assigned_multiple.apply(problem, stale_multiple_move); });
    test_multiple_moves(problem, distance);
    cout << "ok\n";
}
