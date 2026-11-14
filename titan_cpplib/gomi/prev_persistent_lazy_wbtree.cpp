/// https://github.com/titan-23/Library_cpp/blob/main/titan_cpplib/gomi/prev_persistent_lazy_wbtree.cpp
#pragma once

#include <iostream>
#include <vector>
#include <cmath>
#include <cassert>
#include <stack>
#include <memory>
#include "titan_cpplib/others/print.cpp"
using namespace std;

namespace titan23 {

/// @brief 区間作用、区間の総積、区間反転を扱う永続重み平衡木による列の旧版 / ノード領域 ma は全インスタンスで共有する
template <class SizeType, class T, class F,
        T (*op)(T, T),
        T (*mapping)(F, T),
        F (*composition)(F, F),
        T (*e)(),
        F (*id)()>
class PersistentLazyWBTree {
public:
    /// @brief ノード領域を管理する / 番号 0 は番兵
    struct MemoryAllocator {

        // #pragma pack(push, 1)
        /// @brief 子の番号と部分木サイズを持つ
        struct Node {
            SizeType left, right, size;
            Node() : left(0), right(0), size(0) {}
            Node(SizeType l, SizeType r, SizeType s) : left(l), right(r), size(s) {}
        };
        // #pragma pack(pop)

        // #pragma pack(push, 1)
        /// @brief 値、部分木の総積、遅延中の作用、反転フラグを持つ
        struct Data {
            T key, data; F lazy; int8_t rev;
            Data() : rev(0) {}
            Data(T k, T d, F l, int8_t r) : key(k), data(d), lazy(l), rev(r) {}
        };
        // #pragma pack(pop)

        vector<Node> tree;
        vector<Data> data;
        size_t ptr, cap;

        /// @brief 番兵を持つ領域を初期化する / O(1)
        MemoryAllocator() : ptr(1), cap(1) {
            tree.emplace_back(0, 0, 0);
            data.emplace_back(T{}, T{}, F{}, 0);
        }

        /// @brief ノードを複製して番号を返す / 償却 O(1)
        SizeType copy(SizeType node) {
            SizeType idx = new_node(data[node].key, data[node].lazy);
            tree[idx] = {tree[node].left, tree[node].right, tree[node].size};
            data[idx].data = data[node].data;
            data[idx].rev = data[node].rev;
            return idx;
        }

        /// @brief 値 key と作用 f を持つノードを作成して番号を返す / 償却 O(1)
        SizeType new_node(const T key, const F f) {
            if (tree.size() > ptr) {
                tree[ptr] = {0, 0, 1};
                data[ptr] = {key, key, f, 0};
            } else {
                tree.emplace_back(0, 0, 1);
                data.emplace_back(key, key, f, 0);
                if (tree.size() >= cap) {
                    cap++;
                }
            }
            ptr++;
            return ptr - 1;
        }

        /// @brief n ノード分の容量を確保する
        void reserve(SizeType n) {
            tree.reserve(n);
            data.reserve(n);
            cap += n;
        }

        /// @brief 確保済み領域を再利用できる状態に戻す / O(1)
        /// @note この領域を使う既存の木は無効になる
        void reset() {
            ptr = 1;
        }

        /// @brief 確保済み領域がほぼ埋まっているか判定する / O(1)
        bool almost_full() const {
            return (double)ptr > max(1.0, cap*0.99);
        }
    };

    static MemoryAllocator ma;

private:
    using PLTM = PersistentLazyWBTree<SizeType, T, F, op, mapping, composition, e, id>;
    static constexpr int DELTA = 3;
    static constexpr int GAMMA = 2;
    SizeType root;

    inline SizeType weight_right(SizeType node) const {
        return ma.tree[ma.tree[node].right].size + 1;
    }

    inline SizeType weight_left(SizeType node) const {
        return ma.tree[ma.tree[node].left].size + 1;
    }

    inline SizeType weight(SizeType node) const {
        return ma.tree[node].size + 1;
    }

