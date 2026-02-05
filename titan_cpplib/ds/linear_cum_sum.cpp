#include <vector>
#include <cassert>
using namespace std;

namespace titan23 {

template<typename T>
class LinearCumSum {
private:
    int n;
    int B; // バケットサイズ
    vector<T> A;
    vector<vector<vector<T>>> S, Si;

    void build() {
        S.resize(B+1);
        Si.resize(B+1);
        for (int d = 1; d < B; ++d) {
            S[d].resize(d+1);
            Si[d].resize(d+1);
            for (int k = 0; k < d; ++k) {
                S[d][k].resize(n/d+2);
                Si[d][k].resize(n/d+2);
            }
            for (int i = 0; i < n; ++i) {
                S[d][i%d][i/d+1] = S[d][i%d][i/d] + A[i];
                Si[d][i%d][i/d+1] = Si[d][i%d][i/d] + A[i]*(i/d+1);
            }
        }
    }

public:
    LinearCumSum() {}

    LinearCumSum(const vector<T> &a) {
        n = (int)a.size();
        B = (int)sqrt(n);
        A = a;
        build();
    }

    // a_l * 1 + a_{l+d} * 2 + ... + a_{l+(k-1)*d} * k
    T sum(int l, int d, int k, ll a, ll b) {
        // https://codeforces.com/contest/1921/problem/F
        assert(l+d*(k-1) < n);
        assert(a == 1);
        if (d >= B) { // naive
            T s = 0;
            int idx = 1;
            for (int i = l; i <= l+d*(k-1); i += d) {
                s += A[i] * idx;
                idx++;
            }
            return s;
        } else {
            int end = l+d*(k-1);
            T s = Si[d][l%d][end/d+1] - Si[d][l%d][l/d];
            s -= (S[d][l%d][end/d+1] - S[d][l%d][l/d]) * (l/d);
            return s;
        }
    }
};
} // namespace titan23
