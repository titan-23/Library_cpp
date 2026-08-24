/// https://github.com/titan-23/Library_cpp/blob/main/titan_cpplib/math/linear_equation.cpp
#pragma once

#include <bits/stdc++.h>
using namespace std;

namespace titan23 {

template<typename T>
struct LinearEquation {
    bool solvable;             // 解が存在するか
    int rank;                  // 係数行列 A の rank
    vector<T> x;               // 特殊解 (solvable のとき size m, そうでなければ空)
    vector<vector<T>> kernel;  // 解空間 (核) の基底. 自由変数ごとに1本, 各 size m
    // 一般解は x + sum_i t_i * kernel[i] (t_i は任意). kernel が空なら解は一意.
};

/// 連立一次方程式 Ax = b の解
/// 整数型を渡してはいけないかもしれない
template<typename T>
LinearEquation<T> linear_equation(vector<vector<T>> A, vector<T> b, const double eps = 1e-9) {
    int n = A.size();
    int m = n ? (int)A[0].size() : 0;

    auto is_zero = [&](const T &v) -> bool {
        if constexpr (is_floating_point_v<T>) return fabs(v) < eps;
        else return v == T(0);
    };

    vector<int> pivot_col;
    int rank = 0;
    for (int col = 0; col < m && rank < n; ++col) {
        int sel = -1;
        if constexpr (is_floating_point_v<T>) {
            double best = eps;
            for (int i = rank; i < n; ++i) {
                if (fabs(A[i][col]) > best) { best = fabs(A[i][col]); sel = i; }
            }
        } else {
            for (int i = rank; i < n; ++i) {
                if (!is_zero(A[i][col])) { sel = i; break; }
            }
        }
        if (sel == -1) continue;
        swap(A[rank], A[sel]);
        swap(b[rank], b[sel]);

        T inv = T(1) / A[rank][col];
        for (int j = col; j < m; ++j) A[rank][j] *= inv;
        b[rank] *= inv;

        for (int i = 0; i < n; ++i) {
            if (i == rank || is_zero(A[i][col])) continue;
            T f = A[i][col];
            for (int j = col; j < m; ++j) A[i][j] -= f * A[rank][j];
            b[i] -= f * b[rank];
        }
        pivot_col.push_back(col);
        ++rank;
    }

    LinearEquation<T> res;
    res.rank = rank;
    for (int i = rank; i < n; ++i) {
        if (!is_zero(b[i])) { res.solvable = false; return res; }
    }
    res.solvable = true;

    res.x.assign(m, T(0));
    for (int r = 0; r < rank; ++r) res.x[pivot_col[r]] = b[r];

    vector<bool> is_pivot(m, false);
    for (int r = 0; r < rank; ++r) is_pivot[pivot_col[r]] = true;
    for (int col = 0; col < m; ++col) {
        if (is_pivot[col]) continue;
        vector<T> v(m, T(0));
        v[col] = T(1);
        for (int r = 0; r < rank; ++r) v[pivot_col[r]] = -A[r][col];
        res.kernel.push_back(move(v));
    }
    return res;
}
}  // namespace titan23
