#include "titan_cpplib/data_structures/segment_tree.cpp"
#include "titan_cpplib/data_structures/fenwick_tree.cpp"
#include "titan_cpplib/data_structures/wordsize_tree_set.cpp"
#include "titan_cpplib/data_structures/splay_node.cpp"
#include "titan_cpplib/others/print.cpp"

#include <bits/stdc++.h>
using namespace std;

namespace titan23 {

/// 区間ソートが可能なセグ木
/// Tには `<` と `==` 演算子が必要
template <class T, T (*op)(T, T), T (*e)()>
class SortableSegmentTree {
private:
    using BST = titan23::SplayTreeNode<T, op, e>;
    BST stree;
    int n;
    titan23::SegmentTree<T, op, e> seg;
    titan23::FenwickTree<int> fw;
    titan23::WordsizeTreeSet ws;
    vector<typename BST::NodePtr> nodeptr;
    vector<bool> is_rev;

    void make_kyokai(int k) {
        // ..., k, ... で切る
        // ひとつのSplayTreeNodeはソート済みなので、キー順と添え字順が一致している！
        // ので、nodeptr[k]でsplay()すると、左側はkey以下(未満？)かつ添え字もk以下になる！
        if (k == n) return;
        if (ws.contains(k)) return;
        ws.add(k);
        auto [idx, tree_idx] = get_index(k);
        typename BST::NodePtr p = nodeptr[idx];
        p->splay();
        int tree_size = p->size;
        int split_idx = is_rev[idx] ? tree_size-tree_idx : tree_idx;
        // idx, ..., k, ..., idx+tree_size
        p = stree.kth_splay(p, split_idx);
        typename BST::NodePtr left = p->left;
        p->left = nullptr;
        p->update();
        if (left) left->par = nullptr;
        p = p->left_splay();
        if (!is_rev[idx]) { // 普通
            seg.set(idx, left ? left->data : e());
            seg.set(k, p->data);
            nodeptr[idx] = left;
            nodeptr[k] = p;
        } else {
            seg.set(idx, p->rdata);
            seg.set(k, left ? left->rdata : e());
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

    typename BST::NodePtr merge(typename BST::NodePtr a, typename BST::NodePtr b) {
        if (!a) return b;
        if (!b) return a;
        a->splay();
        b->splay();

        auto concat = [&] (typename BST::NodePtr l, typename BST::NodePtr r) -> typename BST::NodePtr {
            if (!l) return r;
            if (!r) return l;
            // 小さい方をsplayする
            if (l->size < r->size) {
                l = l->right_splay();
                l->right = r;
                r->par = l;
                l->update();
                return l;
            } else {
                r = r->left_splay();
                r->left = l;
                l->par = r;
                r->update();
                return r;
            }
        };

        typename BST::NodePtr X = nullptr;
        while (a && b) {
            a = a->left_splay();
            b = b->left_splay();
            if (!(a->key < b->key || a->key == b->key)) swap(a, b);
            auto [left, right] = stree.split(a, b->key);
            a = right;
            X = concat(X, left);
        }
        if (a) X = concat(X, a);
        if (b) X = concat(X, b);
        return X;
    }

public:
    SortableSegmentTree() {}
    SortableSegmentTree(int n) : n(n), seg(n), fw(vector<int>(n, 1)), ws(n), ws(n), nodeptr(n), is_rev(n, false) {
        ws.fill(n);
        for (int i = 0; i < n; ++i) {
            nodeptr[i] = new typename BST::Node(e());
        }
    }

    SortableSegmentTree(vector<T> a) : n(a.size()), seg(a), fw(vector<int>(n, 1)), ws(n), nodeptr(n), is_rev(n, false) {
        ws.fill(n);
        for (int i = 0; i < n; ++i) {
            nodeptr[i] = new typename BST::Node(a[i]);
        }
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
        seg.set(k, ptr->data);
        is_rev[k] = false;
    }

    void sort(int l, int r, bool reverse=false) {
        assert(0 <= l && l <= r && r <= n);
        if (l == r) return;
        make_kyokai(l);
        make_kyokai(r);
        typename BST::NodePtr pre = nodeptr[l];
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
            seg.set(gt, e());
            ws.remove(gt);
            idx = gt;
        }
        pre->splay();
        seg.set(l, reverse ? pre->rdata : pre->data);
        nodeptr[l] = pre;
        is_rev[l] = reverse;
        fw.add(l, s);
    }

    T prod(int l, int r) {
        assert(0 <= l && l <= r && r <= n);
        make_kyokai(l);
        make_kyokai(r);
        return seg.prod(l, r);
    }

    vector<T> tovector() {
        vector<T> a; a.reserve(n);
        for (int i = 0; i < n; ++i) {
            if (!nodeptr[i]) continue;
            auto p = nodeptr[i];
            auto dfs = [&] (auto &&dfs, typename BST::NodePtr node) -> void {
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