    void update(SizeType node) {
        ma.tree[node].size = 1 + ma.tree[ma.tree[node].left].size + ma.tree[ma.tree[node].right].size;
        ma.data[node].data = ma.data[node].key;
        if (ma.tree[node].left) ma.data[node].data = op(ma.data[ma.tree[node].left].data, ma.data[node].key);
        if (ma.tree[node].right) ma.data[node].data = op(ma.data[node].data, ma.data[ma.tree[node].right].data);
    }

    void push(SizeType node, F f) {
        ma.data[node].key = mapping(f, ma.data[node].key);
        ma.data[node].data = mapping(f, ma.data[node].data);
        ma.data[node].lazy = composition(f, ma.data[node].lazy);
    }

    void propagate(SizeType node) {
        if (ma.data[node].rev) {
            SizeType l = ma.tree[node].left ? ma.copy(ma.tree[node].left) : 0;
            SizeType r = ma.tree[node].right ? ma.copy(ma.tree[node].right) : 0;
            ma.tree[node].left = r;
            ma.tree[node].right = l;
            if (ma.tree[node].left) ma.data[ma.tree[node].left].rev ^= 1;
            if (ma.tree[node].right) ma.data[ma.tree[node].right].rev ^= 1;
            ma.data[node].rev = 0;
        }
        if (ma.data[node].lazy != id()) {
            if (ma.tree[node].left) {
                ma.tree[node].left = ma.copy(ma.tree[node].left);
                push(ma.tree[node].left, ma.data[node].lazy);
            }
            if (ma.tree[node].right) {
                ma.tree[node].right = ma.copy(ma.tree[node].right);
                push(ma.tree[node].right, ma.data[node].lazy);
            }
            ma.data[node].lazy = id();
        }
    }

    void balance_check(SizeType node) const {
        if (!(weight_left(node)*DELTA >= weight_right(node))) {
            assert(false);
        }
        if (!(weight_right(node) * DELTA >= weight_left(node))) {
            assert(false);
        }
    }

    void _build(vector<T> const &a) {
        auto build = [&] (auto &&build, SizeType l, SizeType r) -> SizeType {
            SizeType mid = (l + r) >> 1;
            SizeType node = ma.new_node(a[mid], id());
            if (l != mid) ma.tree[node].left = build(build, l, mid);
            if (mid+1 != r) ma.tree[node].right = build(build, mid+1, r);
            update(node);
            return node;
        };
        if (a.empty()) {
            root = 0;
            return;
        }
        root = build(build, 0, (SizeType)a.size());
    }

    SizeType _rotate_right(SizeType node) {
        SizeType u = ma.copy(ma.tree[node].left);
        ma.tree[node].left = ma.tree[u].right;
        ma.tree[u].right = node;
        update(node);
        update(u);
        return u;
    }

    SizeType _rotate_left(SizeType node) {
        SizeType u = ma.copy(ma.tree[node].right);
        ma.tree[node].right = ma.tree[u].left;
        ma.tree[u].left = node;
        update(node);
        update(u);
        return u;
    }

    SizeType _balance_left(SizeType node) {
        propagate(ma.tree[node].right);
        SizeType u = ma.tree[node].right;
        if (weight_left(ma.tree[node].right) >= weight_right(ma.tree[node].right) * GAMMA) {
            propagate(ma.tree[u].left);
            ma.tree[node].right = _rotate_right(u);
        }
        u = _rotate_left(node);
        return u;
    }

    SizeType _balance_right(SizeType node) {
        propagate(ma.tree[node].left);
        SizeType u = ma.tree[node].left;
        if (weight_right(ma.tree[node].left) >= weight_left(ma.tree[node].left) * GAMMA) {
            propagate(ma.tree[u].right);
            ma.tree[node].left = _rotate_left(u);
        }
        u = _rotate_right(node);
        return u;
    }

