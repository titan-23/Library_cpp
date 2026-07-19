#pragma once

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <iostream>
#include <numeric>
#include <queue>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>
#include "titan_cpplib/ds/bit_vector.cpp"
using namespace std;

// WaveletMatrixSum
namespace titan23 {

/**
 * @brief 総和付き静的ウェーブレット行列
 *
 * @tparam T キーの型
 * @tparam W 重みの型
 */
template<typename T, typename W>
class WaveletMatrixSum {
private:
    T _sigma;
    int _log;
    int _n;
    int _stride;
    bool _weight_is_key;
    vector<BitVector> _v;
    vector<int> _mid;
    vector<W> _zero_sum;
    vector<W> _original_sum;
    vector<W> _leaf_sum;

    int bit_length(const unsigned long long n) const { return n == 0 ? 0 : 64 - __builtin_clzll(n); }

    W _zero_range_sum(const int bit, const int l, const int r) const {
        const size_t base = static_cast<size_t>(bit) * _stride;
        return _zero_sum[base + r] - _zero_sum[base + l];
    }

    void _build(const vector<T> &keys, const vector<W> &weights) {
        assert(keys.size() == weights.size());
        for (const T key : keys) assert(0 <= key && key < _sigma);

        _original_sum.assign(_n + 1, W(0));
        for (int i = 0; i < _n; ++i) _original_sum[i + 1] = _original_sum[i] + weights[i];

        vector<T> current_keys = keys;
        vector<T> work_keys(_n);
        vector<W> current_weights = weights;
        vector<W> work_weights(_n);
        _zero_sum.assign(static_cast<size_t>(_log) * _stride, W(0));

        for (int bit = _log - 1; bit >= 0; --bit) {
            _v[bit] = BitVector(_n);
            const size_t base = static_cast<size_t>(bit) * _stride;
            int zeros = 0;
            for (int i = 0; i < _n; ++i) {
                const bool b = (current_keys[i] >> bit) & 1;
                if (b) {
                    _v[bit].set(i);
                } else {
                    ++zeros;
                }
                _zero_sum[base + i + 1] = _zero_sum[base + i] + (b ? W(0) : current_weights[i]);
            }
            _v[bit].build();
            _mid[bit] = zeros;

            int zero = 0;
            int one = zeros;
            for (int i = 0; i < _n; ++i) {
                const bool b = (current_keys[i] >> bit) & 1;
                const int index = b ? one++ : zero++;
                work_keys[index] = current_keys[i];
                work_weights[index] = current_weights[i];
            }
            current_keys.swap(work_keys);
            current_weights.swap(work_weights);
        }

        _leaf_sum.assign(_n + 1, W(0));
        for (int i = 0; i < _n; ++i) _leaf_sum[i + 1] = _leaf_sum[i] + current_weights[i];
    }

    W _sum_k(int l, int r, int k, const bool largest) const {
        if (k == 0) return W(0);
        W current_sum = range_sum(l, r);
        if (k == r - l) return current_sum;
        if (_log == 0) return _leaf_sum[l + k] - _leaf_sum[l];

        W result = 0;
        for (int bit = _log - 1; bit >= 0; --bit) {
            const int l0 = _v[bit].rank0(l);
            const int r0 = _v[bit].rank0(r);
            const int l1 = _mid[bit] + l - l0;
            const int r1 = _mid[bit] + r - r0;
            const int count0 = r0 - l0;
            const int count1 = r1 - l1;
            const W sum0 = _zero_range_sum(bit, l, r);
            const W sum1 = current_sum - sum0;
            const int preferred_count = largest ? count1 : count0;
            const W preferred_sum = largest ? sum1 : sum0;

            if (k <= preferred_count) {
                if (k == preferred_count) return result + preferred_sum;
                if (bit == 0) {
                    const int begin = largest ? l1 : l0;
                    return result + _leaf_sum[begin + k] - _leaf_sum[begin];
                }
                if (largest) {
                    l = l1;
                    r = r1;
                } else {
                    l = l0;
                    r = r0;
                }
                current_sum = preferred_sum;
                continue;
            }

            result += preferred_sum;
            k -= preferred_count;
            if (bit == 0) {
                const int begin = largest ? l0 : l1;
                return result + _leaf_sum[begin + k] - _leaf_sum[begin];
            }
            if (largest) {
                l = l0;
                r = r0;
                current_sum = sum0;
            } else {
                l = l1;
                r = r1;
                current_sum = sum1;
            }
        }
        return result;
    }

