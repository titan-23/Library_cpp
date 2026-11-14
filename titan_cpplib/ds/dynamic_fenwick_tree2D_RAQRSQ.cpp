/// https://github.com/titan-23/Library_cpp/blob/main/titan_cpplib/ds/dynamic_fenwick_tree2D_RAQRSQ.cpp
#pragma once

#include "titan_cpplib/ds/dynamic_fenwick_tree2D.cpp"
#include <cassert>
using namespace std;

namespace titan23 {

/// @brief 矩形加算と矩形和を扱う動的2次元木 / ノードは必要な分だけ作る
/// x*h*w を W で、h*w を T で計算するため、それぞれの型の範囲に収めること
template<typename T, typename W>
struct DynamicFenwicktree2D_RAQRSQ {
    /// @brief 加えた x、x*w、x*h、x*h*w のそれぞれの和を持つノードの値
    struct NodeValue {
        W d0 = 0;
        W d1 = 0;
        W d2 = 0;
        W d3 = 0;

        NodeValue& operator+=(const NodeValue& o) {
            d0 += o.d0;
            d1 += o.d1;
            d2 += o.d2;
            d3 += o.d3;
            return *this;
        }
    };

    /// @brief ノードを配列で持ち、解放した番号を再利用する割り当て器
    template <typename V>
    struct MemoryAllocator {
        struct Node {
            T left, right;
            V val;
        };

        vector<Node> d;
        vector<T> unused;
        T ptr;

        MemoryAllocator() : ptr(1) {
            d.assign(1, {0, 0, V()});
        }

        /// @brief ノード cap 個分の領域を予約する
        void reserve(T cap) {
            d.reserve(cap);
        }

        /// @brief 初期化したノードを確保して番号を返す / 償却 O(1)
        T new_node() {
            if (!unused.empty()) {
                T u = unused.back();
                unused.pop_back();
                d[u] = {0, 0, V()};
                return u;
            }
            if (ptr >= (T)d.size()) {
                d.push_back({0, 0, V()});
            } else {
                d[ptr] = {0, 0, V()};
            }
            return ptr++;
        }

        /// @brief ノード u を再利用できるように戻す / 償却 O(1)
        void deallocate(T u) {
            unused.push_back(u);
        }

        /// @brief 全ノードを未使用に戻す / この割り当て器を使う木は全て無効になる
        void reset() {
            ptr = 1;
            d[0] = {0, 0, V()};
            unused.clear();
        }
    };

    // ノード領域は同じテンプレート引数の全インスタンスで共有する
    static MemoryAllocator<T> out;
    static MemoryAllocator<NodeValue> in;

    T _h, _w;
    T root;

    DynamicFenwicktree2D_RAQRSQ() : _h(0), _w(0), root(0) {}

    /// @brief 縦 h、横 w の空間を管理する木を構築する / O(1)
    DynamicFenwicktree2D_RAQRSQ(T h, T w) : _h(h + 1), _w(w + 1), root(0) {}

    /// @brief 領域 [h1, h2) × [w1, w2) に x を加算する / O(logH logW)
    void add(T h1, T w1, T h2, T w2, W x) {
        assert(0 <= h1 && h1 <= h2 && h2 <= _h);
        assert(0 <= w1 && w1 <= w2 && w2 <= _w);
        add_point(h1, w1, x);
        add_point(h1, w2, -x);
        add_point(h2, w1, -x);
        add_point(h2, w2, x);
    }

    /// @brief 領域 [h1, h2) × [w1, w2) の要素の和を返す / O(logH logW)
    W sum(T h1, T w1, T h2, T w2) const {
        assert(0 <= h1 && h1 <= h2 && h2 <= _h);
        assert(0 <= w1 && w1 <= w2 && w2 <= _w);
        return sum_zero_to(h2, w2) - sum_zero_to(h2, w1) - sum_zero_to(h1, w2) + sum_zero_to(h1, w1);
    }

