/// https://github.com/titan-23/Library_cpp/blob/main/titan_cpplib/ds/tree_contour_add.cpp
#pragma once

#include <algorithm>
#include <cassert>
#include <vector>

#include "titan_cpplib/graph/contour_index.cpp"
using namespace std;

namespace titan23 {

/// Distance range add and point get on a static tree
/// Operations are O(log^2 N)
template<typename T>
class TreeContourAdd {
private:
    ContourIndex idx;
    vector<T> base, bit;

    static void add_bit(T *bit, int l, int n, int k, const T &x) {
        for (; k < n; k |= k + 1) bit[l + k] += x;
    }

    static T get_bit(const T *bit, const int *off, int id, int k) {
        const int l = off[id];
        T res{};
        for (++k; k > 0; k &= k - 1) res += bit[l + k - 1];
        return res;
    }

    static void add_range(T *bit, const int *off, int id, int ql, int qr, const T &x, const T &nx) {
        const int l = off[id];
        const int n = off[id + 1] - l;
        if (ql < 0) ql = 0;
        if (qr > n) qr = n;
        if (ql >= qr) return;
        add_bit(bit, l, n, ql, x);
        if (qr < n) add_bit(bit, l, n, qr, nx);
    }

    void add_impl(int v, int l, int r, T x, const int *pd) {
        const T nx = T{} - x;
        T *b = bit.data();
        const int *po = idx.path_off.data();
        const int *ao = idx.all_off.data();
        const int *so = idx.sub_off.data();
        const int *cp = idx.cpar.data();
        int k = po[v];
        const int end = po[v + 1];
        int c = v;
        add_range(b, ao, c, l, r, x, nx);
        int child = c;
        c = cp[c];
        for (; k < end; ++k) {
            const int dist = pd[k];
            const int ql = l - dist;
            const int qr = r - dist;
            add_range(b, ao, c, ql, qr, x, nx);
            add_range(b, so, child, ql - 1, qr - 1, nx, x);
            child = c;
            c = cp[c];
        }
    }

    T get_impl(int v, const int *pd) const {
        T res = base[v];
        const T *b = bit.data();
        const int *po = idx.path_off.data();
        const int *ao = idx.all_off.data();
        const int *so = idx.sub_off.data();
        const int *cp = idx.cpar.data();
        int k = po[v];
        const int end = po[v + 1];
        int c = v;
        res += get_bit(b, ao, c, 0);
        int child = c;
        c = cp[c];
        for (; k < end; ++k) {
            const int dist = pd[k];
            res += get_bit(b, ao, c, dist);
            res += get_bit(b, so, child, dist - 1);
            child = c;
            c = cp[c];
        }
        return res;
    }

    void init(const vector<T> &a) {
        assert((int)a.size() == idx.n);
        base = a;
        bit.assign(idx.sub_off.back(), T{});
    }

public:
    TreeContourAdd() = default;

    explicit TreeContourAdd(const vector<vector<int>> &G) : idx(G) {
        init(vector<T>(G.size()));
    }

    TreeContourAdd(const vector<vector<int>> &G, const vector<T> &a) : idx(G) {
        init(a);
    }

    int len() const {
        return (int)base.size();
    }

    void add(int v, int l, int r, const T &x) {
        assert(0 <= v && v < len());
        l = clamp(l, 0, idx.n);
        r = clamp(r, 0, idx.n);
        if (l >= r) return;
        const T y = x;
        add_impl(v, l, r, y, idx.path_dist.data());
    }

    T get(int v) const {
        assert(0 <= v && v < len());
        return get_impl(v, idx.path_dist.data());
    }
};

}  // namespace titan23
