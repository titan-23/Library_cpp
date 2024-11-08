#include <iostream>
#include <vector>
#include <stack>
#include <cassert>
using namespace std;

namespace titan23 {

    template <class T,
                class F,
                T (*op)(T, T),
                T (*mapping)(F, T),
                F (*composition)(F, F),
                T (*e)(),
                F (*id)()>
    class LazyWBTree {
      private:
        class Node;
        using NodePtr = Node*;
        using MyLazyWBTree = LazyWBTree<T, F, op, mapping, composition, e, id>;
        static constexpr int DELTA = 3;
        static constexpr int GAMMA = 2;

        class Node {
          public:
            NodePtr left;
            NodePtr right;
            T key, data;
            F lazy;
            bool rev;
            int size;

            Node(T key, F lazy) : left(nullptr), right(nullptr), key(key), data(key), lazy(lazy), rev(false), size(1) {}

            int weight_right() const {
                return right ? right->size + 1 : 1;
            }

            int weight_left() const {
                return left ? left->size + 1 : 1;
            }

            void balance_check() const {
                if (!weight_left()*DELTA >= weight_right()) {
                    cerr << weight_left() << ", " << weight_right() << endl;
                    cerr << "not weight_left()*DELTA >= weight_right()." << endl;
                    assert(false);
                }
                if (!weight_right() * DELTA >= weight_left()) {
                    cerr << weight_left() << ", " << weight_right() << endl;
                    cerr << "not weight_right() * DELTA >= weight_left()." << endl;
                    assert(false);
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

            void propagate() {
                if (rev) {
                    swap(left, right);
                    if (left) left->rev ^= 1;
                    if (right) right->rev ^= 1;
                    rev = 0;
                }
                if (lazy != id()) {
                    if (left) {
                        left->key = mapping(lazy, left->key);
                        left->data = mapping(lazy, left->data);
                        left->lazy = composition(lazy, left->lazy);
                    }
                    if (right) {
                        right->key = mapping(lazy, right->key);
                        right->data = mapping(lazy, right->data);
                        right->lazy = composition(lazy, right->lazy);
                    }
                    lazy = id();
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
                NodePtr node = new Node(a[mid], id());
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

        NodePtr _rotate_right(NodePtr node) {
            NodePtr u = node->left;
            node->left = u->right;
            u->right = node;
            node->update();
            u->update();
            return u;
        }

        NodePtr _rotate_left(NodePtr node) {
            NodePtr u = node->right;
            node->right = u->left;
            u->left = node;
            node->update();
            u->update();
            return u;
        }

        NodePtr _balance_left(NodePtr node) {
            NodePtr u = node->right;
            u->propagate();
            if (node->right->weight_left() >= node->right->weight_right() * GAMMA) {
                u->left->propagate();
                node->right = _rotate_right(u);
            }
            return _rotate_left(node);
        }

        NodePtr _balance_right(NodePtr node) {
            NodePtr u = node->left;
            u->propagate();
            if (node->left->weight_right() >= node->left->weight_left() * GAMMA) {
                u->right->propagate();
                node->left = _rotate_left(u);
            }
            return _rotate_right(node);
        }

        int weight(NodePtr node) const {
            return node ? node->size + 1 : 1;
        }

        NodePtr _merge_with_root(NodePtr l, NodePtr root, NodePtr r) {
            if (weight(l) * DELTA < weight(r)) {
                r->propagate();
                r->left = _merge_with_root(l, root, r->left);
                r->update();
                if (weight(r->right) * DELTA < weight(r->left)) {
                    return _balance_right(r);
                }
                return r;
            } else if (weight(r) * DELTA < weight(l)) {
                l->propagate();
                l->right = _merge_with_root(l->right, root, r);
                l->update();
                if (weight(l->left) * DELTA < weight(l->right)) {
                    return _balance_left(l);
                }
                return l;
            }
            root->left = l;
            root->right = r;
            root->update();
            return root;
        }

        pair<NodePtr, NodePtr> _pop_right(NodePtr node) {
            return _split_node(node, node->size-1);
        }

        NodePtr _merge_node(NodePtr l, NodePtr r) {
            if ((!l) && (!r)) {return nullptr;}
            if (!l) {return r;}
            if (!r) {return l;}
            auto [l_, root_] = _pop_right(l);
            return _merge_with_root(l_, root_, r);
        }

        pair<NodePtr, NodePtr> _split_node(NodePtr node, int k) {
            if (!node) return {nullptr, nullptr};
            node->propagate();
            int tmp = node->left ? k-node->left->size : k;
            NodePtr nl = node->left;
            NodePtr nr = node->right;
            if (tmp == 0) {
                return {nl, _merge_with_root(nullptr, node, nr)};
            } else if (tmp < 0) {
                auto [l, r] = _split_node(nl, k);
                return {l, _merge_with_root(r, node, nr)};
            } else {
                auto [l, r] = _split_node(nr, tmp-1);
                return {_merge_with_root(node->left, node, l), r};
            }
        }

        LazyWBTree(NodePtr &root): root(root) {}

        MyLazyWBTree _new(NodePtr root) {
            return MyLazyWBTree(root);
        }

    public:
        NodePtr root;

        LazyWBTree() : root(nullptr) {}

        LazyWBTree(vector<T> const &a) { _build(a); }

        void merge(MyLazyWBTree &other) {
            this->root = _merge_node(this->root, other.root);
        }

        pair<MyLazyWBTree, MyLazyWBTree> split(const int k) {
            auto [l, r] = _split_node(this->root, k);
            return {_new(l), _new(r)};
        }

        void all_apply(const F f) {
            if (!this->root) return;
            this->root->key = mapping(f, this->root->key);
            this->root->data = mapping(f, this->root->data);
            this->root->lazy = composition(f, this->root->lazy);
        }

        void apply(const int l, const int r, const F f) {
            assert(0 <= l && l <= r && r <= len());
            if (l == r) return;
            auto dfs = [&] (auto &&dfs, NodePtr node, int left, int right) -> void {
                if (right <= l || r <= left) return;
                node->propagate();
                if (l <= left && right < r) {
                    node->key = mapping(f, node->key);
                    node->data = mapping(f, node->data);
                    node->lazy = composition(f, node->lazy);
                    return;
                }
                int lsize = node->left ? node->left->size : 0;
                if (node->left) dfs(dfs, node->left, left, left+lsize);
                if (l <= left+lsize && left+lsize < r) node->key = mapping(f, node->key);;
                if (node->right) dfs(dfs, node->right, left+lsize+1, right);
                node->update();
            };
            dfs(dfs, root, 0, len());
        }

        T all_prod() const {
            return this->root ? this->root->data : e();
        }

        T prod(const int l, const int r) {
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

        void insert(int k, const T key) {
            assert(0 <= k && k <= len());
            auto [s, t] = _split_node(root, k);
            NodePtr new_node = new Node(key, id());
            this->root = _merge_with_root(s, new_node, t);
        }

        T pop(int k) {
            assert(0 <= k && k < len());
            stack<NodePtr> path;
            stack<short> path_d;
            NodePtr node = this->root;
            T res;
            int d = 0;
            while (1) {
                node->propagate();
                int t = node->left? node->left->size: 0;
                if (t == k) {
                    res = node->key;
                    break;
                }
                int d = (t < k)? 0: 1;
                path.emplace(node);
                path_d.emplace(d);
                if (d) {
                    node = node->left;
                } else {
                    k -= t + 1;
                    node = node->right;
                }
            }
            if (node->left && node->right) {
                node->propagate();
                NodePtr lmax = node->left;
                path.emplace(node);
                path_d.emplace(1);
                lmax->propagate();
                while (lmax->right) {
                    path.emplace(lmax);
                    path_d.emplace(0);
                    lmax = lmax->right;
                    lmax->propagate();
                }
                node->key = lmax->key;
                node = lmax;
            }
            NodePtr cnode = (node->left) ? node->left : node->right;
            if (!path.empty()) {
                if (path_d.top()) path.top()->left = cnode;
                else path.top()->right = cnode;
            } else {
                this->root = cnode;
                return res;
            }
            while (!path.empty()) {
                NodePtr new_node = nullptr;
                node = path.top();
                node->update();
                path.pop();
                path_d.pop();
                if (weight(node->left) * DELTA < weight(node->right)) {
                    new_node = _balance_left(node);
                } else if (weight(node->right) * DELTA < weight(node->left)) {
                    new_node = _balance_right(node);
                }
                if (new_node) {
                    if (path.empty()) {
                        this->root = new_node;
                        break;
                    }
                    if (path_d.top()) {
                        path.top()->left = new_node;
                    } else {
                        path.top()->right = new_node;
                    }
                }
            }
            return res;
        }

        void reverse(const int l, const int r) {
            assert(0 <= l && l <= r && r <= len());
            if (l == r) return;
            auto [s_, t] = _split_node(root, r);
            auto [u, s] = _split_node(s_, l);
            s->rev ^= 1;
            this->root = _merge_node(_merge_node(u, s), t);
        }

        vector<T> tovector() {
            NodePtr node = root;
            stack<NodePtr> s;
            vector<T> a;
            a.reserve(len());
            while ((!s.empty()) || node) {
                if (node) {
                    node->propagate();
                    s.emplace(node);
                    node = node->left;
                } else {
                    node = s.top();
                    s.pop();
                    a.emplace_back(node->key);
                    node = node->right;
                }
            }
            return a;
        }

        void set(int k, T v) {
            assert(0 <= k && k < len());
            NodePtr node = root;
            NodePtr root = node;
            NodePtr pnode = nullptr;
            int d = 0;
            stack<NodePtr> path = {node};
            while (1) {
                node->propagate();
                int t = node->left? node->left->size: 0;
                if (t == k) {
                    node->key = v;
                    path.emplace(node);
                    if (pnode) {
                        if (d) pnode->left = node;
                        else pnode->right = node;
                    }
                    while (!path.empty()) {
                        path.top()->update();
                        path.pop();
                    }
                    return;
                }
                pnode = node;
                d = (t < k) ? 0 : 1;
                if (d) {
                    pnode->left = node = node->left;
                } else {
                    k -= t + 1;
                    pnode->right = node = node->right;
                }
                path.emplace(node);
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

        friend ostream& operator<<(ostream& os, LazyWBTree<T, F, op, mapping, composition, e, id> &tree) {
            vector<T> a = tree.tovector();
            os << "[";
            for (int i = 0; i < (int)a.size()-1; ++i) {
                os << a[i] << ", ";
            }
            if (!a.empty()) os << a.back();
            os << "]";
            return os;
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
    };
} // namespace titan23
