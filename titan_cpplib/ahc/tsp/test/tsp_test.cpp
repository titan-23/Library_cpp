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

struct EdgePenaltyReferenceResult {
    vector<int> best_order;
    Cost best_cost;
    int64_t evaluated_moves;
    int64_t applied_moves;
    int64_t best_updates;
};

template<class Problem>
EdgePenaltyReferenceResult run_edge_penalty_reference(
    const Problem& problem,
    const TspCandidates& candidates,
    vector<int> order,
    int rounds
) {
    int n = order.size();
    vector<vector<int64_t>> penalties(n, vector<int64_t>(n, 1));
    vector<int> position(n);
    for (int i = 0; i < n; ++i) position[order[i]] = i;
    Cost current_cost = 0;
    for (int i = 0; i < n; ++i) current_cost += problem.edge_cost(order[i], order[(i + 1) % n]);
    Cost best_cost = current_cost;
    vector<int> best_order = order;
    long double lambda = 0.3L * best_cost / n;
    int64_t evaluated_moves = 0, applied_moves = 0, best_updates = 0;
    for (int round = 0; round < rounds; ++round) {
        int index1 = -1;
        long double maximum_utility = -1;
        for (int i = 0; i < n; ++i) {
            int u = order[i], v = order[(i + 1) % n];
            long double utility = (long double)problem.edge_cost(u, v) / penalties[u][v];
            if (maximum_utility < utility) {
                maximum_utility = utility;
                index1 = i;
            }
        }
        int index2 = (index1 + 1) % n;
        int city1 = order[index1], city2 = order[index2];
        ++penalties[city1][city2];
        ++penalties[city2][city1];
        for (int city3 : candidates[city1]) {
            int index3 = position[city3];
            int index4 = (index3 + 1) % n;
            if (index2 == index3 || index1 == index4) continue;
            int city4 = order[index4];
            ++evaluated_moves;
            Cost true_delta =
                problem.edge_cost(city1, city3) +
                problem.edge_cost(city2, city4) -
                problem.edge_cost(city1, city2) -
                problem.edge_cost(city3, city4);
            int64_t penalty_delta =
                penalties[city1][city3] + penalties[city2][city4] -
                penalties[city1][city2] - penalties[city3][city4];
            if ((long double)true_delta + lambda * penalty_delta >= 0) continue;
            current_cost += true_delta;
            int left = index2, right = index3;
            if (left > right) {
                left = index3 + 1;
                right = index2 - 1;
            }
            reverse(order.begin() + left, order.begin() + right + 1);
            for (int i = left; i <= right; ++i) position[order[i]] = i;
            ++applied_moves;
            if (current_cost < best_cost) {
                best_cost = current_cost;
                best_order = order;
                lambda = 0.1L * best_cost / n;
                ++best_updates;
            }
            break;
        }
    }
    return {move(best_order), best_cost, evaluated_moves, applied_moves, best_updates};
}

vector<pair<int, int>> cycle_edges(const vector<int>& order) {
    vector<pair<int, int>> edges;
    edges.reserve(order.size());
    for (int i = 0; i < (int)order.size(); ++i) {
        int u = order[i], v = order[(i + 1) % order.size()];
        if (u > v) swap(u, v);
        edges.emplace_back(u, v);
    }
    sort(edges.begin(), edges.end());
    return edges;
}