    int _leaf_min_count(const int l, const int r, const T key, const W target) const {
        if constexpr (is_integral_v<T> && is_integral_v<W>) {
            if (_weight_is_key) {
                const W weight = static_cast<W>(key);
                assert(weight > 0);
                return static_cast<int>(target / weight + (target % weight != 0));
            }
        }
        const W goal = _leaf_sum[l] + target;
        const auto it = lower_bound(_leaf_sum.begin() + l + 1, _leaf_sum.begin() + r + 1, goal);
        assert(it != _leaf_sum.begin() + r + 1);
        return it - (_leaf_sum.begin() + l);
    }

    int _min_count_sum_ge(int l, int r, W target, const bool largest) const {
        if (target <= W(0)) return 0;
        W current_sum = range_sum(l, r);
        if (current_sum < target) return -1;
        if (_log == 0) return _leaf_min_count(l, r, 0, target);

        int result = 0;
        T key = 0;
        for (int bit = _log - 1; bit >= 0; --bit) {
            const int l0 = _v[bit].rank0(l);
            const int r0 = _v[bit].rank0(r);
            const int l1 = _mid[bit] + l - l0;
            const int r1 = _mid[bit] + r - r0;
            const int count0 = r0 - l0;
            const int count1 = r1 - l1;
            const W sum0 = _zero_range_sum(bit, l, r);
            const W sum1 = current_sum - sum0;
            const int preferred_count = largest ? count1 : count0;
            const W preferred_sum = largest ? sum1 : sum0;

            bool b;
            if (preferred_sum >= target) {
                b = largest;
            } else {
                result += preferred_count;
                target -= preferred_sum;
                b = !largest;
            }
            if (b) key |= static_cast<T>(1) << bit;

            if (bit == 0) {
                const int begin = b ? l1 : l0;
                const int end = b ? r1 : r0;
                return result + _leaf_min_count(begin, end, key, target);
            }
            if (b) {
                l = l1;
                r = r1;
                current_sum = sum1;
            } else {
                l = l0;
                r = r0;
                current_sum = sum0;
            }
        }
        return result;
    }

public:
    /// 空の総和付きウェーブレット行列を作成する / `O(1)`
    WaveletMatrixSum() : _sigma(1), _log(0), _n(0), _stride(1), _weight_is_key(false), _original_sum(1, W(0)), _leaf_sum(1, W(0)) {}

    /// 各キーが `[0, sigma)` の空の総和付きウェーブレット行列を作成する / `O(log(σ))`
    WaveletMatrixSum(const T sigma)
        : _sigma(sigma), _log(bit_length(static_cast<unsigned long long>(sigma - 1))), _n(0), _stride(1), _weight_is_key(false), _v(_log), _mid(_log), _original_sum(1, W(0)), _leaf_sum(1, W(0)) {
        assert(sigma > 0);
        _build({}, {});
    }

    /// キーと重みから総和付きウェーブレット行列を作成する / `O(nlog(σ))`
    WaveletMatrixSum(const T sigma, const vector<T> &keys, const vector<W> &weights)
        : _sigma(sigma), _log(bit_length(static_cast<unsigned long long>(sigma - 1))), _n(keys.size()), _stride(_n + 1), _weight_is_key(false), _v(_log), _mid(_log) {
        assert(sigma > 0);
        _build(keys, weights);
    }

    /// `weight = key` として総和付きウェーブレット行列を作成する / `O(nlog(σ))`
    WaveletMatrixSum(const T sigma, const vector<T> &keys)
        : _sigma(sigma), _log(bit_length(static_cast<unsigned long long>(sigma - 1))), _n(keys.size()), _stride(_n + 1), _weight_is_key(true), _v(_log), _mid(_log) {
        assert(sigma > 0);
        vector<W> weights(keys.begin(), keys.end());
        _build(keys, weights);
    }