    SizeType _merge_with_root(SizeType l, SizeType root, SizeType r) {
        if (weight(r) * DELTA < weight(l)) {
            propagate(l);
            l = ma.copy(l);
            ma.tree[l].right = _merge_with_root(ma.tree[l].right, root, r);
            update(l);
            if (weight(ma.tree[l].left) * DELTA < weight(ma.tree[l].right)) {
                return _balance_left(l);
            }
            return l;
        } else if (weight(l) * DELTA < weight(r)) {
            propagate(r);
            r = ma.copy(r);
            ma.tree[r].left = _merge_with_root(l, root, ma.tree[r].left);
            update(r);
            if (weight(ma.tree[r].right) * DELTA < weight(ma.tree[r].left)) {
                return _balance_right(r);
            }
            return r;
        }
        root = ma.copy(root);
        ma.tree[root].left = l;
        ma.tree[root].right = r;
        update(root);
        return root;
    }

    pair<SizeType, SizeType> _pop_right(SizeType node) {
        return _split_node(node, ma.tree[node].size-1);
    }

    SizeType _merge_node(SizeType l, SizeType r) {
        if ((!l) && (!r)) { return 0; }
        if (!l) { return ma.copy(r); }
        if (!r) { return ma.copy(l); }
        l = ma.copy(l);
        r = ma.copy(r);
        auto [l_, root_] = _pop_right(l);
        return _merge_with_root(l_, root_, r);
    }

    pair<SizeType, SizeType> _split_node(SizeType node, SizeType k) {
        if (!node) { return {0, 0}; }
        propagate(node);
        SizeType lch = ma.tree[node].left, rch = ma.tree[node].right;
        SizeType tmp = lch ? k-ma.tree[lch].size : k;
        if (tmp == 0) {
            return {lch, _merge_with_root(0, node, rch)};
        } else if (tmp < 0) {
            auto [l, r] = _split_node(lch, k);
            return {l, _merge_with_root(r, node, rch)};
        } else {
            auto [l, r] = _split_node(rch, tmp-1);
            return {_merge_with_root(lch, node, l), r};
        }
    }

    SizeType _split_node_left(SizeType node, SizeType k) {
        if (!node) { return 0; }
        propagate(node);
        SizeType lch = ma.tree[node].left, rch = ma.tree[node].right;
        SizeType tmp = lch ? k-ma.tree[lch].size : k;
        if (tmp == 0) {
            return lch;
        } else if (tmp < 0) {
            SizeType l = _split_node_left(lch, k);
            return l;
        } else {
            SizeType l = _split_node_left(rch, tmp-1);
            return _merge_with_root(lch, node, l);
        }
    }

    SizeType _split_node_right(SizeType node, SizeType k) {
        if (!node) { return 0; }
        propagate(node);
        SizeType lch = ma.tree[node].left, rch = ma.tree[node].right;
        SizeType tmp = lch ? k-ma.tree[lch].size : k;
        if (tmp == 0) {
            return _merge_with_root(0, node, rch);
        } else if (tmp < 0) {
            SizeType r = _split_node_right(lch, k);
            return _merge_with_root(r, node, rch);
        } else {
            SizeType r = _split_node_right(rch, tmp-1);
            return r;
        }
    }

    PLTM _new(SizeType root) {
        return PLTM(root);
    }

    PersistentLazyWBTree(SizeType root) : root(root) {}

  public:
    /// @brief 空の列を構築する / O(1)
    PersistentLazyWBTree() : root(0) {}

    /// @brief 配列 a から構築する / O(N)
    PersistentLazyWBTree(const vector<T> &a) { _build(a); }

    /// @brief 末尾に other を連結した列を返す
    PLTM merge(PLTM other) {
        SizeType root = _merge_node(this->root, other.root);
        return _new(root);
    }

    /// @brief 先頭 k 要素と残りに分割して返す
    pair<PLTM, PLTM> split(SizeType k) {
        auto [l, r] = _split_node(this->root, k);
        return {_new(l), _new(r)};
    }

    /// @brief 先頭 k 要素からなる列を返す
    PLTM split_left(SizeType k) {
        SizeType l = _split_node_left(this->root, k);
        return _new(l);
    }

    /// @brief 先頭 k 要素を除いた列を返す
    PLTM split_right(SizeType k) {
        SizeType r = _split_node_right(this->root, k);
        return _new(r);
    }

