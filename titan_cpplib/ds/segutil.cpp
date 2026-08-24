/// https://github.com/titan-23/Library_cpp/blob/main/titan_cpplib/ds/segutil.cpp
#pragma once

#include <algorithm>
#include <numeric>
#include <limits>
#include <utility>
#include <tuple>
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

template<typename T> T op_xor(T s, T t) { return s ^ t; }
template<typename T> T e_xor() { return 0; }

template<typename T> T op_or(T s, T t) { return s | t; }
template<typename T> T e_or() { return 0; }

template<typename T> T op_and(T s, T t) { return s & t; }
template<typename T> T e_and() { return ~T(0); }

template<typename T> pair<T, int> op_min_idx(pair<T, int> s, pair<T, int> t) { return min(s, t); }
template<typename T> pair<T, int> e_min_idx() { return {numeric_limits<T>::max(), -1}; }

template<typename T> pair<T, int> op_max_idx(pair<T, int> s, pair<T, int> t) { return tie(t.first, s.second) < tie(s.first, t.second) ? s : t; }
template<typename T> pair<T, int> e_max_idx() { return {numeric_limits<T>::lowest(), -1}; }

// (値, 個数)
template<typename T> pair<T, int> op_min_cnt(pair<T, int> s, pair<T, int> t) {
    if (s.first != t.first) return s.first < t.first ? s : t;
    return {s.first, s.second + t.second};
}
template<typename T> pair<T, int> e_min_cnt() { return {numeric_limits<T>::max(), 0}; }

template<typename T> pair<T, int> op_max_cnt(pair<T, int> s, pair<T, int> t) {
    if (s.first != t.first) return s.first > t.first ? s : t;
    return {s.first, s.second + t.second};
}
template<typename T> pair<T, int> e_max_cnt() { return {numeric_limits<T>::lowest(), 0}; }

// 一次関数 f(x) = ax + b の合成
template<typename T>
struct Affine {
    T a, b;
    T eval(T x) const { return a * x + b; }
};
template<typename T> Affine<T> op_affine(Affine<T> s, Affine<T> t) {
    return {t.a * s.a, t.a * s.b + t.b};
}
template<typename T> Affine<T> e_affine() { return {T(1), T(0)}; }

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

// 区間 gcd
template<typename T>
using SegGcd = titan23::SegmentTree<T, op_gcd<T>, e_gcd<T>>;

// 区間 max
template<typename T>
using SegMax = titan23::SegmentTree<T, op_max<T>, e_max<T>>;

// 区間 min
template<typename T>
using SegMin = titan23::SegmentTree<T, op_min<T>, e_min<T>>;

// 区間和
template<typename T>
using SegSum = titan23::SegmentTree<T, op_sum<T>, e_sum<T>>;

// prod(l, r).ans が区間 [l, r) の最大部分配列和
template<typename T>
using SegMaxSubSum = titan23::SegmentTree<MaxSubSum<T>, op_max_sub_sum<T>, e_max_sub_sum<T>>;

// 区間 xor
template<typename T>
using SegXor = titan23::SegmentTree<T, op_xor<T>, e_xor<T>>;

// 区間 or
template<typename T>
using SegOr = titan23::SegmentTree<T, op_or<T>, e_or<T>>;

// 区間 and
template<typename T>
using SegAnd = titan23::SegmentTree<T, op_and<T>, e_and<T>>;

// (区間min, その添字) を返す / 要素は {値, 添字} で持つ
template<typename T>
using SegMinIdx = titan23::SegmentTree<pair<T, int>, op_min_idx<T>, e_min_idx<T>>;

// (区間max, その添字) を返す / 要素は {値, 添字} で持つ
template<typename T>
using SegMaxIdx = titan23::SegmentTree<pair<T, int>, op_max_idx<T>, e_max_idx<T>>;

// (区間min, その個数) を返す / 要素は {値, 1} で持つ
template<typename T>
using SegMinCnt = titan23::SegmentTree<pair<T, int>, op_min_cnt<T>, e_min_cnt<T>>;

// (区間max, その個数) を返す / 要素は {値, 1} で持つ
template<typename T>
using SegMaxCnt = titan23::SegmentTree<pair<T, int>, op_max_cnt<T>, e_max_cnt<T>>;

// 一次関数合成 / prod(l, r).eval(x) が f_{r-1}(...f_l(x)...) を返す
template<typename T>
using SegAffine = titan23::SegmentTree<Affine<T>, op_affine<T>, e_affine<T>>;
}
} // namespace titan23
