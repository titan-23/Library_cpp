#include <algorithm>
#include <cassert>
#include <iostream>
#include <queue>
#include <random>
#include <type_traits>
#include <utility>
#include <vector>

#include "titan_cpplib/ds/tree_contour_add.cpp"
#include "titan_cpplib/ds/tree_contour_sum.cpp"

using namespace std;
using titan23::TreeContourAdd;
using titan23::TreeContourSum;

using Graph = vector<vector<int>>;
using Sum = TreeContourSum<long long>;
using Add = TreeContourAdd<long long>;

static_assert(is_constructible_v<Sum, const Graph &>);
static_assert(is_constructible_v<Sum, const Graph &, const vector<long long> &>);
static_assert(is_constructible_v<Add, const Graph &>);
static_assert(is_constructible_v<Add, const Graph &, const vector<long long> &>);

vector<vector<int>> distances(const vector<vector<int>> &g) {
    const int n = (int)g.size();
    vector<vector<int>> dist(n, vector<int>(n, -1));
    for (int s = 0; s < n; ++s) {
        queue<int> que;
        dist[s][s] = 0;
        que.push(s);
        while (!que.empty()) {
            int v = que.front();
            que.pop();
            for (int x : g[v]) {
                if (dist[s][x] != -1) continue;
                dist[s][x] = dist[s][v] + 1;
                que.push(x);
            }
        }
    }
    return dist;
}

long long naive_prod(const vector<long long> &a, const vector<vector<int>> &dist, int v, int l, int r) {
    long long res = 0;
    for (int u = 0; u < (int)a.size(); ++u) {
        if (l <= dist[v][u] && dist[v][u] < r) res += a[u];
    }
    return res;
}

void test_graph(const vector<vector<int>> &g, mt19937_64 &rng) {
    const int n = (int)g.size();
    const vector<vector<int>> dist = distances(g);
    vector<long long> a(n), b(n);
    for (long long &x : a) x = (int)(rng() % 101) - 50;
    for (long long &x : b) x = (int)(rng() % 101) - 50;

    TreeContourSum<long long> sum(g, a);
    TreeContourAdd<long long> add(g, b);
    assert(sum.len() == n);
    assert(add.len() == n);

    for (int step = 0; step < 5000; ++step) {
        const int type = (int)(rng() % 5);
        const int v = (int)(rng() % n);
        int l = (int)(rng() % (n + 5)) - 2;
        int r = (int)(rng() % (n + 5)) - 2;
        if (l > r) swap(l, r);
        if (type == 0) {
            const long long x = (int)(rng() % 101) - 50;
            sum.add(v, x);
            a[v] += x;
        } else if (type == 1) {
            const long long x = (int)(rng() % 101) - 50;
            sum.set(v, x);
            a[v] = x;
        } else if (type == 2) {
            assert(sum.prod(v, l, r) == naive_prod(a, dist, v, l, r));
            assert(sum.get(v) == a[v]);
        } else if (type == 3) {
            const long long x = (int)(rng() % 101) - 50;
            add.add(v, l, r, x);
            for (int u = 0; u < n; ++u) {
                if (l <= dist[v][u] && dist[v][u] < r) b[u] += x;
            }
        } else {
            assert(add.get(v) == b[v]);
        }
        if (step % 101 == 0) {
            for (int u = 0; u < n; ++u) {
                assert(sum.get(u) == a[u]);
                assert(add.get(u) == b[u]);
                for (int ql = -1; ql <= n + 1; ++ql) {
                    assert(sum.prod(u, ql, ql + 2) == naive_prod(a, dist, u, ql, ql + 2));
                }
            }
        }
    }
}

vector<vector<int>> make_path(int n) {
    vector<vector<int>> g(n);
    for (int i = 1; i < n; ++i) {
        g[i - 1].push_back(i);
        g[i].push_back(i - 1);
    }
    return g;
}

vector<vector<int>> make_star(int n) {
    vector<vector<int>> g(n);
    for (int i = 1; i < n; ++i) {
        g[0].push_back(i);
        g[i].push_back(0);
    }
    return g;
}

vector<vector<int>> make_random_tree(int n, mt19937_64 &rng) {
    vector<vector<int>> g(n);
    for (int i = 1; i < n; ++i) {
        const int p = (int)(rng() % i);
        g[p].push_back(i);
        g[i].push_back(p);
    }
    return g;
}

vector<vector<int>> make_balanced_tree(int n) {
    vector<vector<int>> g(n);
    for (int v = 1; v < n; ++v) {
        const int p = (v - 1) / 2;
        g[p].push_back(v);
        g[v].push_back(p);
    }
    return g;
}

