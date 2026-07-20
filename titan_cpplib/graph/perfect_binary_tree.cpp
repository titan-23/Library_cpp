#pragma once

#include <vector>
#include <cassert>
#include <algorithm>
#include <limits>
#include <type_traits>
using namespace std;

// PerfectBinaryTree / (T: 符号付き整数)
namespace titan23 {

// 1-indexedの完全二分木クラス

template<typename T=long long>
class PerfectBinaryTree {
private:
    int bit_length(T x) {
        if (x == 0) return 0;
        using U = make_unsigned_t<T>;
        U y = (U)x;
        if constexpr (is_same_v<U, unsigned __int128>) {
            unsigned long long hi = (unsigned long long)(y >> 64);
            if (hi) return 128 - __builtin_clzll(hi);
        }
        return 64 - __builtin_clzll((unsigned long long)y);
    }

public:
    PerfectBinaryTree() {}

    // 根を返す
    T root() {
        return 1;
    }

    // 親を返す
    T par(T u) {
        assert(1 <= u && u <= numeric_limits<T>::max());
        return u > 1 ? (u >> 1) : -1;
    }

    // 左の子を返す
    T child_left(T u) {
        assert(1 <= u && u <= numeric_limits<T>::max() / 2);
        return u << 1;
    }

    // 右の子を返す
    T child_right(T u) {
        assert(1 <= u && u <= numeric_limits<T>::max() / 2);
        return u << 1 | 1;
    }

    // 左右の子をタプルで返す
    pair<T, T> children(T u) {
        assert(1 <= u && u <= numeric_limits<T>::max() / 2);
        return {child_left(u), child_right(u)};
    }

    // 兄弟を返す
    T sibling(T u) {
        assert(2 <= u && u <= numeric_limits<T>::max());
        return u ^ 1;
    }

    // 深さを返す / :math:`O(1)`
    T dep(T u) {
        assert(1 <= u && u <= numeric_limits<T>::max());
        return bit_length(u);
    }

    // k個上の祖先を返す
    // k == 0 のとき、u自身を返す
    T la(T u, T k) {
        return u >> k;
    }

    // uがvの祖先かを返す
    // u == v のとき true を返す
    bool is_ancestor(T u, T v) {
        assert(1 <= u && u <= numeric_limits<T>::max());
        assert(1 <= v && v <= numeric_limits<T>::max());
        return dep(u) <= dep(v) && la(v, dep(v) - dep(u)) == u;
    }

    // lcaを返す
    T lca(T u, T v) {
        assert(1 <= u && u <= numeric_limits<T>::max());
        assert(1 <= v && v <= numeric_limits<T>::max());
        if (dep(u) > dep(v)) {
            swap(u, v);
        }
        v = la(v, dep(v) - dep(u));
        return u >> bit_length(u ^ v);
    }

    // 距離を返す
    T dist(T u, T v) {
        assert(1 <= u && u <= numeric_limits<T>::max());
        assert(1 <= v && v <= numeric_limits<T>::max());
        return dep(u) + dep(v) - 2*dep(lca(u, v));
    }

    // uからvへのパス上のk番目の頂点を返す
    // k == 0 のとき u、k == dist(u, v) のとき v を返す
    T kth(T u, T v, T k) {
        assert(1 <= u && u <= numeric_limits<T>::max());
        assert(1 <= v && v <= numeric_limits<T>::max());
        T l = lca(u, v);
        T du = dep(u) - dep(l);
        assert(0 <= k && k <= du + dep(v) - dep(l));
        if (k <= du) {
            return la(u, k);
        }
        return la(v, du + dep(v) - dep(l) - k);
    }

    // uからvへのパスをリストで返す
    vector<T> get_path(T u, T v) {
        assert(1 <= u && u <= numeric_limits<T>::max());
        assert(1 <= v && v <= numeric_limits<T>::max());
        T l = lca(u, v);
        auto get = [&] (T x) -> vector<T> {
            vector<T> a;
            while (x != l) {
                a.push_back(x);
                x = par(x);
            }
            return a;
        };
        vector<T> res = get(u);
        res.push_back(l);
        vector<T> r = get(v);
        reverse(r.begin(), r.end());
        for (T k : r) {
            res.push_back(k);
        }
        return res;
    }
};
} // namespace titan23
