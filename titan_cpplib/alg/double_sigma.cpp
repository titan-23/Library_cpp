#pragma once

#include <vector>
#include <algorithm>
#include "titan_cpplib/ds/multiset_sum.cpp"
using namespace std;

// DoubleSigma
namespace titan23 {

class DoubleSigma {
public:

/// `Σ|A[r]-A[l]|`
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

/// `Σ|A[r]-A[l]|`
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

/// `Σ(A[r] - A[l])`
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

/// `Σmax(A[r], A[l])`
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

/// `Σmin(A[r], A[l])`
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

/// `Σmax(A[r], A[l])`
template<typename T> static T sigma_max(vector<T> A) {
    sort(A.begin(), A.end());
    T ans = 0;
    for (int i = 0; i < A.size(); i++) ans += (T)i * A[i];
    return ans;
}

/// `Σmin(A[r], A[l])`
template<typename T> static T sigma_min(vector<T> A) {
    sort(A.begin(), A.end());
    T ans = 0;
    int n = A.size();
    for (int i = 0; i < n; i++) ans += (T)(n - 1 - i) * A[i];
    return ans;
}

};
} // namespace titan23
