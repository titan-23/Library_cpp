/// https://github.com/titan-23/Library_cpp/blob/main/titan_cpplib/ds/lazysegutil.cpp
#pragma once

#include <algorithm>
#include <limits>
#include "titan_cpplib/ds/lazy_segment_tree.cpp"
using namespace std;

namespace titan23 {
namespace lazy_segment_tree {
    template<typename T> T op_min(T a, T b) { return min(a, b); }
    template<typename T> T op_max(T a, T b) { return max(a, b); }
    template<typename T> T e_min() { return numeric_limits<T>::max(); }
    template<typename T> T e_max() { return numeric_limits<T>::lowest(); }

    template<typename T>
    struct Dat {
        T val;
        int size;
    };
    template<typename T> Dat<T> op_sum_size(Dat<T> a, Dat<T> b) { return {a.val + b.val, a.size + b.size}; }
    template<typename T> Dat<T> e_sum_size() { return {0, 0}; }

    template<typename T> T map_add(T f, T s) { return s + f; }
    template<typename T> T comp_add(T f, T g) { return f + g; }
    template<typename T> T id_add() { return 0; }

    template<typename T> Dat<T> map_add_sum(T f, Dat<T> s) { return {s.val + f * s.size, s.size}; }

    template<typename T> const T ID = numeric_limits<T>::max();

    template<typename T> T map_set(T f, T s) { return (f == ID<T>) ? s : f; }
    template<typename T> T comp_set(T f, T g) { return (f == ID<T>) ? g : f; }
    template<typename T> T id_set() { return ID<T>; }

    template<typename T> Dat<T> map_set_sum(T f, Dat<T> s) {
        if (f == ID<T>) return s;
        return Dat<T>{f * s.size, s.size};
    }

    // =========================================================

    // 区間加算 / 区間最小値
    template<typename T>
    using LazySegAddMin = titan23::LazySegmentTree<
        T, op_min<T>, e_min<T>,
        T, map_add<T>, comp_add<T>, id_add<T>
    >;

    // 区間加算 / 区間最大値
    template<typename T>
    using LazySegAddMax = titan23::LazySegmentTree<
        T, op_max<T>, e_max<T>,
        T, map_add<T>, comp_add<T>, id_add<T>
    >;

    // 区間加算 / 区間和
    template<typename T>
    using LazySegAddSum = titan23::LazySegmentTree<
        Dat<T>, op_sum_size<T>, e_sum_size<T>,
        T, map_add_sum<T>, comp_add<T>, id_add<T>
    >;

    // 区間更新 / 区間最小値
    template<typename T>
    using LazySegSetMin = titan23::LazySegmentTree<
        T, op_min<T>, e_min<T>,
        T, map_set<T>, comp_set<T>, id_set<T>
    >;

    // 区間更新 / 区間最大値
    template<typename T>
    using LazySegSetMax = titan23::LazySegmentTree<
        T, op_max<T>, e_max<T>,
        T, map_set<T>, comp_set<T>, id_set<T>
    >;

    // 区間更新 / 区間和
    template<typename T>
    using LazySegSetSum = titan23::LazySegmentTree<
        Dat<T>, op_sum_size<T>, e_sum_size<T>,
        T, map_set_sum<T>, comp_set<T>, id_set<T>
    >;

    // 区間一次関数更新 / 区間和
    // 各要素 x に f(x) = a*x+b を作用させる
    template<typename T>
    struct F_Affine {
        T a, b;
        bool operator==(const F_Affine &rhs) const { return a == rhs.a && b == rhs.b; }
        bool operator!=(const F_Affine &rhs) const { return !(*this == rhs); }
    };
    template<typename T> Dat<T> map_affine_sum(F_Affine<T> f, Dat<T> s) {
        return {f.a * s.val + f.b * s.size, s.size};
    }
    // f(g(x))
    template<typename T> F_Affine<T> comp_affine(F_Affine<T> f, F_Affine<T> g) {
        return {f.a * g.a, f.a * g.b + f.b};
    }
    template<typename T> F_Affine<T> id_affine() { return {1, 0}; }
    // 初期化: `{val, 1}`
    // 更新: `seg.apply(l, r, {a, b});`
    template<typename T>
    using LazySegAffineSum = titan23::LazySegmentTree<
        Dat<T>, op_sum_size<T>, e_sum_size<T>,
        F_Affine<T>, map_affine_sum<T>, comp_affine<T>, id_affine<T>
    >;

