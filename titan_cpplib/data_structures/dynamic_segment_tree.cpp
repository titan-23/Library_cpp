#include <iostream>
#include <cassert>
using namespace std;

// DynamicSegmentTree
namespace titan23 {

    template <class IndexType,
              class T,
              T (*op)(T, T),
              T (*e)()>
    class DynamicSegmentTree {
      private:
        static int bit_length(IndexType x) {
            return x == 0 ? 0 : 64 - __builtin_clzll(x);
        }

        struct Node;
        using NodePtr = Node*;
        NodePtr root;
        IndexType u;

        struct Node {
            NodePtr left, right;
            IndexType l, r;
            T data;

            Node() {}
            Node(IndexType l, IndexType r, T data) :
                    left(nullptr), right(nullptr),
                    l(l), r(r),
                    data(data) {
                assert(l < r);
            }

            bool is_leaf() const {
                return this->r - this->l == 1;
            }

            void update() {
                if (this->left) {
                    this->data = this->left->data;
                } else {
                    this->data = e();
                }
                if (this->right) {
                    this->data = op(this->data, this->right->data);
                }
            }

            IndexType mid() const {
                return (l + r) / 2;
            }
        };

      private:
        T inner_prod(NodePtr node, IndexType l, IndexType r) const {
            if (!node || l >= r || r <= node->l || node->r <= l) return e();
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
                if (!node->left) node->left = new Node(node->l, node->mid(), e());
                inner_set(node->left, k, val);
            } else {
                if (!node->right) node->right = new Node(node->mid(), node->r, e());
                inner_set(node->right, k, val);
            }
            node->update();
        }

      public:
        DynamicSegmentTree() : root(nullptr), u(0) {}

        DynamicSegmentTree(const IndexType u_) {
            assert(u_ > 0);
            this->u = 1ll << bit_length(u_);
            this->root = new Node(0, this->u, e());
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
                    if (!node->left) return e();
                    node = node->left;
                } else {
                    if (!node->right) return e();
                    node = node->right;
                }
            }
        }

        void set(IndexType k, T val) {
            inner_set(root, k, val);
        }

        void print() const {
            for (int i = 0; i < u; ++i) {
                cout << get(i) << ", ";
            }
            cout << endl;
        }
    };
}  // namespace titan23
