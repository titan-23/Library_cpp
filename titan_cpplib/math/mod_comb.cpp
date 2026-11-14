/// https://github.com/titan-23/Library_cpp/blob/main/titan_cpplib/math/mod_comb.cpp
#pragma once

#include <algorithm>
#include <vector>
#include <atcoder/modint>
using namespace std;

namespace titan23 {

/// @brief 階乗とその逆元を前計算し、mint 上の順列と組合せの数を求める
template<typename mint>
class ModComb {
private:
    vector<mint> _fact, _factinv, _inv;

public:
    ModComb() {}
    /// @brief n までの階乗、階乗の逆元、逆元を前計算する / mod は n より大きい素数とする / O(n)
    ModComb(int n) {
        const int mod = mint::mod();
        n = max(n, 1) + 1;
        _fact.resize(n+1);
        _factinv.resize(n+1);
        _inv.resize(n+1);

        _fact[0] = mint(1);
        _fact[1] = mint(1);
        _factinv[0] = mint(1);
        _factinv[1] = mint(1);
        _inv[0] = mint(0);
        _inv[1] = mint(1);

        for (int i = 2; i <= n; ++i) {
            _fact[i] = _fact[i-1] * i;
            _inv[i] = -_inv[mod%i] * (mod/i);
            _factinv[i] = _factinv[i-1] * _inv[i];
        }
    }

    /// @brief nPr を返す / r < 0 または n < r なら 0 / O(1)
    mint nPr(int n, int r) const {
        if (r < 0 || n < r) return mint::raw(0);
        return _fact[n] * _factinv[n-r];
    }

    /// @brief nCr を返す / r < 0 または n < r なら 0 / O(1)
    mint nCr(int n, int r) const {
        if (r < 0 || n < r) return mint::raw(0);
        return _fact[n] * _factinv[r] * _factinv[n-r];
    }

    /// @brief 重複組合せ nHr = (n+r-1)Cr を返す / O(1)
    mint nHr(int n, int r) const {
        return nCr(n+r-1, r);
    }

    /// @brief n! を返す / O(1)
    mint fact(int n) const { return _fact[n]; }

    /// @brief 1 / n! を返す / O(1)
    mint factinv(int n) const { return _factinv[n]; }

    /// @brief 1 / n を返す / O(1)
    mint inv(int n) const { return _inv[n]; }
};
} // namespace titan23
