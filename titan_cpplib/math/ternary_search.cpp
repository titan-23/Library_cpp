/// https://github.com/titan-23/Library_cpp/blob/main/titan_cpplib/math/ternary_search.cpp
#pragma once

#include <cmath>
using namespace std;

// golden_search
namespace titan23 {

/// @brief 区間 `[L, R]` で単峰な関数 `f` の極小値をとる点を黄金分割探索で求める
template<class F>
double golden_search(double L, double R, F f) { // 極小値
    const double C = 2-(1+sqrt(5))/2;
    double m1 = L + C * (R - L);
    double m2 = R - C * (R - L);
    auto f1 = f(m1), f2 = f(m2);
    for (int _ = 0; _ < 100; ++_) {
        if (f1 < f2) {
            R = m2; m2 = m1; f2 = f1;
            m1 = L + C*(R-L); f1 = f(m1);
        } else {
            L = m1; m1 = m2; f1 = f2;
            m2 = R - C*(R-L); f2 = f(m2);
        }
    }
    return (L+R)/2;
}

} // namespace titan23
