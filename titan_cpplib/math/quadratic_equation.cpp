/// https://github.com/titan-23/Library_cpp/blob/main/titan_cpplib/math/quadratic_equation.cpp
#pragma once

#include <bits/stdc++.h>
using namespace std;

namespace titan23 {

vector<long double> solve_quadratic_equation(long double a, long double b, long double c, const long double eps = 1e-9) {
    if (fabs(a) < eps) {
        if (fabs(b) < eps) return {};  
        return {-c / b};               
    }
    long double D = b * b - 4 * a * c;
    if (D < -eps) return {};                 
    if (D < eps) return {-b / (2 * a)};      
    long double sq = sqrt(D);
    long double q = -0.5 * (b + (b >= 0 ? sq : -sq));
    long double x1 = q / a;   
    long double x2 = c / q;   
    if (x1 > x2) swap(x1, x2);
    return {x1, x2};
}
}  // namespace titan23
