#include <vector>
#include <cassert>
#include "titan_cpplib/data_structures/multiset_sum.cpp"
#include "titan_cpplib/data_structures/cumulative_sum.cpp"
using namespace std;

// DoubleSigma
namespace titan23 {

class DoubleSigma {
public:

/// `Σ|A[r]-A[l]|`
template<typename T> static T sigma_abs(const vector<T> &A) {
    T ans = 0;
    const int n = A.size();
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

/// `Σ(A[r] - A[l])`
template<typename T> static T sigma_minus(const vector<T> &A) {
    const int n = A.size();
    T ans = 0;
    T sum = 0;
    for (int i = 0; i < n; ++i) {
        ll a = A[i];
        ans += a * i - sum;
        sum += a;
    }
    return ans;
}

/// `Σmax(A[r], A[l])`
template<typename T> static T sigma_max(const vector<T> &A) {
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
template<typename T> static T sigma_min(const vector<T> &A) {
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

template<typename T> static void test_abs(const vector<T> &A) {
    const int n = A.size();
    T ans = 0;
    for (int l = 0; l < n; ++l) for (int r = l+1; r < n; ++r) {
        ans += abs(A[r] - A[l]);
    }
    T ans_sigma = sigma_abs(A);
    cerr << ans << " " << ans_sigma << endl;
    assert(ans == ans_sigma);
    cerr << "ok." << endl;
}

template<typename T> static void test_minus(const vector<T> &A) {
    const int n = A.size();
    T ans = 0;
    for (int l = 0; l < n; ++l) for (int r = l+1; r < n; ++r) {
        ans += A[r] - A[l];
    }
    T ans_sigma = sigma_minus(A);
    cerr << ans << " " << ans_sigma << endl;
    assert(ans == ans_sigma);
    cerr << "ok." << endl;
}

template<typename T> static void test_min(const vector<T> &A) {
    const int n = A.size();
    T ans = 0;
    for (int l = 0; l < n; ++l) for (int r = l+1; r < n; ++r) {
        ans += min(A[r], A[l]);
    }
    T ans_sigma = sigma_min(A);
    cerr << ans << " " << ans_sigma << endl;
    assert(ans == ans_sigma);
    cerr << "ok." << endl;
}

template<typename T> static void test_max(const vector<T> &A) {
    const int n = A.size();
    T ans = 0;
    for (int l = 0; l < n; ++l) for (int r = l+1; r < n; ++r) {
        ans += max(A[r], A[l]);
    }
    T ans_sigma = sigma_max(A);
    cerr << ans << " " << ans_sigma << endl;
    assert(ans == ans_sigma);
    cerr << "ok." << endl;
}

};
} // namespace titan23
