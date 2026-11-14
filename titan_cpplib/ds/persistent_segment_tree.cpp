/// https://github.com/titan-23/Library_cpp/blob/main/titan_cpplib/ds/persistent_segment_tree.cpp
#pragma once

#include <iostream>
#include <vector>
#include "titan_cpplib/others/print.cpp"
#include "titan_cpplib/ds/fast_stack.cpp"
#include "titan_cpplib/others/bit.cpp"
using namespace std;

namespace titan23 {

/// @brief 永続セグメント木 / 0-indexed
/// 全インスタンスが静的なノード領域 ma を共有する
template<class T, T (*op)(T, T), T (*e)()>
class PersistentSegmentTree {
public:
    /// @brief set が根からの経路を積む作業用スタック
    static FastStack<int> path;

    /// @brief 全ての木が共有するノード領域
    struct MemoryAllocator {

        // #pragma pack(push, 1)
        /// @brief 左右の子の番号を持つノード
        struct Node {
            int left, right;
            Node() : left(0), right(0) {}
            Node(int l, int r) : left(l), right(r) {}
        };
        // #pragma pack(pop)

        vector<Node> tree;
        vector<T> data;
        size_t ptr, cap;

        /// @brief 番号 0 に番兵ノードを置いて初期化する / O(1)
        MemoryAllocator() : ptr(1), cap(1) {
            tree.emplace_back(0, 0);
            data.emplace_back(e());
        }

        /// @brief ノードを複製して番号を返す / 償却 O(1)
        int copy(int node) {
            int idx = new_node(data[node]);
            tree[idx] = {tree[node].left, tree[node].right};
            return idx;
        }

        /// @brief 値 key を持つノードを作成して番号を返す / 償却 O(1)
        int new_node(const T key) {
            if (tree.size() > ptr) {
                tree[ptr] = {0, 0};
                data[ptr] = key;
            } else {
                tree.emplace_back(0, 0);
                data.emplace_back(key);
                if (tree.size() >= cap) {
                    cap++;
                }
            }
            ptr++;
            return ptr - 1;
        }

        /// @brief n ノード分の容量を確保する
        void reserve(int n) {
            tree.reserve(n);
            data.reserve(n);
            cap += n;
        }

        /// @brief 確保済み領域を再利用できる状態に戻す / O(1)
        /// この領域を使う既存の木は無効になる
        void reset() {
            ptr = 1;
        }

        /// @brief 確保済み領域がほぼ埋まっているか判定する / O(1)
        bool almost_full() const {
            return (double)ptr > max(1.0, cap*0.99);
        }
    };

    /// @brief 全インスタンスが共有するノード領域
    static MemoryAllocator ma;

private:
    using PSEG = PersistentSegmentTree<T, op, e>;
    int root;
    int _len;

    void update(int node) {
        ma.data[node] = op(ma.data[ma.tree[node].left], ma.data[ma.tree[node].right]);
    }

    void _build(vector<T> const &a) {
        auto build = [&] (auto &&build, int l, int r) -> int {
            if (r - l == 1) {
                int node = ma.new_node(a[l]);
                return node;
            }
            int mid = (l + r) / 2;
            int node = ma.new_node(e());
            ma.tree[node].left = build(build, l, mid);
            ma.tree[node].right = build(build, mid, r);
            update(node);
            return node;
        };
        _len = a.size();
        if (a.empty()) {
            root = 0;
            return;
        }
        root = build(build, 0, (int)a.size());
    }

    PersistentSegmentTree(int root, int l) : root(root), _len(l) {}

  public:
    /// @brief 空の列を構築する / O(1)
    PersistentSegmentTree() : root(0), _len(0) {}

    /// @brief 列 a から構築する / O(n)
    PersistentSegmentTree(const vector<T> &a) { _build(a); }

    /// @brief 区間 [l, r) の総積を返す / O(log n)
    T prod(int l, int r) {
        assert(0 <= l && l <= r && r <= len());
        if (l == r) return e();
        auto dfs = [&] (auto &&dfs, int node, int left, int right) -> T {
            if (right <= l || r <= left) return e();
            if (l <= left && right <= r) return ma.data[node];
            int mid = (left + right) / 2;
            T res = e();
            if (ma.tree[node].left) {
                res = dfs(dfs, ma.tree[node].left, left, mid);
            }
            if (ma.tree[node].right) {
                res = op(res, dfs(dfs, ma.tree[node].right, mid, right));
            }
            return res;
        };
        return dfs(dfs, root, 0, len());
    }

