#pragma once

#include <array>
#include <cassert>
#include <cstdint>
#include <iostream>
#include <limits>
#include <numeric>
#include <queue>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>
#include "titan_cpplib/ds/b_tree_bit_vector_sum.cpp"
using namespace std;

// DynamicWaveletTreeSum
namespace titan23 {

/**
 * @brief 総和付き動的ウェーブレット木
 *
 * @tparam T キーの型
 * @tparam W 重みの型
 */
template<typename T, typename W>
class DynamicWaveletTreeSum {
private:
    static constexpr int _NIL = -1;
    static constexpr int _MAX_LOG = 64;

    using BitVector = BTreeBitVectorSum<W>;
    using Sequence = typename BitVector::Sequence;

    struct Node {
        int child[2];
        Sequence v;

        Node() : child{_NIL, _NIL} {}
    };

    BitVector _bitvectors;
    vector<Node> _nodes;
    int _root;
    int _free;
    T _sigma;
    int _log;
    int _size;

    int bit_length(const unsigned long long n) const { return n == 0 ? 0 : 64 - __builtin_clzll(n); }

    size_t _estimated_node_count(int n) const {
        if (n == 0) return 1;
        size_t limit = static_cast<size_t>(n) * 3;
        size_t ans = 0;
        size_t width = 1;
        for (int depth = 0; depth < _log && ans < limit; ++depth) {
            ans += min(static_cast<size_t>(n), width);
            width = min(static_cast<size_t>(n), width * 2);
        }
        return min(ans, limit);
    }

    int _make_node() {
        if (_free != _NIL) {
            int node = _free;
            _free = _nodes[node].child[0];
            _nodes[node] = Node();
            return node;
        }
        int node = _nodes.size();
        _nodes.emplace_back();
        return node;
    }

    int _make_node(const vector<uint8_t> &bits, const vector<int> &order, const vector<W> &weights, const int start, const int end) {
        int node = _make_node();
        _nodes[node].v = _bitvectors.build(bits, order, weights, start, end);
        return node;
    }

    void _release_node(const int node) {
        assert(node != _root);
        assert(_nodes[node].child[0] == _NIL && _nodes[node].child[1] == _NIL);
        assert(_bitvectors.empty(_nodes[node].v));
        _nodes[node].child[0] = _free;
        _nodes[node].child[1] = _NIL;
        _free = node;
    }

    void _prune_path(const array<int, _MAX_LOG> &path, const array<uint8_t, _MAX_LOG> &directions, const int depth) {
        for (int i = depth - 1; i > 0; --i) {
            int node = path[i];
            if (!_bitvectors.empty(_nodes[node].v)) break;
            int parent = path[i - 1];
            _nodes[parent].child[directions[i - 1]] = _NIL;
            _release_node(node);
        }
    }

    void _build(const vector<T> &keys, const vector<W> &weights) {
        if (keys.empty()) {
            _root = _make_node();
            return;
        }

        int n = keys.size();
        size_t reserve_nodes = _estimated_node_count(n);
        long long total_size = static_cast<long long>(n) * _log;
        assert(reserve_nodes <= static_cast<size_t>(numeric_limits<int>::max()));
        assert(total_size <= numeric_limits<int>::max());
        _nodes.reserve(reserve_nodes);
        _bitvectors.reserve(static_cast<int>(total_size), static_cast<int>(reserve_nodes));

        vector<int> order(n), work(n);
        vector<uint8_t> bits(n);
        iota(order.begin(), order.end(), 0);

        auto build = [&](auto &&build, int bit, int left, int right) -> int {
            if (left == right || bit < 0) return _NIL;

            int zeros = 0;
            for (int i = left; i < right; ++i) {
                bool b = (keys[order[i]] >> bit) & 1;
                bits[i] = b;
                zeros += !b;
            }

            int node = _make_node(bits, order, weights, left, right);
            int zero = left;
            int one = left + zeros;
            for (int i = left; i < right; ++i) {
                if (bits[i]) {
                    work[one++] = order[i];
                } else {
                    work[zero++] = order[i];
                }
            }
            for (int i = left; i < right; ++i) order[i] = work[i];

            int mid = left + zeros;
            int child0 = build(build, bit - 1, left, mid);
            int child1 = build(build, bit - 1, mid, right);
            _nodes[node].child[0] = child0;
            _nodes[node].child[1] = child1;
            return node;
        };

        _root = build(build, _log - 1, 0, n);
    }

