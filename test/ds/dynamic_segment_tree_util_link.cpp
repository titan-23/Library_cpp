// Companion translation unit for dynamic_segment_tree_util_test.cpp.
#include "titan_cpplib/ds/dynamic_lazy_segment_tree_util.cpp"
#include "titan_cpplib/ds/dynamic_segment_tree_util.cpp"

long long dynamic_segment_tree_util_other_tu() {
    titan23::dynamic_segment_tree::SegSumInit<long long> plain(10, -2);
    titan23::dynamic_lazy_segment_tree::LazySegSetSum<long long> lazy(10, {3, 1});
    lazy.apply(0, 10, titan23::dynamic_lazy_segment_tree::Assign<long long>::set(-1));
    return plain.all_prod() + lazy.all_prod().sum;
}
