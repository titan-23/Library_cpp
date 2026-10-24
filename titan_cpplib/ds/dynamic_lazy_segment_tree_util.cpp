/// https://github.com/titan-23/Library_cpp/blob/main/titan_cpplib/ds/dynamic_lazy_segment_tree_util.cpp
#pragma once

#include <algorithm>
#include <limits>

#include "titan_cpplib/ds/dynamic_lazy_segment_tree.cpp"

using namespace std;

namespace titan23 {
namespace dynamic_lazy_segment_tree {

// 第1テンプレート引数は値の型で、添字と長さの型は既定でlong long
// 和を扱う木には、値が0でも1要素分の長さを含む初期値を渡す
//   LazySegAddSum<long long> sums(1000000000LL, {0, 1});
//   sums.apply(10, 20, -3); auto result = sums.prod(0, 100); // {-30, 100}
//   LazySegSetMin<long long> minima(1000000000LL, 0);
//   minima.apply(10, 20, Assign<long long>::set(-5));
//   LazySegAffineSum<long long> affine(100, {2, 1});
//   affine.apply(0, 10, {3, 4}); // xを3*x+4に変換
//   LazySegSetAddStats<long long> stats(100, {2, 2, 2, 1});
//   stats.apply(10, 20, SetAdd<long long>::set(5));
//   LazySegAddAPSum<long long> ap(100, {0, 0, 1});
//   ap.apply(l, r, {x - d*l, d}); // 初項x、公差dを加算
//   auto total = ap.prod(l, r).eval(l); // APSum.sumは実際の区間和ではない
// mapping(F,S)はACLと同じ形式
// 各型の四則演算・比較・コピーはO(1)とし、和や長さを含む計算結果をその型の範囲に収める

template <class T, class Index = long long>
struct Sum {
    T sum;
    Index len;

    /// @brief 保持している値が等しいか判定する / O(1)
    bool operator==(const Sum& other) const { return sum == other.sum && len == other.len; }

    /// @brief 保持している値が異なるか判定する / O(1)
    bool operator!=(const Sum& other) const { return !(*this == other); }
};

/// @brief 和と長さをそれぞれ結合する / O(1)
template <class T, class Index>
Sum<T, Index> op_sum(Sum<T, Index> a, Sum<T, Index> b) {
    return {a.sum + b.sum, a.len + b.len};
}

/// @brief 和と長さが0の単位元を返す / O(1)
template <class T, class Index>
Sum<T, Index> e_sum() { return {T(0), 0}; }

/// @brief 同じ集約値をn個結合した和と長さを返す / O(1)
template <class T, class Index>
Sum<T, Index> pow_sum(Sum<T, Index> a, Index n) {
    return {a.sum * T(n), a.len * n};
}

/// @brief 区間の各要素にfを加えた和を求める / O(1)
template <class T, class Index>
Sum<T, Index> map_add_sum(T f, Sum<T, Index> s) {
    return {s.sum + f * T(s.len), s.len};
}

/// @brief 値に加算作用を適用する / O(1)
template <class T> T map_add(T f, T s) { return s + f; }

/// @brief 加算作用gの後にfを適用する作用を返す / O(1)
template <class T> T comp_add(T f, T g) { return f + g; }

/// @brief 値を変更しない加算作用を返す / O(1)
template <class T> T id_add() { return T(0); }

// 有効フラグを別に持ち、numeric_limits<T>::max()/lowest()を含む全ての値を代入可能にする
template <class T>
struct Assign {
    T value{};
    bool active = false;

    /// @brief 指定値を代入する作用を作る / O(1)
    static Assign set(T value) { return {value, true}; }

    /// @brief 保持している値が等しいか判定する / O(1)
    bool operator==(const Assign& other) const {
        return active == other.active && (!active || value == other.value);
    }