    /// @brief 区間 [l, r) からなる列を返す
    PLTM split_range(SizeType l, SizeType r) {
        assert(0 <= l && l <= r && r <= len());
        return split_left(r).split_right(l);
    }

    /// @brief 区間 [l, r) に作用 f を適用した列を返す
    PLTM apply(SizeType l, SizeType r, F f) {
        assert(0 <= l && l <= r && r <= len());
        if (l == r) return _new(ma.copy(root));
        auto dfs = [&] (auto &&dfs, SizeType node, SizeType left, SizeType right) -> SizeType {
            if (right <= l || r <= left) return node;
            propagate(node);
            SizeType nnode = ma.copy(node);
            if (l <= left && right < r) {
                push(nnode, f);
                return nnode;
            }
            SizeType lsize = ma.tree[ma.tree[nnode].left].size;
            if (ma.tree[nnode].left) ma.tree[nnode].left = dfs(dfs, ma.tree[nnode].left, left, left+lsize);
            if (l <= left+lsize && left+lsize < r) ma.data[nnode].key = mapping(f, ma.data[nnode].key);
            if (ma.tree[nnode].right) ma.tree[nnode].right = dfs(dfs, ma.tree[nnode].right, left+lsize+1, right);
            update(nnode);
            return nnode;
        };
        return _new(dfs(dfs, root, 0, len()));
    }

    /// @brief 区間 [l, r) の総積を返す
    T prod(SizeType l, SizeType r) {
        assert(0 <= l && l <= r && r <= len());
        if (l == r) return e();
        auto dfs = [&] (auto &&dfs, SizeType node, SizeType left, SizeType right) -> T {
            if (right <= l || r <= left) return e();
            if (l <= left && right < r) return ma.data[node].data;
            propagate(node);
            SizeType lsize = ma.tree[ma.tree[node].left].size;
            T res = e();
            if (ma.tree[node].left) res = dfs(dfs, ma.tree[node].left, left, left+lsize);
            if (l <= left+lsize && left+lsize < r) res = op(res, ma.data[node].key);
            if (ma.tree[node].right) res = op(res, dfs(dfs, ma.tree[node].right, left+lsize+1, right));
            return res;
        };
        return dfs(dfs, root, 0, len());
    }

    /// @brief 位置 k に key を挿入した列を返す
    PLTM insert(SizeType k, T key) {
        assert(0 <= k && k <= len());
        auto [s, t] = _split_node(root, k);
        SizeType new_node = ma.new_node(key, id());
        return _new(_merge_with_root(s, new_node, t));
    }

    /// @brief 位置 k の値を削除した列と削除値を返す
    pair<PLTM, T> pop(SizeType k) {
        assert(0 <= k && k < len());
        auto [s_, t] = _split_node(this->root, k+1);
        auto [s, tmp] = _pop_right(s_);
        T res = ma.data[tmp].key;
        SizeType root = _merge_node(s, t);
        return {_new(root), res};
    }

    /// @brief 区間 [l, r) を反転した列を返す
    PLTM reverse(SizeType l, SizeType r) {
        assert(0 <= l && l <= r && r <= len());
        if (l >= r) return _new(ma.copy(root));
        auto [s_, t] = _split_node(root, r);
        auto [u, s] = _split_node(s_, l);
        ma.data[s].rev ^= 1;
        SizeType root = _merge_node(_merge_node(u, s), t);
        return _new(root);
    }

    /// @brief 全要素を配列にして返す / O(N)
    vector<T> tovector() {
        SizeType node = root;
        stack<SizeType> s;
        vector<T> a;
        a.reserve(len());
        while (!s.empty() || node) {
            if (node) {
                propagate(node);
                s.emplace(node);
                node = ma.tree[node].left;
            } else {
                node = s.top(); s.pop();
                a.emplace_back(ma.data[node].key);
                node = ma.tree[node].right;
            }
        }
        return a;
    }

    /// @brief 列のコピーを返す
    PLTM copy() const {
        return _new(copy(root));
    }

