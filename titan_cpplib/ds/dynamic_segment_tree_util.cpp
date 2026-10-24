/// https://github.com/titan-23/Library_cpp/blob/main/titan_cpplib/ds/dynamic_segment_tree_util.cpp
#pragma once

#include <algorithm>
#include <limits>
#include <numeric>
#include <utility>

#include "titan_cpplib/ds/dynamic_segment_tree.cpp"
#include "titan_cpplib/ds/dynamic_segment_tree_init.cpp"

using namespace std;

namespace titan23 {
namespace dynamic_segment_tree {

// 第1テンプレート引数は値の型で、添字の型は既定でlong long
// SegSum<long long> sparse(1000000000LL);          // 初期値は0
// SegSumInit<long long> uniform(1000000000LL, -3); // 初期値は-3
// uniform.set(42, 5); auto sum = uniform.prod(0, 100);
// 各型の四則演算・比較・ビット演算はO(1)とし、計算結果はその型の範囲に収める
// Gcdは非負値を扱い、計算量のVはmax(1,a,b)を表す
// Xor・Or・Andは整数型を使う
// Idxの添字とCntの個数もIndexで保持し、個数の計算結果はIndexの範囲に収める
// Cntの各要素は{値,1}、Idxの各要素は{値,添字}として渡す
// IdxのInit版は渡した{値,添字}を反復し、各位置の添字を自動生成しない
// SegAffineのprod(l,r).eval(x)はf_{r-1}(...f_l(x)...)を返す
// SegMaxSubSumのprod(l,r).ansは空区間を許す最大部分配列和を返す
// powのnは非負で、Affine以外の反復集約はO(1)
// Affineの反復集約は一般の環でも使える二分累乗でO(logn)
// SegAffineInitのset/prod/max_right/min_leftはO(log²N)、all_prodはO(logN)

/// @brief 和を求める / O(1)
template <class T> T op_sum(T a, T b) { return a + b; }

/// @brief 和の単位元を返す / O(1)
template <class T> T e_sum() { return T(0); }

/// @brief 同じ値をn個結合した和を返す / O(1)
template <class T, class Index> T pow_sum(T a, Index n) { return a * T(n); }

/// @brief 最小値を求める / O(1)
template <class T> T op_min(T a, T b) { return min(a, b); }

/// @brief 最小値の単位元を返す / O(1)
template <class T> T e_min() { return numeric_limits<T>::max(); }

/// @brief 同じ値をn個結合した最小値を返す / O(1)
template <class T, class Index> T pow_min(T a, Index n) { return n == 0 ? e_min<T>() : a; }

/// @brief 最大値を求める / O(1)
template <class T> T op_max(T a, T b) { return max(a, b); }

/// @brief 最大値の単位元を返す / O(1)
template <class T> T e_max() { return numeric_limits<T>::lowest(); }

/// @brief 同じ値をn個結合した最大値を返す / O(1)
template <class T, class Index> T pow_max(T a, Index n) { return n == 0 ? e_max<T>() : a; }

/// @brief 最大公約数を求める / O(logV)
template <class T> T op_gcd(T a, T b) { return gcd(a, b); }

/// @brief 最大公約数の単位元を返す / O(1)
template <class T> T e_gcd() { return T(0); }

/// @brief 同じ値をn個結合した最大公約数を返す / O(1)
template <class T, class Index> T pow_gcd(T a, Index n) { return n == 0 ? e_gcd<T>() : a; }

/// @brief 排他的論理和を求める / O(1)
template <class T> T op_xor(T a, T b) { return a ^ b; }

/// @brief 排他的論理和の単位元を返す / O(1)
template <class T> T e_xor() { return T(0); }

/// @brief 同じ値をn個結合した排他的論理和を返す / O(1)
template <class T, class Index> T pow_xor(T a, Index n) { return n % 2 ? a : T(0); }

/// @brief 論理和を求める / O(1)
template <class T> T op_or(T a, T b) { return a | b; }

/// @brief 論理和の単位元を返す / O(1)
template <class T> T e_or() { return T(0); }

/// @brief 同じ値をn個結合した論理和を返す / O(1)
template <class T, class Index> T pow_or(T a, Index n) { return n == 0 ? e_or<T>() : a; }

/// @brief 論理積を求める / O(1)
template <class T> T op_and(T a, T b) { return a & b; }

/// @brief 論理積の単位元を返す / O(1)
template <class T> T e_and() { return ~T(0); }

/// @brief 同じ値をn個結合した論理積を返す / O(1)
template <class T, class Index> T pow_and(T a, Index n) { return n == 0 ? e_and<T>() : a; }

/// @brief 最小値とその値を取る最小の添字を求める / O(1)
template <class T, class Index = long long>
pair<T, Index> op_min_idx(pair<T, Index> a, pair<T, Index> b) { return min(a, b); }

/// @brief 最小値と添字を扱う単位元を返す / O(1)
template <class T, class Index = long long>
pair<T, Index> e_min_idx() { return {numeric_limits<T>::max(), numeric_limits<Index>::max()}; }

/// @brief 同じ値と添字をn個結合した最小値と添字を返す / O(1)
template <class T, class Index>
pair<T, Index> pow_min_idx(pair<T, Index> a, Index n) { return n == 0 ? e_min_idx<T, Index>() : a; }

/// @brief 最大値とその値を取る最小の添字を求める / O(1)
template <class T, class Index = long long>
pair<T, Index> op_max_idx(pair<T, Index> a, pair<T, Index> b) {
    if (a.first < b.first) return b;
    if (b.first < a.first) return a;
    return a.second < b.second ? a : b;
}

/// @brief 最大値と添字を扱う単位元を返す / O(1)
template <class T, class Index = long long>
pair<T, Index> e_max_idx() { return {numeric_limits<T>::lowest(), numeric_limits<Index>::max()}; }

/// @brief 同じ値と添字をn個結合した最大値と添字を返す / O(1)
template <class T, class Index>
pair<T, Index> pow_max_idx(pair<T, Index> a, Index n) { return n == 0 ? e_max_idx<T, Index>() : a; }

/// @brief 最小値とその値を取る要素数を求める / O(1)
template <class T, class Index = long long>
pair<T, Index> op_min_cnt(pair<T, Index> a, pair<T, Index> b) {
    if (a.first != b.first) return a.first < b.first ? a : b;
    return {a.first, a.second + b.second};
}

/// @brief 最小値と個数を扱う単位元を返す / O(1)
template <class T, class Index = long long>
pair<T, Index> e_min_cnt() { return {numeric_limits<T>::max(), 0}; }

/// @brief 同じ値と個数をn個結合した最小値と個数を返す / O(1)
template <class T, class Index>
pair<T, Index> pow_min_cnt(pair<T, Index> a, Index n) {
    return n == 0 ? e_min_cnt<T, Index>() : pair<T, Index>{a.first, a.second * n};
}

/// @brief 最大値とその値を取る要素数を求める / O(1)
template <class T, class Index = long long>
pair<T, Index> op_max_cnt(pair<T, Index> a, pair<T, Index> b) {
    if (a.first != b.first) return a.first > b.first ? a : b;
    return {a.first, a.second + b.second};
}

/// @brief 最大値と個数を扱う単位元を返す / O(1)
template <class T, class Index = long long>
pair<T, Index> e_max_cnt() { return {numeric_limits<T>::lowest(), 0}; }

/// @brief 同じ値と個数をn個結合した最大値と個数を返す / O(1)
template <class T, class Index>
pair<T, Index> pow_max_cnt(pair<T, Index> a, Index n) {
    return n == 0 ? e_max_cnt<T, Index>() : pair<T, Index>{a.first, a.second * n};
}

template <class T>
struct Affine {
    T a, b;

