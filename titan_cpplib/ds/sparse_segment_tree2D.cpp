/// https://github.com/titan-23/Library_cpp/blob/main/titan_cpplib/ds/sparse_segment_tree2D.cpp
#pragma once

#include <cassert>
#include <vector>
using namespace std;

namespace titan23 {

/// @brief 必要な節点だけを作る 2 次元セグメント木 / op は可換であること
template <class IndexType, class T, T (*op)(T, T), T (*e)()>
class SparseSegmentTree2D {
private:
    IndexType H, W;
    int root;

    struct NodeX {
        T data;
        int lch, rch;
    };

    struct NodeY {
        int root, uch, dch;
    };

    vector<NodeX> X;
    vector<NodeY> Y;

    void updateX(int node) {
        X[node].data = op(X[X[node].lch].data, X[X[node].rch].data);
    }

    int new_node_x() {
        X.emplace_back(NodeX{e(), 0, 0});
        return X.size() - 1;
    }

    int new_node_y() {
        Y.emplace_back(NodeY{0, 0, 0});
        return Y.size() - 1;
    }

    T inner_getX(int node, IndexType x, IndexType L, IndexType R) const {
        if (!node) return e();
        if (R-L == 1) return X[node].data;
        IndexType M = L+(R-L)/2;
        if (x < M) return inner_getX(X[node].lch, x, L, M);
        else return inner_getX(X[node].rch, x, M, R);
    }

    T inner_getY(int node, IndexType y, IndexType x, IndexType U, IndexType D) const {
        if (!node) return e();
        if (D-U == 1) return inner_getX(Y[node].root, x, 0, W);
        IndexType M = U+(D-U)/2;
        if (y < M) return inner_getY(Y[node].uch, y, x, U, M);
        else return inner_getY(Y[node].dch, y, x, M, D);
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

    void inner_setY(int node, IndexType y, IndexType x, T v, IndexType U, IndexType D) {
        if (!Y[node].root) Y[node].root = new_node_x();
        if (D-U == 1) {
            inner_setX(Y[node].root, x, v, 0, W);
            return;
        }
        IndexType M = U+(D-U)/2;
        if (y < M) {
            if (!Y[node].uch) Y[node].uch = new_node_y();
            inner_setY(Y[node].uch, y, x, v, U, M);
        } else {
            if (!Y[node].dch) Y[node].dch = new_node_y();
            inner_setY(Y[node].dch, y, x, v, M, D);
        }
        inner_setX(Y[node].root, x, op(
            inner_getX(Y[Y[node].uch].root, x, 0, W),
            inner_getX(Y[Y[node].dch].root, x, 0, W)
        ), 0, W);
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

    T inner_prodY(int node, IndexType u, IndexType d, IndexType l, IndexType r, IndexType U, IndexType D) const {
        if (!node || u >= d || d <= U || D <= u) return e();
        if (u <= U && D <= d) return inner_prodX(Y[node].root, l, r, 0, W);
        IndexType M = U+(D-U)/2;
        return op(
            inner_prodY(Y[node].uch, u, d, l, r, U, M),
            inner_prodY(Y[node].dch, u, d, l, r, M, D)
        );
    }

public:
    SparseSegmentTree2D() : root(0) {}
    /// @brief H × W の領域で構築する / 初期値 e() / O(1)
    SparseSegmentTree2D(IndexType H, IndexType W) : H(H), W(W) {
        new_node_y(); // 番兵
        new_node_x(); // 番兵
        root = new_node_y();
        Y[root].root = new_node_x();
    }

    /// @brief 節点 cap 個分の領域を予約する
    void reserve(int cap) {
        X.reserve(cap);
        Y.reserve(cap);
    }

    /// @brief (y, x) の値を返す / O(log H + log W)
    T get(IndexType y, IndexType x) const {
        assert(0 <= y && y < H);
        assert(0 <= x && x < W);
        return inner_getY(root, y, x, 0, H);
    }

    /// @brief (y, x) の値を v に変更する / O(log H log W)
    void set(IndexType y, IndexType x, T v) {
        inner_setY(root, y, x, v, 0, H);
    }

    /// @brief [u, d) × [l, r) の総積を返す / O(log H log W)
    T prod(IndexType u, IndexType d, IndexType l, IndexType r) const {
        assert(0 <= u && u <= d && d <= H);
        assert(0 <= l && l <= r && r <= W);
        return inner_prodY(root, u, d, l, r, 0, H);
    }
};
} // namespace titan23