    /// @brief 位置 k の値を v に変更した列を返す
    PLTM set(SizeType k, T v) {
        assert(0 <= k && k < len());
        SizeType node = ma.copy(root);
        SizeType root = node;
        SizeType pnode = 0;
        int d = 0;
        stack<SizeType> path = {node};
        while (1) {
            propagate(node);
            SizeType t = ma.tree[ma.tree[node].left].size;
            if (t == k) {
                node = ma.copy(node);
                ma.data[node].key = v;
                path.emplace(node);
                if (d) ma.tree[pnode].left = node;
                else ma.tree[pnode].right = node;
                while (!path.empty()) {
                    update(path.top());
                    path.pop();
                }
                return _new(root);
            }
            pnode = node;
            if (t < k) {
                k -= t + 1;
                node = ma.copy(ma.tree[node].right);
                d = 0;
            } else {
                d = 1;
                node = ma.copy(ma.tree[node].left);
            }
            path.emplace_back(node);
            if (d) ma.tree[pnode].left = node;
            else ma.tree[pnode].right = node;
        }
    }

    /// @brief 位置 k の値を返す
    T get(SizeType k) {
        assert(0 <= k && k < len());
        SizeType node = root;
        while (1) {
            propagate(node);
            SizeType t = ma.tree[ma.tree[node].left].size;
            if (t == k) {
                return ma.data[node].key;
            }
            if (t < k) {
                k -= t + 1;
                node = ma.tree[node].right;
            } else {
                node = ma.tree[node].left;
            }
        }
    }

    /// @brief 全要素を標準出力に出力する / O(N)
    void print() {
        vector<T> a = tovector();
        cout << "[";
        for (SizeType i = 0; i < (SizeType)a.size()-1; ++i) {
            cout << a[i] << ", ";
        }
        if (!a.empty()) cout << a.back();
        cout << "]" << endl;
    }

    /// @brief 要素数を返す / O(1)
    SizeType len() const {
        return ma.tree[root].size;
    }

    /// @brief 部分木サイズと平衡条件を検査する / O(N)
    void check() const {
        auto rec = [&] (auto &&rec, SizeType node) -> pair<SizeType, SizeType> {
            SizeType ls = 0, rs = 0;
            SizeType height = 0;
            SizeType h;
            if (ma.tree[node].left) {
                pair<SizeType, SizeType> res = rec(rec, ma.tree[node].left);
                ls = res.first;
                h = res.second;
                height = max(height, h);
            }
            if (ma.tree[node].right) {
                pair<SizeType, SizeType> res = rec(rec, ma.tree[node].right);
                rs = res.first;
                h = res.second;
                height = max(height, h);
            }
            SizeType s = ls + rs + 1;
            assert(s == ma.tree[node].size);
            balance_check(node);
            return {s, height+1};
        };
        if (root == 0) return;
        auto [_, h] = rec(rec, root);
    }

    /// @brief 全要素をストリームへ出力する / O(N)
    friend ostream& operator<<(ostream& os, PersistentLazyWBTree<SizeType, T, F, op, mapping, composition, e, id> &tree) {
        vector<T> a = tree.tovector();
        os << "[";
        for (SizeType i = 0; i < (SizeType)a.size()-1; ++i) {
            os << a[i] << ", ";
        }
        if (!a.empty()) os << a.back();
        os << "]";
        return os;
    }

    /// @brief 確保済み領域を再利用して木を構築し直す / O(N)
    /// @note 同じ領域を共有する他の木は無効になる
    static void rebuild(PLTM &tree) {
        PLTM::ma.reset();
        vector<T> a = tree.tovector();
        tree = PLTM(a);
    }
};

template <class SizeType, class T, class F,
        T (*op)(T, T),
        T (*mapping)(F, T),
        F (*composition)(F, F),
        T (*e)(),
        F (*id)()>
typename PersistentLazyWBTree<SizeType, T, F, op, mapping, composition, e, id>::MemoryAllocator PersistentLazyWBTree<SizeType, T, F, op, mapping, composition, e, id>::ma;

}  // namespace titan23
