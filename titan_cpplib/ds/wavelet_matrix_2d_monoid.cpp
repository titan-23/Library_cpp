/// https://github.com/titan-23/Library_cpp/blob/main/titan_cpplib/ds/wavelet_matrix_2d_monoid.cpp
#pragma once

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <tuple>
#include <utility>
#include <vector>
#include "titan_cpplib/ds/bit_vector.cpp"
#include "titan_cpplib/ds/segment_tree.cpp"
#include "titan_cpplib/others/bit.cpp"
using namespace std;

namespace titan23 {

/**
 * @brief オフライン2次元点集合の可換モノイド積
 *
 * 長方形内の点には自然な一次元順序がないため、`op` は可換であることを前提とする。
 * 同じ座標へ複数回登録した点は別々の点として集約する。
 *
 * @tparam T 座標の型
 * @tparam S モノイドの型
 * @tparam op 二項演算
 * @tparam e 単位元
 */
template<typename T, typename S, S (*op)(S, S), S (*e)()>
class WaveletMatrix2DMonoid {
private:
    vector<tuple<T, T, S>> _points;
    vector<T> _x;
    vector<T> _y;
    vector<BitVector> _v;
    vector<int> _mid;
    vector<SegmentTree<S, op, e>> _seg;
    SegmentTree<S, op, e> _original;
    SegmentTree<S, op, e> _leaf;
    int _log;
    int _n;
    bool _built;

    pair<int, int> _x_range(T x1, T x2) const {
        assert(_built);
        assert(x1 <= x2);
        return {
            static_cast<int>(lower_bound(_x.begin(), _x.end(), x1) - _x.begin()),
            static_cast<int>(lower_bound(_x.begin(), _x.end(), x2) - _x.begin())
        };
    }

    S _range_prod(int l, int r, int lower, int upper) const {
        if (l == r || lower >= upper) return e();
        if (lower <= 0 && upper >= static_cast<int>(_y.size())) return _original.prod(l, r);

        S ans = e();
        auto dfs = [&](auto &&dfs, int bit, int ql, int qr, uint64_t a, uint64_t b) -> void {
            if (ql == qr || b <= static_cast<uint64_t>(lower) || static_cast<uint64_t>(upper) <= a) return;
            if (static_cast<uint64_t>(lower) <= a && b <= static_cast<uint64_t>(upper)) {
                ans = op(ans, bit < 0 ? _leaf.prod(ql, qr) : _seg[bit].prod(ql, qr));
                return;
            }

            int l0 = _v[bit].rank0(ql);
            int r0 = _v[bit].rank0(qr);
            int l1 = _mid[bit] + ql - l0;
            int r1 = _mid[bit] + qr - r0;
            uint64_t mid = (a + b) / 2;
            dfs(dfs, bit - 1, l0, r0, a, mid);
            dfs(dfs, bit - 1, l1, r1, mid, b);
        };
        dfs(dfs, _log - 1, l, r, 0, uint64_t(1) << _log);
        return ans;
    }

public:
    WaveletMatrix2DMonoid() : _log(0), _n(0), _built(false) {}

    explicit WaveletMatrix2DMonoid(const vector<tuple<T, T, S>> &points) : WaveletMatrix2DMonoid() {
        _points = points;
        build();
    }

    /// 登録予定の点数を予約する / `O(1)`
    void reserve(int capacity) {
        assert(!_built);
        _points.reserve(capacity);
    }

    /// 点 `(x, y)` へ値 `value` を登録する / ならし `O(1)`
    void add_point(T x, T y, S value) {
        assert(!_built);
        _points.emplace_back(x, y, value);
    }

    /// 登録した点から構築する / `O(nlog(n))`
    void build() {
        vector<tuple<T, T, S>> a = _points;
        stable_sort(a.begin(), a.end(), [](const auto &x, const auto &y) { return get<0>(x) < get<0>(y); });

        _n = a.size();
        _x.resize(_n);
        _y.clear();
        _y.reserve(_n);
        vector<S> data(_n);
        for (int i = 0; i < _n; ++i) {
            auto &[x, y, value] = a[i];
            _x[i] = x;
            _y.emplace_back(y);
            data[i] = value;
        }
        sort(_y.begin(), _y.end());
        _y.erase(unique(_y.begin(), _y.end()), _y.end());

        vector<int> y(_n);
        for (int i = 0; i < _n; ++i) {
            auto &[x, py, value] = a[i];
            y[i] = lower_bound(_y.begin(), _y.end(), py) - _y.begin();
        }
        _log = bit_length(_y.empty() ? 0 : _y.size() - 1);
        _v.assign(_log, BitVector());
        _mid.assign(_log, 0);
        _seg.clear();
        _seg.reserve(_log);
        _original = SegmentTree<S, op, e>(data);

        vector<int> ny(_n);
        vector<S> ndata(_n);
        for (int bit = _log - 1; bit >= 0; --bit) {
            _seg.emplace_back(data);
            _v[bit] = BitVector(_n);
            int zeros = 0;
            for (int i = 0; i < _n; ++i) {
                bool b = (y[i] >> bit) & 1;
                if (b) {
                    _v[bit].set(i);
                } else {
                    ++zeros;
                }
            }
            _v[bit].build();
            _mid[bit] = zeros;

            int zero = 0;
            int one = zeros;
            for (int i = 0; i < _n; ++i) {
                bool b = (y[i] >> bit) & 1;
                int p = b ? one++ : zero++;
                ny[p] = y[i];
                ndata[p] = data[i];
            }
            y.swap(ny);
            data.swap(ndata);
        }
        reverse(_seg.begin(), _seg.end());
        _leaf = SegmentTree<S, op, e>(data);
        _built = true;
    }

    /// `x` が `[x1, x2)` にある点のモノイド積を返す / `O(log(n))`
    S range_prod(T x1, T x2) const {
        auto [l, r] = _x_range(x1, x2);
        return _original.prod(l, r);
    }

    /// 長方形 `[x1, x2) × [y1, y2)` にある点のモノイド積を返す / `O(log^2(n))`
    /// 例: `op = max` なら、長方形内に登録した値の最大値を返す
    S range_prod(T x1, T x2, T y1, T y2) const {
        if (y1 >= y2) return e();
        auto [l, r] = _x_range(x1, x2);
        int lower = lower_bound(_y.begin(), _y.end(), y1) - _y.begin();
        int upper = lower_bound(_y.begin(), _y.end(), y2) - _y.begin();
        return _range_prod(l, r, lower, upper);
    }

    /// 登録点数を返す / `O(1)`
    int len() const { return _n; }
};

} // namespace titan23
