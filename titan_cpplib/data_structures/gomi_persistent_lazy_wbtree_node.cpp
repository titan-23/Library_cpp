#include <iostream>
#include <vector>
#include <cmath>
#include <cassert>
#include <stack>
#include <memory>
#include "titan_cpplib/others/print.cpp"
using namespace std;

namespace titan23 {

template <class T,
            class F,
            T (*op)(T, T),
            T (*mapping)(F, T),
            F (*composition)(F, F),
            T (*e)(),
            F (*id)()>
class PersistentLazyWBTree {
  private:
    class Node;
    using NodePtr = shared_ptr<Node>;
    // using NodePtr = Node*;
    using MyPersistentLazyWBTree = PersistentLazyWBTree<T, F, op, mapping, composition, e, id>;
    static constexpr int DELTA = 3;
    static constexpr int GAMMA = 2;
    NodePtr root;

    class Node {
      public:
        T key, data;
        NodePtr left;
        NodePtr right;
        F lazy;
        int rev, size;

        Node(T key, F lazy) : key(key), data(key), left(nullptr), right(nullptr), lazy(lazy), rev(0), size(1) {}

        NodePtr copy() const {
            NodePtr node = make_shared<Node>(key, lazy);
            // NodePtr node = new Node(key, lazy);
            node->data = data;
            node->left = left;
            node->right = right;
            node->rev = rev;
            node->size = size;
            return node;
        }

        int weight_right() const {
            return right ? right->size + 1 : 1;
        }

        int weight_left() const {
            return left ? left->size + 1 : 1;
        }

        void propagate() {
            if (rev) {
                NodePtr l = left  ? left->copy()  : nullptr;
                NodePtr r = right ? right->copy() : nullptr;
                left = r;
                right = l;
                if (left) left->rev ^= 1;
                if (right) right->rev ^= 1;
                rev = 0;
            }
            if (lazy != id()) {
                if (left) {
                    left = left->copy();
                    left->key = mapping(lazy, left->key);
                    left->data = mapping(lazy, left->data);
                    left->lazy = composition(lazy, left->lazy);
                }
                if (right) {
                    right = right->copy();
                    right->key = mapping(lazy, right->key);
                    right->data = mapping(lazy, right->data);
                    right->lazy = composition(lazy, right->lazy);
                }
                lazy = id();
            }
        }

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

        void balance_check() const {
            if (!(weight_left()*DELTA >= weight_right())) {
                cerr << weight_left() << ", " << weight_right() << endl;
                cerr << "not weight_left()*DELTA >= weight_right()." << endl;
                assert(false);
            }
            if (!(weight_right() * DELTA >= weight_left())) {
                cerr << weight_left() << ", " << weight_right() << endl;
                cerr << "not weight_right() * DELTA >= weight_left()." << endl;
                assert(false);
            }
        }

        void print() const {
            cout << "this : key=" << key << ", size=" << size << endl;
            if (left)  cout << "to-left" << endl;
            if (right) cout << "to-right" << endl;
            cout << endl;
            if (left)  left->print();
            if (right) right->print();
        }
    };

    void _build(vector<T> const &a) {
        auto build = [&] (auto &&build, int l, int r) -> NodePtr {
            int mid = (l + r) >> 1;
            NodePtr node = make_shared<Node>(a[mid], id());
            // NodePtr node = new Node(a[mid], id());
            if (l != mid) node->left = build(build, l, mid);
            if (mid+1 != r) node->right = build(build, mid+1, r);
            node->update();
            return node;
        };
        root = build(build, 0, (int)a.size());
    }

    NodePtr _rotate_right(NodePtr &node) {
        NodePtr u = node->left->copy();
        node->left = u->right;
        u->right = node;
        node->update();
        u->update();
        return u;
    }

    NodePtr _rotate_left(NodePtr &node) {
        NodePtr u = node->right->copy();
        node->right = u->left;
        u->left = node;
        node->update();
        u->update();
        return u;
    }

    NodePtr _balance_left(NodePtr &node) {
        node->right->propagate();
        node->right = node->right->copy();
        NodePtr u = node->right;
        if (node->right->weight_left() >= node->right->weight_right() * GAMMA) {
            u->left->propagate();
            node->right = _rotate_right(u);
        }
        u = _rotate_left(node);
        return u;
    }