    /// `k` 番目のキーを返す / `O(log(σ))`
    T access(int k) const {
        assert(0 <= k && k < len());
        T result = 0;
        for (int bit = _log - 1; bit >= 0; --bit) {
            if (_v[bit].access(k)) {
                result |= static_cast<T>(1) << bit;
                k = _v[bit].rank1(k) + _mid[bit];
            } else {
                k = _v[bit].rank0(k);
            }
        }
        return result;
    }

    /// `k` 番目の重みを返す / `O(1)`
    W access_weight(const int k) const {
        assert(0 <= k && k < len());
        return _original_sum[k + 1] - _original_sum[k];
    }

    /// `k` 番目の `(key, weight)` を返す / `O(log(σ))`
    pair<T, W> access_pair(const int k) const { return {access(k), access_weight(k)}; }

    /// 区間 `[0, r)` の `key` の個数を返す / `O(log(σ))`
    int rank(int r, const T key) const {
        assert(0 <= r && r <= len());
        assert(0 <= key && key < _sigma);
        int l = 0;
        for (int bit = _log - 1; bit >= 0; --bit) {
            if ((key >> bit) & 1) {
                l = _v[bit].rank1(l) + _mid[bit];
                r = _v[bit].rank1(r) + _mid[bit];
            } else {
                l = _v[bit].rank0(l);
                r = _v[bit].rank0(r);
            }
        }
        return r - l;
    }

    /// 区間 `[l, r)` の `key` の個数を返す / `O(log(σ))`
    int range_count(int l, int r, const T key) const {
        assert(0 <= l && l <= r && r <= len());
        assert(0 <= key && key < _sigma);
        for (int bit = _log - 1; bit >= 0; --bit) {
            if ((key >> bit) & 1) {
                l = _v[bit].rank1(l) + _mid[bit];
                r = _v[bit].rank1(r) + _mid[bit];
            } else {
                l = _v[bit].rank0(l);
                r = _v[bit].rank0(r);
            }
        }
        return r - l;
    }

    /// `k` 番目の `key` の位置を返す / `O(log(n)log(σ))`
    int select(int k, const T key) const {
        assert(0 <= key && key < _sigma);
        int position = 0;
        for (int bit = _log - 1; bit >= 0; --bit) {
            if ((key >> bit) & 1) {
                position = _mid[bit] + _v[bit].rank1(position);
            } else {
                position = _v[bit].rank0(position);
            }
        }
        position += k;
        for (int bit = 0; bit < _log; ++bit) {
            if ((key >> bit) & 1) {
                position = _v[bit].select1(position - _mid[bit]);
            } else {
                position = _v[bit].select0(position);
            }
        }
        return position;
    }

    /// 区間 `[l, r)` にある `k` 番目の `key` の位置を返す / `O(log(n)log(σ))`
    int range_select(const int l, const int r, const int k, const T key) const {
        assert(0 <= l && l <= r && r <= len());
        assert(0 <= k && k < range_count(l, r, key));
        return select(rank(l, key) + k, key);
    }

    /// 区間 `[l, r)` で昇順 `k` 番目のキーを返す / `O(log(σ))`
    T kth_smallest(int l, int r, int k) const {
        assert(0 <= l && l <= r && r <= len());
        assert(0 <= k && k < r - l);
        T result = 0;
        for (int bit = _log - 1; bit >= 0; --bit) {
            const int l0 = _v[bit].rank0(l);
            const int r0 = _v[bit].rank0(r);
            const int count0 = r0 - l0;
            if (count0 <= k) {
                result |= static_cast<T>(1) << bit;
                k -= count0;
                l = _mid[bit] + l - l0;
                r = _mid[bit] + r - r0;
            } else {
                l = l0;
                r = r0;
            }
        }
        return result;
    }

    /// 区間 `[l, r)` で降順 `k` 番目のキーを返す / `O(log(σ))`
    T kth_largest(const int l, const int r, const int k) const { return kth_smallest(l, r, r - l - k - 1); }

