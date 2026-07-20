#pragma once

#include <algorithm>
#include <cassert>
#include <iostream>
#include <queue>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>
#include "titan_cpplib/ds/bit_vector.cpp"
#include "titan_cpplib/ds/fenwick_tree.cpp"
using namespace std;

// WaveletMatrixFenwick
namespace titan23 {

/**
 * @brief 静的なキーと更新可能な重みを持つウェーブレット行列
 *
 * @tparam T キーの型
 * @tparam W 重みの型
 */
template<typename T, typename W>
class WaveletMatrixFenwick {
private:
    T _sigma;
    int _log;
    int _n;
    bool _weight_is_key;
    vector<BitVector> _v;
    vector<int> _mid;
    vector<W> _weights;
    vector<FenwickTree<W>> _zero_sum;
    FenwickTree<W> _original_sum;
    FenwickTree<W> _leaf_sum;

    int bit_length(const unsigned long long n) const { return n == 0 ? 0 : 64 - __builtin_clzll(n); }

    W _zero_range_sum(const int bit, const int l, const int r) const { return _zero_sum[bit].sum(l, r); }

    void _build(const vector<T> &keys, const vector<W> &weights) {
        assert(keys.size() == weights.size());
        for (T key : keys) assert(0 <= key && key < _sigma);

        _weights = weights;
        _original_sum = FenwickTree<W>(weights);

        vector<T> a = keys, na(_n);
        vector<W> w = weights, nw(_n);
        vector<W> zero_weights(_n);

        for (int bit = _log - 1; bit >= 0; --bit) {
            _v[bit] = BitVector(_n);
            int zeros = 0;
            for (int i = 0; i < _n; ++i) {
                bool b = (a[i] >> bit) & 1;
                if (b) {
                    _v[bit].set(i);
                    zero_weights[i] = W(0);
                } else {
                    ++zeros;
                    zero_weights[i] = w[i];
                }
            }
            _v[bit].build();
            _mid[bit] = zeros;
            _zero_sum[bit] = FenwickTree<W>(zero_weights);

            int zero = 0;
            int one = zeros;
            for (int i = 0; i < _n; ++i) {
                bool b = (a[i] >> bit) & 1;
                int p = b ? one++ : zero++;
                na[p] = a[i];
                nw[p] = w[i];
            }
            a.swap(na);
            w.swap(nw);
        }

        _leaf_sum = FenwickTree<W>(w);
    }

