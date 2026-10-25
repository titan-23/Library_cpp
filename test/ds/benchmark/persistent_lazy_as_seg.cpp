#pragma once

#include <cstdint>
#include "test/ds/benchmark/baseline/persistent_lazy_wbtree_before_fast.cpp"

namespace titan23::benchmark_detail {

template<class T>
T shared_seg_mapping(std::int64_t action, T value) {
    return {value.sum + action * value.count, value.count};
}

inline std::int64_t shared_seg_composition(std::int64_t f, std::int64_t g) {
    return f + g;
}

inline std::int64_t shared_seg_id() { return 0; }

}

namespace titan23 {

template<class SizeType, class T, T (*op)(T, T), T (*e)()>
using PersistentSegWBTree = PersistentLazyWBTree<
    SizeType, T, std::int64_t, op, benchmark_detail::shared_seg_mapping<T>,
    benchmark_detail::shared_seg_composition, e, benchmark_detail::shared_seg_id>;

}
