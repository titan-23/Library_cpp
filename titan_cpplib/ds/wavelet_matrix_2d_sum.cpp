#pragma once

#include <algorithm>
#include <cassert>
#include <tuple>
#include <utility>
#include <vector>
#include "titan_cpplib/ds/bit_vector.cpp"
#include "titan_cpplib/others/bit.cpp"
using namespace std;

namespace titan23 {

/**
 * @brief オフライン2次元点集合の重み総和
 *
 * 同じ座標へ複数回登録した点は別々の点として数える。
 * 同じ `y` を途中まで採用する場合は `x` の昇順、同じ `x` では登録順を使う。
 *
 * @tparam T 座標の型
 * @tparam W 重みの型
 */
template<typename T, typename W>
class WaveletMatrix2DSum {
private:
    vector<tuple<T, T, W>> _points;
    vector<T> _x;
    vector<T> _y;
    vector<BitVector> _v;
    vector<int> _mid;
    vector<W> _zero_sum;
    vector<W> _original_sum;
    vector<W> _leaf_sum;
    int _log;
    int _n;
    int _stride;
    bool _built;

    pair<int, int> _x_range(T x1, T x2) const {
        assert(_built);
        assert(x1 <= x2);
        return {
            static_cast<int>(lower_bound(_x.begin(), _x.end(), x1) - _x.begin()),
            static_cast<int>(lower_bound(_x.begin(), _x.end(), x2) - _x.begin())
        };
    }

    W _zero_range_sum(int bit, int l, int r) const {
        size_t offset = static_cast<size_t>(bit) * _stride;
        return _zero_sum[offset + r] - _zero_sum[offset + l];
    }

    pair<int, W> _count_sum_lt(int l, int r, int upper) const {
        if (upper <= 0 || l == r) return {0, W(0)};
        if (upper >= static_cast<int>(_y.size())) return {r - l, _original_sum[r] - _original_sum[l]};
        int count = 0;
        W sum = 0;
        for (int bit = _log - 1; bit >= 0; --bit) {
            int l0 = _v[bit].rank0(l);
            int r0 = _v[bit].rank0(r);
            if ((upper >> bit) & 1) {
                count += r0 - l0;
                sum += _zero_range_sum(bit, l, r);
                l = _mid[bit] + l - l0;
                r = _mid[bit] + r - r0;
            } else {
                l = l0;
                r = r0;
            }
        }
        return {count, sum};
    }

    W _sum_k(int l, int r, int k, bool largest) const {
        if (k == 0) return W(0);
        W sum = _original_sum[r] - _original_sum[l];
        if (k == r - l) return sum;
        if (_log == 0) return _leaf_sum[l + k] - _leaf_sum[l];

        W ans = 0;
        for (int bit = _log - 1; bit >= 0; --bit) {
            int l0 = _v[bit].rank0(l);
            int r0 = _v[bit].rank0(r);
            int l1 = _mid[bit] + l - l0;
            int r1 = _mid[bit] + r - r0;
            int cnt0 = r0 - l0;
            int cnt1 = r1 - l1;
            W sum0 = _zero_range_sum(bit, l, r);
            W sum1 = sum - sum0;
            int cnt = largest ? cnt1 : cnt0;
            W s = largest ? sum1 : sum0;

            if (k <= cnt) {
                if (k == cnt) return ans + s;
                if (bit == 0) {
                    int begin = largest ? l1 : l0;
                    return ans + _leaf_sum[begin + k] - _leaf_sum[begin];
                }
                if (largest) {
                    l = l1;
                    r = r1;
                } else {
                    l = l0;
                    r = r0;
                }
                sum = s;
                continue;
            }

            ans += s;
            k -= cnt;
            if (bit == 0) {
                int begin = largest ? l0 : l1;
                return ans + _leaf_sum[begin + k] - _leaf_sum[begin];
            }
            if (largest) {
                l = l0;
                r = r0;
                sum = sum0;
            } else {
                l = l1;
                r = r1;
                sum = sum1;
            }
        }
        return ans;
    }

public:
    WaveletMatrix2DSum() : _log(0), _n(0), _stride(1), _built(false) {}

    explicit WaveletMatrix2DSum(const vector<tuple<T, T, W>> &points) : WaveletMatrix2DSum() {
        _points = points;
        build();
    }

    /// 登録予定の点数を予約する / `O(1)`
    void reserve(int capacity) {
        assert(!_built);
        _points.reserve(capacity);
    }

    /// 点 `(x, y)` へ重み `weight` を登録する / ならし `O(1)`
    void add_point(T x, T y, W weight) {
        assert(!_built);
        _points.emplace_back(x, y, weight);
    }

