/// https://github.com/titan-23/Library_cpp/blob/main/titan_cpplib/math/is_primell.cpp
#pragma once

#include <iostream>
#include <vector>
#include "titan_cpplib/math/math.cpp"
#include "titan_cpplib/others/bit.cpp"
using namespace std;

namespace titan23 {

const vector<long long> is_primell_A1 = {2, 7, 61};
const vector<long long> is_primell_A2 = {2, 325, 9375, 28178, 450775, 9780504, 1795265022};

bool is_primell(long long n) {
    if (n <= 1) return false;
    if (n == 2 || n == 7 || n == 61) return true;
    if (n % 2 == 0) return false;
    long long d = n - 1;
    long long s = countr_zero(d);
    d >>= s;
    const auto &A = n < 4759123141 ? is_primell_A1 : is_primell_A2;
    for (const long long &a : A) {
        if (n <= a) return true;
        long long t, x = pow_mod<__int128>(a, d, n);
        if (x != 1) {
            for (t = 0; t < s; ++t) {
                if (x == n - 1) break;
                x = (__int128)x * x % n;
            }
            if (t == s) return false;
        }
    }
    return true;
}
} // namespace titan23
