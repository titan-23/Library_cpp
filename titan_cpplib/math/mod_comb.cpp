#include <algorithm>
#include <vector>
#include <atcoder/modint>
using namespace std;

namespace titan23 {

template<typename mint>
class ModComb {
private:
    vector<mint> _fact, _factinv, _inv;

public:

    ModComb(int n) {
        const int mod = mint::mod();
        n = max(n, 1) + 1;
        _fact.resize(n);
        _factinv.resize(n);
        _inv.resize(n);

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

    mint nPr(int n, int r) const {
        if (r < 0 || n < r) return mint::raw(0);
        return _fact[n] * _factinv[n - r];
    }

    mint nCr(int n, int r) const {
        if (r < 0 || n < r) return mint::raw(0);
        return _fact[n] * _factinv[r] * _factinv[n-r];
    }

    mint nHr(int n, int r) const {
        return nCr(n+r-1, n-1);
    }

    mint fact(int n) const { return _fact[n]; }

    mint factinv(int n) const { return _factinv[n]; }

    mint inv(int n) const { return _inv[n]; }
};
} // namespace titan23
