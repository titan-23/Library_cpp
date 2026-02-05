#include "titan_cpplib/ds/dynamic_lazy_segment_tree.cpp"

namespace range_add_range_sum {
    using S = long long;
    using F = long long;
    using IndexType = long long;
    const int bit = 30;
    const int msk = (1ll << bit) - 1;

    S op(S a, S b) {
        long long a0 = a >> bit, a1 = a & msk;
        long long b0 = b >> bit, b1 = b & msk;
        return (a0 + b0) << bit | (a1 + b1);
    }

    S e() {
        return 0;
    }

    S mapping(F f, S s) {
        long long s0 = s >> bit, s1 = s & msk;
        return (s0 + f*s1) << bit | s1;
    }

    F composition(F f, F g) {
        return f + g;
    }

    F id() {
        return 0;
    }

    S pow(S s, IndexType k) {
        long long s0 = s >> bit, s1 = s & msk;
        return (s0 * k) << bit | (s1 * k);
    }

    // 初期値は1
    titan23::DynamicLazySegmentTree<IndexType, S, op, e, F, mapping, composition, id, pow> pst(1e9, 1);
    IndexType l, r;
    int res = pst.prod(l, r) >> bit;
}
