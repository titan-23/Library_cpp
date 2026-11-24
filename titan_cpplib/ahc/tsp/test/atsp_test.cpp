/// https://github.com/titan-23/Library_cpp/blob/main/titan_cpplib/ahc/tsp/test/atsp_test.cpp
#include <algorithm>
#include <cassert>
#include <climits>
#include <iostream>
#include <numeric>
#include <stdexcept>
#include <vector>
#include "titan_cpplib/ahc/tsp/atsp_iterated_local_search.cpp"
using namespace std;
using namespace titan23;

using Cost = long long;

// 向きによってコストが異なる距離行列
vector<vector<Cost>> make_distance(int n) {
    vector<vector<Cost>> distance(n, vector<Cost>(n));
    for (int u = 0; u < n; ++u) for (int v = 0; v < n; ++v) {
        if (u != v) distance[u][v] = 1 + ((u + 3) * (v + 5) * 17 + u * 29 + v * 11) % 101;
    }
    return distance;
}

Cost cycle_cost(const vector<int>& order, const vector<vector<Cost>>& distance) {
    Cost result = 0;
    for (int i = 0; i < (int)order.size(); ++i) result += distance[order[i]][order[(i + 1) % order.size()]];
    return result;
}

Cost path_cost(const vector<int>& order, const vector<vector<Cost>>& distance) {
    Cost result = 0;
    for (int i = 0; i + 1 < (int)order.size(); ++i) result += distance[order[i]][order[i + 1]];
    return result;
}

void validate_permutation(const vector<int>& order, int n) {
    assert((int)order.size() == n);
    vector<int> seen(n);
    for (int v : order) {
        assert(0 <= v && v < n && !seen[v]);
        seen[v] = 1;
    }
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

int main() {
    int n = 9;
    vector<vector<Cost>> distance = make_distance(n);
    auto edge_cost = [&](int u, int v) -> Cost { return distance[u][v]; };
    TspProblem problem(n, edge_cost);

    // 候補表は、出る辺と入る辺のそれぞれについてコストの昇順
    AtspCandidates candidates = make_atsp_candidates(problem, 4);
    assert(candidates.node_count == n && candidates.candidate_count == 4);
    for (int u = 0; u < n; ++u) {
        for (int i = 0; i + 1 < 4; ++i) {
            assert(distance[u][candidates.out[u * 4 + i]] <= distance[u][candidates.out[u * 4 + i + 1]]);
            assert(distance[candidates.in[u * 4 + i]][u] <= distance[candidates.in[u * 4 + i + 1]][u]);
        }
    }

    // 全ての巡回順を列挙して求めた最小のコストと照合する
    vector<int> all(n);
    iota(all.begin(), all.end(), 0);
    Cost best_cycle = LLONG_MAX;
    vector<Cost> best_path(n, LLONG_MAX);
    Cost best_free_path = LLONG_MAX;
    do {
        best_cycle = min(best_cycle, cycle_cost(all, distance));
        best_path[all.back()] = min(best_path[all.back()], path_cost(all, distance));
        best_free_path = min(best_free_path, path_cost(all, distance));
    } while (next_permutation(all.begin() + 1, all.end()));

    AtspCandidates full_candidates = make_atsp_candidates(problem, n - 1);
    vector<int> order(n);
    iota(order.begin(), order.end(), 0);
    Cost initial_cost = atsp_tour_cost(problem, order);
    assert(initial_cost == cycle_cost(order, distance));
    AtspIteratedLocalSearchOptions options;
    options.max_iterations = 3000;
    auto result = atsp_iterated_local_search(problem, full_candidates, order, options);
    validate_permutation(order, n);
    assert(order[0] == 0);
    assert(result.iterations == options.max_iterations);
    assert(result.stop_reason == TspSearchStopReason::iteration_limit);
    assert(cycle_cost(order, distance) == best_cycle);

    // 区間を反転する操作を使用しない設定
    vector<int> no_reverse(n);
    iota(no_reverse.begin(), no_reverse.end(), 0);
    options.max_reverse_length = 0;
    atsp_iterated_local_search(problem, full_candidates, no_reverse, options);
    validate_permutation(no_reverse, n);
    assert(cycle_cost(no_reverse, distance) <= initial_cost);

    // 部分巡回路は、含まれる点だけを並べ替える
    vector<int> partial = {1, 2, 4, 6, 8};
    Cost partial_cost = cycle_cost(partial, distance);
    options.max_reverse_length = 200;
    atsp_iterated_local_search(problem, full_candidates, partial, options);
    assert(partial.size() == 5 && partial[0] == 1);
    assert(cycle_cost(partial, distance) <= partial_cost);

    expect_exception([&] {
        vector<int> duplicated = {0, 1, 1, 2};
        atsp_iterated_local_search(problem, full_candidates, duplicated, options);
    });
    expect_exception([&] {
        AtspIteratedLocalSearchOptions no_limit;
        atsp_iterated_local_search(problem, full_candidates, order, no_limit);
    });

    vector<int> assignment = make_atsp_assignment_order(problem, 3);
    validate_permutation(assignment, n);
    assert(assignment[0] == 3);

    vector<int> solved = solve_atsp(n, edge_cost, 20.0, 4);
    validate_permutation(solved, n);
    assert(solved[0] == 4);
    assert(cycle_cost(solved, distance) == best_cycle);

    // 戻らない経路 / 始点は 0 で、終点を固定する場合と固定しない場合
    vector<int> free_path = solve_atsp_path(n, edge_cost, 20.0, 0);
    validate_permutation(free_path, n);
    assert(free_path[0] == 0);
    assert(path_cost(free_path, distance) == best_free_path);
    for (int goal : {1, 5, 8}) {
        vector<int> fixed_path = solve_atsp_path(n, edge_cost, 20.0, 0, goal);
        validate_permutation(fixed_path, n);
        assert(fixed_path[0] == 0 && fixed_path.back() == goal);
        assert(path_cost(fixed_path, distance) == best_path[goal]);
    }
    expect_exception([&] { solve_atsp_path(n, edge_cost, 20.0, 0, 0); });
    cout << "ok\n";
}
