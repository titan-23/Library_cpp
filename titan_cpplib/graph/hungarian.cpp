/// https://github.com/titan-23/Library_cpp/blob/main/titan_cpplib/graph/hungarian.cpp
// https://kopricky.github.io/code/NetworkFlow/hungarian.html
#pragma once

#include <vector>
#include <utility>
#include <limits>
#include <algorithm>
#include <cassert>
using namespace std;

namespace titan23 {

/// @brief 最小費用割当 (ハンガリアン法) U>=V を要求し 全 V 列を相異なる行に割り当てて費用最小化する
///        余った U-V 行は未割り当て alloc[i]=-1 になる
///        禁止辺を numeric_limits の最大値で表すと桁あふれするので 有限費用の密行列を渡す
template<typename T>
class Hungarian {
private:
    const int U, V;
    vector<vector<int> > graph;
    vector<T> dual;
    vector<int> alloc, rev_alloc, prev;
    const vector<vector<T> >& cost;
    int matching_size;

    T diff(const int i, const int j) {
        return cost[i][j] - dual[i] - dual[U + j];
    }
    void init_feasible_dual() {
        for (int i = 0; i < U; ++i) {
            dual[i] = 0;
            for (int j = 0; j < V; ++j) dual[U + j] = min(dual[U + j], cost[i][j]);
        }
    }
    void construct_graph() {
        for (int i = 0; i < U; ++i)
            for (int j = 0; j < V; ++j)
                graph[i][j] = (diff(i, j) == 0 && rev_alloc[j] != i);
    }
    bool find_augmenting_path(const int cur, const int prv, int &pos) {
        prev[cur] = prv;
        if (cur >= U) {
            if (rev_alloc[cur - U] < 0) return true;
            if (find_augmenting_path(rev_alloc[cur - U], cur, pos)) {
                graph[rev_alloc[cur - U]][cur - U] = 1;
                return true;
            }
        } else {
            const int MX = (alloc[cur] < 0 && pos == U) ? U : V;
            for (int i = 0; i < MX; ++i) {
                if (graph[cur][i] && prev[U + i] < 0 && find_augmenting_path(U + i, cur, pos)) {
                    graph[cur][i] = 0, alloc[cur] = i, rev_alloc[i] = cur;
                    return true;
                }
            }
            if (alloc[cur] < 0 && pos < U) {
                graph[cur][pos] = 0, alloc[cur] = pos, rev_alloc[pos] = cur, prev[U + pos] = cur;
                return ++pos, true;
            }
        }
        return false;
    }
    void update_dual(const T delta) {
        for (int i = 0; i < U; ++i) if (prev[i] >= 0) dual[i] += delta;
        for (int i = U; i < U + V; ++i) if (prev[i] >= 0) dual[i] -= delta;
    }
    void maximum_matching(bool initial = false) {
        int pos = initial ? V : U;
        for (bool update = false;; update = false) {
            fill(prev.begin(), prev.end(), -1);
            for (int i = 0; i < U; ++i) {
                if (alloc[i] < 0 && find_augmenting_path(i, 2 * U, pos)) {
                    update = true, ++matching_size;
                    break;
                }
            }
            if (!update) break;
        }
    }
    int dfs(const int cur, const int prv, vector<int> &new_ver) {
        prev[cur] = prv;
        if (cur >= U) {
            if (rev_alloc[cur - U] < 0) return cur;
            else return dfs(rev_alloc[cur - U], cur, new_ver);
        } else {
            new_ver.push_back(cur);
            for (int i = 0; i < V; ++i) {
                if (graph[cur][i] && prev[U + i] < 0) {
                    const int res = dfs(U + i, cur, new_ver);
                    if (res >= U) return res;
                }
            }
        }
        return -1;
    }
    int increase_matching(const vector<pair<int, int> > &vec, vector<int> &new_ver) {
        for (const auto &e : vec) {
            if (prev[e.first] < 0) {
                const int res = dfs(e.first, e.second, new_ver);
                if (res >= U) return res;
            }
        }
        return -1;
    }
    void hint_increment(int cur) {
        while (prev[cur] != 2 * U) {
            if (cur >= U) graph[prev[cur]][cur - U] = 0, alloc[prev[cur]] = cur - U, rev_alloc[cur - U] = prev[cur];
            else graph[cur][prev[cur] - U] = 1;
            cur = prev[cur];
        }
    }

public:
    /// @brief cost は U 行 V 列 U>=V が前提
    Hungarian(const vector<vector<T> > &_cost)
        : U((int)_cost.size()), V((int)_cost[0].size()), graph(U, vector<int>(U, 1)),
          dual(U + V, numeric_limits<T>::max()), alloc(U, -1), rev_alloc(U, -1),
          prev(2 * U), cost{_cost}, matching_size(0) {
        assert(U >= V);
    }

    /// @brief {最小費用, alloc} を返す alloc[i] は行 i が担当する列 未割り当ては -1
    pair<T, vector<int> > solve() {
        init_feasible_dual(), construct_graph();
        bool end = false;
        maximum_matching(true);
        while (matching_size < U) {
            vector<pair<T, int> > cand(V, {numeric_limits<T>::max(), numeric_limits<int>::max()});
            for (int i = 0; i < U; ++i) {
                if (prev[i] < 0) continue;
                for (int j = 0; j < V; ++j) {
                    if (prev[U + j] >= 0) continue;
                    cand[j] = min(cand[j], {diff(i, j), i});
                }
            }
            while (true) {
                T delta = numeric_limits<T>::max();
                for (int i = 0; i < V; ++i) {
                    if (prev[U + i] >= 0) continue;
                    delta = min(delta, cand[i].first);
                }
                update_dual(delta);
                vector<pair<int, int> > vec;
                vector<int> new_ver;
                for (int i = 0; i < V; ++i) {
                    if (prev[U + i] >= 0) continue;
                    if ((cand[i].first -= delta) == 0) vec.emplace_back(U + i, cand[i].second);
                }
                int res = increase_matching(vec, new_ver);
                if (res >= U) {
                    hint_increment(res);
                    if (++matching_size == U) end = true;
                    else construct_graph();
                    break;
                } else {
                    for (const int v : new_ver) {
                        for (int i = 0; i < V; ++i) {
                            if (prev[U + i] >= 0) continue;
                            cand[i] = min(cand[i], {diff(v, i), v});
                        }
                    }
                }
            }
            if (!end) maximum_matching();
        }
        T total_cost = 0;
        for (int i = 0; i < U; ++i) {
            if (alloc[i] < V) total_cost += cost[i][alloc[i]];
            else alloc[i] = -1;
        }
        return make_pair(total_cost, alloc);
    }
};

/// @brief 行数と列数の大小を気にせず最小費用割当を解く
///        小さい方の次元を全て割り当てる assign は行数ぶんの長さ 未割り当ては -1
template<typename T>
pair<T, vector<int> > min_cost_assignment(const vector<vector<T> > &cost) {
    int n = (int)cost.size();
    int m = n ? (int)cost[0].size() : 0;
    if (n >= m) {
        Hungarian<T> h(cost);
        return h.solve();
    }
    vector<vector<T> > t(m, vector<T>(n));
    for (int i = 0; i < n; ++i)
        for (int j = 0; j < m; ++j) t[j][i] = cost[i][j];
    Hungarian<T> h(t);
    auto [c, alloc_t] = h.solve();
    vector<int> assign(n, -1);
    for (int j = 0; j < m; ++j)
        if (alloc_t[j] != -1) assign[alloc_t[j]] = j;
    return make_pair(c, assign);
}

}  // namespace titan23