    /// 登録した点から構築する / `O(nlog(n))`
    void build() {
        vector<tuple<T, T, W>> a = _points;
        stable_sort(a.begin(), a.end(), [](const auto &x, const auto &y) { return get<0>(x) < get<0>(y); });

        _n = a.size();
        _stride = _n + 1;
        _x.resize(_n);
        _y.clear();
        _y.reserve(_n);
        vector<W> weight(_n);
        for (int i = 0; i < _n; ++i) {
            auto &[x, y, w] = a[i];
            _x[i] = x;
            _y.emplace_back(y);
            weight[i] = w;
        }
        sort(_y.begin(), _y.end());
        _y.erase(unique(_y.begin(), _y.end()), _y.end());

        vector<int> y(_n);
        for (int i = 0; i < _n; ++i) {
            auto &[x, py, w] = a[i];
            y[i] = lower_bound(_y.begin(), _y.end(), py) - _y.begin();
        }
        _log = bit_length(_y.empty() ? 0 : _y.size() - 1);
        _v.assign(_log, BitVector());
        _mid.assign(_log, 0);
        _zero_sum.assign(static_cast<size_t>(_log) * _stride, W(0));
        _original_sum.assign(_n + 1, W(0));
        for (int i = 0; i < _n; ++i) _original_sum[i + 1] = _original_sum[i] + weight[i];

        vector<int> ny(_n);
        vector<W> nweight(_n);
        for (int bit = _log - 1; bit >= 0; --bit) {
            _v[bit] = BitVector(_n);
            size_t offset = static_cast<size_t>(bit) * _stride;
            int zeros = 0;
            for (int i = 0; i < _n; ++i) {
                bool b = (y[i] >> bit) & 1;
                if (b) {
                    _v[bit].set(i);
                } else {
                    ++zeros;
                }
                _zero_sum[offset + i + 1] = _zero_sum[offset + i] + (b ? W(0) : weight[i]);
            }
            _v[bit].build();
            _mid[bit] = zeros;

            int zero = 0;
            int one = zeros;
            for (int i = 0; i < _n; ++i) {
                bool b = (y[i] >> bit) & 1;
                int p = b ? one++ : zero++;
                ny[p] = y[i];
                nweight[p] = weight[i];
            }
            y.swap(ny);
            weight.swap(nweight);
        }

        _leaf_sum.assign(_n + 1, W(0));
        for (int i = 0; i < _n; ++i) _leaf_sum[i + 1] = _leaf_sum[i] + weight[i];
        _built = true;
    }

    /// `x` が `[x1, x2)` にある点の重みの総和を返す / `O(log(n))`
    W range_sum(T x1, T x2) const {
        auto [l, r] = _x_range(x1, x2);
        return _original_sum[r] - _original_sum[l];
    }

    /// 長方形 `[x1, x2) × [y1, y2)` にある点の重みの総和を返す / `O(log(n))`
    W range_sum(T x1, T x2, T y1, T y2) const {
        if (y1 >= y2) return W(0);
        return sum_lt(x1, x2, y2) - sum_lt(x1, x2, y1);
    }

    /// `x` が `[x1, x2)`、`y` が `upper` 未満の点の個数と重みの総和を返す / `O(log(n))`
    pair<int, W> count_sum_lt(T x1, T x2, T upper) const {
        auto [l, r] = _x_range(x1, x2);
        int y = lower_bound(_y.begin(), _y.end(), upper) - _y.begin();
        return _count_sum_lt(l, r, y);
    }

    /// `x` が `[x1, x2)`、`y` が `upper` 未満の点の重みの総和を返す / `O(log(n))`
    W sum_lt(T x1, T x2, T upper) const { return count_sum_lt(x1, x2, upper).second; }

    /// 長方形 `[x1, x2) × [y1, y2)` にある点数を返す / `O(log(n))`
    int range_count(T x1, T x2, T y1, T y2) const {
        if (y1 >= y2) return 0;
        return count_sum_lt(x1, x2, y2).first - count_sum_lt(x1, x2, y1).first;
    }

    /// `x` が `[x1, x2)` にある点のうち、昇順 `k` 番目の `y` を返す / `O(log(n))`
    /// 例: `[(0, 5), (1, 2), (2, 2)]` で `kth_y(0, 3, 1)` は `2`
    T kth_y(T x1, T x2, int k) const {
        auto [l, r] = _x_range(x1, x2);
        assert(0 <= k && k < r - l);
        int ans = 0;
        for (int bit = _log - 1; bit >= 0; --bit) {
            int l0 = _v[bit].rank0(l);
            int r0 = _v[bit].rank0(r);
            int cnt = r0 - l0;
            if (cnt <= k) {
                ans |= 1 << bit;
                k -= cnt;
                l = _mid[bit] + l - l0;
                r = _mid[bit] + r - r0;
            } else {
                l = l0;
                r = r0;
            }
        }
        return _y[ans];
    }

    /// `x` が `[x1, x2)` にある点を `y` の昇順に並べ、先頭 `k` 点の重みの総和を返す / `O(log(n))`
    /// 例: `[(0, 5, 4), (1, 2, 3), (2, 7, 6)]` で `sum_k_smallest_y(0, 3, 2)` は `7`
    W sum_k_smallest_y(T x1, T x2, int k) const {
        auto [l, r] = _x_range(x1, x2);
        assert(0 <= k && k <= r - l);
        return _sum_k(l, r, k, false);
    }

    /// `x` が `[x1, x2)` にある点を `y` の降順に並べ、先頭 `k` 点の重みの総和を返す / `O(log(n))`
    /// 例: `[(0, 5, 4), (1, 2, 3), (2, 7, 6)]` で `sum_k_largest_y(0, 3, 2)` は `10`
    W sum_k_largest_y(T x1, T x2, int k) const {
        auto [l, r] = _x_range(x1, x2);
        assert(0 <= k && k <= r - l);
        return _sum_k(l, r, k, true);
    }

    /// 登録点数を返す / `O(1)`
    int len() const { return _n; }
};

} // namespace titan23
