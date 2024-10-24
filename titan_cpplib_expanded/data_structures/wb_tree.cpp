// #include "titan_cpplib/data_structures/wb_tree.cpp"
#include <iostream>
#include <vector>
#include <cmath>
#include <cassert>
#include <memory>
using namespace std;

namespace titan23 {
    const double ALPHA = 1 - sqrt(2) / 2;
    const double BETA = (1 - 2 * ALPHA) / (1 - ALPHA);

    template <class T>
    class WBTree {

      private:
        class Node;
        using NodePtr = Node*;
        using MyWBTree = WBTree<T>;

        class Node {
          public:
            T key;
            NodePtr left, right;
            int size;

            Node(T key) : key(key), left(nullptr), right(nullptr), size(1) {}

            double balance() const {
                return ((left ? left->size : 0) + 1.0) / (size + 1.0);
            }

            void _update() {
                size = 1;
                if (left) {
                    size += left->size;
                }
                if (right) {
                    size += right->size;
                }
            }
        };

        void _build(vector<T> const &a) {
            if (a.empty()) {
                root = nullptr;
                return;
            }
            auto build = [&] (auto &&build, int l, int r) -> NodePtr {
                int mid = (l + r) >> 1;
                NodePtr node = new Node(a[mid]);
                if (l != mid) node->left = build(build, l, mid);
                if (mid+1 != r) node->right = build(build, mid+1, r);
                node->_update();
                return node;
            };
            root = build(build, 0, (int)a.size());
        }

        NodePtr _rotate_right(NodePtr node) {
            NodePtr u = node->left;
            node->left = u->right;
            u->right = node;
            node->_update();
            u->_update();
            return u;
        }

        NodePtr _rotate_left(NodePtr node) {
            NodePtr u = node->right;
            node->right = u->left;
            u->left = node;
            node->_update();
            u->_update();
            return u;
        }

        NodePtr _balance_left(NodePtr node) {
            NodePtr u = node->right;
            if (u->balance() >= BETA) {
                node->right = _rotate_right(u);
            }
            return _rotate_left(node);
        }

        NodePtr _balance_right(NodePtr node) {
            NodePtr u = node->left;
            if (u->balance() <= 1.0 - BETA) {
                node->left = _rotate_left(u);
            }
            return _rotate_right(node);
        }

        NodePtr _merge_with_root(NodePtr l, NodePtr root, NodePtr r) {
            int ls = l ? l->size : 0;
            int rs = r ? r->size : 0;
            double diff = (double)(ls+1.0) / (ls+rs+2.0);
            if (diff > 1.0-ALPHA) {
                l->right = _merge_with_root(l->right, root, r);
                l->_update();
                if (!(ALPHA <= l->balance() && l->balance() <= 1.0-ALPHA)) {
                    return _balance_left(l);
                }
                return l;
            }
            if (diff < ALPHA) {
                r->left = _merge_with_root(l, root, r->left);
                r->_update();
                if (!(ALPHA <= r->balance() && r->balance() <= 1.0-ALPHA)) {
                    return _balance_right(r);
                }
                return r;
            }
            root->left = l;
            root->right = r;
            root->_update();
            return root;
        }

