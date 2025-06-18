#include <iostream>
#include <algorithm>
#include <vector>
#include <cassert>
#include "titan_cpplib/data_structures/bbst_node.cpp"
using namespace std;

// MultisetSum
namespace titan23 {

    template<typename T>
    class MultisetSum {
      public:
        class MultisetSumNode {
          public:
            using MultisetSumNodePtr = MultisetSumNode*;
            T key, data;
            long long val, valsize;
            int height;
            MultisetSumNodePtr par, left, right;

            MultisetSumNode() {}
            MultisetSumNode(const T &key, const long long val) : key(key), data(key*val), val(val), valsize(val), height(1), par(nullptr), left(nullptr), right(nullptr) {}

            int balance() const {
                int hl = left ? left->height : 0;
                int hr = right ? right->height : 0;
                return hl - hr;
            }

            void update() {
                data = key*val + (left ? left->data : 0) + (right ? right->data : 0);
                valsize = val + (left ? left->valsize : 0) + (right ? right->valsize : 0);
                height = 1 + max((left ? left->height : 0), (right ? right->height : 0));
            }
        };

        T missing;
        using MultisetSumNodePtr = MultisetSumNode*;
        MultisetSumNodePtr root;

        MultisetSumNodePtr build(vector<T> a) {
            vector<T> x;
            vector<int> y;

            auto _build = [&] (auto &&_build, int l, int r) -> MultisetSumNodePtr {
                int mid = (l + r) / 2;
                MultisetSumNodePtr node = new MultisetSumNode(x[mid], y[mid]);
                if (l != mid) {
                    node->left = _build(_build, l, mid);
                    node->left->par = node;
                }
                if (mid+1 != r) {
                    node->right = _build(_build, mid+1, r);
                    node->right->par = node;
                }
                node->update();
            return node;
            };

            if (a.empty()) return nullptr;
            int n = a.size();
            bool is_sorted = true;
            for (int i = 0; i < n-1; ++i) {
                if (!(a[i] <= a[i+1])) {
                    is_sorted = false;
                    break;
                }
            }
            if (!is_sorted) {
                sort(a.begin(), a.end());
            }

            x = {a[0]};
            y = {1};
            for (int i = 1; i < n; ++i) {
                if (a[i] == x.back()) {
                    ++y.back();
                    continue;
                }
                x.emplace_back(a[i]);
                y.emplace_back(1);
            }
            return _build(_build, 0, x.size());
        }

        void _remove_balance(MultisetSumNodePtr node) {
            while (node) {
                MultisetSumNodePtr new_node = nullptr;
                node->update();
                if (node->balance() == 2) {
                    new_node = node->left->balance() == -1 ? BBSTNode<MultisetSumNodePtr>::rotate_LR(node) : BBSTNode<MultisetSumNodePtr>::rotate_right(node);
                } else if (node->balance() == -2) {
                    new_node = node->right->balance() == 1 ? BBSTNode<MultisetSumNodePtr>::rotate_RL(node) : BBSTNode<MultisetSumNodePtr>::rotate_left(node);
                } else if (node->balance() != 0) {
                    node = node->par;
                    break;
                }
                if (!new_node) {
                    node = node->par;
                    continue;
                }
                if (!new_node->par) {
                    this->root = new_node;
                    return;
                }
                node = new_node->par;
                if (new_node->key < node->key) {
                    node->left = new_node;
                } else {
                    node->right = new_node;
                }
                if (new_node->balance() != 0) break;
            }
            while (node) {
                node->update();
                node = node->par;
            }
        }

        void _add_balance(MultisetSumNodePtr node) {
            MultisetSumNodePtr new_node = nullptr;
            while (node) {
                node->update();
                if (node->balance() == 0) {
                    node = node->par;
                    break;
                }
                if (node->balance() == 2) {
                    new_node = node->left->balance() == -1 ? BBSTNode<MultisetSumNodePtr>::rotate_LR(node) : BBSTNode<MultisetSumNodePtr>::rotate_right(node);
                    break;
                } else if (node->balance() == -2) {
                    new_node = node->right->balance() == 1 ? BBSTNode<MultisetSumNodePtr>::rotate_RL(node) : BBSTNode<MultisetSumNodePtr>::rotate_left(node);
                    break;
                }
                node = node->par;
            }
            if (new_node) {
                node = new_node->par;
                if (node) {
                    if (new_node->key < node->key) {
                    node->left = new_node;
                    } else {
                    node->right = new_node;
                    }
                } else {
                    this->root = new_node;
                }
            }
            while (node) {
                node->update();
                node = node->par;
            }
        }

