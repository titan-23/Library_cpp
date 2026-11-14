/// https://github.com/titan-23/Library_cpp/blob/main/titan_cpplib/ds/avl_tree_set.cpp
#pragma once

#include <iostream>
#include <algorithm>
#include <vector>
#include <cassert>
#include <stack>
#include "titan_cpplib/ds/bbst_node.cpp"
using namespace std;

namespace titan23 {

/// @brief AVL 木による集合
template<typename T>
class AVLTreeSet {
public:
    /// @brief 木の節点 / size は部分木の要素数
    class AVLTreeSetNode {
    public:
        using AVLTreeSetNodePtr = AVLTreeSetNode*;
        T key;
        int size;
        char height;
        AVLTreeSetNodePtr par, left, right;

        AVLTreeSetNode() : size(0), height(0), par(nullptr), left(nullptr), right(nullptr) {}
        AVLTreeSetNode(const T &key) : key(key), size(1), height(1), par(nullptr), left(nullptr), right(nullptr) {}

        /// @brief 左の高さから右の高さを引いた値を返す / O(1)
        constexpr char balance() const {
            return (left ? left->height : 0) - (right ? right->height : 0);
        }

        /// @brief 子から size と height を計算し直す / O(1)
        constexpr void update() {
            size = 1 + (left ? left->size : 0) + (right ? right->size : 0);
            height = 1 + max((left ? left->height : 0), (right ? right->height : 0));
        }
    };

private:
    // 該当する要素がないときに返す値
    T missing;
    using AVLTreeSetNodePtr = AVLTreeSetNode*;
    AVLTreeSetNodePtr root;

