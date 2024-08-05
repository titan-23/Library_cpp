#include <iostream>
using namespace std;

namespace titan23 {
    template <class T,
            T (*op)(T, T),
            T (*e)()>
    class DynamicSegmentTree {
    private:
        int bit_length(long long x) {
            int res = 0;
            while (x) {
                x /= 2;
                res++;
            }
            return res;
        }

        struct Node;
        using NodePtr = Node*;
        struct Node {
            NodePtr left, right;
            long long l, r;
            T data;

            Node() {}
            Node(long long l, long long r, T data) :
                left(nullptr), right(nullptr), l(l), r(r), data(data) {}

            bool is_leaf() {
                return r - l == 1;
            }

            void update() {
                data = e();
                if (left) {
                    data = left->data;
                }
                if (right) {
                    data = op(data, right->data);
                }
            }

            long long mid() const {
                return (l + r) / 2;
            }
        };

    private:
        NodePtr root;
        long long u;

        T inner_prod(NodePtr node, long long l, long long r) {
            if (!node || l >= r) {
                return e();
            }
            if (r <= node->l || node->r <= l) {
                return e();
            }
            if (l <= node->l && node->r <= r) {
                return node->data;
            }
            T res = op(
                inner_prod(node->left, l, min(r, node->mid())),
                inner_prod(node->right, max(l, node->mid()), r)
            );
            return res;
        }

        void inner_set(NodePtr node, long long k, T val) {
            if (node->is_leaf()) {
                node->data = val;
                return;
            }
            if (k < node->mid()) {
                if (!node->left) {
                    node->left = new Node(node->l, node->mid(), e());
                }
                inner_set(node->left, k, val);
            } else {
                if (!node->right) {
                    node->right = new Node(node->mid(), node->r, e());
                }
                inner_set(node->right, k, val);
            }
            node->update();
        }

    public:
        DynamicSegmentTree() : root(nullptr) {}
        DynamicSegmentTree(const long long u) : root(nullptr), u(1ll<<bit_length(u)) {
            root = new Node(0, u, e());
        }

        T prod(long long l, long long r) {
            return inner_prod(root, l, r);
        }

        T get(long long k) {
            NodePtr node = root;
            while (node) {
                if (node->is_leaf()) {
                    return node->data;
                }
                if (k < node->mid()) {
                    if (!node->left) break;
                    node = node->left;
                } else {
                    if (!node->right) break;
                    node = node->right;
                }
            }
            return e();
        }

        void set(long long k, T val) {
            inner_set(root, k, val);
        }

        void print() {
            rep(i, u) cout << get(i) << ", ";
            cout << endl;
        }
    };
}  // namespace titan23