    /// 区間 `[l, r)` に過半数を占めるキーがあるか判定する / `O(log(σ))`
    pair<bool, T> has_majority(int l, int r) const {
        assert(0 <= l && l < r && r <= len());
        const int majority = (r - l) / 2 + 1;
        T result = 0;
        for (int bit = _log - 1; bit >= 0; --bit) {
            const int l0 = _v[bit].rank0(l);
            const int r0 = _v[bit].rank0(r);
            const int count0 = r0 - l0;
            const int count1 = (r - l) - count0;
            if (count0 >= majority) {
                l = l0;
                r = r0;
            } else if (count1 >= majority) {
                result |= static_cast<T>(1) << bit;
                l = _mid[bit] + l - l0;
                r = _mid[bit] + r - r0;
            } else {
                return {false, 0};
            }
        }
        return {true, result};
    }

    /// 区間 `[l, r)` で頻度が高いキーを最大 `k` 種類返す / 訪問Node数を `p` として `O(plog(p))`
    /// 区間内の相異なるキー数を `D` とすると、最悪 `p = O(Dlog(σ))`
    vector<pair<T, int>> topk(const int l, const int r, int k) const {
        assert(0 <= l && l <= r && r <= len());
        vector<pair<T, int>> result;
        if (l == r || k <= 0) return result;
        priority_queue<tuple<int, T, int, int>> heap;
        heap.emplace(r - l, 0, l, _log - 1);
        while (!heap.empty() && k > 0) {
            const auto [length, key, left, bit] = heap.top();
            heap.pop();
            if (bit < 0) {
                result.emplace_back(key, length);
                --k;
                continue;
            }
            const int right = left + length;
            const int l0 = _v[bit].rank0(left);
            const int r0 = _v[bit].rank0(right);
            if (l0 < r0) heap.emplace(r0 - l0, key, l0, bit - 1);
            const int l1 = _mid[bit] + left - l0;
            const int r1 = _mid[bit] + right - r0;
            if (l1 < r1) heap.emplace(r1 - l1, key | (static_cast<T>(1) << bit), l1, bit - 1);
        }
        return result;
    }

    /// 区間 `[l, r)` で `upper` 未満のキーの個数を返す / `O(log(σ))`
    int range_freq(int l, int r, const T upper) const {
        assert(0 <= l && l <= r && r <= len());
        if (upper <= 0) return 0;
        if (upper >= _sigma) return r - l;
        int result = 0;
        for (int bit = _log - 1; bit >= 0; --bit) {
            const int l0 = _v[bit].rank0(l);
            const int r0 = _v[bit].rank0(r);
            if ((upper >> bit) & 1) {
                result += r0 - l0;
                l = _mid[bit] + l - l0;
                r = _mid[bit] + r - r0;
            } else {
                l = l0;
                r = r0;
            }
        }
        return result;
    }

    /// 区間 `[l, r)` で `[lower, upper)` のキーの個数を返す / `O(log(σ))`
    int range_freq(const int l, const int r, const T lower, const T upper) const {
        if (lower >= upper) return 0;
        return range_freq(l, r, upper) - range_freq(l, r, lower);
    }

    /// 区間 `[l, r)` で `upper` 未満のうち最大のキーを返す / `O(log(σ))`
    T prev_value(const int l, const int r, const T upper) const {
        const int count = range_freq(l, r, upper);
        return count == 0 ? static_cast<T>(-1) : kth_smallest(l, r, count - 1);
    }

    /// 区間 `[l, r)` で `lower` 以上のうち最小のキーを返す / `O(log(σ))`
    T next_value(const int l, const int r, const T lower) const {
        const int count = range_freq(l, r, lower);
        return count == r - l ? static_cast<T>(-1) : kth_smallest(l, r, count);
    }

    /// 区間 `[l, r)` の重みの総和を返す / `O(1)`
    W range_sum(const int l, const int r) const {
        assert(0 <= l && l <= r && r <= len());
        return _original_sum[r] - _original_sum[l];
    }

