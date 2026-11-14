/// https://github.com/titan-23/Library_cpp/blob/main/titan_cpplib/ds/multiset_sum.cpp
#pragma once

#include <iostream>
#include <algorithm>
#include <vector>
#include <cassert>
#include "titan_cpplib/ds/bbst_node.cpp"
using namespace std;

namespace titan23 {

/// @brief 総和を求められる多重集合 / AVL 木で実装する
template<typename T>
class MultisetSum {
public:
    // key を val 個持ち、data に部分木の総和、valsize に部分木の要素数を持つ
    class MultisetSumNode {
    public:
        using MultisetSumNodePtr = MultisetSumNode*;
        T key, data;
        long long val, valsize;
        char height;
        MultisetSumNodePtr par, left, right;

        MultisetSumNode() {}
        MultisetSumNode(const T &key, const long long val) : key(key), data(key*val), val(val), valsize(val), height(1), par(nullptr), left(nullptr), right(nullptr) {}

        char balance() const {
            char hl = left ? left->height : 0;
            char hr = right ? right->height : 0;
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
    /// @brief 空で構築する / 該当する要素がないとき le などは missing を返す
    MultisetSum(T missing) : missing(missing), root(nullptr) {}
    /// @brief 列 a の要素で構築する / O(n log n)
    MultisetSum(const vector<T> &a, T missing=-1) : missing(missing) {
        this->root = build(a);
    }

    /// @brief key を val 個追加する / O(log n)
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

    /// @brief ノード node を木から取り除く / O(log n)
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

    /// @brief key を最大 val 個削除し、key があったかを返す / O(log n)
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

    /// @brief key を最大 val 個削除する / key があることが前提 / O(log n)
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

    /// @brief key 以下で最大の要素を返す / O(log n)
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

    /// @brief key 未満で最大の要素を返す / O(log n)
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

    /// @brief key 以上で最小の要素を返す / O(log n)
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

    /// @brief key より大きい最小の要素を返す / O(log n)
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

    /// @brief key 未満の要素数を返す / O(log n)
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

    /// @brief key 以下の要素数を返す / O(log n)
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

    /// @brief 小さい順に取るとき、総和が w を超えない最大の個数を返す / O(log n)
    long long bisect_left_sum(T w) const {
        long long s = 0;
        MultisetSumNodePtr node = root;
        T rem = w;
        while (node) {
            if (node->left) {
                if (node->left->data <= rem) {
                    s += node->left->valsize;
                    rem -= node->left->data;
                } else {
                    node = node->left;
                    continue;
                }
            }
            T sm = node->key * node->val;
            if (sm <= rem) {
                s += node->val;
                rem -= sm;
                node = node->right;
            } else {
                if (node->key == (T)0) {
                    s += node->val;
                    node = node->right;
                    continue;
                }
                long long ok = (long long)(rem / node->key);
                if (ok > node->val) ok = node->val;
                if (ok < 0) ok = 0;
                s += ok;
                return s;
            }
        }
        return s;
    }

    /// @brief high 未満の要素の総和を返す / O(log n)
    T sum(T high) const {
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

    /// @brief 全要素の総和を返す / 空でないことが前提 / O(1)
    T all_prod() const {
        return root->data;
    }

    /// @brief 昇順 k 番目の要素を 1 個削除して返す / 0 <= k < len() が前提 / O(log n)
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

    /// @brief 昇順に並べた vector を返す / O(n)
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

    /// @brief key があるかを返す / O(log n)
    bool contains(T key) const {
        return find_key(key) != nullptr;
    }

    /// @brief 昇順 k 番目の要素を返す / O(log n)
    T get(int k) const {
        return find_kth(k)->key;
    }

    /// @brief 要素数を返す / O(1)
    int len() const {
        return root ? root->valsize : 0;
    }

    /// @brief 要素を出力する
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

    /// @brief 木の不変条件を assert で検査する / デバッグ用
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
