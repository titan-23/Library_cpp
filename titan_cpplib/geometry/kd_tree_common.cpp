/// https://github.com/titan-23/Library_cpp/blob/main/titan_cpplib/geometry/kd_tree_common.cpp
#pragma once
#include <cmath>
#include <limits>
#include <stdexcept>
#include <type_traits>

using namespace std;

namespace titan23 {

/// @brief 近傍探索の結果1件 / 点番号 index と距離の二乗 squared_distance を持つ
template <class T>
struct KdNeighborT {
    int index;
    T squared_distance;

    /// @brief ユークリッド距離を返す / O(1)
    T distance() const { return sqrt(squared_distance); }
};

/// @brief long double 版の KdNeighborT
using KdNeighbor = KdNeighborT<long double>;

namespace detail {

// 座標を To へ変換する。有限でない値や To に収まらない値なら invalid_argument を投げる
template <class To, class From>
To kd_checked_coordinate(const From& value, const char* message) {
    using Value = remove_cv_t<remove_reference_t<From>>;
    static_assert(is_floating_point_v<To>);
    static_assert(is_convertible_v<From, To>);
    if constexpr (is_arithmetic_v<Value>) {
        long double wide = (long double)value;
        long double limit = (long double)numeric_limits<To>::max();
        if (!isfinite(wide) || wide < -limit || wide > limit) throw invalid_argument(message);
    }
    To converted = (To)value;
    if (!isfinite(converted)) throw invalid_argument(message);
    return converted;
}

}

}
