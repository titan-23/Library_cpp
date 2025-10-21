#include <bits/stdc++.h>
#include "titan_cpplib/data_structures/fenwick_tree.cpp"
#include "titan_cpplib/data_structures/wordsize_tree_set.cpp"
#include "titan_cpplib/others/print.cpp"
using namespace std;

namespace titan23 {

/// 区間ソートが可能な配列
/// Tには `<` と `==` 演算子が必要
template <class T>
class SortableArray {
private:
    struct Node;
    using NodePtr = Node*;

    int n;
    titan23::FenwickTree<int> fw;
    titan23::WordsizeTreeSet ws;
    vector<NodePtr> nodeptr;
    vector<bool> is_rev;

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
            this->size = pnode->size;
            pnode->update();
            // update();
        }

        void splay() {
            while (par && par->par) {
                ((par->par->left == par) == (par->left == this) ? par : this)->rotate();
                rotate();
            }
            if (par) rotate();
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
        NodePtr res = nullptr, pnode = node;
        while (node) {
            if (node->key < key || node->key == key) {
                res = node;
                node = node->right;
            } else {
                node = node->left;
            }
        }
        pnode->splay();
        if (res) res->splay();
        return res;
    }

    NodePtr left_splay(NodePtr node) {
        while (node->left) node = node->left;
        node->splay();
        return node;
    }

    NodePtr right_splay(NodePtr node) {
        while (node->right) node = node->right;
        node->splay();
        return node;
    }

    // key以下の要素を持つ部分木, keyより大きい要素を持つ部分木 / 返り値のノードはsplay済み
    pair<NodePtr, NodePtr> split(NodePtr node, T key) {
        if (!node) return {nullptr, nullptr};
        node->splay();
        NodePtr nxt = find_splay(node, key);
        if (!nxt) return {nullptr, node}; // nodeはsplay済み
        NodePtr r = nxt->right;
        nxt->right = nullptr;
        if (r) r->par = nullptr;
        nxt->update();
        return {nxt, r};
    }

