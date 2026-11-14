/// https://github.com/titan-23/Library_cpp/blob/main/titan_cpplib/graph/tree/perfect_binary_tree.cpp
#pragma once

#include <vector>
#include <cassert>
#include <algorithm>
#include <limits>
#include "titan_cpplib/others/bit.cpp"
using namespace std;

namespace titan23 {

/// @brief 1-indexed の完全二分木 / T は符号付き整数
template<typename T=long long>
class PerfectBinaryTree {
public:
    PerfectBinaryTree() {}

    /// @brief 根を返す / O(1)
    T root() {
        return 1;
    }

    /// @brief 親を返す / 根なら -1 / O(1)
    T par(T u) {
        assert(1 <= u && u <= numeric_limits<T>::max());
        return u > 1 ? (u >> 1) : -1;
    }

    /// @brief 左の子を返す / O(1)
    T child_left(T u) {
        assert(1 <= u && u <= numeric_limits<T>::max() / 2);
        return u << 1;
    }

    /// @brief 右の子を返す / O(1)
    T child_right(T u) {
        assert(1 <= u && u <= numeric_limits<T>::max() / 2);
        return u << 1 | 1;
    }

    /// @brief 左右の子を pair で返す / O(1)
    pair<T, T> children(T u) {
        assert(1 <= u && u <= numeric_limits<T>::max() / 2);
        return {child_left(u), child_right(u)};
    }

    /// @brief 兄弟を返す / u は根でないこと / O(1)
    T sibling(T u) {
        assert(2 <= u && u <= numeric_limits<T>::max());
        return u ^ 1;
    }

    /// @brief 深さを返す / 根の深さは 1 / O(1)
    T dep(T u) {
        assert(1 <= u && u <= numeric_limits<T>::max());
        return bit_length(u);
    }

    /// @brief k 個上の祖先を返す / k == 0 なら u 自身 / O(1)
    T la(T u, T k) {
        return u >> k;
    }

    /// @brief u が v の祖先かを返す / u == v なら true / O(1)
    bool is_ancestor(T u, T v) {
        assert(1 <= u && u <= numeric_limits<T>::max());
        assert(1 <= v && v <= numeric_limits<T>::max());
        return dep(u) <= dep(v) && la(v, dep(v) - dep(u)) == u;
    }

    /// @brief u と v の LCA を返す / O(1)
    T lca(T u, T v) {
        assert(1 <= u && u <= numeric_limits<T>::max());
        assert(1 <= v && v <= numeric_limits<T>::max());
        if (dep(u) > dep(v)) {
            swap(u, v);
        }
        v = la(v, dep(v) - dep(u));
        return u >> bit_length(u ^ v);
    }

    /// @brief u と v の距離を返す / O(1)
    T dist(T u, T v) {
        assert(1 <= u && u <= numeric_limits<T>::max());
        assert(1 <= v && v <= numeric_limits<T>::max());
        return dep(u) + dep(v) - 2*dep(lca(u, v));
    }

    /// @brief u から v へのパス上の k 番目の頂点を返す / k == 0 なら u、k == dist(u, v) なら v / O(1)
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

    /// @brief u から v へのパスを頂点の列で返す / O(dist(u, v))
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
