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
        T val, size;
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

    template<typename T> Dat<T> map_set_sum(T f, Dat<T> s) { return (f == ID<T>) ? s : {f * s.size, s.size}; }

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
} // namespace lazy_segment_tree
} // namespace titan23
