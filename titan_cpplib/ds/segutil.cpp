#pragma once

#include <algorithm>
#include <numeric>
#include <limits>
#include "titan_cpplib/ds/segment_tree.cpp"
using namespace std;

namespace titan23 {
namespace segment_tree {

template<typename T> T op_max(T s, T t) { return max(s, t); }
template<typename T> T e_max() { return numeric_limits<T>::lowest(); }

template<typename T> T op_min(T s, T t) { return min(s, t); }
template<typename T> T e_min() { return numeric_limits<T>::max(); }

template<typename T> T op_sum(T s, T t) { return s+t; }
template<typename T> T e_sum() { return 0; }

template<typename T> T op_gcd(T s, T t) { return gcd(s, t); }
template<typename T> T e_gcd() { return 0; }

// 区間の最大部分配列和(空区間を許す)を管理するモノイド
template<typename T>
struct MaxSubSum {
    T sum; // 区間の総和
    T pre; // 最大接頭辞和
    T suf; // 最大接尾辞和
    T ans; // 最大部分配列和
    MaxSubSum() : sum(0), pre(0), suf(0), ans(0) {}
    MaxSubSum(T x) : sum(x), pre(max(x, T(0))), suf(max(x, T(0))), ans(max(x, T(0))) {}
};

template<typename T> MaxSubSum<T> op_max_sub_sum(MaxSubSum<T> a, MaxSubSum<T> b) {
    MaxSubSum<T> res;
    res.sum = a.sum + b.sum;
    res.pre = max(a.pre, a.sum + b.pre);
    res.suf = max(b.suf, a.suf + b.sum);
    res.ans = max({a.ans, b.ans, a.suf + b.pre});
    return res;
}

template<typename T> MaxSubSum<T> e_max_sub_sum() { return MaxSubSum<T>(); }

template<typename T>
using SegGcd = titan23::SegmentTree<T, op_gcd<T>, e_gcd<T>>;

template<typename T>
using SegMax = titan23::SegmentTree<T, op_max<T>, e_max<T>>;

template<typename T>
using SegMin = titan23::SegmentTree<T, op_min<T>, e_min<T>>;

template<typename T>
using SegSum = titan23::SegmentTree<T, op_sum<T>, e_sum<T>>;

// prod(l, r).ans が区間 [l, r) の最大部分配列和
template<typename T>
using SegMaxSubSum = titan23::SegmentTree<MaxSubSum<T>, op_max_sub_sum<T>, e_max_sub_sum<T>>;
}
} // namespace titan23
