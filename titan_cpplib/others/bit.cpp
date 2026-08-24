/// https://github.com/titan-23/Library_cpp/blob/main/titan_cpplib/others/bit.cpp
#pragma once

#include <climits>
#include <type_traits>
using namespace std;

namespace titan23 {

template<typename T>
constexpr int countl_zero(T x) {
    constexpr int bits = sizeof(T) * CHAR_BIT;
    if constexpr (sizeof(T) <= sizeof(unsigned int)) {
        using U = make_unsigned_t<T>;
        U y = static_cast<U>(x);
        if (y == 0) return bits;
        return __builtin_clz(static_cast<unsigned int>(y)) - (sizeof(unsigned int) - sizeof(T)) * CHAR_BIT;
    } else if constexpr (sizeof(T) <= sizeof(unsigned long long)) {
        using U = make_unsigned_t<T>;
        U y = static_cast<U>(x);
        if (y == 0) return bits;
        return __builtin_clzll(static_cast<unsigned long long>(y)) - (sizeof(unsigned long long) - sizeof(T)) * CHAR_BIT;
    } else {
        __uint128_t y = static_cast<__uint128_t>(x);
        if (y == 0) return bits;
        const unsigned long long high = static_cast<unsigned long long>(y >> 64);
        return high ? __builtin_clzll(high) : 64 + __builtin_clzll(static_cast<unsigned long long>(y));
    }
}

template<typename T>
constexpr int countr_zero(T x) {
    constexpr int bits = sizeof(T) * CHAR_BIT;
    if constexpr (sizeof(T) <= sizeof(unsigned int)) {
        using U = make_unsigned_t<T>;
        U y = static_cast<U>(x);
        if (y == 0) return bits;
        return __builtin_ctz(static_cast<unsigned int>(y));
    } else if constexpr (sizeof(T) <= sizeof(unsigned long long)) {
        using U = make_unsigned_t<T>;
        U y = static_cast<U>(x);
        if (y == 0) return bits;
        return __builtin_ctzll(static_cast<unsigned long long>(y));
    } else {
        __uint128_t y = static_cast<__uint128_t>(x);
        if (y == 0) return bits;
        const unsigned long long low = static_cast<unsigned long long>(y);
        return low ? __builtin_ctzll(low) : 64 + __builtin_ctzll(static_cast<unsigned long long>(y >> 64));
    }
}

template<typename T>
constexpr int bit_length(T x) {
    return sizeof(T) * CHAR_BIT - countl_zero(x);
}

template<typename T>
constexpr int popcount(T x) {
    if constexpr (sizeof(T) <= sizeof(unsigned int)) {
        using U = make_unsigned_t<T>;
        U y = static_cast<U>(x);
        return __builtin_popcount(static_cast<unsigned int>(y));
    } else if constexpr (sizeof(T) <= sizeof(unsigned long long)) {
        using U = make_unsigned_t<T>;
        U y = static_cast<U>(x);
        return __builtin_popcountll(static_cast<unsigned long long>(y));
    } else {
        __uint128_t y = static_cast<__uint128_t>(x);
        return __builtin_popcountll(static_cast<unsigned long long>(y)) + __builtin_popcountll(static_cast<unsigned long long>(y >> 64));
    }
}

}  // namespace titan23