    /// @brief 一次関数a*x+bの値を求める / O(1)
    T eval(T x) const { return a * x + b; }
};

/// @brief 一次関数aの後にbを適用する合成関数を返す / O(1)
template <class T>
Affine<T> op_affine(Affine<T> a, Affine<T> b) {
    return {b.a * a.a, b.a * a.b + b.b};
}

/// @brief 値を変更しない一次関数を返す / O(1)
template <class T> Affine<T> e_affine() { return {T(1), T(0)}; }

/// @brief 同じ一次関数をn回適用する合成関数を返す / O(logn)
template <class T, class Index>
Affine<T> pow_affine(Affine<T> value, Index n) {
    Affine<T> result = e_affine<T>();
    while (n > 0) {
        if (n & 1) result = op_affine(result, value);
        n >>= 1;
        if (n != 0) value = op_affine(value, value);
    }
    return result;
}

template <class T>
struct MaxSubSum {
    T sum; // 区間の総和
    T pre; // 最大接頭辞和
    T suf; // 最大接尾辞和
    T ans; // 空区間を許す最大部分配列和

    /// @brief 空区間の集約値を作る / O(1)
    MaxSubSum() : sum(0), pre(0), suf(0), ans(0) {}

    /// @brief 値xの1要素から集約値を作る / O(1)
    MaxSubSum(T x) : sum(x), pre(max(x, T(0))), suf(max(x, T(0))), ans(max(x, T(0))) {}
};

/// @brief 隣接する2区間の最大部分配列和を結合する / O(1)
template <class T>
MaxSubSum<T> op_max_sub_sum(MaxSubSum<T> a, MaxSubSum<T> b) {
    MaxSubSum<T> result;
    result.sum = a.sum + b.sum;
    result.pre = max(a.pre, a.sum + b.pre);
    result.suf = max(b.suf, a.suf + b.sum);
    result.ans = max({a.ans, b.ans, a.suf + b.pre});
    return result;
}

/// @brief 最大部分配列和を扱う単位元を返す / O(1)
template <class T> MaxSubSum<T> e_max_sub_sum() { return {}; }

/// @brief 同じ区間をn個結合した最大部分配列和を返す / O(1)
template <class T, class Index>
MaxSubSum<T> pow_max_sub_sum(MaxSubSum<T> value, Index n) {
    if (n == 0) return e_max_sub_sum<T>();
    if (n == 1) return value;
    const T gain = max(value.sum, T(0));
    MaxSubSum<T> result;
    result.sum = value.sum * T(n);
    result.pre = value.pre + gain * T(n - 1);
    result.suf = value.suf + gain * T(n - 1);
    result.ans = max(value.ans, value.suf + value.pre + gain * T(n - 2));
    return result;
}

template <class T, class Index = long long>
using SegSum = DynamicSegmentTree<Index, T, op_sum<T>, e_sum<T>>;
template <class T, class Index = long long>
using SegSumInit = DynamicSegmentTreeInit<Index, T, op_sum<T>, e_sum<T>, pow_sum<T, Index>>;

template <class T, class Index = long long>
using SegMin = DynamicSegmentTree<Index, T, op_min<T>, e_min<T>>;
template <class T, class Index = long long>
using SegMinInit = DynamicSegmentTreeInit<Index, T, op_min<T>, e_min<T>, pow_min<T, Index>>;

template <class T, class Index = long long>
using SegMax = DynamicSegmentTree<Index, T, op_max<T>, e_max<T>>;
template <class T, class Index = long long>
using SegMaxInit = DynamicSegmentTreeInit<Index, T, op_max<T>, e_max<T>, pow_max<T, Index>>;

template <class T, class Index = long long>
using SegGcd = DynamicSegmentTree<Index, T, op_gcd<T>, e_gcd<T>>;
template <class T, class Index = long long>
using SegGcdInit = DynamicSegmentTreeInit<Index, T, op_gcd<T>, e_gcd<T>, pow_gcd<T, Index>>;

template <class T, class Index = long long>
using SegXor = DynamicSegmentTree<Index, T, op_xor<T>, e_xor<T>>;
template <class T, class Index = long long>
using SegXorInit = DynamicSegmentTreeInit<Index, T, op_xor<T>, e_xor<T>, pow_xor<T, Index>>;

template <class T, class Index = long long>
using SegOr = DynamicSegmentTree<Index, T, op_or<T>, e_or<T>>;
template <class T, class Index = long long>
using SegOrInit = DynamicSegmentTreeInit<Index, T, op_or<T>, e_or<T>, pow_or<T, Index>>;

template <class T, class Index = long long>
using SegAnd = DynamicSegmentTree<Index, T, op_and<T>, e_and<T>>;
template <class T, class Index = long long>
using SegAndInit = DynamicSegmentTreeInit<Index, T, op_and<T>, e_and<T>, pow_and<T, Index>>;

template <class T, class Index = long long>
using SegMinIdx = DynamicSegmentTree<Index, pair<T, Index>, op_min_idx<T, Index>, e_min_idx<T, Index>>;

template <class T, class Index = long long>
using SegMinIdxInit = DynamicSegmentTreeInit<
    Index, pair<T, Index>, op_min_idx<T, Index>, e_min_idx<T, Index>, pow_min_idx<T, Index>>;

template <class T, class Index = long long>
using SegMaxIdx = DynamicSegmentTree<Index, pair<T, Index>, op_max_idx<T, Index>, e_max_idx<T, Index>>;

template <class T, class Index = long long>
using SegMaxIdxInit = DynamicSegmentTreeInit<
    Index, pair<T, Index>, op_max_idx<T, Index>, e_max_idx<T, Index>, pow_max_idx<T, Index>>;

template <class T, class Index = long long>
using SegMinCnt = DynamicSegmentTree<Index, pair<T, Index>, op_min_cnt<T, Index>, e_min_cnt<T, Index>>;

template <class T, class Index = long long>
using SegMinCntInit = DynamicSegmentTreeInit<
    Index, pair<T, Index>, op_min_cnt<T, Index>, e_min_cnt<T, Index>, pow_min_cnt<T, Index>>;

template <class T, class Index = long long>
using SegMaxCnt = DynamicSegmentTree<Index, pair<T, Index>, op_max_cnt<T, Index>, e_max_cnt<T, Index>>;

template <class T, class Index = long long>
using SegMaxCntInit = DynamicSegmentTreeInit<
    Index, pair<T, Index>, op_max_cnt<T, Index>, e_max_cnt<T, Index>, pow_max_cnt<T, Index>>;

template <class T, class Index = long long>
using SegAffine = DynamicSegmentTree<Index, Affine<T>, op_affine<T>, e_affine<T>>;

template <class T, class Index = long long>
using SegAffineInit = DynamicSegmentTreeInit<Index, Affine<T>, op_affine<T>, e_affine<T>, pow_affine<T, Index>>;

template <class T, class Index = long long>
using SegMaxSubSum = DynamicSegmentTree<Index, MaxSubSum<T>, op_max_sub_sum<T>, e_max_sub_sum<T>>;

template <class T, class Index = long long>
using SegMaxSubSumInit = DynamicSegmentTreeInit<
    Index, MaxSubSum<T>, op_max_sub_sum<T>, e_max_sub_sum<T>, pow_max_sub_sum<T, Index>>;

}  // namespace dynamic_segment_tree
}  // namespace titan23
