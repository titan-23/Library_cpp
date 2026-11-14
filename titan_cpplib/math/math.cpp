/// https://github.com/titan-23/Library_cpp/blob/main/titan_cpplib/math/math.cpp
#pragma once

#include <bits/stdc++.h>
using namespace std;

using i128 = __int128_t;

/// @brief floor(√v) を返す / v < 0 なら 0
i128 isqrt(i128 v) {
    if (v < 0) return 0;
    if (v == 0) return 0;
    i128 x = sqrt((long double)v);
    while ((x-1)*(x-1) >= v) x--;
    while ((x+1)*(x+1) <= v) x++;
    return x;
}

/// @brief ax^2 + bx + c = 0 の実数解を a 倍した値 (-b ± √D) / 2 を小さい順に返す / 実数解がなければ先頭が false
/// a で割らないので、解そのものになるのは a == 1 のときだけ
template<typename T>
tuple<bool, T, T> solve_quadratic_equation(T a, T b, T c) {
    T D = b*b - 4*a*c;
    if (D < 0) return {false, T{}, T{}};
    T v = sqrt(D);
    return {true, (-b-v)/2, (-b+v)/2};
}

// pow ----------------
/// @brief a^b mod mod を返す / (mod-1)^2 が T に収まること / O(log b)
template<class T>
T pow_mod(T a, T b, const T mod) {
    T res = 1;
    a %= mod;
    while (b) {
        if (b & 1) res = res * a % mod;
        a = a * a % mod;
        b >>= 1;
    }
    return res;
}

/// @brief a^b を返す / O(log b)
long long powll(long long a, long long b) {
    long long res = 1;
    while (b) {
        if (b & 1) res *= a;
        a *= a;
        b >>= 1;
    }
    return res;
}
// pow ----------------

// factorial -----------------
/// @brief x! を返す / O(x)
long long factorial(int x) {
    long long res = 1;
    for (int i = 1ll; i <= x; ++i) {
        res *= i;
    }
    return res;
}
// factorial -----------------
