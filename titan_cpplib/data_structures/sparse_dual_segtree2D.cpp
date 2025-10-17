template <class IndexType, class T, T (*op)(T, T), T (*e)()>
class SparseSegmentTree2D {
private:
public:
    IndexType h, w;
    int root;

    struct NodeX;
    struct NodeY;
    vector<NodeX> X;
    vector<NodeY> Y;

    struct NodeX {
        T data;
        int lch = 0, rch = 0;
        IndexType l, r;
        bool is_leaf() const { return r - l == 1; }
        IndexType mid() const { return (l + r) / 2; }
        friend ostream& operator<<(ostream& os, const NodeX &node) {
            os << "(" << node.l << ", " << node.r << ")";
            return os;
        }
    };

    void updateX(int node) {
        X[node].data = op(X[X[node].lch].data, X[X[node].rch].data);
    }

    struct NodeY {
        int root = 0, uch = 0, dch = 0;
        IndexType u, d;
        bool is_leaf() const { return d - u == 1; }
        IndexType mid() const { return (u + d) / 2; }
        friend ostream& operator<<(ostream& os, const NodeY &node) {
            os << "(" << node.u << ", " << node.d << ")";
            return os;
        }
    };

    int new_node_x(IndexType l, IndexType r) {
        X.emplace_back(NodeX{e(), 0, 0, l, r});
        return X.size() - 1;
    }

    int new_node_y(IndexType u, IndexType d) {
        Y.emplace_back(NodeY{0, 0, 0, u, d});
        return Y.size() - 1;
    }

    T inner_getX(int node, IndexType x) {
        if (!node) return e();
        if (X[node].is_leaf()) return X[node].data;
        if (x < X[node].mid()) return inner_getX(X[node].lch, x);
        else return inner_getX(X[node].rch, x);
    }

    void inner_setX(int node, IndexType x, T v) {
        if (X[node].is_leaf()) {
            X[node].data = v;
            return;
        }
        if (x < X[node].mid()) {
            if (!X[node].lch) X[node].lch = new_node_x(X[node].l, X[node].mid());
            inner_setX(X[node].lch, x, v);
        } else {
            if (!X[node].rch) X[node].rch = new_node_x(X[node].mid(), X[node].r);
            inner_setX(X[node].rch, x, v);
        }
        updateX(node);
    }

    void inner_setY(int node, IndexType y, IndexType x, T v) {
        if (!Y[node].root) Y[node].root = new_node_x(0, w);
        inner_setX(Y[node].root, x, v);
        if (Y[node].is_leaf()) return;
        if (y < Y[node].mid()) {
            if (!Y[node].uch) Y[node].uch = new_node_y(Y[node].u, Y[node].mid());
            inner_setY(Y[node].uch, y, x, v);
        } else {
            if (!Y[node].dch) Y[node].dch = new_node_y(Y[node].mid(), Y[node].d);
            inner_setY(Y[node].dch, y, x, v);
        }
        inner_setX(Y[node].root, x, op(
            inner_getX(Y[Y[node].uch].root, x),
            inner_getX(Y[Y[node].dch].root, x)
        ));
    }

    T inner_prodX(int node, IndexType l, IndexType r) const {
        if (!node || l >= r || r <= X[node].l || X[node].r <= l) return e();
        if (l <= X[node].l && X[node].r <= r) return X[node].data;
        return op(
            inner_prodX(X[node].lch, l, r),
            inner_prodX(X[node].rch, l, r)
        );
    }

    T inner_prodY(int node, IndexType u, IndexType d, IndexType l, IndexType r) const {
        if (!node || u >= d || d <= Y[node].u || Y[node].d <= u) return e();
        if (u <= Y[node].u && Y[node].d <= d) return inner_prodX(Y[node].root, l, r);
        return op(
            inner_prodY(Y[node].uch, u, d, l, r),
            inner_prodY(Y[node].dch, u, d, l, r)
        );
    }

public:
    SparseSegmentTree2D() : root(0) {}
    SparseSegmentTree2D(IndexType h, IndexType w) : h(h), w(w) {
        X.reserve(1e7);
        Y.reserve(1e7);
        new_node_y(0, h); // for dammy
        new_node_x(0, w); // for dammy
        root = new_node_y(0, h);
        Y[root].root = new_node_x(0, w);
    }

    void set(IndexType y, IndexType x, T v) {
        inner_setY(root, y, x, v);
    }

    T prod(IndexType u, IndexType d, IndexType l, IndexType r) {
        assert(0 <= u && u <= d && d <= h);
        assert(0 <= l && l <= r && r <= w);
        return inner_prodY(root, u, d, l, r);
    }
};