    /// @brief 保持している値が異なるか判定する / O(1)
    bool operator!=(const Assign& other) const { return !(*this == other); }
};

/// @brief 代入作用gの後にfを適用する作用を返す / O(1)
template <class T> Assign<T> comp_set(Assign<T> f, Assign<T> g) { return f.active ? f : g; }

/// @brief 値を変更しない代入作用を返す / O(1)
template <class T> Assign<T> id_set() { return {}; }

/// @brief 有効な代入作用なら指定値に置き換える / O(1)
template <class T> T map_set(Assign<T> f, T s) { return f.active ? f.value : s; }

/// @brief 区間の各要素に値を代入した和を求める / O(1)
template <class T, class Index>
Sum<T, Index> map_set_sum(Assign<T> f, Sum<T, Index> s) {
    return f.active ? Sum<T, Index>{f.value * T(s.len), s.len} : s;
}

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

template <class T>
struct Affine {
    T a, b;

    /// @brief 保持している値が等しいか判定する / O(1)
    bool operator==(const Affine& other) const { return a == other.a && b == other.b; }

    /// @brief 保持している値が異なるか判定する / O(1)
    bool operator!=(const Affine& other) const { return !(*this == other); }
};

/// @brief 区間の各要素をa*x+bに変換した和を求める / O(1)
template <class T, class Index>
Sum<T, Index> map_affine_sum(Affine<T> f, Sum<T, Index> s) {
    return {f.a * s.sum + f.b * T(s.len), s.len};
}

/// @brief アフィン変換gの後にfを適用する作用を返す / O(1)
template <class T> Affine<T> comp_affine(Affine<T> f, Affine<T> g) {
    return {f.a * g.a, f.a * g.b + f.b};
}

/// @brief 値を変更しないアフィン変換を返す / O(1)
template <class T> Affine<T> id_affine() { return {T(1), T(0)}; }

template <class T, class Index = long long>
struct Stats {
    T sum, mn, mx;
    Index len;

    /// @brief 保持している値が等しいか判定する / O(1)
    bool operator==(const Stats& other) const {
        return sum == other.sum && mn == other.mn && mx == other.mx && len == other.len;
    }

    /// @brief 保持している値が異なるか判定する / O(1)
    bool operator!=(const Stats& other) const { return !(*this == other); }
};

/// @brief 和・最小値・最大値・長さを結合する / O(1)
template <class T, class Index>
Stats<T, Index> op_stats(Stats<T, Index> a, Stats<T, Index> b) {
    return {a.sum + b.sum, min(a.mn, b.mn), max(a.mx, b.mx), a.len + b.len};
}

/// @brief 統計値の単位元を返す / O(1)
template <class T, class Index>
Stats<T, Index> e_stats() {
    return {T(0), numeric_limits<T>::max(), numeric_limits<T>::lowest(), 0};
}

/// @brief 同じ統計値をn個結合する / O(1)
template <class T, class Index>
Stats<T, Index> pow_stats(Stats<T, Index> s, Index n) {
    if (n == 0) return e_stats<T, Index>();
    return {s.sum * T(n), s.mn, s.mx, s.len * n};
}

template <class T>
struct SetAdd {
    bool has_set;
    T set_val, add_val;

    /// @brief 指定値を代入する作用を作る / O(1)
    static SetAdd set(T value) { return {true, value, T(0)}; }

    /// @brief 指定値を加算する作用を作る / O(1)
    static SetAdd add(T value) { return {false, T(0), value}; }

    /// @brief 作用が等しいか判定する / O(1)
    bool operator==(const SetAdd& other) const {
        return has_set == other.has_set && add_val == other.add_val && (!has_set || set_val == other.set_val);
    }