    int _range_freq_node(int node, int bit, int l, int r, const T x) const {
        int result = 0;
        while (node != _NIL && bit >= 0 && l < r) {
            auto data = _bitvectors.range_data(_nodes[node].v, l, r);
            bool b = (x >> bit) & 1;
            if (b) {
                result += data.r0 - data.l0;
                l -= data.l0;
                r -= data.r0;
            } else {
                l = data.l0;
                r = data.r0;
            }
            node = _nodes[node].child[b];
            --bit;
        }
        return result;
    }

    pair<int, W> _count_sum_lt_node(int node, int bit, int l, int r, const T x) const {
        int count = 0;
        W sum = 0;
        while (node != _NIL && bit >= 0 && l < r) {
            auto data = _bitvectors.range_data(_nodes[node].v, l, r);
            bool b = (x >> bit) & 1;
            if (b) {
                count += data.r0 - data.l0;
                sum += data.sum0;
                l -= data.l0;
                r -= data.r0;
            } else {
                l = data.l0;
                r = data.r0;
            }
            node = _nodes[node].child[b];
            --bit;
        }
        return {count, sum};
    }

    T _extreme(int node, int bit, int l, int r, T value, const bool largest) const {
        while (bit >= 0) {
            auto data = _bitvectors.range_data(_nodes[node].v, l, r);
            int count0 = data.r0 - data.l0;
            int count1 = (r - l) - count0;
            bool b = largest ? count1 > 0 : count0 == 0;
            if (b) {
                value |= static_cast<T>(1) << bit;
                l -= data.l0;
                r -= data.r0;
            } else {
                l = data.l0;
                r = data.r0;
            }
            node = _nodes[node].child[b];
            --bit;
        }
        return value;
    }

    W _sum_k(int l, int r, int k, const bool largest) const {
        if (k == 0) return W(0);
        if (k == r - l) return range_sum(l, r);
        int node = _root;
        W result = 0;
        for (int bit = _log - 1; bit >= 0; --bit) {
            auto data = _bitvectors.range_data(_nodes[node].v, l, r);
            int count0 = data.r0 - data.l0;
            int count1 = (r - l) - count0;
            int preferred_count = largest ? count1 : count0;
            W preferred_sum = largest ? data.sum1 : data.sum0;
            bool preferred_bit = largest;

            if (k <= preferred_count) {
                if (k == preferred_count) return result + preferred_sum;
                if (bit == 0) return result + _bitvectors.sum_first_k(_nodes[node].v, l, r, preferred_bit, k);
                if (preferred_bit) {
                    l -= data.l0;
                    r -= data.r0;
                } else {
                    l = data.l0;
                    r = data.r0;
                }
                node = _nodes[node].child[preferred_bit];
                continue;
            }

            result += preferred_sum;
            k -= preferred_count;
            bool other_bit = !preferred_bit;
            if (bit == 0) return result + _bitvectors.sum_first_k(_nodes[node].v, l, r, other_bit, k);
            if (other_bit) {
                l -= data.l0;
                r -= data.r0;
            } else {
                l = data.l0;
                r = data.r0;
            }
            node = _nodes[node].child[other_bit];
        }
        return result;
    }

    int _min_count_sum_ge(int l, int r, W target, const bool largest) const {
        if (target <= W(0)) return 0;
        if (l == r) return -1;
        int node = _root;
        int result = 0;
        for (int bit = _log - 1; bit >= 0; --bit) {
            auto data = _bitvectors.range_data(_nodes[node].v, l, r);
            if (bit == _log - 1 && data.sum0 + data.sum1 < target) return -1;
            int count0 = data.r0 - data.l0;
            int count1 = (r - l) - count0;
            int preferred_count = largest ? count1 : count0;
            W preferred_sum = largest ? data.sum1 : data.sum0;
            bool preferred_bit = largest;

            if (preferred_sum >= target) {
                if (bit == 0) return result + _bitvectors.min_count_sum_ge(_nodes[node].v, l, r, preferred_bit, target);
                if (preferred_bit) {
                    l -= data.l0;
                    r -= data.r0;
                } else {
                    l = data.l0;
                    r = data.r0;
                }
                node = _nodes[node].child[preferred_bit];
                continue;
            }

            result += preferred_count;
            target -= preferred_sum;
            bool other_bit = !preferred_bit;
            if (bit == 0) return result + _bitvectors.min_count_sum_ge(_nodes[node].v, l, r, other_bit, target);
            if (other_bit) {
                l -= data.l0;
                r -= data.r0;
            } else {
                l = data.l0;
                r = data.r0;
            }
            node = _nodes[node].child[other_bit];
        }
        return result;
    }