    /// @brief 全要素を配列にして返す / O(n)
    vector<T> tovector() {
        if (len() == 0) return {};
        vector<T> a(len());
        auto dfs = [&](auto &&dfs, int node, int l, int r) -> void {
            if (r - l == 1) {
                a[l] = ma.data[node];
                return;
            }
            int mid = (l + r) / 2;
            dfs(dfs, ma.tree[node].left, l, mid);
            dfs(dfs, ma.tree[node].right, mid, r);
        };
        dfs(dfs, root, 0, _len);
        return a;
    }

    /// @brief 根だけを複製した木を返す / O(1)
    PSEG copy() {
        return PSEG(ma.copy(root), len());
    }

    /// @brief a[k] を v に変更した木を返す / O(log n)
    PSEG set(int k, T v) {
        assert(0 <= k && k < len());
        int new_root = ma.copy(root);
        if (len() <= 1) {
            ma.data[new_root] = v;
            return PSEG(new_root, len());
        }
        int node = new_root;
        int l = 0, r = len();
        path.clear(); path.emplace(node);
        while (r - l > 1) {
            int pnode = node;
            int mid = (l + r) / 2;
            if (k < mid) {
                node = ma.copy(ma.tree[pnode].left);
                ma.tree[pnode].left = node;
                r = mid;
            } else {
                node = ma.copy(ma.tree[pnode].right);
                ma.tree[pnode].right = node;
                l = mid;
            }
            path.emplace(node);
        }
        ma.data[node] = v;
        path.pop();
        while (!path.empty()) {
            update(path.top());
            path.pop();
        }
        return PSEG(new_root, len());
    }

    /// @brief a[k] を返す / O(log n)
    T get(int k) {
        assert(0 <= k && k < len());
        int node = root;
        int l = 0, r = len();
        while (r - l > 1) {
            int mid = (l + r) / 2;
            if (k < mid) {
                node = ma.tree[node].left;
                r = mid;
            } else {
                node = ma.tree[node].right;
                l = mid;
            }
        }
        return ma.data[node];
    }

    /// @brief 全要素を標準出力に出力する / O(n)
    void print() {
        vector<T> a = tovector();
        cout << "[";
        for (int i = 0; i < (int)a.size()-1; ++i) {
            cout << a[i] << ", ";
        }
        if (!a.empty()) cout << a.back();
        cout << "]" << endl;
    }

    /// @brief from の [l, r) を自身の [l, r) に写した木を返す / 自身は変更しない / O(log n)
    /// from と自身の要素数が等しいことが前提
    PSEG copy_from(PSEG &from, int l, int r) {
        assert(0 <= l && l <= r && r <= len());
        auto dfs = [&] (auto &&dfs, int fr, int to, int left, int right) -> int {
            if (!fr && !to) return fr;
            to = ma.copy(to);
            if (right <= l || r <= left) { return to; }
            if (l <= left && right <= r) { return fr; }
            int mid = (left + right) / 2;
            // data[to] は子を付け替えた後の update で求める
            ma.tree[to].left = dfs(dfs, ma.tree[fr].left, ma.tree[to].left, left, mid);
            ma.tree[to].right = dfs(dfs, ma.tree[fr].right, ma.tree[to].right, mid, right);
            update(to);
            return to;
        };
        int new_root = dfs(dfs, ma.copy(from.root), ma.copy(root), 0, len());
        return PSEG(new_root, len());
    }

    /// @brief 要素数を返す / O(1)
    int len() const {
        return _len;
    }

    /// @brief 全要素をストリームへ出力する / O(n)
    friend ostream& operator<<(ostream& os, PersistentSegmentTree<T, op, e> &tree) {
        vector<T> a = tree.tovector();
        os << "[";
        for (int i = 0; i < (int)a.size()-1; ++i) {
            os << a[i] << ", ";
        }
        if (!a.empty()) os << a.back();
        os << "]";
        return os;
    }

    /// @brief 木を再構築して確保済み領域を再利用する / O(n)
    /// 同じ領域を共有する他の木は無効になる
    static void rebuild(PSEG &tree) {
        PSEG::ma.reset();
        vector<T> a = tree.tovector();
        tree = PSEG(a);
    }
};

template<class T, T (*op)(T, T), T (*e)()>
typename PersistentSegmentTree<T, op, e>::MemoryAllocator PersistentSegmentTree<T, op, e>::ma;

template<class T, T (*op)(T, T), T (*e)()>
FastStack<int> PersistentSegmentTree<T, op, e>::path;

}  // namespace titan23