    // 区間代入・区間加算 / 区間和・区間最小値・区間最大値
    template<typename T>
    struct Stats {
        T sum, mn, mx;
        int size;
    };
    template<typename T> Stats<T> op_stats(Stats<T> a, Stats<T> b) {
        return {a.sum + b.sum, min(a.mn, b.mn), max(a.mx, b.mx), a.size + b.size};
    }
    template<typename T> Stats<T> e_stats() {
        return {0, numeric_limits<T>::max(), numeric_limits<T>::lowest(), 0};
    }

    // x -> (has_set ? set_val : x) + add_val
    template<typename T>
    struct F_SetAdd {
        bool has_set;
        T set_val, add_val;

        static F_SetAdd set(T x) { return {true, x, 0}; }
        static F_SetAdd add(T x) { return {false, 0, x}; }

        bool operator==(const F_SetAdd &rhs) const {
            return has_set == rhs.has_set && set_val == rhs.set_val && add_val == rhs.add_val;
        }
        bool operator!=(const F_SetAdd &rhs) const { return !(*this == rhs); }
    };
    template<typename T> Stats<T> map_set_add_stats(F_SetAdd<T> f, Stats<T> s) {
        if (s.size == 0) return s;
        if (f.has_set) {
            T val = f.set_val + f.add_val;
            return {val * s.size, val, val, s.size};
        }
        return {s.sum + f.add_val * s.size, s.mn + f.add_val, s.mx + f.add_val, s.size};
    }
    // f(g(x))
    template<typename T> F_SetAdd<T> comp_set_add(F_SetAdd<T> f, F_SetAdd<T> g) {
        if (f.has_set) return f;
        return {g.has_set, g.set_val, g.add_val + f.add_val};
    }
    template<typename T> F_SetAdd<T> id_set_add() { return {false, 0, 0}; }
    // 初期化: `{val, val, val, 1}`
    // 更新: `F_SetAdd<T>::set(x)` / `F_SetAdd<T>::add(x)`
    // 取得: `seg.prod(l, r).sum` / `.mn` / `.mx`
    template<typename T>
    using LazySegSetAddStats = titan23::LazySegmentTree<
        Stats<T>, op_stats<T>, e_stats<T>,
        F_SetAdd<T>, map_set_add_stats<T>, comp_set_add<T>, id_set_add<T>
    >;

    // 区間chmin / 区間最小値
    template<typename T>
    using LazySegChminMin = titan23::LazySegmentTree<
        T, op_min<T>, e_min<T>,
        T, op_min<T>, op_min<T>, e_min<T>
    >;

    // 区間chmin / 区間最大値
    template<typename T>
    using LazySegChminMax = titan23::LazySegmentTree<
        T, op_max<T>, e_max<T>,
        T, op_min<T>, op_min<T>, e_min<T>
    >;

    // 区間chmax / 区間最小値
    template<typename T>
    using LazySegChmaxMin = titan23::LazySegmentTree<
        T, op_min<T>, e_min<T>,
        T, op_max<T>, op_max<T>, e_max<T>
    >;

    // 区間chmax / 区間最大値
    template<typename T>
    using LazySegChmaxMax = titan23::LazySegmentTree<
        T, op_max<T>, e_max<T>,
        T, op_max<T>, op_max<T>, e_max<T>
    >;

    // =========================================================
    // 区間等差数列加算 / 区間和
    // =========================================================

    template<typename T>
    struct S_AP {
        T val;
        int size;
        T idx_sum;
    };
    template<typename T>
    struct F_AP {
        T a, b;
        bool operator==(const F_AP &rhs) const { return a == rhs.a && b == rhs.b; }
        bool operator!=(const F_AP &rhs) const { return !(*this == rhs); }
    };
    template<typename T> S_AP<T> op_ap_sum_size(S_AP<T> l, S_AP<T> r) { return {l.val+r.val, l.size+r.size, l.idx_sum+r.idx_sum}; }
    template<typename T> S_AP<T> e_ap_sum_size() { return {0, 0, 0}; }
    template<typename T> S_AP<T> map_ap_add(F_AP<T> f, S_AP<T> s) { return {s.val+f.a*s.size+f.b*s.idx_sum, s.size, s.idx_sum}; }
    template<typename T> F_AP<T> comp_ap_add(F_AP<T> f, F_AP<T> g) { return {f.a+g.a, f.b+g.b}; }
    template<typename T> F_AP<T> id_ap_add() { return {0, 0}; }
    // 初期化: `{val, 1, index}`
    // クエリ: 初項x、公差dの等差数列を加算->`seg.apply(l, r, {x-l*d, d});`
    template<typename T>
    using LazySegAddAPSum = titan23::LazySegmentTree<
        S_AP<T>, op_ap_sum_size<T>, e_ap_sum_size<T>,
        F_AP<T>, map_ap_add<T>, comp_ap_add<T>, id_ap_add<T>
    >;
} // namespace lazy_segment_tree
} // namespace titan23
