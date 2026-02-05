#include <vector>
using namespace std;

namespace titan23 {

// y軸が狭いセグ木
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
    SparseSegmentTree2DFAST(IndexType H, IndexType W) : H(H), W(W) {
        assert(H < 1e8);
        log = H == 0 ? 0 : 32-__builtin_clz(H);
        size = 1<<log;
        dat.resize(size*2, 0);
        new_node_x(); // for dammy
    }

    void reserve(int cap) {
        X.reserve(cap);
    }

    T get(IndexType y, IndexType x) const {
        assert(0 <= y && y < H);
        assert(0 <= x && x < W);
        return inner_getX(dat[y+size], x, 0, W);
    }

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
