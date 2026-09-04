/// https://github.com/titan-23/Library_cpp/blob/main/titan_cpplib/ds/tree_contour_sum.cpp
#pragma once

#include <cassert>
#include <vector>

#include "titan_cpplib/graph/contour_index.cpp"
using namespace std;

namespace titan23 {

/// Point update and distance range sum on a static tree
/// Updates and prod are O(log^2 N)
template<typename T>
class TreeContourSum {
private:
    ContourIndex idx;
    vector<T> val, bit;

    static void add_bit(T *bit, const int *off, int id, int k, const T &x) {
        const int l = off[id];
        const int n = off[id + 1] - l;
        for (; k < n; k |= k + 1) bit[l + k] += x;
    }

    static T prod_bit(const T *bit, const int *off, int id, long long ql, long long qr) {
        const int l = off[id];
        const int n = off[id + 1] - l;
        if (ql < 0) ql = 0;
        if (qr > n) qr = n;
        if (ql >= qr) return T{};
        T res{};
        for (int k = (int)qr; k > 0; k &= k - 1) res += bit[l + k - 1];
        for (int k = (int)ql; k > 0; k &= k - 1) res -= bit[l + k - 1];
        return res;
    }

    static void build_bit(vector<T> &bit, const vector<int> &off) {
        for (int id = 0; id + 1 < (int)off.size(); ++id) {
            const int l = off[id];
            const int n = off[id + 1] - l;
            if (n <= 1) continue;
            for (int k = 0; k < n; ++k) {
                const int p = k | (k + 1);
                if (p < n) bit[l + p] += bit[l + k];
            }
        }
    }

    void init_bit(const vector<T> &a, const int *pd) {
        T *b = bit.data();
        const T *av = a.data();
        const int *po = idx.path_off.data();
        const int *ao = idx.all_off.data();
        const int *so = idx.sub_off.data();
        const int *cp = idx.cpar.data();
        for (int v = 0; v < idx.n; ++v) {
            const T x = av[v];
            int k = po[v];
            const int end = po[v + 1];
            int c = v;
            b[ao[c]] += x;
            int child = c;
            c = cp[c];
            for (; k < end; ++k) {
                const int dist = pd[k];
                b[ao[c] + dist] += x;
                b[so[child] + dist - 1] += x;
                child = c;
                c = cp[c];
            }
        }
    }

    void add_impl(int v, T x, const int *pd) {
        T *b = bit.data();
        const int *po = idx.path_off.data();
        const int *ao = idx.all_off.data();
        const int *so = idx.sub_off.data();
        const int *cp = idx.cpar.data();
        int k = po[v];
        const int end = po[v + 1];
        int c = v;
        add_bit(b, ao, c, 0, x);
        int child = c;
        c = cp[c];
        for (; k < end; ++k) {
            const int dist = pd[k];
            add_bit(b, ao, c, dist, x);
            add_bit(b, so, child, dist - 1, x);
            child = c;
            c = cp[c];
        }
    }

    T prod_impl(int v, int l, int r, const int *pd) const {
        T res{};
        const T *b = bit.data();
        const int *po = idx.path_off.data();
        const int *ao = idx.all_off.data();
        const int *so = idx.sub_off.data();
        const int *cp = idx.cpar.data();
        int k = po[v];
        const int end = po[v + 1];
        int c = v;
        res += prod_bit(b, ao, c, l, r);
        int child = c;
        c = cp[c];
        for (; k < end; ++k) {
            const int dist = pd[k];
            const long long ql = (long long)l - dist;
            const long long qr = (long long)r - dist;
            res += prod_bit(b, ao, c, ql, qr);
            res -= prod_bit(b, so, child, ql - 1, qr - 1);
            child = c;
            c = cp[c];
        }
        return res;
    }

    void init(const vector<T> &a) {
        assert((int)a.size() == idx.n);
        val = a;
        bit.assign(idx.sub_off.back(), T{});
        init_bit(a, idx.path_dist.data());
        build_bit(bit, idx.all_off);
        build_bit(bit, idx.sub_off);
    }

public:
    TreeContourSum() = default;

    explicit TreeContourSum(const vector<vector<int>> &G) : idx(G) {
        init(vector<T>(G.size()));
    }

    TreeContourSum(const vector<vector<int>> &G, const vector<T> &a) : idx(G) {
        init(a);
    }

    int len() const {
        return (int)val.size();
    }

    T get(int v) const {
        assert(0 <= v && v < len());
        return val[v];
    }

    void add(int v, const T &x) {
        assert(0 <= v && v < len());
        const T y = x;
        val[v] += y;
        add_impl(v, y, idx.path_dist.data());
    }

    void set(int v, const T &x) {
        assert(0 <= v && v < len());
        add(v, x - val[v]);
    }

    T prod(int v, int l, int r) const {
        assert(0 <= v && v < len());
        if (l >= r) return T{};
        return prod_impl(v, l, r, idx.path_dist.data());
    }
};

}  // namespace titan23
