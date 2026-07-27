#pragma once

#include <vector>
#include <random>
#include <cassert>
#include <climits>
#include <numeric>
#include <algorithm>
#include <unordered_map>
using namespace std;

// DyConeSum
namespace titan23 {

/// @brief オフライン動的連結性(点加算・成分加算・成分和つき) / 全体で期待 `O((n+q)logn)`
/// @note 更新はためておき、`run()` で順に処理する
template<typename T>
class DyConeSum {
private:
    static constexpr int ADD = 0, DEL = 1, POINT = 2, GROUP = 3, QUERY = 4;
    static constexpr int NEVER = -INT_MAX;

    struct Node { int par, w; };
    struct Event { int type, u, v, w; };

    int n;
    int group_count_;
    vector<Node> nd;
    vector<int> rd, sz, stk;
    vector<T> val, lz, acc;
    vector<Event> Q;
    vector<T> vals;
    vector<pair<int, int>> chain;
    unordered_map<long long, int> mp;

    void shortcut(int u) {
        int p = nd[u].par;
        T d = lz[p] * sz[u];
        val[p] -= val[u] + d;
        sz[p] -= sz[u];
        val[u] += d;
        lz[u] += lz[p];
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
        acc[u] = lz[u];
        int m = stk.size();
        for (int i = m-1; i >= 0; --i) {
            int v = stk[i], p = nd[v].par;
            val[p] -= val[v] + lz[p] * sz[v];
            sz[p] -= sz[v];
            acc[v] = acc[p] + lz[v];
        }
    }

    int connect(int u, int w = 0) {
        while (nd[u].w <= w) {
            int p = nd[u].par;
            val[p] += val[u] + lz[p] * sz[u];
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
            T nlz = acc[v] - acc[u];
            val[v] += (nlz - lz[v]) * sz[v];
            lz[v] = nlz;
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
                T L = T();
                while (nd[x].par != x) {
                    int p = nd[x].par;
                    L += lz[p];
                    val[p] -= val[u] + L * sz[u];
                    sz[p] -= sz[u];
                    x = p;
                }
                val[u] += L * sz[u];
                lz[u] += L;
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

    void inner_add_point(int u, T x) {
        while (true) {
            if (nd[u].par == u) {
                val[u] += x;
                break;
            }
            while (nd[nd[u].par].w <= nd[u].w) shortcut(u);
            val[u] += x;
            u = nd[u].par;
        }
    }

    void inner_add_group(int u, T x) {
        int r = find(u);
        lz[r] += x;
        val[r] += x * sz[r];
    }

public:
    DyConeSum() : n(0) {}

    /// @brief 頂点数 `n`、各頂点の値 `0` で初期化する / `O(n)`
    DyConeSum(int n, int seed = 1321312)
            : n(n), group_count_(n), nd(n), rd(n), sz(n, 1), val(n), lz(n), acc(n) {
        for (int i = 0; i < n; ++i) nd[i] = {i, 1};
        iota(rd.begin(), rd.end(), 0);
        shuffle(rd.begin(), rd.end(), mt19937(seed));
    }

    /// @brief 各頂点の値を `init` で初期化する / `O(n)`
    DyConeSum(const vector<T> &init, int seed = 1321312) : DyConeSum(init.size(), seed) {
        val = init;
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

    /// @brief 辺 `(u, v)` を削除する。今ある辺だけ削除できる / 期待 `O(logn)`
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

    /// @brief 頂点 `u` に `x` を加算する / 期待 `O(logn)`
    void add_point(int u, T x) {
        assert(0 <= u && u < n);
        Q.emplace_back(POINT, u, vals.size(), NEVER);
        vals.push_back(x);
    }

    /// @brief `u` と同じ成分の全頂点に `x` を加算する / 期待 `O(logn)`
    void add_group(int u, T x) {
        assert(0 <= u && u < n);
        Q.emplace_back(GROUP, u, vals.size(), NEVER);
        vals.push_back(x);
    }

    /// @brief ここをクエリ時刻とする / `O(1)`
    void next_query() {
        Q.emplace_back(QUERY, 0, 0, NEVER);
    }

    /// @brief 更新を順に処理し、`k` 番目のクエリ時刻で `out(k)` を呼ぶ / 全体で期待 `O((n+q)logn)`
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
                case POINT: inner_add_point(e.u, vals[e.v]); break;
                case GROUP: inner_add_group(e.u, vals[e.v]); break;
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

    /// @brief `u` と同じ成分の値の総和を返す / 期待 `O(logn)`
    T sum(int u) {
        assert(0 <= u && u < n);
        return val[find(u)];
    }

    /// @brief 成分の個数を返す / `O(1)`
    int group_count() const {
        return group_count_;
    }
};
} // namespace titan23
