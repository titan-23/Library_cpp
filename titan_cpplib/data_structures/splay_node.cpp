#pragma once
#include <iostream>
using namespace std;

namespace titan23 {

template <class T, T (*op)(T, T), T (*e)(),
          class F, T (*mapping)(F, T), F (*composition)(F, F), F (*id)()>
class LazySplayTreeNode {
public:
    struct Node;
    using NodePtr = Node*;

    struct Node {
        NodePtr left, right, par;
        T key, data, rdata;
        F lazy;
        int size;

        Node(T key, F lazy) :
                left(nullptr), right(nullptr), par(nullptr),
                key(key), data(key), rdata(key),
                lazy(lazy), size(1) {}

        void propagate_lazy(F f) {
            key = mapping(f, key);
            data = mapping(f, data);
            rdata = mapping(f, rdata);
            lazy = composition(f, lazy);
        }

        void propagate() {
            if (lazy != id()) {
                if (left)  left->propagate_lazy(lazy);
                if (right) right->propagate_lazy(lazy);
                lazy = id();
            }
        }

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

    NodePtr find_splay(NodePtr node, T key) {
        NodePtr pnode = nullptr;
        while (node) {
            node->propagate();
            if (node->key == key) {
                node->splay();
                return node;
            }
            pnode = node;
            if (key < node->key) {
                node = node->left;
            } else {
                node = node->right;
            }
        }
        if (pnode) {
            pnode->splay();
            return pnode;
        }
        return node;
    }

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


template <class T, T (*op)(T, T), T (*e)()>
class SplayTreeNode {
public:
    struct Node;
    using NodePtr = Node*;

    struct Node {
        NodePtr left, right, par;
        T key, data, rdata;
        int size;

        Node(T key) :
                left(nullptr), right(nullptr), par(nullptr),
                key(key), data(key), rdata(key), size(1) {}

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

        void splay() {
            while (par && par->par) {
                ((par->par->left == par) == (par->left == this) ? par : this)->rotate();
                rotate();
            }
            if (par) rotate();
        }

        NodePtr left_splay() {
            NodePtr node = this;
            while (node->left) node = node->left;
            node->splay();
            return node;
        }

        NodePtr right_splay() {
            NodePtr node = this;
            while (node->right) node = node->right;
            node->splay();
            return node;
        }
    };

    NodePtr kth_splay(NodePtr node, int k) {
        node->splay();
        while (1) {
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

    NodePtr find_splay(NodePtr node, T key) {
        if (!node) return nullptr;
        NodePtr pnode = nullptr;
        while (node) {
            if (node->key == key) {
                node->splay();
                return node;
            }
            pnode = node;
            node = key < node->key ? node->left : node->right;
        }
        if (pnode) {
            pnode->splay();
            return pnode;
        }
        return node;
    }

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

    void print(NodePtr node) {
        while (node && node->par) node = node->par;
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


template <class T>
class SplayTreeNodeKey {
public:
    struct Node;
    using NodePtr = Node*;

    struct Node {
        NodePtr left, right, par;
        T key;
        int size;

        Node(T key) :
                left(nullptr), right(nullptr), par(nullptr),
                key(key), size(1) {}

        void update() {
            size = 1;
            if (left) size += left->size;
            if (right) size += right->size;
        }

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

        void splay() {
            while (par && par->par) {
                ((par->par->left == par) == (par->left == this) ? par : this)->rotate();
                rotate();
            }
            if (par) rotate();
        }

        NodePtr left_splay() {
            NodePtr node = this;
            while (node->left) node = node->left;
            node->splay();
            return node;
        }

        NodePtr right_splay() {
            NodePtr node = this;
            while (node->right) node = node->right;
            node->splay();
            return node;
        }
    };

    NodePtr kth_splay(NodePtr node, int k) {
        node->splay();
        while (1) {
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

    NodePtr find_splay(NodePtr node, T key) {
        if (!node) return nullptr;
        NodePtr pnode = nullptr;
        while (node) {
            if (node->key == key) {
                node->splay();
                return node;
            }
            pnode = node;
            node = key < node->key ? node->left : node->right;
        }
        if (pnode) {
            pnode->splay();
            return pnode;
        }
        return node;
    }

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

    void print(NodePtr node) {
        while (node && node->par) node = node->par;
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