    /// 区間 `[l, r)` で `upper` 未満のキーの個数と重みの総和を返す / `O(log(σ))`
    pair<int, W> count_sum_lt(int l, int r, const T upper) const {
        assert(0 <= l && l <= r && r <= len());
        if (upper <= 0) return {0, W(0)};
        if (upper >= _sigma) return {r - l, range_sum(l, r)};
        int count = 0;
        W sum = 0;
        for (int bit = _log - 1; bit >= 0; --bit) {
            const int l0 = _v[bit].rank0(l);
            const int r0 = _v[bit].rank0(r);
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

    /// 区間 `[l, r)` で `upper` 未満のキーを持つ要素の重みの総和を返す / `O(log(σ))`
    W sum_lt(const int l, const int r, const T upper) const { return count_sum_lt(l, r, upper).second; }

    /// 区間 `[l, r)` で `[lower, upper)` のキーを持つ要素の重みの総和を返す / `O(log(σ))`
    W sum_range(const int l, const int r, const T lower, const T upper) const {
        if (lower >= upper) return W(0);
        return sum_lt(l, r, upper) - sum_lt(l, r, lower);
    }

    /// 区間 `[l, r)` のキーが小さい方から `k` 個の重みの総和を返す / `O(log(σ))`
    W sum_k_smallest(const int l, const int r, const int k) const {
        assert(0 <= l && l <= r && r <= len());
        assert(0 <= k && k <= r - l);
        return _sum_k(l, r, k, false);
    }

    /// 区間 `[l, r)` のキーが大きい方から `k` 個の重みの総和を返す / `O(log(σ))`
    W sum_k_largest(const int l, const int r, const int k) const {
        assert(0 <= l && l <= r && r <= len());
        assert(0 <= k && k <= r - l);
        return _sum_k(l, r, k, true);
    }

    /// 小さいキーから採用し、重みの総和が `target` 以上になる最小個数を返す / 一般重みは `O(log(σ)+log(n))`、`weight = key` は `O(log(σ))`
    /// 重みが非負であることを前提とし、達成できない場合は `-1` を返す
    int min_count_smallest_sum_ge(const int l, const int r, const W target) const {
        assert(0 <= l && l <= r && r <= len());
        return _min_count_sum_ge(l, r, target, false);
    }

    /// 大きいキーから採用し、重みの総和が `target` 以上になる最小個数を返す / 一般重みは `O(log(σ)+log(n))`、`weight = key` は `O(log(σ))`
    /// 重みが非負であることを前提とし、達成できない場合は `-1` を返す
    int min_count_largest_sum_ge(const int l, const int r, const W target) const {
        assert(0 <= l && l <= r && r <= len());
        return _min_count_sum_ge(l, r, target, true);
    }

    /// 要素数を返す / `O(1)`
    int len() const { return _n; }

    /// キー列を返す / `O(nlog(σ))`
    vector<T> tovector() const {
        vector<T> result(_n);
        for (int i = 0; i < _n; ++i) result[i] = access(i);
        return result;
    }

    /// 重み列を返す / `O(n)`
    vector<W> toweights() const {
        vector<W> result(_n);
        for (int i = 0; i < _n; ++i) result[i] = _original_sum[i + 1] - _original_sum[i];
        return result;
    }

    /// `(key, weight)` の列を返す / `O(nlog(σ))`
    vector<pair<T, W>> toitems() const {
        vector<pair<T, W>> result(_n);
        for (int i = 0; i < _n; ++i) result[i] = access_pair(i);
        return result;
    }

    /// `(key, weight)` の列を表示する / `O(nlog(σ))`
    void print() const {
        const vector<pair<T, W>> items = toitems();
        cout << "[";
        for (int i = 0; i < _n; ++i) {
            if (i != 0) cout << ", ";
            cout << "(" << items[i].first << ", " << items[i].second << ")";
        }
        cout << "]" << endl;
    }

    /// `(key, weight)` の列を出力する / `O(nlog(σ))`
    friend ostream& operator<<(ostream& os, const WaveletMatrixSum<T, W> &wm) {
        const vector<pair<T, W>> items = wm.toitems();
        os << "[";
        for (int i = 0; i < wm.len(); ++i) {
            if (i != 0) os << ", ";
            os << "(" << items[i].first << ", " << items[i].second << ")";
        }
        return os << "]";
    }
};

} // namespace titan23
