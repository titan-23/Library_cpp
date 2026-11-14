/// https://github.com/titan-23/Library_cpp/blob/main/titan_cpplib/ds/avl_tree_multiset.cpp
#pragma once

#include <iostream>
#include <algorithm>
#include <vector>
#include <cassert>
#include "titan_cpplib/ds/bbst_node.cpp"
using namespace std;

namespace titan23 {

/// @brief AVL 木による多重集合 / 同じ値は 1 個の節点にまとめて個数を持つ
template<typename T>
class AVLTreeMultiset {
    public:
    /// @brief 木の節点 / val は key の個数、valsize は部分木の個数の和
    class AVLTreeMultisetNode {
        public:
        using AVLTreeMultisetNodePtr = AVLTreeMultisetNode*;
        T key;
        long long val, valsize;
        int height;
        AVLTreeMultisetNodePtr par, left, right;

        AVLTreeMultisetNode() {}
        AVLTreeMultisetNode(const T &key, const long long val) : key(key), val(val), valsize(val), height(1), par(nullptr), left(nullptr), right(nullptr) {}

        /// @brief 左の高さから右の高さを引いた値を返す / O(1)
        int balance() const {
            int hl = left ? left->height : 0;
            int hr = right ? right->height : 0;
            return hl - hr;
        }

        /// @brief 子から valsize と height を計算し直す / O(1)
        void update() {
            valsize = val + (left ? left->valsize : 0) + (right ? right->valsize : 0);
            height = 1 + max((left ? left->height : 0), (right ? right->height : 0));
        }
    };

    // 該当する要素がないときに返す値
    T missing;
    using AVLTreeMultisetNodePtr = AVLTreeMultisetNode*;
    AVLTreeMultisetNodePtr root;