    /// @brief 作用が異なるか判定する / O(1)
    bool operator!=(const SetAdd& other) const { return !(*this == other); }
};

/// @brief 代入と加算を統計値へ適用する / O(1)
template <class T, class Index>
Stats<T, Index> map_set_add_stats(SetAdd<T> f, Stats<T, Index> s) {
    if (s.len == 0) return s;
    if (f.has_set) {
        const T value = f.set_val + f.add_val;
        return {value * T(s.len), value, value, s.len};
    }
    return {s.sum + f.add_val * T(s.len), s.mn + f.add_val, s.mx + f.add_val, s.len};
}

/// @brief 代入・加算作用gの後にfを適用する作用を返す / O(1)
template <class T>
SetAdd<T> comp_set_add(SetAdd<T> f, SetAdd<T> g) {
    if (f.has_set) return f;
    return {g.has_set, g.set_val, g.add_val + f.add_val};
}

/// @brief 値を変更しない代入・加算作用を返す / O(1)
template <class T> SetAdd<T> id_set_add() { return {false, T(0), T(0)}; }

// 始点をoriginとしたときの和をsum+slope*originで表す
// 実際の和だけでなく、sum・slopeと途中の積もTの範囲に収める
// dense版のS_AP.valと異なり、prod(l,r).eval(l)、get(i).eval(i)で実際の値を得る
// all_prod().eval(0)は全体の和で、初期値vには{v,0,1}を渡す
// 点代入もset(i,{v,0,1})のように1要素の長さを渡す
// max_right(l,pred)ではs.eval(l)、min_left(r,pred)ではs.eval(r-s.len)を判定する
template <class T, class Index = long long>
struct APSum {
    T sum, slope;
    Index len;

    /// @brief 指定した始点で区間和を評価する / O(1)
    T eval(Index origin) const { return sum + slope * T(origin); }

    /// @brief 保持している式と長さが等しいか判定する / O(1)
    bool operator==(const APSum& other) const {
        return sum == other.sum && slope == other.slope && len == other.len;
    }

    /// @brief 保持している式または長さが異なるか判定する / O(1)
    bool operator!=(const APSum& other) const { return !(*this == other); }
};

// a+b*iを加算し、iは[0,N)での絶対添字を表す
template <class T>
struct APAdd {
    T a, b;

    /// @brief 作用が等しいか判定する / O(1)
    bool operator==(const APAdd& other) const { return a == other.a && b == other.b; }

