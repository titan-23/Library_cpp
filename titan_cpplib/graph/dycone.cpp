/// https://github.com/titan-23/Library_cpp/blob/main/titan_cpplib/graph/dycone.cpp
#pragma once

#include <vector>
#include <random>
#include <cassert>
#include <climits>
#include <numeric>
#include <algorithm>
#include <unordered_map>
using namespace std;

// DyCone
namespace titan23 {

/// @brief オフラインダイコネ / 全体で期待 `O((n+q)logn)`
class DyCone {
private:
    static constexpr int ADD = 0, DEL = 1, QUERY = 2;
    static constexpr int NEVER = -INT_MAX;

    struct Node { int par, w; };
    struct Event { int type, u, v, w; };

    int n;
    int group_count_;
    vector<Node> nd;
    vector<int> rd, sz, stk;
    vector<Event> Q;
    vector<pair<int, int>> chain;
    unordered_map<long long, int> mp;

    void shortcut(int u) {
        int p = nd[u].par;
        sz[p] -= sz[u];
        nd[u].par = nd[p].par;
    }

    int find(int u, int w = 0) {
        while (nd[u].w <= w) {
            while (nd[nd[u].par].w <= nd[u].w) shortcut(u);
            u = nd[u].par;
        }
        return u;
    }

    void disconnect(int u) {
        stk.clear();
        while (nd[u].par != u) {
            stk.push_back(u);
            u = nd[u].par;
        }
        int m = stk.size();
        for (int i = m-1; i >= 0; --i) {
            int v = stk[i];
            sz[nd[v].par] -= sz[v];
        }
    }

    int connect(int u, int w = 0) {
        while (nd[u].w <= w) {
            int p = nd[u].par;
            sz[p] += sz[u];
            u = p;
        }
        return u;
    }

    int max_edge(int u, int v) {
        if (find(u) != find(v)) return -1;
        while (true) {
            if (nd[u].w > nd[v].w) swap(u, v);
            if (nd[u].par == v) break;
            u = nd[u].par;
        }
        return u;
    }

    void sub_add(int u, int v, int w) {
        disconnect(u);
        disconnect(v);
        group_count_--;
        while (u != v) {
            u = connect(u, w);
            v = connect(v, w);
            if (rd[u] < rd[v]) swap(u, v);
            int np = nd[v].par, nw = nd[v].w;
            nd[v] = {u, w};
            u = np;
            w = nw;
        }
        connect(u);
    }

    void sub_del(int u, int v, int w) {
        while (nd[u].par != u) {
            if (nd[u].w == w) {
                int x = u;
                while (nd[x].par != x) {
                    x = nd[x].par;
                    sz[x] -= sz[u];
                }
                nd[u] = {u, 1};
                if (find(u) != find(v)) group_count_++;
                return;
            }
            while (nd[nd[u].par].w <= nd[u].w) shortcut(u);
            u = nd[u].par;
        }
    }

    void inner_add_edge(int u, int v, int w) {
        int p = max_edge(u, v);
        if (p == -1) {
            sub_add(u, v, w);
        } else if (nd[p].w > w) {
            sub_del(p, nd[p].par, nd[p].w);
            sub_add(u, v, w);
        }
    }

    void inner_delete_edge(int u, int v, int w) {
        sub_del(u, v, w);
        sub_del(v, u, w);
    }

public:
    DyCone() : n(0) {}

    /// @brief 頂点数 `n` で初期化する / `O(n)`
    DyCone(int n, int seed = 1321312) : n(n), group_count_(n), nd(n), rd(n), sz(n, 1) {
        for (int i = 0; i < n; ++i) nd[i] = {i, 1};
        iota(rd.begin(), rd.end(), 0);
        shuffle(rd.begin(), rd.end(), mt19937(seed));
    }

    /// @brief 更新 `cap` 個分の領域を確保する / `O(cap)`
    void reserve(int cap) {
        Q.reserve(cap);
        chain.reserve(cap);
        mp.reserve(cap);
    }

    /// @brief 辺 `(u, v)` を追加する / 期待 `O(logn)`
    void add_edge(int u, int v) {
        assert(0 <= u && u < n && 0 <= v && v < n);
        if (u == v) return;
        if (u > v) swap(u, v);
        int ci = chain.size();
        auto [it, is_new] = mp.try_emplace((long long)u * n + v, ci);
        int prev = -1;
        if (!is_new) {
            prev = it->second;
            it->second = ci;
        }
        chain.emplace_back(Q.size(), prev);
        Q.emplace_back(ADD, u, v, NEVER);
    }

    /// @brief 辺 `(u, v)` を削除する 存在する辺のみ削除できる / 期待 `O(logn)`
    void delete_edge(int u, int v) {
        assert(0 <= u && u < n && 0 <= v && v < n);
        if (u == v) return;
        if (u > v) swap(u, v);
        auto it = mp.find((long long)u * n + v);
        assert(it != mp.end() && it->second != -1);
        auto [qi, prev] = chain[it->second];
        it->second = prev;
        int t = Q.size();
        Q[qi].w = -t;
        Q.emplace_back(DEL, u, v, -t);
    }

    /// @brief クエリ時刻とする / `O(1)`
    void next_query() {
        Q.emplace_back(QUERY, 0, 0, NEVER);
    }

    /// @brief 更新を順に処理し、`k` 番目のクエリ時刻で `out(k)` を呼ぶ
    /// 全体で期待 `O((n+q)logn)`
    /// @param out `void out(int k)`
    template<typename F>
    void run(F &&out) {
        vector<pair<int, int>>().swap(chain);
        unordered_map<long long, int>().swap(mp);
        int k = 0;
        for (const Event &e : Q) {
            switch (e.type) {
                case ADD: inner_add_edge(e.u, e.v, e.w); break;
                case DEL: inner_delete_edge(e.u, e.v, e.w); break;
                case QUERY: out(k++); break;
            }
        }
    }

    /// @brief `u` と `v` が同じ成分にあるかを返す / 期待 `O(logn)`
    bool same(int u, int v) {
        assert(0 <= u && u < n && 0 <= v && v < n);
        return find(u) == find(v);
    }

    /// @brief `u` と同じ成分の頂点数を返す / 期待 `O(logn)`
    int size(int u) {
        assert(0 <= u && u < n);
        return sz[find(u)];
    }

    /// @brief 成分の個数を返す / `O(1)`
    int group_count() const {
        return group_count_;
    }
};
} // namespace titan23