vector<vector<int>> make_broom(int n) {
    vector<vector<int>> g(n);
    const int h = n / 2;
    for (int v = 1; v < h; ++v) {
        g[v - 1].push_back(v);
        g[v].push_back(v - 1);
    }
    for (int v = h; v < n; ++v) {
        g[h - 1].push_back(v);
        g[v].push_back(h - 1);
    }
    return g;
}

vector<vector<int>> make_distance_width_regression(int &arm_end, int &center) {
    constexpr int arm_len = 200;
    constexpr int center_leaves = 600;
    constexpr int root_leaves = 1001;
    vector<vector<int>> g(2 + arm_len * 2 + center_leaves + root_leaves);
    auto add_edge = [&](int u, int v) {
        g[u].push_back(v);
        g[v].push_back(u);
    };
    add_edge(0, 1);
    int next = 2;
    int prev = 1;
    for (int i = 0; i < arm_len; ++i) {
        add_edge(prev, next);
        prev = next++;
    }
    arm_end = prev;
    prev = 1;
    for (int i = 0; i < arm_len; ++i) {
        add_edge(prev, next);
        prev = next++;
    }
    center = prev;
    for (int i = 0; i < center_leaves; ++i) add_edge(center, next++);
    for (int i = 0; i < root_leaves; ++i) add_edge(0, next++);
    assert(next == (int)g.size());
    return g;
}

int main() {
    TreeContourSum<long long> empty_sum(vector<vector<int>>{});
    TreeContourAdd<long long> empty_add(vector<vector<int>>{});
    assert(empty_sum.len() == 0);
    assert(empty_add.len() == 0);

    mt19937_64 rng(123456789);
    test_graph(make_path(1), rng);
    test_graph(make_path(2), rng);
    test_graph(make_path(3), rng);
    test_graph(make_path(30), rng);
    test_graph(make_star(4), rng);
    test_graph(make_star(30), rng);
    test_graph(make_balanced_tree(31), rng);
    test_graph(make_broom(31), rng);
    for (int tc = 0; tc < 30; ++tc) test_graph(make_random_tree((int)(rng() % 45) + 1, rng), rng);

    vector<vector<int>> g = make_path(5);
    TreeContourSum<long long> tree(g);
    assert(tree.len() == 5);
    TreeContourSum<long long> sum(g, {1, 2, 3, 4, 5});
    TreeContourAdd<long long> add(g, {1, 2, 3, 4, 5});
    assert(sum.prod(2, 0, 1) == 3);
    assert(sum.prod(2, 1, 2) == 6);
    assert(sum.prod(2, 0, 100) == 15);
    add.add(2, 1, 3, 10);
    assert(add.get(0) == 11);
    assert(add.get(1) == 12);
    assert(add.get(2) == 3);
    assert(add.get(3) == 14);
    assert(add.get(4) == 15);

    int arm_end, center;
    vector<vector<int>> width_graph = make_distance_width_regression(arm_end, center);
    vector<long long> width_values(width_graph.size());
    width_values[arm_end] = 7;
    TreeContourSum<long long> width_sum(width_graph, width_values);
    TreeContourAdd<long long> width_add(width_graph);
    assert(width_sum.prod(center, 400, 401) == 7);
    width_add.add(center, 400, 401, 9);
    assert(width_add.get(arm_end) == 9);

    constexpr int long_n = 131072;
    vector<vector<int>> long_path = make_path(long_n);
    vector<long long> long_values(long_n);
    long_values[0] = 11;
    TreeContourSum<long long> long_sum(long_path, long_values);
    TreeContourAdd<long long> long_add(long_path);
    assert(long_sum.prod(long_n - 1, long_n - 1, long_n) == 11);
    long_add.add(long_n - 1, long_n - 1, long_n, 13);
    assert(long_add.get(0) == 13);

    TreeContourSum<long long> owned_sum(make_path(5), {1, 2, 3, 4, 5});
    TreeContourAdd<long long> owned_add(make_path(5), {1, 2, 3, 4, 5});
    assert(owned_sum.prod(0, 0, 5) == 15);
    owned_add.add(0, 0, 5, 2);
    assert(owned_add.get(4) == 7);

    TreeContourSum<long long> copy_sum = owned_sum;
    copy_sum.add(0, 10);
    assert(copy_sum.get(0) == 11);
    assert(owned_sum.get(0) == 1);
    TreeContourSum<long long> move_sum = move(copy_sum);
    assert(move_sum.prod(0, 0, 5) == 25);

    TreeContourAdd<long long> copy_add = owned_add;
    copy_add.add(0, 4, 5, 3);
    assert(copy_add.get(4) == 10);
    assert(owned_add.get(4) == 7);
    TreeContourAdd<long long> move_add = move(copy_add);
    assert(move_add.get(4) == 10);

    cout << "tree contour random tests passed\n";
}