    /// @brief 作用が異なるか判定する / O(1)
    bool operator!=(const APAdd& other) const { return !(*this == other); }
};

/// @brief 0以上n未満の整数の和に係数を掛け、係数0の中間値のオーバーフローを避ける / O(1)
template <class T, class Index>
T scaled_triangular(T coefficient, Index n) {
    if (n == 0) return T(0);
    return n % 2 == 0 ? (coefficient * T(n / 2)) * T(n - 1) : (coefficient * T(n)) * T((n - 1) / 2);
}

/// @brief 右の式を左の長さだけずらして区間和の式を結合する / O(1)
template <class T, class Index>
APSum<T, Index> op_ap_sum(APSum<T, Index> a, APSum<T, Index> b) {
    return {a.sum + b.sum + b.slope * T(a.len), a.slope + b.slope, a.len + b.len};
}

/// @brief 等差数列加算用の単位元を返す / O(1)
template <class T, class Index>
APSum<T, Index> e_ap_sum() { return {T(0), T(0), 0}; }

/// @brief 同じ区間和の式をn個並べて結合する / O(1)
template <class T, class Index>
APSum<T, Index> pow_ap_sum(APSum<T, Index> s, Index n) {
    if (n == 0) return e_ap_sum<T, Index>();
    if (n == 1) return s;
    return {s.sum * T(n) + scaled_triangular(s.slope * T(s.len), n), s.slope * T(n), s.len * n};
}

/// @brief 各要素iへのa+b*iの加算を区間和の式へ反映する / O(1)
template <class T, class Index>
APSum<T, Index> map_ap_add(APAdd<T> f, APSum<T, Index> s) {
    return {s.sum + f.a * T(s.len) + scaled_triangular(f.b, s.len), s.slope + f.b * T(s.len), s.len};
}

/// @brief 等差数列加算作用gの後にfを適用する作用を返す / O(1)
template <class T> APAdd<T> comp_ap_add(APAdd<T> f, APAdd<T> g) { return {f.a + g.a, f.b + g.b}; }

/// @brief 値を変更しない等差数列加算作用を返す / O(1)
template <class T> APAdd<T> id_ap_add() { return {T(0), T(0)}; }

template <class T, class Index = long long>
using LazySegAddMin = DynamicLazySegmentTree<
    Index, T, op_min<T>, e_min<T>,
    T, map_add<T>, comp_add<T>, id_add<T>, pow_min<T, Index>>;

template <class T, class Index = long long>
using LazySegAddMax = DynamicLazySegmentTree<
    Index, T, op_max<T>, e_max<T>,
    T, map_add<T>, comp_add<T>, id_add<T>, pow_max<T, Index>>;

template <class T, class Index = long long>
using LazySegAddSum = DynamicLazySegmentTree<
    Index, Sum<T, Index>, op_sum<T, Index>, e_sum<T, Index>,
    T, map_add_sum<T, Index>, comp_add<T>, id_add<T>, pow_sum<T, Index>>;

template <class T, class Index = long long>
using LazySegSetSum = DynamicLazySegmentTree<
    Index, Sum<T, Index>, op_sum<T, Index>, e_sum<T, Index>,
    Assign<T>, map_set_sum<T, Index>, comp_set<T>, id_set<T>, pow_sum<T, Index>>;

template <class T, class Index = long long>
using LazySegSetMin = DynamicLazySegmentTree<
    Index, T, op_min<T>, e_min<T>,
    Assign<T>, map_set<T>, comp_set<T>, id_set<T>, pow_min<T, Index>>;

template <class T, class Index = long long>
using LazySegSetMax = DynamicLazySegmentTree<
    Index, T, op_max<T>, e_max<T>,
    Assign<T>, map_set<T>, comp_set<T>, id_set<T>, pow_max<T, Index>>;

template <class T, class Index = long long>
using LazySegAffineSum = DynamicLazySegmentTree<
    Index, Sum<T, Index>, op_sum<T, Index>, e_sum<T, Index>,
    Affine<T>, map_affine_sum<T, Index>, comp_affine<T>, id_affine<T>, pow_sum<T, Index>>;

template <class T, class Index = long long>
using LazySegSetAddStats = DynamicLazySegmentTree<
    Index, Stats<T, Index>, op_stats<T, Index>, e_stats<T, Index>,
    SetAdd<T>, map_set_add_stats<T, Index>, comp_set_add<T>, id_set_add<T>, pow_stats<T, Index>>;

template <class T, class Index = long long>
using LazySegChminMin = DynamicLazySegmentTree<
    Index, T, op_min<T>, e_min<T>,
    T, op_min<T>, op_min<T>, e_min<T>, pow_min<T, Index>>;

template <class T, class Index = long long>
using LazySegChminMax = DynamicLazySegmentTree<
    Index, T, op_max<T>, e_max<T>,
    T, op_min<T>, op_min<T>, e_min<T>, pow_max<T, Index>>;

template <class T, class Index = long long>
using LazySegChmaxMin = DynamicLazySegmentTree<
    Index, T, op_min<T>, e_min<T>,
    T, op_max<T>, op_max<T>, e_max<T>, pow_min<T, Index>>;

template <class T, class Index = long long>
using LazySegChmaxMax = DynamicLazySegmentTree<
    Index, T, op_max<T>, e_max<T>,
    T, op_max<T>, op_max<T>, e_max<T>, pow_max<T, Index>>;

template <class T, class Index = long long>
using LazySegAddAPSum = DynamicLazySegmentTree<
    Index, APSum<T, Index>, op_ap_sum<T, Index>, e_ap_sum<T, Index>,
    APAdd<T>, map_ap_add<T, Index>, comp_ap_add<T>, id_ap_add<T>, pow_ap_sum<T, Index>>;

}  // namespace dynamic_lazy_segment_tree
}  // namespace titan23
