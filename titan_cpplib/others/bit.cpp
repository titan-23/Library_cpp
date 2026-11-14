/// https://github.com/titan-23/Library_cpp/blob/main/titan_cpplib/others/bit.cpp
#pragma once

#include <array>
#include <climits>
#include <cstdint>
#include <type_traits>
// Zen 1/2 では PDEP が遅いので、それ以外の BMI2 対応 CPU でだけ使う
#if defined(__BMI2__) && defined(__x86_64__) && !defined(__znver1__) && !defined(__znver2__)
#define TITAN23_FAST_PDEP 1
#include <immintrin.h>
#endif
using namespace std;

namespace titan23 {

/// @brief 上位から連続する 0 の bit 数を返す / x == 0 なら bit 幅を返す
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

/// @brief 下位から連続する 0 の bit 数を返す / x == 0 なら bit 幅を返す
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

/// @brief x を表すのに要る bit 数を返す / x == 0 なら 0
template<typename T>
constexpr int bit_length(T x) {
    return sizeof(T) * CHAR_BIT - countl_zero(x);
}

/// @brief 立っている bit の数を返す
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

namespace bit_detail {

// table[b][j] は、1 バイトの値 b で下から j 番目に立っている bit の位置
constexpr array<array<uint8_t, 8>, 256> make_select_in_byte_table() {
    array<array<uint8_t, 8>, 256> table{};
    for (int b = 0; b < 256; ++b) {
        int j = 0;
        for (int i = 0; i < 8; ++i) {
            if ((b >> i) & 1) table[b][j++] = i;
        }
    }
    return table;
}

inline constexpr array<array<uint8_t, 8>, 256> select_in_byte_table = make_select_in_byte_table();

}  // namespace bit_detail

/// @brief x で下から k 番目に立っている bit の位置を返す / k は 0 始まりで、k < popcount(x) が前提
inline int select64(unsigned long long x, int k) {
#if defined(TITAN23_FAST_PDEP)
    return __builtin_ctzll(_pdep_u64(1ull << k, x));
#else
    // 下半分の 1 の個数で、k 番目が上下どちらにあるかを 3 回絞り込み、最後の 1 バイトは表で引く
    int pos = 0;
    for (int width = 32; width >= 8; width >>= 1) {
        const int count = __builtin_popcountll(x & ((1ull << width) - 1));
        const bool upper = k >= count;
        k -= upper ? count : 0;
        x >>= upper ? width : 0;
        pos += upper ? width : 0;
    }
    return pos + bit_detail::select_in_byte_table[x & 0xFF][k];
#endif
}

}  // namespace titan23
