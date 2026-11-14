/// https://github.com/titan-23/Library_cpp/blob/main/titan_cpplib/ds/imos.cpp
#pragma once

#include <cassert>
#include <vector>
using namespace std;

namespace titan23 {
/// @brief 区間への定数加算と一次関数加算を最後にまとめて反映する imos 法
template<typename T>
class Imos {
private:
    int n;
    vector<T> s0, s1;

public:
    Imos() : n(0), s0(0), s1(0) {}
    /// @brief 長さ n で作る / 初期値 0 / O(n)
    Imos(int n) : n(n), s0(n+2, 0), s1(n+2, 0) {}

    /// @brief 区間 [l, r) に v を加算する / O(1)
    void add_const(int l, int r, T v) {
        assert(0 <= l && l <= r && r <= n);
        s0[l] += v;
        s0[r] -= v;
    }

    /// @brief 区間 [l, r) の a[l+i] に v + i*d を加算する / O(1)
    void add_linear(int l, int r, T v, T d) {
        assert(0 <= l && l <= r && r <= n);
        s1[l] += v;
        s1[l+1] += d-v;
        s1[r] -= v+d*(r-l);
        s1[r+1] -= d-(v+d*(r-l));
    }

    /// @brief 加算を反映した長さ n の列を返す / 内部の配列を書き換えるので 1 回だけ呼ぶ / O(n)
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
} // namespace titan23
