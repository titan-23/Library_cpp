/// https://github.com/titan-23/Library_cpp/blob/main/titan_cpplib/ds/wavelet_matrix_2d_min.cpp
#pragma once

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <limits>
#include <numeric>
#include <tuple>
#include <utility>
#include <vector>
#include "titan_cpplib/ds/bit_vector.cpp"
#include "titan_cpplib/ds/static_RmQ.cpp"
#include "titan_cpplib/others/bit.cpp"
using namespace std;

namespace titan23 {

/**
 * @brief オフライン2次元点集合の最小値・最大値
 *
 * 同じ座標へ複数回登録した点は別々の点として扱う。
 * 同値の場合、先に登録した点を `range_argmin` / `range_argmax` が返す。
 * 各重みは `minus_inf < weight < inf` を満たすことを前提とする。
 *
 * @tparam T 座標の型
 * @tparam W 重みの型
 */
template<typename T, typename W>
class WaveletMatrix2DMin {
private:
    template<bool MAX>
    struct RmQItem {
        W value;
        int index;

        friend bool operator<(const RmQItem &a, const RmQItem &b) {
            if constexpr (MAX) {
                if (b.value < a.value) return true;
                if (a.value < b.value) return false;
            } else {
                if (a.value < b.value) return true;
                if (b.value < a.value) return false;
            }
            return a.index < b.index;
        }

        friend bool operator>(const RmQItem &a, const RmQItem &b) { return b < a; }
    };

    using MinItem = RmQItem<false>;
    using MaxItem = RmQItem<true>;

    vector<tuple<T, T, W>> _points;
    vector<T> _x;
    vector<T> _y;
    vector<BitVector> _v;
    vector<int> _mid;
    vector<StaticRmQ<MinItem>> _min;
    vector<StaticRmQ<MaxItem>> _max;
    StaticRmQ<MinItem> _leaf_min;
    StaticRmQ<MaxItem> _leaf_max;
    W _inf;
    W _minus_inf;
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

