#include <vector>
#include <cassert>

using namespace std;

namespace titan23 {

template<typename T, typename W>
struct DynamicFenwickTree2D {
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

        void reserve(T cap) {
            d.reserve(cap);
        }

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

        void deallocate(T u) {
            unused.push_back(u);
        }

        void reset() {
            ptr = 1;
            d[0] = {0, 0, V()};
            unused.clear();
        }
    };

    static MemoryAllocator<T> out;
    static MemoryAllocator<W> in;

    T _h, _w;
    T root;

    // デフォルトコンストラクタ / O(1)
    DynamicFenwickTree2D() : _h(0), _w(0), root(0) {}

    // 縦 h、横 w の空間を管理する木を構築する / O(1)
    DynamicFenwickTree2D(T h, T w) : _h(h + 1), _w(w + 1), root(0) {}

    // a[h][w] に x を加算する / O(logH logW)
    void add(T h, T w, W x) {
        assert(0 <= h && h < _h);
        assert(0 <= w && w < _w);
        if (!root) root = out.new_node();
        T u = root;
        T hl = 0, hr = _h;

        while (true) {
            if (!out.d[u].val) {
                out.d[u].val = in.new_node();
            }
            T v = out.d[u].val;
            T wl = 0, wr = _w;

            while (true) {
                in.d[v].val += x;
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

    // a[h][w] を x に変更する / O(logH logW)
    void set(T h, T w, W x) {
        assert(0 <= h && h < _h);
        assert(0 <= w && w < _w);
        add(h, w, x - get(h, w));
    }

    // 領域 [0, h) × [0, w) の要素の和を取得する / O(logH logW)
    W sum(T h, T w) const {
        assert(0 <= h && h <= _h);
        assert(0 <= w && w <= _w);
        return outer_sum(root, 0, _h, 0, h, 0, w);
    }

    // 領域 [h1, h2) × [w1, w2) の要素の和を取得する / O(logH logW)
    W sum(T h1, T w1, T h2, T w2) const {
        assert(0 <= h1 && h1 <= h2 && h2 <= _h);
        assert(0 <= w1 && w1 <= w2 && w2 <= _w);
        return outer_sum(root, 0, _h, h1, h2, w1, w2);
    }

    // a[h][w] の値を取得する / O(logH logW)
    W get(T h, T w) const {
        assert(0 <= h && h < _h);
        assert(0 <= w && w < _w);
        return outer_sum(root, 0, _h, h, h + 1, w, w + 1);
    }

    // 木が保持するノードを開放し、根を初期化する / O(K) ただしKは木のノード数
    void reset() {
        if (root) {
            free_outer(root);
            root = 0;
        }
    }

private:
    W inner_sum(T u, T wl, T wr, T qwl, T qwr) const {
        if (!u) return 0;
        W res = 0;
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

    W outer_sum(T u, T hl, T hr, T qhl, T qhr, T qwl, T qwr) const {
        if (!u) return 0;
        W res = 0;
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
typename DynamicFenwickTree2D<T, W>::template MemoryAllocator<T> DynamicFenwickTree2D<T, W>::out;

template<typename T, typename W>
typename DynamicFenwickTree2D<T, W>::template MemoryAllocator<W> DynamicFenwickTree2D<T, W>::in;

}  // namespace titan23
