/// https://github.com/titan-23/Library_cpp/blob/main/titan_cpplib/ds/sparse_segment_tree2DFAST.cpp
#pragma once

#include <cassert>
#include <vector>
#include "titan_cpplib/others/bit.cpp"
using namespace std;

namespace titan23 {

/// @brief y の範囲が狭いときの 2 次元セグメント木 / x 方向は必要な節点だけを作る / op は可換であること
template <class IndexType, class T, T (*op)(T, T), T (*e)()>
class SparseSegmentTree2DFAST {
private:
    IndexType H, W;
    int size, log;
    vector<int> dat;

    struct NodeX {
        T data;
        int lch, rch;
    };

    vector<NodeX> X;

    void updateX(int node) {
        X[node].data = op(X[X[node].lch].data, X[X[node].rch].data);
    }

    int new_node_x() {
        X.emplace_back(NodeX{e(), 0, 0});
        return X.size() - 1;
    }

    T inner_getX(int node, IndexType x, IndexType L, IndexType R) const {
        if (!node) return e();
        if (R-L == 1) return X[node].data;
        IndexType M = L+(R-L)/2;
        if (x < M) return inner_getX(X[node].lch, x, L, M);
        else return inner_getX(X[node].rch, x, M, R);
    }

    void inner_setX(int node, IndexType x, T v, IndexType L, IndexType R) {
        if (R-L == 1) {
            X[node].data = v;
            return;
        }
        IndexType M = L+(R-L)/2;
        if (x < M) {
            if (!X[node].lch) X[node].lch = new_node_x();
            inner_setX(X[node].lch, x, v, L, M);
        } else {
            if (!X[node].rch) X[node].rch = new_node_x();
            inner_setX(X[node].rch, x, v, M, R);
        }
        updateX(node);
    }

    T inner_prodX(int node, IndexType l, IndexType r, IndexType L, IndexType R) const {
        if (!node || l >= r || r <= L || R <= l) return e();
        if (l <= L && R <= r) return X[node].data;
        IndexType M = L+(R-L)/2;
        return op(
            inner_prodX(X[node].lch, l, r, L, M),
            inner_prodX(X[node].rch, l, r, M, R)
        );
    }

public:
    SparseSegmentTree2DFAST() {}
    /// @brief H × W の領域で構築する / 初期値 e() / H < 1e8 / O(H)
    SparseSegmentTree2DFAST(IndexType H, IndexType W) : H(H), W(W) {
        assert(H < 1e8);
        log = bit_length(H);
        size = 1<<log;
        dat.resize(size*2, 0);
        new_node_x(); // 番兵
    }

    /// @brief 節点 cap 個分の領域を予約する
    void reserve(int cap) {
        X.reserve(cap);
    }

    /// @brief (y, x) の値を返す / O(log W)
    T get(IndexType y, IndexType x) const {
        assert(0 <= y && y < H);
        assert(0 <= x && x < W);
        return inner_getX(dat[y+size], x, 0, W);
    }

    /// @brief (y, x) の値を v に変更する / O(log H log W)
    void set(IndexType y, IndexType x, T v) {
        assert(0 <= y && y < H);
        assert(0 <= x && x < W);
        y += size;
        if (!dat[y]) dat[y] = new_node_x();
        inner_setX(dat[y], x, v, 0, W);
        while (y > 1) {
            y >>= 1;
            T lv = inner_getX(dat[y<<1], x, 0, W);
            T rv = inner_getX(dat[y<<1|1], x, 0, W);
            if (!dat[y]) dat[y] = new_node_x();
            inner_setX(dat[y], x, op(lv, rv), 0, W);
        }
    }

    /// @brief [u, d) × [l, r) の総積を返す / O(log H log W)
    T prod(IndexType u, IndexType d, IndexType l, IndexType r) const {
        assert(0 <= u && u <= d && d <= H);
        assert(0 <= l && l <= r && r <= W);
        T ans = e();
        u += size;
        d += size;
        while (u < d) {
            if (u & 1) ans = op(ans, inner_prodX(dat[u++], l, r, 0, W));
            if (d & 1) ans = op(ans, inner_prodX(dat[--d], l, r, 0, W));
            u >>= 1;
            d >>= 1;
        }
        return ans;
    }
};
} // namespace titan23