    NodePtr _balance_right(NodePtr &node) {
        node->left->propagate();
        node->left = node->left->copy();
        NodePtr u = node->left;
        if (node->left->weight_right() >= node->left->weight_left() * GAMMA) {
            u->right->propagate();
            node->left = _rotate_left(u);
        }
        u = _rotate_right(node);
        return u;
    }

    int weight(NodePtr node) const {
        return node ? node->size + 1 : 1;
    }

    NodePtr _merge_with_root(NodePtr l, NodePtr root, NodePtr r) {
        if (weight(r) * DELTA < weight(l)) {
            l->propagate();
            l = l->copy();
            l->right = _merge_with_root(l->right, root, r);
            l->update();
            if (weight(l->left) * DELTA < weight(l->right)) {
                return _balance_left(l);
            }
            return l;
        } else if (weight(l) * DELTA < weight(r)) {
            r->propagate();
            r = r->copy();
            r->left = _merge_with_root(l, root, r->left);
            r->update();
            if (weight(r->right) * DELTA < weight(r->left)) {
                return _balance_right(r);
            }
            return r;
        }
        root = root->copy();
        root->left = l;
        root->right = r;
        root->update();
        return root;
    }

    pair<NodePtr, NodePtr> _pop_right(NodePtr &node) {
        return _split_node(node, node->size-1);
    }

    NodePtr _merge_node(NodePtr l, NodePtr r) {
        if ((!l) && (!r)) { return nullptr; }
        if (!l) { return r->copy(); }
        if (!r) { return l->copy(); }
        l = l->copy();
        r = r->copy();
        auto [l_, root_] = _pop_right(l);
        return _merge_with_root(l_, root_, r);
    }

    pair<NodePtr, NodePtr> _split_node(NodePtr &node, int k) {
        if (!node) {return {nullptr, nullptr};}
        node->propagate();
        int tmp = node->left ? k-node->left->size : k;
        if (tmp == 0) {
            return {node->left, _merge_with_root(nullptr, node, node->right)};
        } else if (tmp < 0) {
            auto [l, r] = _split_node(node->left, k);
            return {l, _merge_with_root(r, node, node->right)};
        } else {
            auto [l, r] = _split_node(node->right, tmp-1);
            return {_merge_with_root(node->left, node, l), r};
        }
    }

    MyPersistentLazyWBTree _new(NodePtr root) {
        return MyPersistentLazyWBTree(root);
    }

    PersistentLazyWBTree(NodePtr root) : root(root) {}

  public:
    PersistentLazyWBTree() : root(nullptr) {}

    PersistentLazyWBTree(vector<T> &a) { _build(a); }

    MyPersistentLazyWBTree merge(MyPersistentLazyWBTree other) {
        NodePtr root = _merge_node(this->root, other.root);
        return _new(root);
    }

    pair<MyPersistentLazyWBTree, MyPersistentLazyWBTree> split(int k) {
        auto [l, r] = _split_node(this->root, k);
        return {_new(l), _new(r)};
    }

    MyPersistentLazyWBTree apply(int l, int r, F f) {
        if (l >= r) return _new(this->root ? root->copy() : nullptr);
        auto dfs = [&] (auto &&dfs, NodePtr node, int left, int right) -> NodePtr {
            if (right <= l || r <= left) return node;
            node->propagate();
            NodePtr nnode = node->copy();
            if (l <= left && right < r) {
                nnode->key = mapping(f, nnode->key);
                nnode->data = mapping(f, nnode->data);
                nnode->lazy = composition(f, nnode->lazy);
                return nnode;
            }
            int lsize = nnode->left ? nnode->left->size : 0;
            if (nnode->left) nnode->left = dfs(dfs, nnode->left, left, left+lsize);
            if (l <= left+lsize && left+lsize < r) nnode->key = mapping(f, nnode->key);;
            if (nnode->right) nnode->right = dfs(dfs, nnode->right, left+lsize+1, right);
            nnode->update();
            return nnode;
        };
        return _new(dfs(dfs, root, 0, len()));
    }

    T prod(int l, int r) {
        assert(0 <= l && l <= r && r <= len());
        if (l == r) return e();
        auto dfs = [&] (auto &&dfs, NodePtr node, int left, int right) -> T {
            if (right <= l || r <= left) return e();
            node->propagate();
            if (l <= left && right < r) return node->data;
            int lsize = node->left ? node->left->size : 0;
            T res = e();
            if (node->left) res = dfs(dfs, node->left, left, left+lsize);
            if (l <= left+lsize && left+lsize < r) res = op(res, node->key);
            if (node->right) res = op(res, dfs(dfs, node->right, left+lsize+1, right));
            return res;
        };
        return dfs(dfs, root, 0, len());
    }