    W _sum_k(int l, int r, int k, const bool largest) const {
        if (k == 0) return W(0);
        W sum = range_sum(l, r);
        if (k == r - l) return sum;
        if (_log == 0) return _leaf_sum.sum(l, l + k);

        W result = 0;
        for (int bit = _log - 1; bit >= 0; --bit) {
            int l0 = _v[bit].rank0(l);
            int r0 = _v[bit].rank0(r);
            int l1 = _mid[bit] + l - l0;
            int r1 = _mid[bit] + r - r0;
            int count0 = r0 - l0;
            int count1 = r1 - l1;
            W sum0 = _zero_range_sum(bit, l, r);
            W sum1 = sum - sum0;
            int preferred_count = largest ? count1 : count0;
            W preferred_sum = largest ? sum1 : sum0;

            if (k <= preferred_count) {
                if (k == preferred_count) return result + preferred_sum;
                if (bit == 0) {
                    int begin = largest ? l1 : l0;
                    return result + _leaf_sum.sum(begin, begin + k);
                }
                if (largest) {
                    l = l1;
                    r = r1;
                } else {
                    l = l0;
                    r = r0;
                }
                sum = preferred_sum;
                continue;
            }

            result += preferred_sum;
            k -= preferred_count;
            if (bit == 0) {
                int begin = largest ? l0 : l1;
                return result + _leaf_sum.sum(begin, begin + k);
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
        return result;
    }

    int _leaf_min_count(const int l, const int r, const T key, const W target) const {
        if constexpr (is_integral_v<T> && is_integral_v<W>) {
            if (_weight_is_key) {
                W weight = static_cast<W>(key);
                assert(weight > 0);
                return static_cast<int>(target / weight + (target % weight != 0));
            }
        }
        int position = _leaf_sum.bisect_left(_leaf_sum.pref(l) + target);
        assert(l <= position && position < r);
        return position - l + 1;
    }

    int _min_count_sum_ge(int l, int r, W target, const bool largest) const {
        if (target <= W(0)) return 0;
        W sum = range_sum(l, r);
        if (sum < target) return -1;
        if (_log == 0) return _leaf_min_count(l, r, 0, target);

        int result = 0;
        T key = 0;
        for (int bit = _log - 1; bit >= 0; --bit) {
            int l0 = _v[bit].rank0(l);
            int r0 = _v[bit].rank0(r);
            int l1 = _mid[bit] + l - l0;
            int r1 = _mid[bit] + r - r0;
            int count0 = r0 - l0;
            int count1 = r1 - l1;
            W sum0 = _zero_range_sum(bit, l, r);
            W sum1 = sum - sum0;
            int preferred_count = largest ? count1 : count0;
            W preferred_sum = largest ? sum1 : sum0;

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
                int begin = b ? l1 : l0;
                int end = b ? r1 : r0;
                return result + _leaf_min_count(begin, end, key, target);
            }
            if (b) {
                l = l1;
                r = r1;
                sum = sum1;
            } else {
                l = l0;
                r = r0;
                sum = sum0;
            }
        }
        return result;
    }

    template<class Pred>
    pair<int, W> _leaf_max_right(const int l, const int r, const W initial, Pred &pred) const {
        W base = _leaf_sum.pref(l);
        int index = 0;
        W prefix = 0;
        for (int step = _leaf_sum._s; step > 0; step >>= 1) {
            int next = index + step;
            if (next > r) continue;
            W next_prefix = prefix + _leaf_sum._tree[next];
            if (next <= l || pred(initial + next_prefix - base)) {
                index = next;
                prefix = next_prefix;
            }
        }
        return {index - l, initial + prefix - base};
    }

    template<class Pred>
    tuple<int, T, W> _max_right(int l, int r, const bool largest, Pred pred) const {
        assert(0 <= l && l <= r && r <= len());
        assert(pred(W(0)));
        int length = r - l;
        W sum = range_sum(l, r);
        if (pred(sum)) return {length, static_cast<T>(-1), sum};

        int count = 0;
        W aggregate = 0;
        T key = 0;
        if (_log == 0) {
            auto [accepted, sum] = _leaf_max_right(l, r, aggregate, pred);
            return {accepted, 0, sum};
        }

        for (int bit = _log - 1; bit >= 0; --bit) {
            int l0 = _v[bit].rank0(l);
            int r0 = _v[bit].rank0(r);
            int l1 = _mid[bit] + l - l0;
            int r1 = _mid[bit] + r - r0;
            int count0 = r0 - l0;
            int count1 = r1 - l1;
            W sum0 = _zero_range_sum(bit, l, r);
            W sum1 = sum - sum0;
            int preferred_count = largest ? count1 : count0;
            W preferred_sum = largest ? sum1 : sum0;

            bool b;
            if (pred(aggregate + preferred_sum)) {
                count += preferred_count;
                aggregate += preferred_sum;
                b = !largest;
            } else {
                b = largest;
            }
            if (b) key |= static_cast<T>(1) << bit;

            if (bit == 0) {
                int begin = b ? l1 : l0;
                int end = b ? r1 : r0;
                auto [accepted, sum] = _leaf_max_right(begin, end, aggregate, pred);
                return {count + accepted, key, sum};
            }
            if (b) {
                l = l1;
                r = r1;
                sum = sum1;
            } else {
                l = l0;
                r = r0;
                sum = sum0;
            }
        }
        assert(false);
        return {};
    }

    W _quantile_target(const W total, const long long numerator, const long long denominator) const {
        if constexpr (is_integral_v<W>) {
            W den = static_cast<W>(denominator);
            W num = static_cast<W>(numerator);
            return total / den * num + (total % den * num + den - 1) / den;
        } else {
            return total * static_cast<W>(numerator) / static_cast<W>(denominator);
        }
    }

public:
    /// 空のFenwick付きウェーブレット行列を作成する / `O(1)`
    WaveletMatrixFenwick() : _sigma(1), _log(0), _n(0), _weight_is_key(false), _original_sum(0), _leaf_sum(0) {}

    /// 各キーが `[0, sigma)` の空のFenwick付きウェーブレット行列を作成する / `O(log(σ))`
    WaveletMatrixFenwick(const T sigma)
        : _sigma(sigma), _log(bit_length(static_cast<unsigned long long>(sigma - 1))), _n(0), _weight_is_key(false), _v(_log), _mid(_log), _zero_sum(_log), _original_sum(0), _leaf_sum(0) {
        assert(sigma > 0);
        _build({}, {});
    }

    /// キーと重みからFenwick付きウェーブレット行列を作成する / `O(nlog(σ))`
    WaveletMatrixFenwick(const T sigma, const vector<T> &keys, const vector<W> &weights)
        : _sigma(sigma), _log(bit_length(static_cast<unsigned long long>(sigma - 1))), _n(keys.size()), _weight_is_key(false), _v(_log), _mid(_log), _zero_sum(_log) {
        assert(sigma > 0);
        _build(keys, weights);
    }

    /// `weight = key` としてFenwick付きウェーブレット行列を作成する / `O(nlog(σ))`
    WaveletMatrixFenwick(const T sigma, const vector<T> &keys)
        : _sigma(sigma), _log(bit_length(static_cast<unsigned long long>(sigma - 1))), _n(keys.size()), _weight_is_key(true), _v(_log), _mid(_log), _zero_sum(_log) {
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
        return _weights[k];
    }

    /// `k` 番目の `(key, weight)` を返す / `O(log(σ))`
    pair<T, W> access_pair(const int k) const { return {access(k), access_weight(k)}; }

    /// `k` 番目の重みを `weight` に変更する / `O(log(n)log(σ))`
    void set_weight(const int k, const W weight) {
        assert(0 <= k && k < len());
        add_weight(k, weight - _weights[k]);
    }

    /// `k` 番目の重みに `delta` を加える / `O(log(n)log(σ))`
    void add_weight(const int k, const W delta) {
        assert(0 <= k && k < len());
        if (delta == W(0)) return;
        _weight_is_key = false;
        _weights[k] += delta;
        _original_sum.add(k, delta);
        int position = k;
        for (int bit = _log - 1; bit >= 0; --bit) {
            bool b = _v[bit].access(position);
            if (!b) _zero_sum[bit].add(position, delta);
            position = b ? _mid[bit] + _v[bit].rank1(position) : _v[bit].rank0(position);
        }
        _leaf_sum.add(position, delta);
    }

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

    /// 区間 `[l, r)` でキーが `[lower, upper)` にある `k` 番目の位置を返す / `O(log(n)log(σ))`
    /// 例: `[5, 1, 4, 1, 9]`, `[lower, upper) = [1, 5)`, `k = 2` なら `3`
    int kth_index_in_value_range(const int l, const int r, const T lower, const T upper, const int k) const {
        assert(0 <= k && k < range_freq(l, r, lower, upper));
        int left = l;
        int right = r;
        while (right - left > 1) {
            int middle = (left + right) / 2;
            if (range_freq(l, middle, lower, upper) <= k) {
                left = middle;
            } else {
                right = middle;
            }
        }
        return right - 1;
    }

    /// 区間 `[l, r)` でキーが `[lower, upper)` にある最初の位置を返す / `O(log(n)log(σ))`
    /// 例: `[5, 1, 4, 1, 9]`, `[lower, upper) = [1, 5)` なら `1`
    int next_index_in_value_range(const int l, const int r, const T lower, const T upper) const {
        return range_freq(l, r, lower, upper) == 0 ? -1 : kth_index_in_value_range(l, r, lower, upper, 0);
    }

    /// 区間 `[l, r)` でキーが `[lower, upper)` にある最後の位置を返す / `O(log(n)log(σ))`
    /// 例: `[5, 1, 4, 1, 9]`, `[lower, upper) = [1, 5)` なら `3`
    int prev_index_in_value_range(const int l, const int r, const T lower, const T upper) const {
        int count = range_freq(l, r, lower, upper);
        return count == 0 ? -1 : kth_index_in_value_range(l, r, lower, upper, count - 1);
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
            int l0 = _v[bit].rank0(l);
            int r0 = _v[bit].rank0(r);
            int count0 = r0 - l0;
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
        int majority = (r - l) / 2 + 1;
        T result = 0;
        for (int bit = _log - 1; bit >= 0; --bit) {
            int l0 = _v[bit].rank0(l);
            int r0 = _v[bit].rank0(r);
            int count0 = r0 - l0;
            int count1 = (r - l) - count0;
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
            auto [length, key, left, bit] = heap.top();
            heap.pop();
            if (bit < 0) {
                result.emplace_back(key, length);
                --k;
                continue;
            }
            int right = left + length;
            int l0 = _v[bit].rank0(left);
            int r0 = _v[bit].rank0(right);
            if (l0 < r0) heap.emplace(r0 - l0, key, l0, bit - 1);
            int l1 = _mid[bit] + left - l0;
            int r1 = _mid[bit] + right - r0;
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
            int l0 = _v[bit].rank0(l);
            int r0 = _v[bit].rank0(r);
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
        int count = range_freq(l, r, upper);
        return count == 0 ? static_cast<T>(-1) : kth_smallest(l, r, count - 1);
    }

    /// 区間 `[l, r)` で `lower` 以上のうち最小のキーを返す / `O(log(σ))`
    T next_value(const int l, const int r, const T lower) const {
        int count = range_freq(l, r, lower);
        return count == r - l ? static_cast<T>(-1) : kth_smallest(l, r, count);
    }

    /// 区間 `[l, r)` の重みの総和を返す / `O(log(n))`
    W range_sum(const int l, const int r) const {
        assert(0 <= l && l <= r && r <= len());
        return _original_sum.sum(l, r);
    }

    /// 区間 `[l, r)` で `upper` 未満のキーの個数と重みの総和を返す / `O(log(n)log(σ))`
    pair<int, W> count_sum_lt(int l, int r, const T upper) const {
        assert(0 <= l && l <= r && r <= len());
        if (upper <= 0) return {0, W(0)};
        if (upper >= _sigma) return {r - l, range_sum(l, r)};
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

    /// 区間 `[l, r)` で `upper` 未満のキーを持つ要素の重みの総和を返す / `O(log(n)log(σ))`
    W sum_lt(const int l, const int r, const T upper) const { return count_sum_lt(l, r, upper).second; }

    /// 区間 `[l, r)` で `[lower, upper)` のキーを持つ要素の重みの総和を返す / `O(log(n)log(σ))`
    W sum_range(const int l, const int r, const T lower, const T upper) const {
        if (lower >= upper) return W(0);
        return sum_lt(l, r, upper) - sum_lt(l, r, lower);
    }

    /// 区間 `[l, r)` のキーが小さい方から `k` 個の重みの総和を返す / `O(log(n)log(σ))`
    W sum_k_smallest(const int l, const int r, const int k) const {
        assert(0 <= l && l <= r && r <= len());
        assert(0 <= k && k <= r - l);
        return _sum_k(l, r, k, false);
    }

    /// 区間 `[l, r)` のキーが大きい方から `k` 個の重みの総和を返す / `O(log(n)log(σ))`
    W sum_k_largest(const int l, const int r, const int k) const {
        assert(0 <= l && l <= r && r <= len());
        assert(0 <= k && k <= r - l);
        return _sum_k(l, r, k, true);
    }

    /// 小さいキーから集約した重みに対して `pred` が真である最大範囲を返す / `O(log(n)log(σ))`
    /// 返り値は `(count, boundary_value, aggregate)`
    /// `pred(0) == true` で、要素を加える順に一度偽になると真へ戻らないことを前提とする
    /// 例: `[(1, 2), (2, 3), (3, 5)]`, `pred(sum) = (sum <= 4)` なら `(1, 2, 2)`
    template<class Pred>
    tuple<int, T, W> max_right_smallest(const int l, const int r, Pred pred) const {
        return _max_right(l, r, false, pred);
    }

    /// 大きいキーから集約した重みに対して `pred` が真である最大範囲を返す / `O(log(n)log(σ))`
    /// 返り値は `(count, boundary_value, aggregate)`
    /// `pred(0) == true` で、要素を加える順に一度偽になると真へ戻らないことを前提とする
    /// 例: `[(1, 2), (2, 3), (3, 5)]`, `pred(sum) = (sum <= 6)` なら `(1, 2, 5)`
    template<class Pred>
    tuple<int, T, W> max_right_largest(const int l, const int r, Pred pred) const {
        return _max_right(l, r, true, pred);
    }

    /// 小さいキーから採用し、重みの総和が `budget` 以下である最大個数を返す / `O(log(n)log(σ))`
    /// 全要素の重みが非負であることを前提とする
    /// 例: `[(1, 2), (2, 3), (3, 5)]`, `budget = 4` なら `1`
    int max_count_smallest_sum_le(const int l, const int r, const W budget) const {
        assert(budget >= W(0));
        auto [count, value, sum] = max_right_smallest(l, r, [&](W sum) { return sum <= budget; });
        return count;
    }

    /// 大きいキーから採用し、重みの総和が `budget` 以下である最大個数を返す / `O(log(n)log(σ))`
    /// 全要素の重みが非負であることを前提とする
    /// 例: `[(1, 2), (2, 3), (3, 5)]`, `budget = 6` なら `1`
    int max_count_largest_sum_le(const int l, const int r, const W budget) const {
        assert(budget >= W(0));
        auto [count, value, sum] = max_right_largest(l, r, [&](W sum) { return sum <= budget; });
        return count;
    }

    /// 小さいキーからの累積重みが初めて `target` 以上になるキーを返す / `O(log(n)log(σ))`
    /// 全要素の重みが非負で、`0 < target <= range_sum(l, r)` であることを前提とする
    /// 例: `[(1, 2), (2, 3), (3, 5)]`, `target = 4` なら `2`
    T weighted_quantile(const int l, const int r, const W target) const {
        assert(W(0) < target && target <= range_sum(l, r));
        auto [count, value, sum] = max_right_smallest(l, r, [&](W sum) { return sum < target; });
        return value;
    }

    /// 重み付き `numerator / denominator` 分位点を返す / `O(log(n)log(σ))`
    /// 全要素の重みが非負であることを前提とする
    /// 例: `[(1, 2), (2, 3), (3, 5)]`, `numerator / denominator = 1 / 2` なら `2`
    T weighted_quantile(const int l, const int r, const long long numerator, const long long denominator) const {
        assert(0 < numerator && numerator <= denominator);
        W total = range_sum(l, r);
        assert(total > W(0));
        return weighted_quantile(l, r, _quantile_target(total, numerator, denominator));
    }

    /// 重み付き中央値を返す / `O(log(n)log(σ))`
    /// 全要素の重みが非負であることを前提とする
    /// 例: `[(1, 2), (2, 3), (3, 5)]` なら `2`
    T weighted_median(const int l, const int r) const {
        return weighted_quantile(l, r, 1, 2);
    }

    /// 小さいキーから採用し、重みの総和が `target` 以上になる最小個数を返す / `O(log(n)log(σ))`
    /// 全要素の重みが非負であることを前提とし、達成できない場合は `-1` を返す
    /// 例: `[(1, 2), (2, 3), (3, 5)]`, `target = 4` なら `2`
    int min_count_smallest_sum_ge(const int l, const int r, const W target) const {
        assert(0 <= l && l <= r && r <= len());
        return _min_count_sum_ge(l, r, target, false);
    }

    /// 大きいキーから採用し、重みの総和が `target` 以上になる最小個数を返す / `O(log(n)log(σ))`
    /// 全要素の重みが非負であることを前提とし、達成できない場合は `-1` を返す
    /// 例: `[(1, 2), (2, 3), (3, 5)]`, `target = 4` なら `1`
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
    vector<W> toweights() const { return _weights; }

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
    friend ostream& operator<<(ostream& os, const WaveletMatrixFenwick<T, W> &wm) {
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