    // デバッグ用
    void print_node(NodePtr node) {
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

    void make_kyokai(int k) {
        // ..., k, ... で切る
        // ひとつのSplayTreeNodeはソート済みなので、キー順と添え字順が一致している！
        // ので、nodeptr[k]でsplay()すると、左側はkey以下(未満？)かつ添え字もk以下になる！
        if (k == n) return;
        if (ws.contains(k)) return;
        ws.add(k);
        auto [idx, tree_idx] = get_index(k);
        NodePtr p = nodeptr[idx];
        p->splay();
        int tree_size = p->size;
        int split_idx = is_rev[idx] ? tree_size-tree_idx : tree_idx;
        // idx, ..., k, ..., idx+tree_size
        p = kth_splay(p, split_idx);
        NodePtr left = p->left;
        p->left = nullptr;
        left->par = nullptr;
        p->update();
        // assert(left);  // k not in ws より、idxより左に必ず要素がある
        p = left_splay(p);
        if (!is_rev[idx]) { // 普通
            nodeptr[idx] = left;
            nodeptr[k] = p;
        } else {
            nodeptr[idx] = p;
            nodeptr[k] = left;
        }
        fw.set(idx, tree_idx);
        fw.set(k, tree_size-tree_idx);
        is_rev[k] = is_rev[idx];
    }

    // idx: nodeptr[idx]がk番目のノードを含む木
    // tree_idx: nodeptr[idx]内でk番目のノードのインデックス
    pair<int, int> get_index(int k) {
        int idx = fw.bisect_right(k);
        int tree_idx = k - fw.pref(idx);
        return {idx, tree_idx};
    }

    NodePtr merge(NodePtr a, NodePtr b) {
        if (!a) return b;
        if (!b) return a;
        a->splay(); b->splay();

        auto concat = [&] (NodePtr l, NodePtr r) -> NodePtr {
            if (!l) return r;
            if (!r) return l;
            r = left_splay(r);
            r->left = l;
            l->par = r;
            r->update();
            return r;
        };

        NodePtr X = nullptr;
        while (a && b) {
            a = left_splay(a);
            b = left_splay(b);
            if (!(a->key < b->key || a->key == b->key)) swap(a, b);
            auto [left, right] = split(a, b->key);
            a = right;
            X = concat(X, left);
        }
        return concat(concat(X, a), b);
    }

    NodePtr build(int l, int r) {
        auto _build = [&] (auto &&_build, int L, int R) -> NodePtr {
            int mid = (L + R) / 2;
            NodePtr node = nodeptr[mid];
            if (L != mid) {
                node->left = _build(_build, L, mid);
                node->left->par = node;
            }
            if (mid+1 != R) {
                node->right = _build(_build, mid+1, R);
                node->right->par = node;
            }
            node->update();
            return node;
        };
        return _build(_build, l, r);
    }

public:
    SortableArray() {}
    // SortableArray(int n) : n(n), fw(vector<int>(n, 1)), ws(n), ws(n), nodeptr(n), is_rev(n, false) {
    //     ws.fill(n);
    //     for (int i = 0; i < n; ++i) {
    //         nodeptr[i] = new Node(T{});
    //     }
    // }

    // SortableArray(vector<T> a) : n(a.size()), fw(vector<int>(n, 1)), ws(n), nodeptr(n), is_rev(n, false) {
    //     ws.fill(n);
    //     for (int i = 0; i < n; ++i) {
    //         nodeptr[i] = new Node(a[i]);
    //     }
    // }

    SortableArray(int n) : n(n), ws(n), nodeptr(n), is_rev(n, false) {
        for (int i = 0; i < n; ++i) {
            nodeptr[i] = new Node(T{});
        }
        vector<int> fw_init(n, 0); fw_init[0] = n;
        fw = titan23::FenwickTree<int>(fw_init);
        ws.add(0);
        nodeptr[0] = build(0, n);
        for (int i = 1; i < n; ++i) nodeptr[i] = nullptr;
    }

    SortableArray(vector<T> a) : n(a.size()), ws(n), nodeptr(n), is_rev(n, false) {
        ws.fill(n);
        for (int i = 0; i < n; ++i) {
            nodeptr[i] = new Node(a[i]);
        }

        vector<int> fw_init(n, 1);
        int i = 0;
        while (i < n) {
            { // 昇順
                int p = i;
                i++;
                while (i < n && (a[i] < a[i] || a[i-1] == a[i])) ++i;
                nodeptr[p] = build(p, i);
                for (int j = p+1; j < i; ++j) {
                    fw_init[j] = 0;
                    nodeptr[j] = nullptr;
                    ws.remove(j);
                }
                fw_init[p] = i-p;
            }
            if (i < n) { // 降順
                int p = i;
                i++;
                while (i < n && (!(a[i-1] < a[i]))) ++i;
                reverse(nodeptr.begin()+p, nodeptr.begin()+i);
                nodeptr[p] = build(p, i);
                for (int j = p+1; j < i; ++j) {
                    fw_init[j] = 0;
                    nodeptr[j] = nullptr;
                    ws.remove(j);
                }
                is_rev[p] = true;
                fw_init[p] = i-p;
            }
        }
        fw = titan23::FenwickTree<int>(fw_init);
    }

    T get(int k) {
        assert(0 <= k && k < n);
        make_kyokai(k);
        make_kyokai(k+1);
        auto [idx, tree_idx] = get_index(k);
        auto ptr = nodeptr[idx]; // make_kyokaiよりnodeptr[idx]にはA[idx]が存在する
        return ptr->key;
    }

    void set(int k, T key) {
        assert(0 <= k && k < n);
        make_kyokai(k);
        make_kyokai(k+1);
        auto [idx, tree_idx] = get_index(k);
        auto ptr = nodeptr[idx]; // make_kyokaiよりnodeptr[idx]にはA[idx]が存在する
        ptr->key = key;
        ptr->update();
        is_rev[k] = false;
    }

    void sort(int l, int r, bool reverse=false) {
        assert(0 <= l && l <= r && r <= n);
        if (r - l <= 1) return;
        make_kyokai(l);
        make_kyokai(r);
        NodePtr pre = nodeptr[l];
        int idx = l;
        int s = 0;
        while (1) {
            int gt = ws.gt(idx);
            // make_kyokaiより、gt=rでとまる(r!=n)
            if (gt == r || gt == -1) break;
            s += fw.get(gt);
            pre = merge(pre, nodeptr[gt]);
            is_rev[gt] = false;
            fw.set(gt, 0);
            nodeptr[gt] = nullptr;
            ws.remove(gt);
            idx = gt;
        }
        pre->splay();
        nodeptr[l] = pre;
        is_rev[l] = reverse;
        fw.add(l, s);
    }

    vector<T> tovector() {
        vector<T> a; a.reserve(n);
        for (int i = 0; i < n; ++i) {
            if (!nodeptr[i]) continue;
            auto p = nodeptr[i];
            auto dfs = [&] (auto &&dfs, NodePtr node) -> void {
                if (!node) return;
                dfs(dfs, node->left);
                a.emplace_back(node->key);
                dfs(dfs, node->right);
            };
            p->splay();
            int l = a.size();
            dfs(dfs, p);
            int r = a.size();
            if (is_rev[i]) reverse(a.begin()+l, a.begin()+r);
        }
        return a;
    }
};
} // namespace titan23