    // 削除の後、node から根へ向かって平衡を直す
    constexpr void _remove_balance(AVLTreeSetNodePtr node) {
        while (node) {
            AVLTreeSetNodePtr new_node = nullptr;
            node->update();
            if (node->balance() == 2) {
                new_node = node->left->balance() == -1 ? BBSTNode<AVLTreeSetNodePtr>::rotate_LR(node) : BBSTNode<AVLTreeSetNodePtr>::rotate_right(node);
            } else if (node->balance() == -2) {
                new_node = node->right->balance() == 1 ? BBSTNode<AVLTreeSetNodePtr>::rotate_RL(node) : BBSTNode<AVLTreeSetNodePtr>::rotate_left(node);
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

    // 挿入の後、node から根へ向かって平衡を直す
    constexpr void _add_balance(AVLTreeSetNodePtr node) {
        AVLTreeSetNodePtr new_node = nullptr;
        while (node) {
            node->update();
            if (node->balance() == 0) {
                node = node->par;
                break;
            }
            if (node->balance() == 2) {
                new_node = node->left->balance() == -1 ? BBSTNode<AVLTreeSetNodePtr>::rotate_LR(node) : BBSTNode<AVLTreeSetNodePtr>::rotate_right(node);
                break;
            } else if (node->balance() == -2) {
                new_node = node->right->balance() == 1 ? BBSTNode<AVLTreeSetNodePtr>::rotate_RL(node) : BBSTNode<AVLTreeSetNodePtr>::rotate_left(node);
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

    public:
    /// @brief 空の集合を作る / missing は -1
    AVLTreeSet() : missing(-1), root(nullptr) {}
    /// @brief 空の集合を作る / 該当する要素がない探索は missing を返す
    AVLTreeSet(T missing) : missing(missing), root(nullptr) {}
    /// @brief a の要素から作る / 重複は除く / O(n log n)
    AVLTreeSet(const vector<T> &a, T missing) : missing(missing) {
        this->root = build(a);
    }

    /// @brief a から木を作り根を返す / 重複は除く / O(n log n)
    AVLTreeSetNodePtr build(vector<T> a) {
        auto _build = [&] (auto &&_build, int l, int r) -> AVLTreeSetNodePtr {
            int mid = (l + r) / 2;
            AVLTreeSetNodePtr node = new AVLTreeSetNode(a[mid]);
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
            if (!(a[i] < a[i+1])) {
                is_sorted = false;
                break;
            }
        }
        if (!is_sorted) {
            sort(a.begin(), a.end());
            a.erase(unique(a.begin(), a.end()), a.end());
        }
        return _build(_build, 0, a.size());
    }

    /// @brief key を追加する / すでにあれば false を返す / O(log n)
    bool add(const T &key) {
        if (!root) {
            root = new AVLTreeSetNode(key);
            return true;
        }
        AVLTreeSetNodePtr pnode = nullptr;
        AVLTreeSetNodePtr node = root;
        while (node) {
            if (key == node->key) return false;
            pnode = node;
            node = key < node->key ? node->left : node->right;
        }
        if (key < pnode->key) {
            pnode->left = new AVLTreeSetNode(key);
            pnode->left->par = pnode;
        } else {
            pnode->right = new AVLTreeSetNode(key);
            pnode->right->par = pnode;
        }
        _add_balance(pnode);
        return true;
    }

    /// @brief 節点 node を削除する / O(log n)
    void remove_iter(AVLTreeSetNodePtr node) {
        AVLTreeSetNodePtr pnode = node->par;
        if (node->left && node->right) {
            pnode = node;
            AVLTreeSetNodePtr mnode = node->left;
            while (mnode->right) {
                pnode = mnode;
                mnode = mnode->right;
            }
            node->key = mnode->key;
            node = mnode;
        }
        AVLTreeSetNodePtr cnode = (!node->left) ? node->right : node->left;
        if (cnode) {
            cnode->par = pnode;
        }
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

    /// @brief key を削除する / なければ false を返す / O(log n)
    bool discard(const T &key) {
        AVLTreeSetNodePtr node = find_key(key);
        if (!node) return false;
        remove_iter(node);
        return true;
    }

    /// @brief key を削除する / key がある必要がある / O(log n)
    void remove(const T &key) {
        AVLTreeSetNodePtr node = find_key(key);
        assert(node);
        remove_iter(node);
    }

    /// @brief key 以下で最大の要素を返す / なければ missing / O(log n)
    T le(const T &key) const {
        T res = missing;
        AVLTreeSetNodePtr node = root;
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
        AVLTreeSetNodePtr node = root;
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
        AVLTreeSetNodePtr node = root;
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
        AVLTreeSetNodePtr node = root;
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
        AVLTreeSetNodePtr node = root;
        while (node) {
            if (key == node->key) {
                k += node->left ? node->left->size : 0;
                break;
            }
            if (key < node->key) {
                node = node->left;
            } else {
                k += node->left ? (node->left->size + 1) : 1;
                node = node->right;
            }
        }
        return k;
    }

    /// @brief key 以下の要素数を返す / O(log n)
    int index_right(const T &key) const {
        int k = 0;
        AVLTreeSetNodePtr node = root;
        while (node) {
            if (key == node->key) {
                k += node->left ? (node->left->size + 1) : 1;
                break;
            }
            if (key < node->key) {
                node = node->left;
            } else {
                k += node->left ? (node->left->size + 1) : 1;
                node = node->right;
            }
        }
        return k;
    }

    /// @brief l 以上 r 未満の要素数を返す / O(log n)
    int range_count(T l, T r) const {
        return index(r) - index(l);
    }

    /// @brief key の節点を返す / なければ nullptr / O(log n)
    AVLTreeSetNodePtr find_key(const T &key) const {
        AVLTreeSetNodePtr node = root;
        while (node) {
            if (key == node->key) return node;
            node = key < node->key ? node->left : node->right;
        }
        return nullptr;
    }

    /// @brief 小さい方から k 番目の節点を返す / 0-indexed / O(log n)
    AVLTreeSetNodePtr find_kth(int k) const {
        AVLTreeSetNodePtr node = root;
        while (true) {
            int t = node->left ? node->left->size : 0;
            if (t == k) return node;
            if (t < k) {
                k -= t + 1;
                node = node->right;
            } else {
                node = node->left;
            }
        }
    }

    /// @brief 小さい方から k 番目の要素を削除して返す / 0-indexed / O(log n)
    T pop(int k) {
        assert(0 <= k && k < len());
        AVLTreeSetNodePtr node = find_kth(k);
        T key = node->key;
        remove_iter(node);
        return key;
    }

    /// @brief 全要素を昇順に並べた列を返す / O(n)
    vector<T> tovector() const {
        vector<T> a;
        a.reserve(len());
        stack<AVLTreeSetNodePtr> st;
        AVLTreeSetNodePtr node = root;
        while ((!st.empty()) || node) {
            if (node) {
                st.emplace(node);
                node = node->left;
            } else {
                node = st.top();
                st.pop();
                a.emplace_back(node->key);
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
        assert(0 <= k && k < len());
        AVLTreeSetNodePtr node = root;
        while (true) {
            int t = node->left ? node->left->size : 0;
            if (t == k) return node->key;
            if (t < k) {
                k -= t + 1;
                node = node->right;
            } else {
                node = node->left;
            }
        }
    }

    /// @brief 要素数を返す / O(1)
    int len() const {
        return root ? root->size : 0;
    }

    /// @brief 木の不変条件を assert で調べ、結果を標準出力に書く / デバッグ用
    void check() const {
        if (!root) {
            cout << "height=0" << endl;;
            cout << "check ok empty." << endl;;
            return;
        }
        cout << "height=" << root->height << endl;

        auto dfs = [&] (auto &&dfs, AVLTreeSetNodePtr node) -> void {
            int h = 0;
            int b = 0;
            int s = 1;
            if (node->left) {
                assert(node->left->par == node);
                assert(node->key > node->left->key);
                dfs(dfs, node->left);
                h = max(h, node->left->height);
                b += node->left->height;
                s += node->left->size;
            }
            if (node->right) {
                assert(node->right->par == node);
                assert(node->key < node->right->key);
                dfs(dfs, node->right);
                h = max(h, node->right->height);
                b -= node->right->height;
                s += node->right->size;
            }
            assert(node->height == h+1);
            assert(-1 <= b && b <= 1);
            assert(node->size == s);
        };
        dfs(dfs, root);
        cout << "check ok." << endl;
    }

    /// @brief 全要素を {a, b, ...} の形で出力する / O(n)
    friend ostream& operator<<(ostream& os, const titan23::AVLTreeSet<T>& s) {
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
}
