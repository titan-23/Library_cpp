#include <algorithm>
#include <limits>
#include "titan_cpplib/ds/segment_tree.cpp"
using namespace std;

namespace titan23 {
namespace segment_tree {

template<typename T> T op_max(T s, T t) { return max(s, t); }
template<typename T> T e_max() { return numeric_limits<T>::lowest();; }

template<typename T> T op_min(T s, T t) { return min(s, t); }
template<typename T> T e_min() { return numeric_limits<T>::max();; }

template<typename T> T op_sum(T s, T t) { return s+t; }
template<typename T> T e_sum() { return 0; }

template<typename T> T op_gcd(T s, T t) { return gcd(s, t); }
template<typename T> T e_gcd() { return T(0); }

template<typename T>
using SegGcd = titan23::SegmentTree<T, op_gcd<T>, e_gcd<T>>;

template<typename T>
using SegMax = titan23::SegmentTree<T, op_max<T>, e_max<T>>;

template<typename T>
using SegMin = titan23::SegmentTree<T, op_min<T>, e_min<T>>;

template<typename T>
using SegSum = titan23::SegmentTree<T, op_sum<T>, e_sum<T>>;

template<typename T>
using SegGcd = titan23::SegmentTree<T, op_gcd<T>, e_gcd<T>>;

}
} // namespace titan23