        void _update_par(MultisetSumNodePtr node) {
            while (node) {
                node->update();
                node = node->par;
            }
        }

        MultisetSumNodePtr find_key(const T &key) const {
            MultisetSumNodePtr node = root;
            while (node) {
                if (key == node->key) return node;
                node = key < node->key ? node->left : node->right;
            }
            return nullptr;
        }

        MultisetSumNodePtr find_kth(int k) const {
            MultisetSumNodePtr node = root;
            while (true) {
                assert(node);
                int t = node->left ? (node->val + node->left->valsize) : node->val;
                if (t-node->val <= k && k < t) return node;
                if (t > k) {
                    node = node->left;
                } else {
                    k -= t;
                    node = node->right;
                }
            }
        }

      public:
        MultisetSum() : root(nullptr) {}
        MultisetSum(T missing) : missing(missing), root(nullptr) {}
        MultisetSum(vector<T> &a, T missing) : missing(missing) {
            this->root = build(a);
        }

        void add(const T &key, int val=1) {
            if (!root) {
                root = new MultisetSumNode(key, val);
                return;
            }
            MultisetSumNodePtr pnode = nullptr;
            MultisetSumNodePtr node = root;
            while (node) {
                if (key == node->key) {
                    node->val += val;
                    _update_par(node);
                    return;
                }
                pnode = node;
                node = key < node->key ? node->left : node->right;
            }
            if (key < pnode->key) {
                pnode->left = new MultisetSumNode(key, val);
                pnode->left->par = pnode;
            } else {
                pnode->right = new MultisetSumNode(key, val);
                pnode->right->par = pnode;
            }
            _add_balance(pnode);
        }

        void remove_iter(MultisetSumNodePtr node) {
            MultisetSumNodePtr pnode = node->par;
            if (node->left && node->right) {
                pnode = node;
                MultisetSumNodePtr mnode = node->left;
                while (mnode->right) {
                    pnode = mnode;
                    mnode = mnode->right;
                }
                node->key = mnode->key;
                node->val = mnode->val;
                node = mnode;
            }
            MultisetSumNodePtr cnode = (!node->left) ? node->right : node->left;
            if (cnode) cnode->par = pnode;
            if (pnode) {
                if (node->key <= pnode->key) {
                    pnode->left = cnode;
                } else {
                    pnode->right = cnode;
                }
                _remove_balance(pnode);
            } else {
                root = cnode;
            }
        }

        bool discard(const T &key, int val=1) {
            MultisetSumNodePtr node = find_key(key);
            if (!node) return false;
                node->val -= val;
                if (node->val <= 0) {
                    remove_iter(node);
                } else {
                    _update_par(node);
            }
            return true;
        }

        void remove(const T &key, int val=1) {
            MultisetSumNodePtr node = find_key(key);
            assert(node);
            node->val -= val;
            if (node->val <= 0) {
                remove_iter(node);
            } else {
                _update_par(node);
            }
        }

        T le(const T &key) const {
            T res = missing;
            MultisetSumNodePtr node = root;
            while (node) {
                if (key == node->key) {
                    res = node->key;
                    break;
                }
                if (key < node->key) {
                    node = node->left;
                } else {
                    res = node->key;
                    node = node->right;
                }
            }
            return res;
        }

        T lt(const T &key) const {
            T res = missing;
            MultisetSumNodePtr node = root;
            while (node) {
                if (key <= node->key) {
                    node = node->left;
                } else {
                    res = node->key;
                    node = node->right;
                }
            }
            return res;
        }

        T ge(const T &key) const {
            T res = missing;
            MultisetSumNodePtr node = root;
            while (node) {
                if (key == node->key) {
                    res = node->key;
                    break;
                }
                if (key < node->key) {
                    res = node->key;
                    node = node->left;
                } else {
                    node = node->right;
                }
            }
            return res;
        }

