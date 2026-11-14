/// https://github.com/titan-23/Library_cpp/blob/main/titan_cpplib/alg/double_sigma.cpp
#pragma once

#include <vector>
#include <algorithm>
#include "titan_cpplib/ds/multiset_sum.cpp"
using namespace std;

namespace titan23 {

/// @brief 列 A の全ての組 l < r について和を求める
class DoubleSigma {
public:

/// @brief Σ|A[r]-A[l]| を返す / MultisetSum で先頭から順に求める
template<typename T> static T sigma_abs_online(const vector<T> &A) {
    T ans = 0;
    titan23::MultisetSum<T> S;
    T sum = 0;
    for (const T a : A) {
        T sum_a = S.sum(a); // a未満の総和
        int cnt = S.index(a); // a未満の個数
        ans += a*cnt - sum_a;
        ans += (sum-sum_a) - a*(S.len()-cnt);
        S.add(a);
        sum += a;
    }
    return ans;
}

/// @brief Σ|A[r]-A[l]| を返す / O(n log n)
template<typename T> static T sigma_abs(vector<T> A) {
    sort(A.begin(), A.end());
    T ans = 0;
    int n = A.size();
    for (int i = 0; i < n; i++) {
        ans += (T)i * A[i];
        ans -= (T)(n - 1 - i) * A[i];
    }
    return ans;
}

/// @brief Σ(A[r] - A[l]) を返す / O(n)
template<typename T> static T sigma_minus(const vector<T> &A) {
    const int n = A.size();
    T ans = 0;
    T sum = 0;
    for (int i = 0; i < n; ++i) {
        T a = A[i];
        ans += a * i - sum;
        sum += a;
    }
    return ans;
}

/// @brief Σmax(A[r], A[l]) を返す / MultisetSum で先頭から順に求める
template<typename T> static T sigma_max_online(const vector<T> &A) {
    const int n = A.size();
    T ans = 0;
    T s = 0;
    MultisetSum<T> S;
    for (int i = 0; i < n; ++i) {
        T a = A[i];
        int cnt = S.index(a);
        T sum = s - S.sum(a);
        ans += sum + a*cnt;
        s += a;
        S.add(a);
    }
    return ans;
}

/// @brief Σmin(A[r], A[l]) を返す / MultisetSum で先頭から順に求める
template<typename T> static T sigma_min_online(const vector<T> &A) {
    const int n = A.size();
    T ans = 0;
    T s = 0;
    MultisetSum<T> S;
    for (int i = 0; i < n; ++i) {
        T a = A[i];
        int cnt = S.len() - S.index(a);
        T sum = S.sum(a);
        ans += sum + a*cnt;
        s += a;
        S.add(a);
    }
    return ans;
}

/// @brief Σmax(A[r], A[l]) を返す / O(n log n)
template<typename T> static T sigma_max(vector<T> A) {
    sort(A.begin(), A.end());
    T ans = 0;
    for (int i = 0; i < A.size(); i++) ans += (T)i * A[i];
    return ans;
}

/// @brief Σmin(A[r], A[l]) を返す / O(n log n)
template<typename T> static T sigma_min(vector<T> A) {
    sort(A.begin(), A.end());
    T ans = 0;
    int n = A.size();
    for (int i = 0; i < n; i++) ans += (T)(n - 1 - i) * A[i];
    return ans;
}

};
} // namespace titan23