    template<class Pred>
    tuple<int, T, W> _max_right(int l, int r, const bool largest, Pred pred) const {
        assert(0 <= l && l <= r && r <= len());
        assert(pred(W(0)));
        int length = r - l;
        W sum = range_sum(l, r);
        if (pred(sum)) return {length, static_cast<T>(-1), sum};

        int node = _root;
        int count = 0;
        W aggregate = 0;
        T key = 0;
        for (int bit = _log - 1; bit >= 0; --bit) {
            auto data = _bitvectors.range_data(_nodes[node].v, l, r);
            int count0 = data.r0 - data.l0;
            int count1 = (r - l) - count0;
            int preferred_count = largest ? count1 : count0;
            W preferred_sum = largest ? data.sum1 : data.sum0;

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
                int accepted = _bitvectors.max_count(_nodes[node].v, l, r, b, aggregate, pred);
                return {count + accepted, key, aggregate};
            }
            if (b) {
                l -= data.l0;
                r -= data.r0;
            } else {
                l = data.l0;
                r = data.r0;
            }
            node = _nodes[node].child[b];
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
    /// 各キーが `[0, sigma)` の空の総和付き動的ウェーブレット木を作成する / `O(1)`
    DynamicWaveletTreeSum(const T sigma) : _root(_NIL), _free(_NIL), _sigma(sigma), _log(max(1, bit_length(static_cast<unsigned long long>(sigma - 1)))), _size(0) {
        assert(sigma > 0);
        _root = _make_node();
    }

    /// キーと重みから総和付き動的ウェーブレット木を作成する / `O(nlog(σ))`
    DynamicWaveletTreeSum(const T sigma, const vector<T> &keys, const vector<W> &weights)
        : _root(_NIL), _free(_NIL), _sigma(sigma), _log(max(1, bit_length(static_cast<unsigned long long>(sigma - 1)))), _size(keys.size()) {
        assert(sigma > 0);
        assert(keys.size() == weights.size());
        for (T key : keys) assert(0 <= key && key < sigma);
        _build(keys, weights);
    }

    /// `weight = key` として構築する / `O(nlog(σ))`
    DynamicWaveletTreeSum(const T sigma, const vector<T> &keys)
        : _root(_NIL), _free(_NIL), _sigma(sigma), _log(max(1, bit_length(static_cast<unsigned long long>(sigma - 1)))), _size(keys.size()) {
        assert(sigma > 0);
        vector<W> weights(keys.begin(), keys.end());
        for (T key : keys) assert(0 <= key && key < sigma);
        _build(keys, weights);
    }

    /// 最終的な要素数を見積もってNodeと動的ビット列の領域を予約する / 最悪 `O(nlog(σ))`
    void reserve(int expected_size) {
        assert(len() <= expected_size);
        size_t nodes = _estimated_node_count(expected_size);
        long long total_size = static_cast<long long>(expected_size) * _log;
        assert(nodes <= static_cast<size_t>(numeric_limits<int>::max()));
        assert(total_size <= numeric_limits<int>::max());
        _nodes.reserve(nodes);
        _bitvectors.reserve(static_cast<int>(total_size), static_cast<int>(nodes));
    }

    /// 位置 `k` に `(key, weight)` を挿入する / `O(log(n)log(σ))`
    void insert(int k, const T key, const W weight) {
        assert(0 <= k && k <= len());
        assert(0 <= key && key < _sigma);
        int node = _root;
        for (int bit = _log - 1; bit >= 0; --bit) {
            bool b = (key >> bit) & 1;
            int rank1 = _bitvectors.insert(_nodes[node].v, k, b, weight);
            k = b ? rank1 : k - rank1;
            if (bit == 0) break;
            int child = _nodes[node].child[b];
            if (child == _NIL) {
                child = _make_node();
                _nodes[node].child[b] = child;
            }
            node = child;
        }
        ++_size;
    }

    /// `weight = key` として挿入する / `O(log(n)log(σ))`
    void insert(const int k, const T key) { insert(k, key, static_cast<W>(key)); }

    /// 位置 `k` の `(key, weight)` を削除して返す / `O(log(n)log(σ))`
    pair<T, W> pop(int k) {
        assert(0 <= k && k < len());
        array<int, _MAX_LOG> path;
        array<uint8_t, _MAX_LOG> directions;
        int depth = 0;
        int node = _root;
        T key = 0;
        W weight = 0;

        for (int bit = _log - 1; bit >= 0; --bit) {
            path[depth] = node;
            auto data = _bitvectors.pop(_nodes[node].v, k);
            if (depth == 0) weight = data.weight;
            bool b = data.bit;
            if (b) {
                key |= static_cast<T>(1) << bit;
                k = data.rank1;
            } else {
                k -= data.rank1;
            }
            if (bit > 0) {
                directions[depth] = b;
                node = _nodes[node].child[b];
            }
            ++depth;
        }

        _prune_path(path, directions, depth);
        --_size;
        return {key, weight};
    }

    /// 位置 `k` を `(key, weight)` に更新する / `O(log(n)log(σ))`
    void set(int k, const T key, const W weight) {
        assert(0 <= k && k < len());
        assert(0 <= key && key < _sigma);
        int node = _root;

        for (int bit = _log - 1; bit >= 0; --bit) {
            bool new_bit = (key >> bit) & 1;
            auto data = _bitvectors.set(_nodes[node].v, k, new_bit, weight);
            int old_k = data.bit ? data.rank1 : k - data.rank1;
            if (data.bit == new_bit) {
                k = old_k;
                if (bit > 0) node = _nodes[node].child[data.bit];
                continue;
            }

            int new_k = new_bit ? data.rank1 : k - data.rank1;
            if (bit == 0) return;

            array<int, _MAX_LOG> old_path;
            array<uint8_t, _MAX_LOG> old_directions;
            int old_depth = 1;
            old_path[0] = node;
            old_directions[0] = data.bit;
            int old_node = _nodes[node].child[data.bit];
            int new_node = _nodes[node].child[new_bit];
            if (new_node == _NIL) {
                new_node = _make_node();
                _nodes[node].child[new_bit] = new_node;
            }

            int old_pos = old_k;
            int new_pos = new_k;
            for (int lower = bit - 1; lower >= 0; --lower) {
                old_path[old_depth] = old_node;
                auto old_data = _bitvectors.pop(_nodes[old_node].v, old_pos);
                old_pos = old_data.bit ? old_data.rank1 : old_pos - old_data.rank1;

                bool new_lower_bit = (key >> lower) & 1;
                int new_rank1 = _bitvectors.insert(_nodes[new_node].v, new_pos, new_lower_bit, weight);
                new_pos = new_lower_bit ? new_rank1 : new_pos - new_rank1;

                if (lower > 0) {
                    old_directions[old_depth] = old_data.bit;
                    old_node = _nodes[old_node].child[old_data.bit];
                    int child = _nodes[new_node].child[new_lower_bit];
                    if (child == _NIL) {
                        child = _make_node();
                        _nodes[new_node].child[new_lower_bit] = child;
                    }
                    new_node = child;
                }
                ++old_depth;
            }

            _prune_path(old_path, old_directions, old_depth);
            return;
        }
    }

    /// `weight = key` として更新する / `O(log(n)log(σ))`
    void set(const int k, const T key) { set(k, key, static_cast<W>(key)); }

    /// 位置 `k` のキーだけを更新する / `O(log(n)log(σ))`
    void set_key(int k, T key) {
        auto [old_key, weight] = access_pair(k);
        if (old_key != key) set(k, key, weight);
    }

    /// 位置 `k` の重みだけを更新する / `O(log(n)log(σ))`
    void set_weight(int k, W weight) {
        W old_weight = access_weight(k);
        if (old_weight != weight) add_weight(k, weight - old_weight);
    }

    /// 位置 `k` の重みに `delta` を加える / `O(log(n)log(σ))`
    void add_weight(int k, W delta) {
        assert(0 <= k && k < len());
        if (delta == W(0)) return;
        int node = _root;
        for (int bit = _log - 1; bit >= 0; --bit) {
            auto data = _bitvectors.add_weight(_nodes[node].v, k, delta);
            k = data.bit ? data.rank1 : k - data.rank1;
            if (bit > 0) node = _nodes[node].child[data.bit];
        }
    }

    /// 区間 `[0, r)` の `key` の個数を返す / `O(log(n)log(σ))`
    int rank(int r, const T key) const {
        assert(0 <= r && r <= len());
        assert(0 <= key && key < _sigma);
        int node = _root;
        for (int bit = _log - 1; bit >= 0; --bit) {
            if (node == _NIL || r == 0) return 0;
            auto data = _bitvectors.range_data(_nodes[node].v, 0, r);
            bool b = (key >> bit) & 1;
            r = b ? r - data.r0 : data.r0;
            node = _nodes[node].child[b];
        }
        return r;
    }

    /// 区間 `[l, r)` の `key` の個数を返す / `O(log(n)log(σ))`
    int range_count(int l, int r, const T key) const {
        assert(0 <= l && l <= r && r <= len());
        assert(0 <= key && key < _sigma);
        int node = _root;
        for (int bit = _log - 1; bit >= 0; --bit) {
            if (node == _NIL || l == r) return 0;
            auto data = _bitvectors.range_data(_nodes[node].v, l, r);
            bool b = (key >> bit) & 1;
            if (b) {
                l -= data.l0;
                r -= data.r0;
            } else {
                l = data.l0;
                r = data.r0;
            }
            node = _nodes[node].child[b];
        }
        return r - l;
    }

    /// 区間 `[l, r)` でキーが `[lower, upper)` にある `k` 番目の位置を返す / `O(log^2(n)log(σ))`
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

    /// 区間 `[l, r)` でキーが `[lower, upper)` にある最初の位置を返す / `O(log^2(n)log(σ))`
    /// 例: `[5, 1, 4, 1, 9]`, `[lower, upper) = [1, 5)` なら `1`
    int next_index_in_value_range(const int l, const int r, const T lower, const T upper) const {
        return range_freq(l, r, lower, upper) == 0 ? -1 : kth_index_in_value_range(l, r, lower, upper, 0);
    }

    /// 区間 `[l, r)` でキーが `[lower, upper)` にある最後の位置を返す / `O(log^2(n)log(σ))`
    /// 例: `[5, 1, 4, 1, 9]`, `[lower, upper) = [1, 5)` なら `3`
    int prev_index_in_value_range(const int l, const int r, const T lower, const T upper) const {
        int count = range_freq(l, r, lower, upper);
        return count == 0 ? -1 : kth_index_in_value_range(l, r, lower, upper, count - 1);
    }

    /// `k` 番目の `(key, weight)` を返す / `O(log(n)log(σ))`
    pair<T, W> access_pair(int k) const {
        assert(0 <= k && k < len());
        int node = _root;
        T key = 0;
        W weight = 0;
        for (int bit = _log - 1; bit >= 0; --bit) {
            auto data = _bitvectors.access(_nodes[node].v, k);
            if (bit == _log - 1) weight = data.weight;
            if (data.bit) {
                key |= static_cast<T>(1) << bit;
                k = data.rank1;
            } else {
                k -= data.rank1;
            }
            node = _nodes[node].child[data.bit];
        }
        return {key, weight};
    }

    /// `k` 番目のキーを返す / `O(log(n)log(σ))`
    T access(const int k) const { return access_pair(k).first; }

    /// `k` 番目の重みを返す / `O(log(n))`
    W access_weight(const int k) const {
        assert(0 <= k && k < len());
        return _bitvectors.access(_nodes[_root].v, k).weight;
    }

    /// 区間 `[l, r)` で昇順 `k` 番目のキーを返す / `O(log(n)log(σ))`
    T kth_smallest(int l, int r, int k) const {
        assert(0 <= l && l <= r && r <= len());
        assert(0 <= k && k < r - l);
        int node = _root;
        T result = 0;
        for (int bit = _log - 1; bit >= 0; --bit) {
            auto data = _bitvectors.range_data(_nodes[node].v, l, r);
            int count0 = data.r0 - data.l0;
            bool b = count0 <= k;
            if (b) {
                result |= static_cast<T>(1) << bit;
                k -= count0;
                l -= data.l0;
                r -= data.r0;
            } else {
                l = data.l0;
                r = data.r0;
            }
            node = _nodes[node].child[b];
        }
        return result;
    }

    /// 区間 `[l, r)` で降順 `k` 番目のキーを返す / `O(log(n)log(σ))`
    T kth_largest(const int l, const int r, const int k) const { return kth_smallest(l, r, r - l - k - 1); }

    /// 区間 `[l, r)` で頻度が高いキーを最大 `k` 種類返す / 訪問Node数を `p` として `O(p(log(n)+log(p)))`
    /// 例: `[1, 2, 1, 3, 1, 2]` で `topk(0, 6, 2)` は `{(1, 3), (2, 2)}`
    vector<pair<T, int>> topk(int l, int r, int k) const {
        assert(0 <= l && l <= r && r <= len());
        vector<pair<T, int>> ans;
        if (l == r || k <= 0) return ans;
        priority_queue<tuple<int, T, int, int, int, int>> hq;
        hq.emplace(r - l, 0, _root, _log - 1, l, r);
        while (!hq.empty() && k > 0) {
            auto [length, key, node, bit, ql, qr] = hq.top();
            hq.pop();
            if (bit < 0) {
                ans.emplace_back(key, length);
                --k;
                continue;
            }
            auto data = _bitvectors.range_data(_nodes[node].v, ql, qr);
            int cnt0 = data.r0 - data.l0;
            int cnt1 = length - cnt0;
            if (cnt0 > 0) hq.emplace(cnt0, key, _nodes[node].child[0], bit - 1, data.l0, data.r0);
            if (cnt1 > 0) hq.emplace(cnt1, key | (static_cast<T>(1) << bit), _nodes[node].child[1], bit - 1, ql - data.l0, qr - data.r0);
        }
        return ans;
    }

    /// 区間 `[l, r)` に過半数を占めるキーがあるか判定する / `O(log(n)log(σ))`
    pair<bool, T> has_majority(int l, int r) const {
        assert(0 <= l && l < r && r <= len());
        int node = _root;
        int length = (r - l) / 2 + 1;
        T result = 0;
        for (int bit = _log - 1; bit >= 0; --bit) {
            auto data = _bitvectors.range_data(_nodes[node].v, l, r);
            int count0 = data.r0 - data.l0;
            int count1 = (r - l) - count0;
            if (count0 >= length) {
                l = data.l0;
                r = data.r0;
                node = _nodes[node].child[0];
            } else if (count1 >= length) {
                result |= static_cast<T>(1) << bit;
                l -= data.l0;
                r -= data.r0;
                node = _nodes[node].child[1];
            } else {
                return {false, 0};
            }
        }
        return {true, result};
    }

    /// 区間 `[l, r)` で `upper` 未満のキーの個数を返す / `O(log(n)log(σ))`
    int range_freq(const int l, const int r, const T upper) const {
        assert(0 <= l && l <= r && r <= len());
        if (upper <= 0) return 0;
        if (upper >= _sigma) return r - l;
        return _range_freq_node(_root, _log - 1, l, r, upper);
    }

    /// 区間 `[l, r)` で `[lower, upper)` のキーの個数を返す / `O(log(n)log(σ))`
    int range_freq(const int l, const int r, const T lower, const T upper) const {
        if (lower >= upper) return 0;
        return range_freq(l, r, upper) - range_freq(l, r, lower);
    }

    /// `k` 番目の `key` の位置を返す / `O(log(n)log(σ))`
    int select(int k, const T key) const {
        assert(0 <= key && key < _sigma);
        array<int, _MAX_LOG> path;
        int node = _root;
        for (int bit = _log - 1, depth = 0; bit >= 0; --bit, ++depth) {
            path[depth] = node;
            if (bit > 0) node = _nodes[node].child[(key >> bit) & 1];
        }
        for (int bit = 0; bit < _log; ++bit) {
            int p = path[_log - bit - 1];
            k = _bitvectors.select(_nodes[p].v, k, (key >> bit) & 1);
        }
        return k;
    }

    /// 区間 `[l, r)` にある `k` 番目の `key` の位置を返す / `O(log(n)log(σ))`
    /// 例: `[5, 1, 4, 1, 9]` で `range_select(1, 5, 1, 1)` は `3`
    int range_select(int l, int r, int k, T key) const {
        assert(0 <= k && k < range_count(l, r, key));
        return select(rank(l, key) + k, key);
    }

    /// `k` 番目の `key` の位置を返して削除する / `O(log(n)log(σ))`
    int select_remove(int k, const T key) {
        assert(0 <= key && key < _sigma);
        array<int, _MAX_LOG> path;
        array<uint8_t, _MAX_LOG> directions;
        int depth = 0;
        int node = _root;
        for (int bit = _log - 1; bit >= 0; --bit) {
            path[depth] = node;
            bool b = (key >> bit) & 1;
            if (bit > 0) {
                directions[depth] = b;
                node = _nodes[node].child[b];
            }
            ++depth;
        }
        for (int bit = 0; bit < _log; ++bit) {
            int p = path[_log - bit - 1];
            k = _bitvectors.select_pop(_nodes[p].v, k, (key >> bit) & 1);
        }
        _prune_path(path, directions, depth);
        --_size;
        return k;
    }

    /// 区間 `[l, r)` で `upper` 未満のうち最大のキーを返す / `O(log(n)log(σ))`
    T prev_value(int l, int r, const T upper) const {
        if (l == r || upper <= 0) return -1;
        if (upper >= _sigma) return _extreme(_root, _log - 1, l, r, 0, true);

        int node = _root;
        int candidate = _NIL;
        int candidate_bit = -1;
        int candidate_l = 0;
        int candidate_r = 0;
        T value = 0;
        T candidate_value = 0;

        for (int bit = _log - 1; node != _NIL && bit >= 0 && l < r; --bit) {
            auto data = _bitvectors.range_data(_nodes[node].v, l, r);
            bool b = (upper >> bit) & 1;
            if (b) {
                if (data.l0 < data.r0) {
                    candidate = _nodes[node].child[0];
                    candidate_bit = bit - 1;
                    candidate_l = data.l0;
                    candidate_r = data.r0;
                    candidate_value = value;
                }
                value |= static_cast<T>(1) << bit;
                l -= data.l0;
                r -= data.r0;
            } else {
                l = data.l0;
                r = data.r0;
            }
            node = _nodes[node].child[b];
        }

        if (candidate_bit < 0) return candidate_r > candidate_l ? candidate_value : static_cast<T>(-1);
        if (candidate == _NIL) return -1;
        return _extreme(candidate, candidate_bit, candidate_l, candidate_r, candidate_value, true);
    }

    /// 区間 `[l, r)` で `lower` 以上のうち最小のキーを返す / `O(log(n)log(σ))`
    T next_value(int l, int r, const T lower) const {
        if (l == r || lower >= _sigma) return -1;
        if (lower <= 0) return _extreme(_root, _log - 1, l, r, 0, false);

        int node = _root;
        int candidate = _NIL;
        int candidate_bit = -1;
        int candidate_l = 0;
        int candidate_r = 0;
        T value = 0;
        T candidate_value = 0;

        for (int bit = _log - 1; node != _NIL && bit >= 0 && l < r; --bit) {
            auto data = _bitvectors.range_data(_nodes[node].v, l, r);
            bool b = (lower >> bit) & 1;
            if (!b && r - data.r0 > l - data.l0) {
                candidate = _nodes[node].child[1];
                candidate_bit = bit - 1;
                candidate_l = l - data.l0;
                candidate_r = r - data.r0;
                candidate_value = value | (static_cast<T>(1) << bit);
            }

            if (b) {
                value |= static_cast<T>(1) << bit;
                l -= data.l0;
                r -= data.r0;
            } else {
                l = data.l0;
                r = data.r0;
            }
            node = _nodes[node].child[b];
        }

        if (l < r) return value;
        if (candidate_bit < 0) return candidate_r > candidate_l ? candidate_value : static_cast<T>(-1);
        if (candidate == _NIL) return -1;
        return _extreme(candidate, candidate_bit, candidate_l, candidate_r, candidate_value, false);
    }

    /// 区間 `[l, r)` の重みの総和を返す / `O(log(n))`
    W range_sum(const int l, const int r) const {
        assert(0 <= l && l <= r && r <= len());
        if (l == r) return W(0);
        auto data = _bitvectors.range_data(_nodes[_root].v, l, r);
        return data.sum0 + data.sum1;
    }

    /// 区間 `[l, r)` で `upper` 未満のキーの個数と重みの総和を返す / `O(log(n)log(σ))`
    pair<int, W> count_sum_lt(const int l, const int r, const T upper) const {
        assert(0 <= l && l <= r && r <= len());
        if (l == r || upper <= 0) return {0, W(0)};
        if (upper >= _sigma) return {r - l, range_sum(l, r)};
        return _count_sum_lt_node(_root, _log - 1, l, r, upper);
    }

    /// 区間 `[l, r)` で `upper` 未満のキーを持つ要素の重みの総和を返す / `O(log(n)log(σ))`
    W sum_lt(const int l, const int r, const T upper) const {
        return count_sum_lt(l, r, upper).second;
    }

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
    int len() const { return _size; }

    /// キー列を返す / `O(nlog(σ))`
    vector<T> tovector() const {
        vector<T> result(_size, 0);
        if (_size == 0) return result;

        vector<int> order(_size), work(_size);
        iota(order.begin(), order.end(), 0);
        auto dfs = [&](auto &&dfs, int node, int bit, int left, int right) -> void {
            if (left == right || bit < 0) return;
            const vector<uint8_t> bits = _bitvectors.tovector(_nodes[node].v).first;
            int zeros = 0;
            for (int i = left; i < right; ++i) {
                bool b = bits[i - left];
                if (b) result[order[i]] |= static_cast<T>(1) << bit;
                zeros += !b;
            }
            int zero = left;
            int one = left + zeros;
            for (int i = left; i < right; ++i) {
                if (bits[i - left]) {
                    work[one++] = order[i];
                } else {
                    work[zero++] = order[i];
                }
            }
            for (int i = left; i < right; ++i) order[i] = work[i];
            int mid = left + zeros;
            dfs(dfs, _nodes[node].child[0], bit - 1, left, mid);
            dfs(dfs, _nodes[node].child[1], bit - 1, mid, right);
        };
        dfs(dfs, _root, _log - 1, 0, _size);
        return result;
    }

    /// 重み列を返す / `O(n)`
    vector<W> toweights() const {
        if (_size == 0) return {};
        return _bitvectors.tovector(_nodes[_root].v).second;
    }

    /// `(key, weight)` の列を返す / `O(nlog(σ))`
    vector<pair<T, W>> toitems() const {
        const vector<T> keys = tovector();
        const vector<W> weights = toweights();
        vector<pair<T, W>> result(_size);
        for (int i = 0; i < _size; ++i) result[i] = {keys[i], weights[i]};
        return result;
    }

    /// `(key, weight)` の列を表示する / `O(nlog(σ))`
    void print() const {
        const vector<pair<T, W>> items = toitems();
        cout << "[";
        for (int i = 0; i < static_cast<int>(items.size()); ++i) {
            if (i != 0) cout << ", ";
            cout << "(" << items[i].first << ", " << items[i].second << ")";
        }
        cout << "]" << endl;
    }

    friend ostream& operator<<(ostream& os, const DynamicWaveletTreeSum<T, W> &dwt) {
        const vector<pair<T, W>> items = dwt.toitems();
        os << "[";
        for (int i = 0; i < static_cast<int>(items.size()); ++i) {
            if (i != 0) os << ", ";
            os << "(" << items[i].first << ", " << items[i].second << ")";
        }
        return os << "]";
    }
};

} // namespace titan23