        pair<NodePtr, NodePtr> _pop_right(NodePtr node) {
            vector<NodePtr> path;
            NodePtr mx = node;
            while (node->right) {
                path.emplace_back(node);
                node = node->right;
                mx = node;
            }
            path.emplace_back(node->left? node->left: nullptr);
            while ((int)path.size() > 1) {
                    node = path.back();
                    path.pop_back();
                    if (!node) {
                    path.back()->right = nullptr;
                    path.back()->_update();
                    continue;
                }
                double b = node->balance();
                if (ALPHA <= b && b <= 1.0-ALPHA) {
                    path.back()->right = node;
                } else if (b > 1.0-ALPHA) {
                    path.back()->right = _balance_right(node);
                } else {
                    path.back()->right = _balance_left(node);
                }
                path.back()->_update();
            }
            if (path[0]) {
                double b = path[0]->balance();
                if (b > 1.0-ALPHA) {
                    path[0] = _balance_right(path[0]);
                } else if (b < ALPHA) {
                    path[0] = _balance_left(path[0]);
                }
            }
            mx->left = nullptr;
            mx->_update();
            return {path[0], mx};
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
            int tmp = node->left? k-node->left->size: k;
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

        WBTree<T> _new(NodePtr root) {
            WBTree<T> p(root);
            return p;
        }

        WBTree(NodePtr &root): root(root) {}

      public:
        NodePtr root;

        WBTree() : root(nullptr) {}

        WBTree(vector<T> const &a) { _build(a); }

        void build(vector<T> a) {
            if (a.empty()) {
                this->root = nullptr;
                return;
            }
            auto _build = [&] (auto &&_build, int l, int r) -> NodePtr {
                int mid = (l + r) >> 1;
                NodePtr node = new Node(a[mid]);
                if (l != mid) node->left = _build(_build, l, mid);
                if (mid+1 != r) node->right = _build(_build, mid+1, r);
                node->_update();
                return node;
            };
            this->root = _build(_build, 0, (int)a.size());
        }

        void merge(MyWBTree &other) {
            this->root = _merge_node(this->root, other.root);
        }

        pair<MyWBTree, MyWBTree> split(const int k) {
            auto [l, r] = _split_node(this->root, k);
            return {_new(l), _new(r)};
        }

        void insert(int k, const T key) {
            assert(0 <= k && k <= len());
            auto [s, t] = _split_node(root, k);
            NodePtr new_node = new Node(key);
            this->root = _merge_with_root(s, new_node, t);
        }

        void emplace_back(T key) {
            insert(len(), key);
        }

        T pop(int k) {
            if (k < 0) k += len();
            assert(0 <= k && k < len());
            vector<NodePtr> path;
            vector<short> path_d;
            NodePtr node = this->root;
            T res;
            int d = 0;
            while (1) {
                int t = node->left? node->left->size: 0;
                if (t == k) {
                    res = node->key;
                    break;
                }
                int d = (t < k)? 0: 1;
                path.emplace_back(node);
                path_d.emplace_back(d);
                if (d) {
                    node = node->left;
                } else {
                    k -= t + 1;
                    node = node->right;
                }
            }
            if (node->left && node->right) {
                NodePtr lmax = node->left;
                path.emplace_back(node);
                path_d.emplace_back(1);
                while (lmax->right) {
                    path.emplace_back(lmax);
                    path_d.emplace_back(0);
                    lmax = lmax->right;
                }
                node->key = lmax->key;
                node = lmax;
            }
            NodePtr cnode = (node->left)? node->left: node->right;
            if (!path.empty()) {
                if (path_d.back()) path.back()->left = cnode;
                else path.back()->right = cnode;
            } else {
                this->root = cnode;
                return res;
            }
            while (!path.empty()) {
                NodePtr new_node = nullptr;
                node = path.back();
                node->_update();
                path.pop_back();
                path_d.pop_back();
                double b = node->balance();
                if (b < ALPHA) {
                    new_node = _balance_left(node);
                } else if (b > 1.0-ALPHA) {
                    new_node = _balance_right(node);
                }
                if (new_node) {
                    if (path.empty()) {
                        this->root = new_node;
                        break;
                    }
                    if (path_d.back()) {
                        path.back()->left = new_node;
                    } else {
                        path.back()->right = new_node;
                    }
                }
            }
            return res;
        }

        vector<T> tovector() {
            NodePtr node = root;
            vector<NodePtr> stack;
            vector<T> a;
            a.reserve(len());
            while ((!stack.empty()) || node) {
                if (node) {
                stack.emplace_back(node);
                node = node->left;
                } else {
                node = stack.back();
                stack.pop_back();
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
            vector<NodePtr> path = {node};
            while (1) {
                int t = node->left? node->left->size: 0;
                if (t == k) {
                    node->key = v;
                    path.emplace_back(node);
                    if (pnode) {
                        if (d) pnode->left = node;
                        else pnode->right = node;
                    }
                    while (!path.empty()) {
                        path.back()->_update();
                        path.pop_back();
                    }
                    return;
                }
                pnode = node;
                d = (t < k)? 0: 1;
                if (d) {
                    pnode->left = node = node->left;
                } else {
                    k -= t + 1;
                    pnode->right = node = node->right;
                }
                path.emplace_back(node);
            }
        }

        T operator[] (int k) const {
            if (k < 0) k += len();
            assert(0 <= k && k < len());
            NodePtr node = root;
            while (1) {
                    int t = node->left? node->left->size: 0;
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

        void clear() {
            this->root = nullptr;
        }

        T& operator[] (int k) {
            if (k < 0) k += len();
            assert(0 <= k && k < len());
            NodePtr node = root;
            while (1) {
                int t = node->left? node->left->size: 0;
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

        void isok() {
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
                int s = ls + rs + 1;
                double b = (double)(ls+1) / (s+1);
                assert(s == node->size);
                assert(ALPHA <= b && b <= 1-ALPHA);
                return {s, height+1};
            };
            if (root == nullptr) return;
            auto [_, h] = rec(rec, root);
            // printf("height=%d\n", h);
        }
    };
} // namespace titan23