    /// @brief a から木を作り根を返す / 等しい値は 1 個の節点にまとめる / O(n log n)
    AVLTreeMultisetNodePtr build(vector<T> a) {
        vector<T> x;
        vector<int> y;

        auto _build = [&] (auto &&_build, int l, int r) -> AVLTreeMultisetNodePtr {
            int mid = (l + r) / 2;
            AVLTreeMultisetNodePtr node = new AVLTreeMultisetNode(x[mid], y[mid]);
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

    /// @brief 削除の後、node から根へ向かって平衡を直す / O(log n)
    void _remove_balance(AVLTreeMultisetNodePtr node) {
        while (node) {
            AVLTreeMultisetNodePtr new_node = nullptr;
            node->update();
            if (node->balance() == 2) {
                new_node = node->left->balance() == -1 ? BBSTNode<AVLTreeMultisetNodePtr>::rotate_LR(node) : BBSTNode<AVLTreeMultisetNodePtr>::rotate_right(node);
            } else if (node->balance() == -2) {
                new_node = node->right->balance() == 1 ? BBSTNode<AVLTreeMultisetNodePtr>::rotate_RL(node) : BBSTNode<AVLTreeMultisetNodePtr>::rotate_left(node);
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

    /// @brief 挿入の後、node から根へ向かって平衡を直す / O(log n)
    void _add_balance(AVLTreeMultisetNodePtr node) {
        AVLTreeMultisetNodePtr new_node = nullptr;
        while (node) {
            node->update();
            if (node->balance() == 0) {
                node = node->par;
                break;
            }
            if (node->balance() == 2) {
                new_node = node->left->balance() == -1 ? BBSTNode<AVLTreeMultisetNodePtr>::rotate_LR(node) : BBSTNode<AVLTreeMultisetNodePtr>::rotate_right(node);
                break;
            } else if (node->balance() == -2) {
                new_node = node->right->balance() == 1 ? BBSTNode<AVLTreeMultisetNodePtr>::rotate_RL(node) : BBSTNode<AVLTreeMultisetNodePtr>::rotate_left(node);
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

    /// @brief node から根までの valsize と height を計算し直す / O(log n)
    void _update_par(AVLTreeMultisetNodePtr node) {
        while (node) {
            node->update();
            node = node->par;
        }
    }

    /// @brief key の節点を返す / なければ nullptr / O(log n)
    AVLTreeMultisetNodePtr find_key(const T &key) const {
        AVLTreeMultisetNodePtr node = root;
        while (node) {
            if (key == node->key) return node;
            node = key < node->key ? node->left : node->right;
        }
        return nullptr;
    }

    /// @brief 小さい方から k 番目の要素を持つ節点を返す / 0-indexed / O(log n)
    AVLTreeMultisetNodePtr find_kth(int k) const {
        AVLTreeMultisetNodePtr node = root;
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
    /// @brief 空の多重集合を作る / missing は初期化しない
    AVLTreeMultiset() : root(nullptr) {}
    /// @brief 空の多重集合を作る / 該当する要素がない探索は missing を返す
    AVLTreeMultiset(T missing) : missing(missing), root(nullptr) {}
    /// @brief a の要素から作る / O(n log n)
    AVLTreeMultiset(const vector<T> &a, T missing) : missing(missing) {
        this->root = build(a);
    }

    /// @brief key を val 個追加する / O(log n)
    void add(const T &key, int val=1) {
        if (!root) {
            root = new AVLTreeMultisetNode(key, val);
            return;
        }
        AVLTreeMultisetNodePtr pnode = nullptr;
        AVLTreeMultisetNodePtr node = root;
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
            pnode->left = new AVLTreeMultisetNode(key, val);
            pnode->left->par = pnode;
        } else {
            pnode->right = new AVLTreeMultisetNode(key, val);
            pnode->right->par = pnode;
        }
        _add_balance(pnode);
    }

    /// @brief 節点 node をその個数ごと削除する / O(log n)
    void remove_iter(AVLTreeMultisetNodePtr node) {
        AVLTreeMultisetNodePtr pnode = node->par;
        if (node->left && node->right) {
            pnode = node;
            AVLTreeMultisetNodePtr mnode = node->left;
            while (mnode->right) {
                pnode = mnode;
                mnode = mnode->right;
            }
            node->key = mnode->key;
            node->val = mnode->val;
            node = mnode;
        }
        AVLTreeMultisetNodePtr cnode = (!node->left) ? node->right : node->left;
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
        AVLTreeMultisetNodePtr node = find_key(key);
        if (!node) return false;
            node->val -= val;
            if (node->val <= 0) {
                remove_iter(node);
            } else {
                _update_par(node);
        }
        return true;
    }

    /// @brief key を最大 val 個削除する / key がある必要がある / O(log n)
    void remove(const T &key, int val=1) {
        AVLTreeMultisetNodePtr node = find_key(key);
        assert(node);
        node->val -= val;
        if (node->val <= 0) {
            remove_iter(node);
        } else {
            _update_par(node);
        }
    }

    /// @brief key 以下で最大の要素を返す / なければ missing / O(log n)
    T le(const T &key) const {
        T res = missing;
        AVLTreeMultisetNodePtr node = root;
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

    /// @brief key 未満で最大の要素を返す / なければ missing / O(log n)
    T lt(const T &key) const {
        T res = missing;
        AVLTreeMultisetNodePtr node = root;
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

    /// @brief key 以上で最小の要素を返す / なければ missing / O(log n)
    T ge(const T &key) const {
        T res = missing;
        AVLTreeMultisetNodePtr node = root;
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

    /// @brief key を超える最小の要素を返す / なければ missing / O(log n)
    T gt(const T &key) const {
        T res = missing;
        AVLTreeMultisetNodePtr node = root;
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
        AVLTreeMultisetNodePtr node = root;
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
        AVLTreeMultisetNodePtr node = root;
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

    /// @brief low 以上 high 未満の要素数を返す / O(log n)
    int count_range(const T low, const T high) const {
        return index(high) - index(low);
    }

    /// @brief 小さい方から k 番目の要素を 1 個削除して返す / 0-indexed / O(log n)
    T pop(int k) {
        AVLTreeMultisetNodePtr node = find_kth(k);
        T key = node->key;
        node->val -= 1;
        if (node->val == 0) {
            remove_iter(node);
        } else {
            _update_par(node);
        }
        return key;
    }

    /// @brief 全要素を昇順に並べた列を返す / O(n)
    vector<T> tovector() const {
        vector<T> a;
        a.reserve(len());
        vector<AVLTreeMultisetNodePtr> st;
        AVLTreeMultisetNodePtr node = root;
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

    /// @brief key を含むかを返す / O(log n)
    bool contains(T key) const {
        return find_key(key) != nullptr;
    }

    /// @brief 小さい方から k 番目の要素を返す / 0-indexed / O(log n)
    T get(int k) const {
        return find_kth(k)->key;
    }

    /// @brief 要素数を返す / O(1)
    int len() const {
        return root ? root->valsize : 0;
    }

    /// @brief 全要素を {a, b, ...} の形で標準出力に書く / O(n)
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

    /// @brief 木の不変条件を assert で調べる / デバッグ用
    void check() const {
        if (!root) {
        // cout << "height=0" << endl;
        // cout << "check ok empty." << endl;
        return;
        }
        // cout << "height=" << root->height << endl;

        auto dfs = [&] (auto &&dfs, AVLTreeMultisetNodePtr node) -> void {
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

    /// @brief 全要素を {a, b, ...} の形で出力する / O(n)
    friend ostream& operator<<(ostream& os, const titan23::AVLTreeMultiset<T>& s) {
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