    /// @brief 木が持つノードを解放し、根を初期化する / O(K)、K はノード数
    void reset() {
        if (root) {
            free_outer(root);
            root = 0;
        }
    }

private:
    void add_point(T h, T w, W x) {
        if (h == _h || w == _w) return;
        if (!root) root = out.new_node();
        T u = root;
        T hl = 0, hr = _h;

        NodeValue xv;
        xv.d0 = x;
        xv.d1 = x * w;
        xv.d2 = x * h;
        xv.d3 = x * h * w;

        while (true) {
            if (!out.d[u].val) {
                out.d[u].val = in.new_node();
            }
            T v = out.d[u].val;
            T wl = 0, wr = _w;

            while (true) {
                in.d[v].val += xv;
                if (wr - wl <= 1) break;
                T wm = wl + (wr - wl) / 2;
                if (w < wm) {
                    if (!in.d[v].left) in.d[v].left = in.new_node();
                    v = in.d[v].left;
                    wr = wm;
                } else {
                    if (!in.d[v].right) in.d[v].right = in.new_node();
                    v = in.d[v].right;
                    wl = wm;
                }
            }

            if (hr - hl <= 1) break;
            T hm = hl + (hr - hl) / 2;
            if (h < hm) {
                if (!out.d[u].left) out.d[u].left = out.new_node();
                u = out.d[u].left;
                hr = hm;
            } else {
                if (!out.d[u].right) out.d[u].right = out.new_node();
                u = out.d[u].right;
                hl = hm;
            }
        }
    }

    NodeValue inner_sum(T u, T wl, T wr, T qwl, T qwr) const {
        if (!u) return NodeValue();
        NodeValue res;
        struct State { T u, l, r; };
        State st[70];
        int top = 0;
        st[top++] = {u, wl, wr};

        while (top > 0) {
            State s = st[--top];
            if (qwl <= s.l && s.r <= qwr) {
                res += in.d[s.u].val;
                continue;
            }
            T wm = s.l + (s.r - s.l) / 2;
            if (qwl < wm && in.d[s.u].left) {
                st[top++] = {in.d[s.u].left, s.l, wm};
            }
            if (wm < qwr && in.d[s.u].right) {
                st[top++] = {in.d[s.u].right, wm, s.r};
            }
        }
        return res;
    }

    NodeValue outer_sum(T u, T hl, T hr, T qhl, T qhr, T qwl, T qwr) const {
        if (!u) return NodeValue();
        NodeValue res;
        struct State { T u, l, r; };
        State st[70];
        int top = 0;
        st[top++] = {u, hl, hr};

        while (top > 0) {
            State s = st[--top];
            if (qhl <= s.l && s.r <= qhr) {
                res += inner_sum(out.d[s.u].val, 0, _w, qwl, qwr);
                continue;
            }
            T hm = s.l + (s.r - s.l) / 2;
            if (qhl < hm && out.d[s.u].left) {
                st[top++] = {out.d[s.u].left, s.l, hm};
            }
            if (hm < qhr && out.d[s.u].right) {
                st[top++] = {out.d[s.u].right, hm, s.r};
            }
        }
        return res;
    }

    W sum_zero_to(T h, T w) const {
        if (h == 0 || w == 0) return 0;
        NodeValue v = outer_sum(root, 0, _h, 0, h, 0, w);
        W res = 0;
        res += h * w * v.d0;
        res -= h * v.d1;
        res -= w * v.d2;
        res += v.d3;
        return res;
    }

    void free_inner(T v) {
        if (!v) return;
        free_inner(in.d[v].left);
        free_inner(in.d[v].right);
        in.deallocate(v);
    }

    void free_outer(T u) {
        if (!u) return;
        free_outer(out.d[u].left);
        free_outer(out.d[u].right);
        if (out.d[u].val) {
            free_inner(out.d[u].val);
        }
        out.deallocate(u);
    }
};

template<typename T, typename W>
typename DynamicFenwicktree2D_RAQRSQ<T, W>::template MemoryAllocator<T> DynamicFenwicktree2D_RAQRSQ<T, W>::out;

template<typename T, typename W>
typename DynamicFenwicktree2D_RAQRSQ<T, W>::template MemoryAllocator<typename DynamicFenwicktree2D_RAQRSQ<T, W>::NodeValue> DynamicFenwicktree2D_RAQRSQ<T, W>::in;

}  // namespace titan23
