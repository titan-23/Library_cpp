#include <iostream>
#include <cassert>
using namespace std;

// DynamicSegmentTreeInit
namespace titan23 {

template <class IndexType,
        class T,
        T (*op)(T, T),
        T (*e)(),
        T (*pow)(T, IndexType)>
class DynamicSegmentTreeInit {
private:
    static int bit_length(IndexType x) {
        return x == 0 ? 0 : 64 - __builtin_clzll(x);
    }

    struct Node;
    using NodePtr = Node*;
    NodePtr root;
    IndexType u;
    T init_val;

    struct Node {
        DynamicSegmentTreeInit* dseg;
        NodePtr left, right;
        IndexType l, r;
        T data;

        Node() {}
        Node(DynamicSegmentTreeInit* dseg, IndexType l, IndexType r) :
                dseg(dseg),
                left(nullptr), right(nullptr),
                l(l), r(r),
                data(pow(dseg->init_val, r - l)) {
            assert(l < r);
        }

        bool is_leaf() const {
            return this->r - this->l == 1;
        }

        void update() {
            this->data = e();
            if (this->left) {
                this->data = op(this->data, this->left->data);
            } else {
                this->data = op(this->data, pow(dseg->init_val, mid()));
            }
            if (this->right) {
                this->data = op(this->data, this->right->data);
            } else {
                this->data = op(this->data, pow(dseg->init_val, mid()));
            }
        }

        IndexType mid() const {
            return (l + r) / 2;
        }
    };

private:
    T inner_prod(NodePtr node, IndexType l, IndexType r) const {
        if (l >= r) {
            return e();
        }
        if (!node) {
            return pow(this->init_val, r - l);
        }
        if (r <= node->l || node->r <= l) return e();
        if (l <= node->l && node->r <= r) return node->data;
        return op(
            inner_prod(node->left, l, min(r, node->mid())),
            inner_prod(node->right, max(l, node->mid()), r)
        );
    }

    void inner_set(NodePtr node, IndexType k, T val) {
        if (node->is_leaf()) {
            node->data = val;
            return;
        }
        if (k < node->mid()) {
            if (!node->left) node->left = new Node(this, node->l, node->mid());
            inner_set(node->left, k, val);
        } else {
            if (!node->right) node->right = new Node(this, node->mid(), node->r);
            inner_set(node->right, k, val);
        }
        node->update();
    }

    public:

    DynamicSegmentTreeInit() : root(nullptr), u(0) {}

    DynamicSegmentTreeInit(const IndexType u_, T init) {
        assert(u_ > 0);
        this->u = 1ll << bit_length(u_);
        this->root = new Node(this, 0, this->u);
        this->init_val = init;
    }

    T prod(IndexType l, IndexType r) const {
        return inner_prod(root, l, r);
    }

    T get(IndexType k) const {
        NodePtr node = root;
        while (true) {
            if (node->is_leaf()) {
                return node->data;
            }
            if (k < node->mid()) {
                if (!node->left) return this->init_val;
                node = node->left;
            } else {
                if (!node->right) return this->init_val;
                node = node->right;
            }
        }
    }

    void set(IndexType k, T val) {
        inner_set(root, k, val);
    }

    void print() {
        for (int i = 0; i < u; ++i) {
            cout << get(i) << ", ";
        }
        cout << endl;
    }
};
}  // namespace titan23