        T gt(const T &key) const {
            T res = missing;
            MultisetSumNodePtr node = root;
            while (node) {
                if (key < node->key) {
                    res = node->key;
                    node = node->left;
                } else {
                    node = node->right;
                }
            }
            return res;
        }

        int index(const T &key) const {
            int k = 0;
            MultisetSumNodePtr node = root;
            while (node) {
                if (key == node->key) {
                    k += node->left ? node->left->valsize : 0;
                    break;
                }
                if (key < node->key) {
                    node = node->left;
                } else {
                    k += node->left ? (node->left->valsize + node->val) : node->val;
                    node = node->right;
                }
            }
            return k;
        }

        int index_right(const T &key) const {
            int k = 0;
            MultisetSumNodePtr node = root;
            while (node) {
                if (key == node->key) {
                    k += node->left ? (node->left->valsize + node->val) : node->val;
                    break;
                }
                if (key < node->key) {
                    node = node->left;
                } else {
                    k += node->left ? (node->left->valsize + node->val) : node->val;
                    node = node->right;
                }
            }
            return k;
        }

        int sum(T high) const {
            T s = 0;
            MultisetSumNodePtr node = root;
            while (node) {
                if (high == node->key) {
                    s += node->left ? node->left->data : 0;
                    break;
                }
                if (high < node->key) {
                    node = node->left;
                } else {
                    s += node->left ? (node->left->data + node->key*node->val) : node->key*node->val;
                    node = node->right;
                }
            }
            return s;
        }

        T pop(int k=-1) {
            MultisetSumNodePtr node = find_kth(k);
            T key = node->key;
            node->val -= 1;
            if (node->val == 0) {
                remove_iter(node);
            } else {
                _update_par(node);
            }
            return key;
        }

        vector<T> tovector() const {
            vector<T> a;
            a.reserve(len());
            vector<MultisetSumNodePtr> st;
            MultisetSumNodePtr node = root;
            while ((!st.empty()) || node) {
                if (node) {
                    st.emplace_back(node);
                    node = node->left;
                } else {
                    node = st.back();
                    st.pop_back();
                    for (int i = 0; i < node->val; ++i) {
                        a.emplace_back(node->key);
                    }
                    node = node->right;
                }
            }
            return a;
        }

        bool contains(T key) const {
            return find_key(key) != nullptr;
        }

        T get(int k) const {
            return find_kth(k)->key;
        }

        int len() const {
            return root ? root->valsize : 0;
        }

        void print() const {
            vector<T> a = tovector();
            int n = a.size();
            cout << "{";
            for (int i = 0; i < n-1; ++i) {
                cout << a[i] << ", ";
            }
            if (n > 0) cout << a.back();
            cout << "}" << endl;
        }

        void check() const {
            if (!root) {
            // cout << "height=0" << endl;
            // cout << "check ok empty." << endl;
            return;
            }
            // cout << "height=" << root->height << endl;

            auto dfs = [&] (auto &&dfs, MultisetSumNodePtr node) -> void {
                int h = 0;
                int b = 0;
                int vs = node->val;
                if (node->left) {
                    assert(node->left->par == node);
                    assert(node->key > node->left->key);
                    dfs(dfs, node->left);
                    h = max(h, node->left->height);
                    b += node->left->height;
                    vs += node->left->valsize;
                }
                if (node->right) {
                    assert(node->right->par == node);
                    assert(node->key < node->right->key);
                    dfs(dfs, node->right);
                    h = max(h, node->right->height);
                    b -= node->right->height;
                    vs += node->right->valsize;
                }
                assert(node->valsize == vs);
                assert(node->height == h+1);
                assert(-1 <= b && b <= 1);
            };
            dfs(dfs, root);
            // cout << "check ok." << endl;
        }

        friend ostream& operator<<(ostream& os, const titan23::MultisetSum<T>& s) {
            vector<T> a = s.tovector();
            int n = a.size();
            os << "{";
            for (int i = 0; i < n - 1; ++i) {
                os << a[i] << ", ";
            }
            if (n > 0) os << a.back();
            os << "}";
            return os;
        }
    };
} // namespace titan23
