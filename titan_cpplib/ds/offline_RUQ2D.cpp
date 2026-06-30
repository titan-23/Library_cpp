#pragma once

#include <vector>
#include <cassert>
using namespace std;

namespace titan23 {

// 2次元 offline RUQ
template<typename T>
class OfflineRUQ2D {
private:
    struct Query {
        int sl, sr, ul, ur;
    };
    struct Rec {
        int k, l, r;
    };

    int h, w, n, m, sz;
    bool swp;
    T init;
    vector<Query> qs;
    vector<T> vals;
    vector<int> off;
    vector<Rec> rec;
    vector<int> nxt, ver;
    int now;
    vector<vector<int>> buf;

    int find(int x) {
        while (true) {
            int p = (ver[x] == now) ? nxt[x] : x;
            if (p == x) return x;
            int pp = (ver[p] == now) ? nxt[p] : p;
            nxt[x] = pp;
            ver[x] = now;
            x = pp;
        }
    }

    void F(int node, const vector<int> &mx, vector<vector<T>> &ans) {
        int p = node - sz;
        if (p >= n) return;
        for (int x = 0; x < m; ++x) {
            if (mx[x] < 0) continue;
            if (swp) {
                ans[x][p] = vals[mx[x]];
            } else {
                ans[p][x] = vals[mx[x]];
            }
        }
    }

    void dfs(int node, const vector<int> &acc, int depth, vector<vector<T>> &ans) {
        if (off[node] == off[node+1]) {
            if (node >= sz) {
                F(node, acc, ans);
            } else {
                dfs(node<<1, acc, depth+1, ans);
                dfs(node<<1|1, acc, depth+1, ans);
            }
            return;
        }
        vector<int> &mx = buf[depth];
        mx = acc;
        ++now;
        for (int t = off[node+1] - 1; t >= off[node]; --t) {
            const Rec &e = rec[t];
            for (int x = find(e.l); x < e.r; x = find(x)) {
                if (e.k > mx[x]) mx[x] = e.k;
                nxt[x] = x+1;
                ver[x] = now;
            }
        }
        if (node >= sz) {
            F(node, mx, ans);
        } else {
            dfs(node<<1, mx, depth+1, ans);
            dfs(node<<1|1, mx, depth+1, ans);
        }
    }

public:
    OfflineRUQ2D() : h(0), w(0) {}

    OfflineRUQ2D(int h, int w, T init) : h(h), w(w), init(init) {
        swp = h > w;
        n = swp ? w : h;
        m = swp ? h : w;
        sz = 1;
        while (sz < n) sz <<= 1;
    }

    void reserve(int q) {
        qs.reserve(q);
        vals.reserve(q);
    }

    // [u, d) x [l, r) <- v / O(log(min(h, w)))
    void apply(int u, int d, int l, int r, T v) {
        assert(0 <= u && u <= d && d <= h);
        assert(0 <= l && l <= r && r <= w);
        if (u == d || l == r) {
            return;
        }
        if (swp) {
            qs.push_back({l, r, u, d});
        } else {
            qs.push_back({u, d, l, r});
        }
        vals.push_back(v);
    }

    // クエリをまとめて実行する / O(hw α + q log(min(h,w)) α)
    vector<vector<T>> tovector() {
        vector<vector<T>> ans(h, vector<T>(w, init));
        if (n == 0 || m == 0 || qs.empty()) return ans;
        int q = qs.size(), N = 2*sz;

        off.assign(N+1, 0);
        for (int k = 0; k < q; ++k) {
            for (int a = qs[k].sl+sz, b = qs[k].sr+sz; a < b; a >>= 1, b >>= 1) {
                if (a  &1) {
                    ++off[a+1];
                    ++a;
                }
                if (b  &1) {
                    --b;
                    ++off[b+1];
                }
            }
        }
        for (int i = 0; i < N; ++i) off[i+1] += off[i];
        rec.resize(off[N]);
        vector<int> pos(off.begin(), off.begin()+N);
        for (int k = 0; k < q; ++k) {
            int l = qs[k].ul, r = qs[k].ur;
            for (int a = qs[k].sl+sz, b = qs[k].sr+sz; a < b; a >>= 1, b >>= 1) {
                if (a & 1) {
                    rec[pos[a]++] = {k, l, r};
                    ++a;
                }
                if (b & 1) {
                    --b;
                    rec[pos[b]++] = {k, l, r};
                }
            }
        }

        nxt.assign(m+1, 0);
        ver.assign(m+1, 0);
        now = 0;
        int log = 0;
        while ((1 << log) < N) {
            ++log;
        }
        buf.assign(log+1, vector<int>(m));
        dfs(1, vector<int>(m, -1), 0, ans);
        return ans;
    }
};
} // namespace titan23