    MyPersistentLazyWBTree insert(int k, T key) {
        assert(0 <= k && k <= len());
        auto [s, t] = _split_node(root, k);
        NodePtr new_node = make_shared<Node>(key, id());
        return _new(_merge_with_root(s, new_node, t));
    }

    pair<MyPersistentLazyWBTree, T> pop(int k) {
        assert(0 <= k && k < len());
        auto [s_, t] = _split_node(this->root, k+1);
        auto [s, tmp] = _pop_right(s_);
        NodePtr root = _merge_node(s, t);
        return {_new(root), tmp->key};
    }

    MyPersistentLazyWBTree reverse(int l, int r) {
        assert(0 <= l && l <= r && r <= len());
        if (l >= r) return _new(root ? root->copy() : nullptr);
        auto [s_, t] = _split_node(root, r);
        auto [u, s] = _split_node(s_, l);
        s->rev ^= 1;
        NodePtr root = _merge_node(_merge_node(u, s), t);
        return _new(root);
    }

    vector<T> tovector() {
        NodePtr node = root;
        stack<NodePtr> s;
        vector<T> a;
        a.reserve(len());
        while (!s.empty() || node) {
            if (node) {
                node->propagate();
                s.emplace(node);
                node = node->left;
            } else {
                node = s.top(); s.pop();
                a.emplace_back(node->key);
                node = node->right;
            }
        }
        return a;
    }

    MyPersistentLazyWBTree copy() const {
        return _new(root ? root->copy() : nullptr);
    }

    MyPersistentLazyWBTree set(int k, T v) {
        assert(0 <= k && k < len());
        NodePtr node = root->copy();
        NodePtr root = node;
        NodePtr pnode = nullptr;
        int d = 0;
        stack<NodePtr> path = {node};
        while (1) {
            node->propagate();
            int t = node->left ? node->left->size : 0;
            if (t == k) {
                node = node->copy();
                node->key = v;
                path.emplace(node);
                if (d) pnode->left = node;
                else pnode->right = node;
                while (!path.empty()) {
                    path.top()->update();
                    path.pop();
                }
                return _new(root);
            }
            pnode = node;
            if (t < k) {
                k -= t + 1;
                node = node->right->copy();
                d = 0;
            } else {
                d = 1;
                node = node->left->copy();
            }
            path.emplace_back(node);
            if (d) pnode->left = node;
            else pnode->right = node;
        }
    }

    T get(int k) {
        assert(0 <= k && k < len());
        NodePtr node = root;
        while (1) {
            node->propagate();
            int t = node->left ? node->left->size : 0;
            if (t == k) {
                return node->key;
            }
            if (t < k) {
                k -= t + 1;
                node = node->right;
            } else {
                node = node->left;
            }
        }
    }

    void print() {
        vector<T> a = tovector();
        cout << "[";
        for (int i = 0; i < (int)a.size()-1; ++i) {
            cout << a[i] << ", ";
        }
        if (!a.empty()) cout << a.back();
        cout << "]" << endl;
    }

    int len() const {
        return root ? root->size : 0;
    }

    void check() const {
        auto rec = [&] (auto &&rec, NodePtr node) -> pair<int, int> {
            int ls = 0, rs = 0;
            int height = 0;
            int h;
            if (node->left) {
                pair<int, int> res = rec(rec, node->left);
                ls = res.first;
                h = res.second;
                height = max(height, h);
            }
            if (node->right) {
                pair<int, int> res = rec(rec, node->right);
                rs = res.first;
                h = res.second;
                height = max(height, h);
            }
            node->balance_check();
            int s = ls + rs + 1;
            assert(s == node->size);
            return {s, height+1};
        };
        if (root == nullptr) return;
        auto [_, h] = rec(rec, root);
        cerr << PRINT_GREEN << "OK : height=" << h << PRINT_NONE << endl;
    }

    friend ostream& operator<<(ostream& os, PersistentLazyWBTree<T, F, op, mapping, composition, e, id> &tree) {
        vector<T> a = tree.tovector();
        os << "[";
        for (int i = 0; i < (int)a.size()-1; ++i) {
            os << a[i] << ", ";
        }
        if (!a.empty()) os << a.back();
        os << "]";
        return os;
    }
};
}  // namespace titan23
