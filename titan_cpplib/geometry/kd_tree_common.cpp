/// https://github.com/titan-23/Library_cpp/blob/main/titan_cpplib/geometry/kd_tree_common.cpp
#pragma once
#include <cmath>
#include <limits>
#include <stdexcept>
#include <type_traits>

using namespace std;

namespace titan23 {

template <class T>
struct KdNeighborT {
    int index;
    T squared_distance;

    T distance() const { return sqrt(squared_distance); }
};

using KdNeighbor = KdNeighborT<long double>;

namespace detail {

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
