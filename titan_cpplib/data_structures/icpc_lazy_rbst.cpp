// n=5e5, q=5e5 -> 1200ms

#include <bits/stdc++.h>
using namespace std;

struct Random {
    std::mt19937 mt;
    Random() : mt(12345) {}
    int randrange(int n) {
        assert(n-1 >= 0);
        std::uniform_int_distribution<int> dist(0, n - 1);
        return dist(mt);
    }
} trnd;

template <class T, T (*op)(T, T), T (*e)(),
          class F, T (*mapping)(F, T), F (*composition)(F, F), F (*id)()>
class LazyRBST {
    struct Node;
    using NodePtr = Node*;
    using RBST = LazyRBST<T, op, e, F, mapping, composition, id>;
    NodePtr root;

    struct Node {
        NodePtr left, right;
        T key, data;
        F lazy;
        bool rev;
        int size;

        Node(T key) : left(nullptr), right(nullptr), key(key), data(key), lazy(id()), rev(false), size(1) {}

        void update() {
            size = 1;
            data = key;
            if (left) {
                size += left->size;
                data = op(left->data, key);
            }
            if (right) {
                size += right->size;
                data = op(data, right->data);
            }
        }

        void push(F f) {
            key = mapping(f, key);
            data = mapping(f, data);
            lazy = composition(f, lazy);
        }

        void propagate() {
            if (rev) {
                swap(left, right);
                if (left) left->rev ^= 1;
                if (right) right->rev ^= 1;
                rev = 0;
            }
            if (lazy != id()) {
                if (left) left->push(lazy);
                if (right) right->push(lazy);
                lazy = id();
            }
        }
    };

    void _build(vector<T> const &a) {
        auto build = [&] (auto &&build, int l, int r) -> NodePtr {
            int mid = (l + r) >> 1;
            NodePtr node = new Node(a[mid]);
            if (l != mid) node->left = build(build, l, mid);
            if (mid+1 != r) node->right = build(build, mid+1, r);
            node->update();
            return node;
        };
        if (a.empty()) {
            this->root = nullptr;
            return;
        }
        this->root = build(build, 0, (int)a.size());
    }

    NodePtr _merge_node(NodePtr l, NodePtr r) {
        if (!l) return r;
        if (!r) return l;
        int s = l->size + r->size;
        if (trnd.randrange(s) < l->size) {
            l->propagate();
            l->right = _merge_node(l->right, r);
            l->update();
            return l;
        } else {
            r->propagate();
            r->left = _merge_node(l, r->left);
            r->update();
            return r;
        }
    }

    pair<NodePtr, NodePtr> _split_node(NodePtr node, int k) {
        if (!node) return {nullptr, nullptr};
        node->propagate();
        int lsize = node->left ? node->left->size : 0;
        if (k <= lsize) {
            auto [l, r] = _split_node(node->left, k);
            node->left = r;
            node->update();
            return {l, node};
        } else {
            auto [l, r] = _split_node(node->right, k - lsize - 1);
            node->right = l;
            node->update();
            return {node, r};
        }
    }

    LazyRBST(NodePtr node) : root(node) {}

public:
    LazyRBST() : root(nullptr) {}
    LazyRBST(const vector<T> &a) : root(nullptr) { _build(a); }

    int len() const { return root ? root->size : 0; }

    void merge(RBST &other) { root = _merge_node(root, other.root); }

    pair<RBST, RBST> split(int k) {
        auto [l, r] = _split_node(root, k);
        return {RBST(l), RBST(r)};
    }

    // void apply(int l, int r, F f) {
    //     assert(0 <= l && l <= r && r <= len());
    //     if (l == r) return;
    //     auto [s2, t] = _split_node(root, r);
    //     auto [u, s] = _split_node(s2, l);
    //     s->push(f);
    //     root = _merge_node(_merge_node(u, s), t);
    // }

    // T prod(int l, int r) {
    //     assert(0 <= l && l <= r && r <= len());
    //     if (l == r) return e();
    //     auto [s2, t] = _split_node(root, r);
    //     auto [u, s] = _split_node(s2, l);
    //     T res = s->data;
    //     root = _merge_node(_merge_node(u, s), t);
    //     return res;
    // }

    // void insert(int k, T key) {
    //     assert(0 <= k && k <= len());
    //     auto [s, t] = _split_node(root, k);
    //     NodePtr node = new Node(key, id());
    //     root = _merge_node(_merge_node(s, node), t);
    // }

    // T pop(int k) {
    //     assert(0 <= k && k < len());
    //     auto [s, t] = _split_node(root, k + 1);
    //     auto [l, tmp] = _split_node(s, s->size - 1);
    //     T res = tmp->key;
    //     root = _merge_node(l, t);
    //     return res;
    // }

    // void reverse(int l, int r) {
    //     assert(0 <= l && l <= r && r <= len());
    //     if (l == r) return;
    //     auto [s2, t] = _split_node(root, r);
    //     auto [u, s] = _split_node(s2, l);
    //     s->rev ^= 1;
    //     root = _merge_node(_merge_node(u, s), t);
    // }

    // vector<T> tovector() {
    //     NodePtr node = root;
    //     stack<NodePtr> s;
    //     vector<T> a; a.reserve(len());
    //     while (!s.empty() || node) {
    //         if (node) {
    //             node->propagate();
    //             s.emplace(node);
    //             node = node->left;
    //         } else {
    //             node = s.top(); s.pop();
    //             a.emplace_back(node->key);
    //             node = node->right;
    //         }
    //     }
    //     return a;
    // }

    // T get(int k) {
    //     assert(0 <= k && k < len());
    //     NodePtr node = root;
    //     while (1) {
    //         node->propagate();
    //         int t = node->left ? node->left->size : 0;
    //         if (t == k) {
    //             return node->key;
    //         } else if (t < k) {
    //             k -= t + 1;
    //             node = node->right;
    //         } else {
    //             node = node->left;
    //         }
    //     }
    // }

    // void set(int k, T key) {
    //     assert(0 <= k && k < len());
    //     NodePtr node = root;
    //     stack<NodePtr> path;
    //     while (1) {
    //         node->propagate(node);
    //         path.emplace(node);
    //         int t = node->left ? node->left->size : 0;
    //         if (t == k) {
    //             node->key = key;
    //             break;
    //         } else if (t < k) {
    //             k -= t + 1;
    //             node = node->right;
    //         } else {
    //             node = node->left;
    //         }
    //     }
    //     while (!path.empty()) {
    //         NodePtr node = path.top(); path.pop();
    //         node->update();
    //     }
    // }

    // int height() {
    //     auto dfs = [&] (auto &&dfs, NodePtr node) -> int {
    //         if (!node) return 0;
    //         return max(dfs(dfs, node->left), dfs(dfs, node->right)) + 1;
    //     };
    //     return dfs(dfs, root);
    // }
};
