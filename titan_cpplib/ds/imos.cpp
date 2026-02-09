#include <vector>
using namespace std;

namespace titan23 {
template<typename T>
class Imos {
private:
    int n;
    vector<T> s0, s1;

public:
    Imos() : n(0), s0(0), s1(0) {}
    Imos(int n) : n(n), s0(n+2, 0), s1(n+2, 0) {}

    // 区間[l, r)にvを加算する
    void add_const(int l, int r, T v) {
        assert(0 <= l && l <= r && r <= n);
        s0[l] += v;
        s0[r] -= v;
    }

    // 区間[l, r)に一次関数を加算する
    // a[l+0] += v
    // a[l+1] += v + 1*d
    // a[l+2] += v + 2*d
    void add_linear(int l, int r, T v, T d) {
        assert(0 <= l && l <= r && r <= n);
        s1[l] += v;
        s1[l+1] += d-v;
        s1[r] -= v+d*(r-l);
        s1[r+1] -= d-(v+d*(r-l));
    }

    vector<T> build() {
        for (int i = 0; i <= n; ++i) {
            s1[i+1] += s1[i];
        }
        for (int i = 0; i <= n + 1; ++i) {
            s1[i] += s0[i];
        }
        for (int i = 0; i <= n; ++i) {
            s1[i+1] += s1[i];
        }
        return vector<T>(s1.begin(), s1.begin() + n);
    }
};
};
