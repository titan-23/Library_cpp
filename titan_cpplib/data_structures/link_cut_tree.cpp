#include <vector>
#include <cassert>
using namespace std;

// LinkCutTree
namespace titan23 {

    template <class T,
            class F,
            T (*op)(T, T),
            T (*mapping)(F, T),
            F (*composition)(F, F),
            T (*e)(),
            F (*id)()>
    class LinkCutTree {
      private:
        struct Node;
        using NodePtr = Node*;
        vector<NodePtr> pool;

        struct Node {
            int index, size, rev;
            T key, data, rdata;
            F lazy;
            NodePtr left, right, par;

            Node(const int index, const T key, const F lazy) :
                    index(index), size(1), rev(0),
                    key(key), data(key), rdata(key),
                    lazy(lazy),
                    left(nullptr), right(nullptr), par(nullptr) {
            }

            bool is_root() const {
                return (!par) || (!(par->left == this || par->right == this));
            }
        };

        void _apply_rev(const NodePtr node) {
            if (!node) return;
            node->rev ^= 1;
        }

        void _apply_f(const NodePtr node, const F f) {
            if (!node) return;
            node->key = mapping(f, node->key);
            node->data = mapping(f, node->data);
            node->rdata = mapping(f, node->rdata);
            node->lazy = composition(f, node->lazy);
        }

        void _propagate(const NodePtr node) {
            if (!node) return;
            if (node->rev) {
                swap(node->data, node->rdata);
                swap(node->left, node->right);
                _apply_rev(node->left);
                _apply_rev(node->right);
                node->rev = 0;
            }
            if (node->lazy != id()) {
                _apply_f(node->left, node->lazy);
                _apply_f(node->right, node->lazy);
                node->lazy = id();
            }
        }

        void _update(const NodePtr node) {
            if (!node) return;
            _propagate(node->left);
            _propagate(node->right);
            node->data = node->key;
            node->rdata = node->key;
            node->size = 1;
            if (node->left) {
                node->data = op(node->left->data, node->data);
                node->rdata = op(node->rdata, node->left->rdata);
                node->size += node->left->size;
            }
            if (node->right) {
                node->data = op(node->data, node->right->data);
                node->rdata = op(node->right->rdata, node->rdata);
                node->size += node->right->size;
            }
        }

        void _rotate(const NodePtr node) {
            const NodePtr pnode = node->par;
            const NodePtr gnode = pnode->par;
            _propagate(pnode);
            _propagate(node);
            if (gnode) {
                if (gnode->left == pnode) {
                    gnode->left = node;
                } else if (gnode->right == pnode) {
                    gnode->right = node;
                }
            }
            node->par = gnode;
            if (pnode->left == node) {
                pnode->left = node->right;
                if (node->right) node->right->par = pnode;
                node->right = pnode;
            } else {
                pnode->right = node->left;
                if (node->left) node->left->par = pnode;
                node->left = pnode;
            }
            pnode->par = node;
            _update(pnode);
            _update(node);
        }

        void _splay(const NodePtr node) {
            while ((!node->is_root()) && (!node->par->is_root())) {
                if ((node->par->par->left == node->par) == (node->par->left == node)) {
                    _rotate(node->par);
                } else {
                    _rotate(node);
                }
                _rotate(node);
            }
            if (!node->is_root()) _rotate(node);
            _propagate(node);
        }

        void _link(const NodePtr c, const NodePtr p) {
            _expose(c);
            _expose(p);
            c->par = p;
            p->right = c;
            _update(p);
        }

        void _cut(const NodePtr c) {
            _expose(c);
            c->left->par = nullptr;
            c->left = nullptr;
            _update(c);
        }

        NodePtr _expose(const NodePtr node) {
            NodePtr pre = node;
            while (node->par) {
                _splay(node);
                node->right = nullptr;
                _update(node);
                if (!node->par) break;
                pre = node->par;
                _splay(node->par);
                node->par->right = node;
                _update(node->par);
            }
            node->right = nullptr;
            _update(node);
            return pre;
        }

        NodePtr _root(NodePtr v) {
            _expose(v);
            _propagate(v);
            while (v->left) {
                v = v->left;
                _propagate(v);
            }
            _splay(v);
            return v;
        }

        void _evert(NodePtr v) {
            _expose(v);
            _apply_rev(v);
            _propagate(v);
        }

      public:
        LinkCutTree() {}

        LinkCutTree(int n) {
            pool.resize(n);
            for (int i = 0; i < n; ++i) {
                pool[i] = new Node(i, e(), id());
            }
        }

        int expose(int v) {
            return _expose(pool[v])->index;
        }

        int lca(int u, int v, int root=-1) {
            if (root != -1) evert(root);
            expose(u);
            return expose(v);
        }

        void link(int c, int p) {
            _link(pool[c], pool[p]);
        }

        void cut(int c) {
            _cut(pool[c]);
        }

        int root(int v) {
            return _root(pool[v])->index;
        }

        bool same(int u, int v) {
            return root(u) == root(v);
        }

        void evert(int v) {
            _evert(pool[v]);
        }

        T path_prod(int u, int v) {
            evert(u);
            expose(v);
            return pool[v]->data;
        }

        void path_apply(int u, int v, F f) {
            evert(u);
            expose(v);
            _apply_f(pool[v], f);
            _propagate(pool[v]);
        }

        bool merge(int u, int v) {
            if (same(u, v)) return false;
            evert(u);
            link(u, v);
            return true;
        }

        void split(int u, int v) {
            evert(u);
            cut(v);
        }

        T get(int k) {
            _splay(pool[k]);
            return pool[k]->key;
        }

        void set(int k, T v) {
            NodePtr node = pool[k];
            _splay(node);
            node->key = v;
            _update(node);
        }

        int path_kth_elm(int s, int t, int k) {
            evert(s);
            expose(t);
            NodePtr node = pool[t];
            if (node->size <= k) return -1;
            while (true) {
                _propagate(node);
                t = node->left? node->left->size: 0;
                if (t == k) {
                    _splay(node);
                    return node->index;
                }
                if (t > k) {
                    node = node->left;
                } else {
                    node = node->right;
                    k -= t + 1;
                }
            }
        }
    };
}  // namespace titan23