    template<typename Item>
    Item _range_query(int l, int r, int lower, int upper, const vector<StaticRmQ<Item>> &rmq, const StaticRmQ<Item> &leaf, Item identity) const {
        if (l == r || lower >= upper) return identity;

        Item ans = identity;
        auto dfs = [&](auto &&dfs, int bit, int ql, int qr, uint64_t a, uint64_t b) -> void {
            if (ql == qr || b <= static_cast<uint64_t>(lower) || static_cast<uint64_t>(upper) <= a) return;
            if (static_cast<uint64_t>(lower) <= a && b <= static_cast<uint64_t>(upper)) {
                ans = min(ans, bit < 0 ? leaf.prod(ql, qr) : rmq[bit].prod(ql, qr));
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

    pair<int, int> _query_range(T y1, T y2) const {
        assert(y1 <= y2);
        int lower = lower_bound(_y.begin(), _y.end(), y1) - _y.begin();
        int upper = lower_bound(_y.begin(), _y.end(), y2) - _y.begin();
        return {lower, upper};
    }

public:
    WaveletMatrix2DMin() : WaveletMatrix2DMin(numeric_limits<W>::max(), numeric_limits<W>::lowest()) {}

    explicit WaveletMatrix2DMin(W inf) : WaveletMatrix2DMin(inf, numeric_limits<W>::lowest()) {}

    WaveletMatrix2DMin(W inf, W minus_inf) : _inf(inf), _minus_inf(minus_inf), _log(0), _n(0), _built(false) {}

    WaveletMatrix2DMin(const vector<tuple<T, T, W>> &points, W inf = numeric_limits<W>::max(), W minus_inf = numeric_limits<W>::lowest())
        : WaveletMatrix2DMin(inf, minus_inf) {
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
        assert(_minus_inf < weight && weight < _inf);
        _points.emplace_back(x, y, weight);
    }

    /// 登録した点から構築する / `O(nlog^2(n))`
    void build() {
        _n = _points.size();
        vector<int> order(_n);
        iota(order.begin(), order.end(), 0);
        stable_sort(order.begin(), order.end(), [&](int a, int b) { return get<0>(_points[a]) < get<0>(_points[b]); });

        _x.resize(_n);
        _y.clear();
        _y.reserve(_n);
        vector<int> a(_n), b(_n);
        vector<MinItem> mn(_n), nmn(_n);
        vector<MaxItem> mx(_n), nmx(_n);
        for (int i = 0; i < _n; ++i) {
            int p = order[i];
            auto &[x, y, weight] = _points[p];
            assert(_minus_inf < weight && weight < _inf);
            _x[i] = x;
            _y.emplace_back(y);
            mn[i] = {weight, p};
            mx[i] = {weight, p};
        }
        sort(_y.begin(), _y.end());
        _y.erase(unique(_y.begin(), _y.end()), _y.end());
        for (int i = 0; i < _n; ++i) {
            auto &[x, y, weight] = _points[order[i]];
            a[i] = lower_bound(_y.begin(), _y.end(), y) - _y.begin();
        }

        _log = bit_length(_y.empty() ? 0 : _y.size() - 1);
        _v.assign(_log, BitVector());
        _mid.assign(_log, 0);
        _min.assign(_log, StaticRmQ<MinItem>());
        _max.assign(_log, StaticRmQ<MaxItem>());
        MinItem min_inf{_inf, -1};
        MaxItem max_inf{_minus_inf, -1};

        for (int bit = _log - 1; bit >= 0; --bit) {
            _min[bit] = StaticRmQ<MinItem>(mn, min_inf);
            _max[bit] = StaticRmQ<MaxItem>(mx, max_inf);
            _v[bit] = BitVector(_n);
            int zeros = 0;
            for (int i = 0; i < _n; ++i) {
                bool f = (a[i] >> bit) & 1;
                if (f) {
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
                bool f = (a[i] >> bit) & 1;
                int p = f ? one++ : zero++;
                b[p] = a[i];
                nmn[p] = mn[i];
                nmx[p] = mx[i];
            }
            a.swap(b);
            mn.swap(nmn);
            mx.swap(nmx);
        }
        _leaf_min = StaticRmQ<MinItem>(mn, min_inf);
        _leaf_max = StaticRmQ<MaxItem>(mx, max_inf);
        _built = true;
    }

    /// 長方形 `[x1, x2) × [y1, y2)` にある重みの最小値を返す / `O(log(n))`
    W range_min(T x1, T x2, T y1, T y2) const {
        auto [l, r] = _x_range(x1, x2);
        auto [lower, upper] = _query_range(y1, y2);
        return _range_query(l, r, lower, upper, _min, _leaf_min, MinItem{_inf, -1}).value;
    }

    /// 長方形内で最小の `(weight, x, y)` を返す / `O(log(n))`
    /// 例: `[(0, 1, 7), (2, 3, 4)]` の全体では `(4, 2, 3)`
    tuple<W, T, T> range_argmin(T x1, T x2, T y1, T y2) const {
        auto [l, r] = _x_range(x1, x2);
        auto [lower, upper] = _query_range(y1, y2);
        MinItem ans = _range_query(l, r, lower, upper, _min, _leaf_min, MinItem{_inf, -1});
        assert(ans.index >= 0);
        auto &[x, y, weight] = _points[ans.index];
        return {weight, x, y};
    }

    /// 長方形 `[x1, x2) × [y1, y2)` にある重みの最大値を返す / `O(log(n))`
    W range_max(T x1, T x2, T y1, T y2) const {
        auto [l, r] = _x_range(x1, x2);
        auto [lower, upper] = _query_range(y1, y2);
        return _range_query(l, r, lower, upper, _max, _leaf_max, MaxItem{_minus_inf, -1}).value;
    }

    /// 長方形内で最大の `(weight, x, y)` を返す / `O(log(n))`
    /// 例: `[(0, 1, 7), (2, 3, 4)]` の全体では `(7, 0, 1)`
    tuple<W, T, T> range_argmax(T x1, T x2, T y1, T y2) const {
        auto [l, r] = _x_range(x1, x2);
        auto [lower, upper] = _query_range(y1, y2);
        MaxItem ans = _range_query(l, r, lower, upper, _max, _leaf_max, MaxItem{_minus_inf, -1});
        assert(ans.index >= 0);
        auto &[x, y, weight] = _points[ans.index];
        return {weight, x, y};
    }

    /// 登録点数を返す / `O(1)`
    int len() const { return _n; }
};

} // namespace titan23
