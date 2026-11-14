/// https://github.com/titan-23/Library_cpp/blob/main/titan_cpplib/ds/splay_node.cpp
#pragma once
#include <iostream>
using namespace std;

namespace titan23 {

/// @brief 遅延伝播つき splay 木の節点と、節点に対する操作
template <class T, T (*op)(T, T), T (*e)(),
          class F, T (*mapping)(F, T), F (*composition)(F, F), F (*id)()>
class LazySplayTreeNode {
public:
    struct Node;
    using NodePtr = Node*;

    /// @brief 節点 / data は部分木の総積、rdata は逆順の総積
    struct Node {
        NodePtr left, right, par;
        T key, data, rdata;
        F lazy;
        int size;

        Node(T key, F lazy) :
                left(nullptr), right(nullptr), par(nullptr),
                key(key), data(key), rdata(key),
                lazy(lazy), size(1) {}

        /// @brief 自身に f を作用させ、遅延値に f を合成する
        void propagate_lazy(F f) {
            key = mapping(f, key);
            data = mapping(f, data);
            rdata = mapping(f, rdata);
            lazy = composition(f, lazy);
        }

        /// @brief 遅延値を子に伝播する
        void propagate() {
            if (lazy != id()) {
                if (left)  left->propagate_lazy(lazy);
                if (right) right->propagate_lazy(lazy);
                lazy = id();
            }
        }

        /// @brief 子から data, rdata, size を計算し直す
        void update() {
            data = key;
            rdata = key;
            size = 1;
            if (left) {
                data = op(left->data, data);
                rdata = op(rdata, left->rdata);
                size += left->size;
            }
            if (right) {
                data = op(data, right->data);
                rdata = op(right->rdata, rdata);
                size += right->size;
            }
        }

        /// @brief 親との間で回転する
        void rotate() {
            NodePtr pnode = par;
            NodePtr gnode = pnode->par;
            if (gnode) {
                if (gnode->left == pnode) {
                    gnode->left = this;
                } else {
                    gnode->right = this;
                }
            }
            par = gnode;
            if (pnode->left == this) {
                pnode->left = right;
                if (right) right->par = pnode;
                right = pnode;
            } else {
                pnode->right = left;
                if (left) left->par = pnode;
                left = pnode;
            }
            pnode->par = this;
            pnode->update();
            update();
        }

        /// @brief 自身を根まで splay する
        void splay() {
            while (par && par->par) {
                par->par->propagate();
                par->propagate();
                propagate();
                ((par->par->left == par) == (par->left == this) ? par : this)->rotate();
                rotate();
            }
            if (par) {
                par->propagate();
                propagate();
                rotate();
            }
        }

        /// @brief 部分木の最左の節点を splay して返す
        NodePtr left_splay() {
            NodePtr node = this;
            propagate();
            while (node->left) {
                node = node->left;
                node->propagate();
            }
            node->splay();
            return node;
        }

        /// @brief 部分木の最右の節点を splay して返す
        NodePtr right_splay() {
            NodePtr node = this;
            propagate();
            while (node->right) {
                node = node->right;
                node->propagate();
            }
            node->splay();
            return node;
        }
    };

    /// @brief node を含む木で k 番目の節点を splay して返す / k は 0 始まり
    NodePtr kth_splay(NodePtr node, int k) {
        node->splay();
        while (1) {
            node->propagate();
            int t = node->left ? node->left->size : 0;
            if (t == k) break;
            if (t > k) {
                node = node->left;
            } else {
                node = node->right;
                k -= t + 1;
            }
        }
        node->splay();
        return node;
    }

    /// @brief key 以下で最大の節点を splay して返す / なければ nullptr
    NodePtr find_splay(NodePtr node, T key) {
        if (!node) return nullptr;
        NodePtr res = nullptr;
        while (node) {
            node->propagate();
            if (node->key < key || node->key == key) {
                res = node;
                node = node->right;
            } else {
                node = node->left;
            }
        }
        if (res) res->splay();
        return res;
    }

    /// @brief node を含む木を key 以下の部分木と key より大きい部分木に分ける / key 以下の節点があること
    pair<NodePtr, NodePtr> split(NodePtr node, T key) {
        if (!node) { return {nullptr, nullptr}; }
        node->splay();
        node = find_splay(node, key);
        if (node->key < key || node->key == key) {
            NodePtr r = node->right;
            node->right = nullptr;
            if (r) r->par = nullptr;
            node->update();
            return {node, r};
        } else {
            NodePtr l = node->left;
            node->left = nullptr;
            if (l) l->par = nullptr;
            node->update();
            return {l, node};
        }
    }

    /// @brief node を含む木を標準エラー出力に出す / デバッグ用
    void print(NodePtr node) {
        while (node->par) {
            node = node->par;
        }
        auto dfs = [&] (auto &&dfs, NodePtr node) {
            if (!node) return;
            dfs(dfs, node->left);
            cerr << node->key << ", ";
            dfs(dfs, node->right);
        };
        cerr << "[";
        dfs(dfs, node);
        cerr << "]" << endl;
    }
};
} // namespace titan23