template<class Problem>
void test_moves(const Problem& problem, const vector<vector<Cost>>& distance, const vector<int>& order) {
    int n = order.size();
    for (int left = 0; left <= n; ++left) for (int right = left; right <= n; ++right) {
        auto state = problem.make_state(order);
        auto move = state.make_two_opt(problem, left, right);
        if (!move) continue;
        Cost expected = state.total_cost() + move->delta();
        state.apply(problem, *move);
        validate_state(state, distance, order[0]);
        assert(state.total_cost() == expected);
    }
    for (int first = 0; first <= n; ++first) for (int length = 0; length <= n; ++length) {
        for (int after = 0; after < n; ++after) for (bool reversed : {false, true}) {
            auto state = problem.make_state(order);
            auto move = state.make_or_opt(problem, first, length, after, reversed);
            if (!move) continue;
            Cost expected = state.total_cost() + move->delta();
            state.apply(problem, *move);
            validate_state(state, distance, order[0]);
            assert(state.total_cost() == expected);
        }
    }
    for (int cut1 = 0; cut1 < n; ++cut1) for (int cut2 = cut1 + 1; cut2 < n; ++cut2) {
        for (int cut3 = cut2 + 1; cut3 < n; ++cut3) for (int cut4 = cut3 + 1; cut4 < n; ++cut4) {
            auto state = problem.make_state(order);
            auto move = state.make_double_bridge(problem, cut1, cut2, cut3, cut4);
            if (!move) continue;
            Cost expected = state.total_cost() + move->delta();
            state.apply(problem, *move);
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
            if (!move) continue;
            Cost expected = state.route_cost(route) + move->delta();
            state.apply(problem, *move);
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
                    if (!move) continue;
                    Cost expected_source = state.route_cost(source) + move->source_delta();
                    Cost expected_target = state.route_cost(target) + move->target_delta();
                    state.apply(problem, *move);
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
                        if (!move) continue;
                        Cost expected1 = state.route_cost(route1) + move->delta1();
                        Cost expected2 = state.route_cost(route2) + move->delta2();
                        state.apply(problem, *move);
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
    assert(first_swap);
    repeated.apply(problem, *first_swap);
    auto second_swap = repeated.make_block_swap(problem, 0, 1, 1, 2, 1, 1);
    assert(second_swap);
    repeated.apply(problem, *second_swap);
    validate_multiple_state(repeated, depots, distance);
    vector<vector<int>> depot_only_routes = {{0, 3, 4, 5, 8}, {1, 6, 7}, {2}};
    auto depot_only = make_multiple_tsp_state(problem, depots, depot_only_routes);
    auto into_depot_only = depot_only.make_block_shift(problem, 0, 1, 2, 2, 0);
    assert(into_depot_only);
    depot_only.apply(problem, *into_depot_only);
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
    assert(stale_move);
    assigned = nearest;
    validate_state(assigned, distance, nearest.order()[0]);
    expect_exception([&] { assigned.apply(problem, *stale_move); });
    assigned = problem.make_state(order);
    validate_state(assigned, distance, order[0]);
    expect_exception([&] { assigned.apply(problem, *stale_move); });
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
    auto edge_penalty = problem.make_state(order);
    TspEdgePenaltySearchOptions edge_options;
    edge_options.max_penalty_rounds = 30;
    edge_options.max_evaluated_moves = 20000;
    auto edge_result = tsp_edge_penalty_search(problem, candidates, edge_penalty, edge_options);
    validate_state(edge_penalty, distance, order[0]);
    assert(edge_result.penalty_rounds <= edge_options.max_penalty_rounds);
    auto edge_reference = run_edge_penalty_reference(problem, candidates, order, edge_options.max_penalty_rounds);
    assert(edge_penalty.total_cost() == edge_reference.best_cost);
    assert(cycle_edges(edge_penalty.order()) == cycle_edges(edge_reference.best_order));
    assert(edge_result.evaluated_moves == edge_reference.evaluated_moves);
    assert(edge_result.applied_moves == edge_reference.applied_moves);
    assert(edge_result.best_updates == edge_reference.best_updates);
    vector<int> comparison_order(n);
    iota(comparison_order.begin(), comparison_order.end(), 0);
    for (int case_id = 0; case_id < 40; ++case_id) {
        auto compared_state = problem.make_state(comparison_order);
        TspEdgePenaltySearchOptions compared_options;
        compared_options.max_penalty_rounds = 80;
        auto compared_result = tsp_edge_penalty_search(problem, candidates, compared_state, compared_options);
        auto reference = run_edge_penalty_reference(problem, candidates, comparison_order, compared_options.max_penalty_rounds);
        validate_state(compared_state, distance, comparison_order[0]);
        assert(compared_state.total_cost() == reference.best_cost);
        assert(cycle_edges(compared_state.order()) == cycle_edges(reference.best_order));
        assert(compared_result.evaluated_moves == reference.evaluated_moves);
        assert(compared_result.applied_moves == reference.applied_moves);
        assert(compared_result.best_updates == reference.best_updates);
        next_permutation(comparison_order.begin() + 1, comparison_order.end());
    }
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
    assert(stale_multiple_move);
    auto replacement_multiple = make_multiple_tsp_state(problem, depots, {{0, 5, 4, 3}, {1, 7, 6}, {2, 8}});
    assigned_multiple = replacement_multiple;
    validate_multiple_state(assigned_multiple, depots, distance);
    expect_exception([&] { assigned_multiple.apply(problem, *stale_multiple_move); });
    test_multiple_moves(problem, distance);
    cout << "ok\n";
}
