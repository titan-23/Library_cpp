/// https://github.com/titan-23/Library_cpp/blob/main/titan_cpplib/ds/dynamic_wavelet_tree_sum.cpp
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
#include "titan_cpplib/others/bit.cpp"
#include "titan_cpplib/ds/b_tree_bit_vector.cpp"
#include "titan_cpplib/ds/b_tree_bit_vector_sum.cpp"
#include "titan_cpplib/ds/b_tree_sum_sequence.cpp"
using namespace std;

// DynamicWaveletTreeSum
namespace titan23 {

/// @brief 総和付き動的ウェーブレット木 / T はキーの型、W は重みの型
template<typename T, typename W>
class DynamicWaveletTreeSum {
private:
    static constexpr int _NIL = -1;
    static constexpr int _MAX_LOG = 64;

    using BottomVector = BTreeBitVectorSum<W>;
    using WeightSequence = BTreeSumSequence<W>;

    struct Node {
        int child[2];
        BTreeBitVector v;                         // bit 1 以上の段のビット列
        typename WeightSequence::Sequence w;      // bit 1 以上の段の重みの列
        typename BottomVector::Sequence s;        // bit 0 の段のビット列と重み

        Node() : child{_NIL, _NIL} {}
    };

    struct RangeData {
        int l0, r0;
        W sum0, sum1;
    };

    BottomVector _bottom;
    WeightSequence _weights;
    vector<Node> _nodes;
    int _root;
    int _free;
    T _sigma;
    int _log;
    int _size;

    size_t _estimated_node_count(int n) const {
        if (n == 0) return 1;
        size_t ans = 0;
        size_t width = 1;
        for (int depth = 0; depth < _log; ++depth) {
            ans += min(static_cast<size_t>(n), width);
            width = min(static_cast<size_t>(n), width * 2);
        }
        return ans;
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

    bool _empty(const int node, const int bit) const {
        return bit == 0 ? _bottom.empty(_nodes[node].s) : _nodes[node].v.empty();
    }

    void _release_node(const int node, const int bit) {
        assert(node != _root);
        assert(_nodes[node].child[0] == _NIL && _nodes[node].child[1] == _NIL);
        assert(_empty(node, bit));
        (void)bit;
        _nodes[node].v.clear();
        _nodes[node].child[0] = _free;
        _nodes[node].child[1] = _NIL;
        _free = node;
    }

    // path[i] は段 top_bit-i のノード。下から空になったノードを外す
    void _prune_path(const array<int, _MAX_LOG> &path, const array<uint8_t, _MAX_LOG> &directions, const int depth,
                     const int top_bit) {
        for (int i = depth - 1; i > 0; --i) {
            int node = path[i];
            const int bit = top_bit - i;
            if (!_empty(node, bit)) break;
            int parent = path[i - 1];
            _nodes[parent].child[directions[i - 1]] = _NIL;
            _release_node(node, bit);
        }
    }

    // ---- 段 bit のノードに対する操作 ----

    pair<int, int> _rank0_pair(const int node, const int bit, const int l, const int r) const {
        if (bit == 0) return _bottom.rank0_pair(_nodes[node].s, l, r);
        return _nodes[node].v._rank0_pair(l, r);
    }

    int _rank0(const int node, const int bit, const int r) const {
        if (bit == 0) return _bottom.rank0(_nodes[node].s, r);
        return _nodes[node].v.rank0(r);
    }

    pair<bool, int> _access_bit(const int node, const int bit, const int k) const {
        if (bit == 0) return _bottom.access_bit(_nodes[node].s, k);
        return _nodes[node].v._access_ans_rank1(k);
    }

    // 段 bit のノードの区間 [l, r) の重みの和
    W _range_sum(const int node, const int bit, const int l, const int r) const {
        if (node == _NIL || l == r) return W(0);
        if (bit == 0) {
            const auto data = _bottom.range_data(_nodes[node].s, l, r);
            return data.sum0 + data.sum1;
        }
        return _weights.range_sum(_nodes[node].w, l, r);
    }

    // 段 bit のノードの区間 [l, r) のうち、値の bit が side の要素の重みの和
    // (l0, r0) は [l, r) の rank0
    W _side_sum(const int node, const int bit, const bool side, const int l, const int r, const int l0, const int r0) const {
        if (bit == 0) {
            const auto data = _bottom.range_data(_nodes[node].s, l, r);
            return side ? data.sum1 : data.sum0;
        }
        if (side) return _range_sum(_nodes[node].child[1], bit - 1, l - l0, r - r0);
        return _range_sum(_nodes[node].child[0], bit - 1, l0, r0);
    }

    // 区間 [l, r) を、値の bit が b である要素の子での区間に移す
    static void _descend(const bool b, int &l, int &r, const int l0, const int r0) {
        if (b) {
            l -= l0;
            r -= r0;
        } else {
            l = l0;
            r = r0;
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
        _bottom.reserve(n, static_cast<int>(reserve_nodes));
        _weights.reserve(static_cast<int>(total_size), static_cast<int>(reserve_nodes));

        vector<int> order(n), work(n);
        vector<uint8_t> bits(n);
        iota(order.begin(), order.end(), 0);

        auto build = [&](auto&& self, int bit, int l, int r, vector<int>& src, vector<int>& dst) -> int {
            if (l == r || bit < 0) return _NIL;

            int zeros = 0;
            for (int i = l; i < r; ++i) {
                bool b = (keys[src[i]] >> bit) & 1;
                bits[i] = b;
                zeros += !b;
            }

            int node = _make_node();
            if (bit == 0) {
                _nodes[node].s = _bottom.build(bits, src, weights, l, r);
            } else {
                _nodes[node].v = BTreeBitVector(bits, l, r);
                _nodes[node].w = _weights.build(src, weights, l, r);
            }
            int zero = l;
            int one = l + zeros;
            for (int i = l; i < r; ++i) {
                if (bits[i]) {
                    dst[one++] = src[i];
                } else {
                    dst[zero++] = src[i];
                }
            }

            int mid = l + zeros;
            int child0 = self(self, bit - 1, l, mid, dst, src);
            int child1 = self(self, bit - 1, mid, r, dst, src);
            _nodes[node].child[0] = child0;
            _nodes[node].child[1] = child1;
            return node;
        };

        _root = build(build, _log - 1, 0, n, order, work);
    }

    // 値の bit が b の要素を段 bit のノードの位置 k に (key, weight) として入れ、子での位置を返す
    int _insert_at(const int node, const int bit, const int k, const bool b, const W weight) {
        int rank1;
        if (bit == 0) {
            rank1 = _bottom.insert(_nodes[node].s, k, b, weight);
        } else {
            rank1 = _nodes[node].v._insert_and_rank1(k, b);
            _weights.insert(_nodes[node].w, k, weight);
        }
        return b ? rank1 : k - rank1;
    }

    // 段 bit のノードの位置 k の要素を取り除き、(bit, 子での位置, 重み) を返す
    tuple<bool, int, W> _pop_at(const int node, const int bit, const int k) {
        if (bit == 0) {
            const auto data = _bottom.pop(_nodes[node].s, k);
            return {data.bit, data.bit ? data.rank1 : k - data.rank1, data.weight};
        }
        const int sb = _nodes[node].v._access_pop_and_rank1(k);
        const bool b = sb & 1;
        const int rank1 = sb >> 1;
        const W weight = _weights.pop(_nodes[node].w, k);
        return {b, b ? rank1 : k - rank1, weight};
    }

    int _range_freq_node(int node, int bit, int l, int r, const T x) const {
        int result = 0;
        while (node != _NIL && bit >= 0 && l < r) {
            auto [l0, r0] = _rank0_pair(node, bit, l, r);
            bool b = (x >> bit) & 1;
            if (b) result += r0 - l0;
            _descend(b, l, r, l0, r0);
            node = _nodes[node].child[b];
            --bit;
        }
        return result;
    }

    pair<int, W> _count_sum_lt_node(int node, int bit, int l, int r, const T x) const {
        int count = 0;
        W sum = 0;
        while (node != _NIL && bit >= 0 && l < r) {
            auto [l0, r0] = _rank0_pair(node, bit, l, r);
            bool b = (x >> bit) & 1;
            if (b) {
                count += r0 - l0;
                if (l0 < r0) sum += _side_sum(node, bit, false, l, r, l0, r0);
            }
            _descend(b, l, r, l0, r0);
            node = _nodes[node].child[b];
            --bit;
        }
        return {count, sum};
    }

    T _extreme(int node, int bit, int l, int r, T value, const bool largest) const {
        while (bit >= 0) {
            auto [l0, r0] = _rank0_pair(node, bit, l, r);
            int count0 = r0 - l0;
            int count1 = (r - l) - count0;
            bool b = largest ? count1 > 0 : count0 == 0;
            if (b) value |= static_cast<T>(1) << bit;
            _descend(b, l, r, l0, r0);
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
        const bool preferred_bit = largest;
        for (int bit = _log - 1; bit >= 0; --bit) {
            if (bit == 0) {
                const auto data = _bottom.range_data(_nodes[node].s, l, r);
                const int count0 = data.r0 - data.l0;
                const int preferred_count = largest ? (r - l) - count0 : count0;
                const W preferred_sum = largest ? data.sum1 : data.sum0;
                const auto &s = _nodes[node].s;
                if (k <= preferred_count) {
                    if (k == preferred_count) return result + preferred_sum;
                    return result + _bottom.sum_first_k(s, l, r, preferred_bit, k);
                }
                return result + preferred_sum + _bottom.sum_first_k(s, l, r, !preferred_bit, k - preferred_count);
            }
            auto [l0, r0] = _rank0_pair(node, bit, l, r);
            int count0 = r0 - l0;
            int preferred_count = largest ? (r - l) - count0 : count0;

            if (k <= preferred_count) {
                if (k == preferred_count) return result + _side_sum(node, bit, preferred_bit, l, r, l0, r0);
                _descend(preferred_bit, l, r, l0, r0);
                node = _nodes[node].child[preferred_bit];
                continue;
            }

            if (preferred_count > 0) result += _side_sum(node, bit, preferred_bit, l, r, l0, r0);
            k -= preferred_count;
            _descend(!preferred_bit, l, r, l0, r0);
            node = _nodes[node].child[!preferred_bit];
        }
        return result;
    }

    int _min_count_sum_ge(int l, int r, W target, const bool largest) const {
        if (target <= W(0)) return 0;
        if (l == r) return -1;
        if (range_sum(l, r) < target) return -1;
        int node = _root;
        int result = 0;
        const bool preferred_bit = largest;
        for (int bit = _log - 1; bit >= 0; --bit) {
            if (bit == 0) {
                const auto data = _bottom.range_data(_nodes[node].s, l, r);
                const int count0 = data.r0 - data.l0;
                const int preferred_count = largest ? (r - l) - count0 : count0;
                const W preferred_sum = largest ? data.sum1 : data.sum0;
                const auto &s = _nodes[node].s;
                if (preferred_sum >= target) return result + _bottom.min_count_sum_ge(s, l, r, preferred_bit, target);
                return result + preferred_count + _bottom.min_count_sum_ge(s, l, r, !preferred_bit, target - preferred_sum);
            }
            auto [l0, r0] = _rank0_pair(node, bit, l, r);
            int count0 = r0 - l0;
            int preferred_count = largest ? (r - l) - count0 : count0;
            W preferred_sum = preferred_count > 0 ? _side_sum(node, bit, preferred_bit, l, r, l0, r0) : W(0);

            if (preferred_sum >= target) {
                _descend(preferred_bit, l, r, l0, r0);
                node = _nodes[node].child[preferred_bit];
                continue;
            }

            result += preferred_count;
            target -= preferred_sum;
            _descend(!preferred_bit, l, r, l0, r0);
            node = _nodes[node].child[!preferred_bit];
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
            if (bit == 0) {
                const auto data = _bottom.range_data(_nodes[node].s, l, r);
                const int count0 = data.r0 - data.l0;
                const int preferred_count = largest ? (r - l) - count0 : count0;
                const W preferred_sum = largest ? data.sum1 : data.sum0;
                bool b;
                if (pred(aggregate + preferred_sum)) {
                    count += preferred_count;
                    aggregate += preferred_sum;
                    b = !largest;
                } else {
                    b = largest;
                }
                if (b) key |= static_cast<T>(1);
                int accepted = _bottom.max_count(_nodes[node].s, l, r, b, aggregate, pred);
                return {count + accepted, key, aggregate};
            }
            auto [l0, r0] = _rank0_pair(node, bit, l, r);
            int count0 = r0 - l0;
            int preferred_count = largest ? (r - l) - count0 : count0;
            W preferred_sum = preferred_count > 0 ? _side_sum(node, bit, largest, l, r, l0, r0) : W(0);

            bool b;
            if (pred(aggregate + preferred_sum)) {
                count += preferred_count;
                aggregate += preferred_sum;
                b = !largest;
            } else {
                b = largest;
            }
            if (b) key |= static_cast<T>(1) << bit;
            _descend(b, l, r, l0, r0);
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
    /// @brief 各キーが [0, sigma) の空の総和付き動的ウェーブレット木を作成する / O(1)
    DynamicWaveletTreeSum(const T sigma)
        : _root(_NIL), _free(_NIL), _sigma(sigma),
          _log(max(1, bit_length(static_cast<unsigned long long>(sigma - 1)))), _size(0) {
        assert(sigma > 0);
        _root = _make_node();
    }

    /// @brief キーと重みから総和付き動的ウェーブレット木を作成する / O(nlog(σ))
    DynamicWaveletTreeSum(const T sigma, const vector<T> &keys, const vector<W> &weights)
        : _root(_NIL), _free(_NIL), _sigma(sigma),
          _log(max(1, bit_length(static_cast<unsigned long long>(sigma - 1)))), _size(keys.size()) {
        assert(sigma > 0);
        assert(keys.size() == weights.size());
        for (T key : keys) assert(0 <= key && key < sigma);
        _build(keys, weights);
    }

    /// @brief weight = key として構築する / O(nlog(σ))
    DynamicWaveletTreeSum(const T sigma, const vector<T> &keys)
        : _root(_NIL), _free(_NIL), _sigma(sigma),
          _log(max(1, bit_length(static_cast<unsigned long long>(sigma - 1)))), _size(keys.size()) {
        assert(sigma > 0);
        vector<W> weights(keys.begin(), keys.end());
        for (T key : keys) assert(0 <= key && key < sigma);
        _build(keys, weights);
    }

    /// @brief 最終的な要素数 expected_size からノードと動的ビット列の領域を予約する / 最悪 O(nlog(σ))
    void reserve(int expected_size) {
        assert(len() <= expected_size);
        size_t nodes = _estimated_node_count(expected_size);
        long long total_size = static_cast<long long>(expected_size) * _log;
        assert(nodes <= static_cast<size_t>(numeric_limits<int>::max()));
        assert(total_size <= numeric_limits<int>::max());
        _nodes.reserve(nodes);
        _bottom.reserve(expected_size, static_cast<int>(nodes));
        _weights.reserve(static_cast<int>(total_size), static_cast<int>(nodes));
        if (_log > 1) _nodes[_root].v.reserve(expected_size);
    }

    /// @brief 位置 k に (key, weight) を挿入する / O(log(n)log(σ))
    void insert(int k, const T key, const W weight) {
        assert(0 <= k && k <= len());
        assert(0 <= key && key < _sigma);
        int node = _root;
        for (int bit = _log - 1; bit >= 0; --bit) {
            bool b = (key >> bit) & 1;
            k = _insert_at(node, bit, k, b, weight);
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

    /// @brief weight = key として挿入する / O(log(n)log(σ))
    void insert(const int k, const T key) { insert(k, key, static_cast<W>(key)); }

    /// @brief 位置 k の (key, weight) を削除して返す / O(log(n)log(σ))
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
            auto [b, next_k, w] = _pop_at(node, bit, k);
            if (depth == 0) weight = w;
            if (b) key |= static_cast<T>(1) << bit;
            k = next_k;
            if (bit > 0) {
                directions[depth] = b;
                node = _nodes[node].child[b];
            }
            ++depth;
        }

        _prune_path(path, directions, depth, _log - 1);
        --_size;
        return {key, weight};
    }

    /// @brief 位置 k を (key, weight) に更新する / O(log(n)log(σ))
    void set(int k, const T key, const W weight) {
        assert(0 <= k && k < len());
        assert(0 <= key && key < _sigma);
        int node = _root;

        for (int bit = _log - 1; bit >= 0; --bit) {
            bool new_bit = (key >> bit) & 1;
            bool old_bit;
            int rank1;
            if (bit == 0) {
                const auto data = _bottom.set(_nodes[node].s, k, new_bit, weight);
                old_bit = data.bit;
                rank1 = data.rank1;
            } else {
                tie(old_bit, rank1) = _nodes[node].v._access_set_and_rank1(k, new_bit);
                _weights.set(_nodes[node].w, k, weight);
            }
            int old_k = old_bit ? rank1 : k - rank1;
            if (old_bit == new_bit) {
                k = old_k;
                if (bit > 0) node = _nodes[node].child[old_bit];
                continue;
            }

            int new_k = new_bit ? rank1 : k - rank1;
            if (bit == 0) return;

            // この段より下では、元のキーの側から取り除き、新しいキーの側へ挿入する
            array<int, _MAX_LOG> old_path;
            array<uint8_t, _MAX_LOG> old_directions;
            int old_depth = 1;
            old_path[0] = node;
            old_directions[0] = old_bit;
            int old_node = _nodes[node].child[old_bit];
            int new_node = _nodes[node].child[new_bit];
            if (new_node == _NIL) {
                new_node = _make_node();
                _nodes[node].child[new_bit] = new_node;
            }

            int old_pos = old_k;
            int new_pos = new_k;
            for (int lower = bit - 1; lower >= 0; --lower) {
                old_path[old_depth] = old_node;
                auto [ob, next_old, ow] = _pop_at(old_node, lower, old_pos);
                (void)ow;
                old_pos = next_old;

                bool new_lower_bit = (key >> lower) & 1;
                new_pos = _insert_at(new_node, lower, new_pos, new_lower_bit, weight);

                if (lower > 0) {
                    old_directions[old_depth] = ob;
                    old_node = _nodes[old_node].child[ob];
                    int child = _nodes[new_node].child[new_lower_bit];
                    if (child == _NIL) {
                        child = _make_node();
                        _nodes[new_node].child[new_lower_bit] = child;
                    }
                    new_node = child;
                }
                ++old_depth;
            }

            _prune_path(old_path, old_directions, old_depth, bit);
            return;
        }
    }

    /// @brief weight = key として更新する / O(log(n)log(σ))
    void set(const int k, const T key) { set(k, key, static_cast<W>(key)); }

    /// @brief 位置 k のキーだけを更新する / O(log(n)log(σ))
    void set_key(int k, T key) {
        assert(0 <= k && k < len());
        set(k, key, access_weight(k));
    }

    /// @brief 位置 k の重みだけを更新する / O(log(n)log(σ))
    void set_weight(int k, W weight) {
        W old_weight = access_weight(k);
        if (old_weight != weight) add_weight(k, weight - old_weight);
    }

    /// @brief 位置 k の重みに delta を加える / O(log(n)log(σ))
    void add_weight(int k, W delta) {
        assert(0 <= k && k < len());
        if (delta == W(0)) return;
        int node = _root;
        for (int bit = _log - 1; bit >= 0; --bit) {
            if (bit == 0) {
                _bottom.add_weight(_nodes[node].s, k, delta);
                return;
            }
            const auto [b, rank1] = _nodes[node].v._access_ans_rank1(k);
            _weights.add(_nodes[node].w, k, delta);
            k = b ? rank1 : k - rank1;
            node = _nodes[node].child[b];
        }
    }

    /// @brief 区間 [0, r) の key の個数を返す / O(log(n)log(σ))
    int rank(int r, const T key) const {
        assert(0 <= r && r <= len());
        assert(0 <= key && key < _sigma);
        int node = _root;
        for (int bit = _log - 1; bit >= 0; --bit) {
            if (node == _NIL || r == 0) return 0;
            bool b = (key >> bit) & 1;
            int r0 = _rank0(node, bit, r);
            r = b ? r - r0 : r0;
            node = _nodes[node].child[b];
        }
        return r;
    }

    /// @brief 区間 [l, r) の key の個数を返す / O(log(n)log(σ))
    int range_count(int l, int r, const T key) const {
        assert(0 <= l && l <= r && r <= len());
        assert(0 <= key && key < _sigma);
        int node = _root;
        for (int bit = _log - 1; bit >= 0; --bit) {
            if (node == _NIL || l == r) return 0;
            auto [l0, r0] = _rank0_pair(node, bit, l, r);
            bool b = (key >> bit) & 1;
            _descend(b, l, r, l0, r0);
            node = _nodes[node].child[b];
        }
        return r - l;
    }

    /// @brief 区間 [l, r) でキーが [lower, upper) にある k 番目の位置を返す / O(log^2(n)log(σ))
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

    /// @brief 区間 [l, r) でキーが [lower, upper) にある最初の位置を返す / なければ -1 / O(log^2(n)log(σ))
    int next_index_in_value_range(const int l, const int r, const T lower, const T upper) const {
        return range_freq(l, r, lower, upper) == 0 ? -1 : kth_index_in_value_range(l, r, lower, upper, 0);
    }

    /// @brief 区間 [l, r) でキーが [lower, upper) にある最後の位置を返す / なければ -1 / O(log^2(n)log(σ))
    int prev_index_in_value_range(const int l, const int r, const T lower, const T upper) const {
        int count = range_freq(l, r, lower, upper);
        return count == 0 ? -1 : kth_index_in_value_range(l, r, lower, upper, count - 1);
    }

    /// @brief k 番目の (key, weight) を返す / O(log(n)log(σ))
    pair<T, W> access_pair(int k) const {
        assert(0 <= k && k < len());
        return {access(k), access_weight(k)};
    }

    /// @brief k 番目のキーを返す / O(log(n)log(σ))
    T access(int k) const {
        assert(0 <= k && k < len());
        int node = _root;
        T key = 0;
        for (int bit = _log - 1; bit >= 0; --bit) {
            auto [b, rank1] = _access_bit(node, bit, k);
            if (b) {
                key |= static_cast<T>(1) << bit;
                k = rank1;
            } else {
                k -= rank1;
            }
            node = _nodes[node].child[b];
        }
        return key;
    }

    /// @brief k 番目の重みを返す / O(log(n))
    W access_weight(const int k) const {
        assert(0 <= k && k < len());
        if (_log == 1) return _bottom.access_weight(_nodes[_root].s, k);
        return _weights.access(_nodes[_root].w, k);
    }

    /// @brief 区間 [l, r) で昇順 k 番目のキーを返す / O(log(n)log(σ))
    T kth_smallest(int l, int r, int k) const {
        assert(0 <= l && l <= r && r <= len());
        assert(0 <= k && k < r - l);
        int node = _root;
        T result = 0;
        for (int bit = _log - 1; bit >= 0; --bit) {
            auto [l0, r0] = _rank0_pair(node, bit, l, r);
            int count0 = r0 - l0;
            bool b = count0 <= k;
            if (b) {
                result |= static_cast<T>(1) << bit;
                k -= count0;
            }
            _descend(b, l, r, l0, r0);
            node = _nodes[node].child[b];
        }
        return result;
    }

    /// @brief 区間 [l, r) で降順 k 番目のキーを返す / O(log(n)log(σ))
    T kth_largest(const int l, const int r, const int k) const { return kth_smallest(l, r, r - l - k - 1); }

    /// @brief 区間 [l, r) で頻度が高いキーを最大 k 種類、(キー, 個数) の組で返す / O(p(log(n)+log(p)))、p は訪問ノード数
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
            auto [l0, r0] = _rank0_pair(node, bit, ql, qr);
            int cnt0 = r0 - l0;
            int cnt1 = length - cnt0;
            if (cnt0 > 0) hq.emplace(cnt0, key, _nodes[node].child[0], bit - 1, l0, r0);
            if (cnt1 > 0) {
                hq.emplace(cnt1, key | (static_cast<T>(1) << bit), _nodes[node].child[1], bit - 1, ql - l0,
                           qr - r0);
            }
        }
        return ans;
    }

    /// @brief 区間 [l, r) に過半数を占めるキーがあるか判定する / O(log(n)log(σ))
    pair<bool, T> has_majority(int l, int r) const {
        assert(0 <= l && l < r && r <= len());
        int node = _root;
        int length = (r - l) / 2 + 1;
        T result = 0;
        for (int bit = _log - 1; bit >= 0; --bit) {
            auto [l0, r0] = _rank0_pair(node, bit, l, r);
            int count0 = r0 - l0;
            int count1 = (r - l) - count0;
            bool b;
            if (count0 >= length) {
                b = false;
            } else if (count1 >= length) {
                b = true;
                result |= static_cast<T>(1) << bit;
            } else {
                return {false, 0};
            }
            _descend(b, l, r, l0, r0);
            node = _nodes[node].child[b];
        }
        return {true, result};
    }

    /// @brief 区間 [l, r) で upper 未満のキーの個数を返す / O(log(n)log(σ))
    int range_freq(const int l, const int r, const T upper) const {
        assert(0 <= l && l <= r && r <= len());
        if (upper <= 0) return 0;
        if (upper >= _sigma) return r - l;
        return _range_freq_node(_root, _log - 1, l, r, upper);
    }

    /// @brief 区間 [l, r) で [lower, upper) のキーの個数を返す / O(log(n)log(σ))
    int range_freq(const int l, const int r, const T lower, const T upper) const {
        assert(0 <= l && l <= r && r <= len());
        if (lower >= upper || upper <= 0 || lower >= _sigma) return 0;
        if (lower <= 0) return range_freq(l, r, upper);
        if (upper >= _sigma) return r - l - range_freq(l, r, lower);

        int node = _root;
        int ql = l;
        int qr = r;
        for (int bit = _log - 1; bit >= 0; --bit) {
            if (node == _NIL || ql == qr) return 0;
            auto [l0, r0] = _rank0_pair(node, bit, ql, qr);
            bool lb = (lower >> bit) & 1;
            bool ub = (upper >> bit) & 1;
            if (lb == ub) {
                _descend(lb, ql, qr, l0, r0);
                node = _nodes[node].child[lb];
                continue;
            }

            int cnt0 = r0 - l0;
            int lo = cnt0 - _range_freq_node(_nodes[node].child[0], bit - 1, l0, r0, lower);
            int hi = _range_freq_node(_nodes[node].child[1], bit - 1, ql - l0, qr - r0, upper);
            return lo + hi;
        }
        return 0;
    }

    /// @brief k 番目の key の位置を返す / O(log(n)log(σ))
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
            const bool b = (key >> bit) & 1;
            k = bit == 0 ? _bottom.select(_nodes[p].s, k, b) : _nodes[p].v.select(k, b);
        }
        return k;
    }

    /// @brief 区間 [l, r) にある k 番目の key の位置を返す / O(log(n)log(σ))
    int range_select(int l, int r, int k, T key) const {
        assert(0 <= k && k < range_count(l, r, key));
        return select(rank(l, key) + k, key);
    }

    /// @brief k 番目の key の位置を返して削除する / O(log(n)log(σ))
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
            const bool b = (key >> bit) & 1;
            if (bit == 0) {
                k = _bottom.select_pop(_nodes[p].s, k, b);
            } else {
                k = _nodes[p].v._select_pop(k, b);
                _weights.pop(_nodes[p].w, k);
            }
        }
        _prune_path(path, directions, depth, _log - 1);
        --_size;
        return k;
    }

    /// @brief 区間 [l, r) で upper 未満のうち最大のキーを返す / なければ -1 / O(log(n)log(σ))
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
            auto [l0, r0] = _rank0_pair(node, bit, l, r);
            bool b = (upper >> bit) & 1;
            if (b) {
                if (l0 < r0) {
                    candidate = _nodes[node].child[0];
                    candidate_bit = bit - 1;
                    candidate_l = l0;
                    candidate_r = r0;
                    candidate_value = value;
                }
                value |= static_cast<T>(1) << bit;
            }
            _descend(b, l, r, l0, r0);
            node = _nodes[node].child[b];
        }

        if (candidate_bit < 0) return candidate_r > candidate_l ? candidate_value : static_cast<T>(-1);
        if (candidate == _NIL) return -1;
        return _extreme(candidate, candidate_bit, candidate_l, candidate_r, candidate_value, true);
    }

    /// @brief 区間 [l, r) で lower 以上のうち最小のキーを返す / なければ -1 / O(log(n)log(σ))
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
            auto [l0, r0] = _rank0_pair(node, bit, l, r);
            bool b = (lower >> bit) & 1;
            if (!b && r - r0 > l - l0) {
                candidate = _nodes[node].child[1];
                candidate_bit = bit - 1;
                candidate_l = l - l0;
                candidate_r = r - r0;
                candidate_value = value | (static_cast<T>(1) << bit);
            }
            if (b) value |= static_cast<T>(1) << bit;
            _descend(b, l, r, l0, r0);
            node = _nodes[node].child[b];
        }

        if (l < r) return value;
        if (candidate_bit < 0) return candidate_r > candidate_l ? candidate_value : static_cast<T>(-1);
        if (candidate == _NIL) return -1;
        return _extreme(candidate, candidate_bit, candidate_l, candidate_r, candidate_value, false);
    }

    /// @brief 区間 [l, r) の重みの総和を返す / O(log(n))
    W range_sum(const int l, const int r) const {
        assert(0 <= l && l <= r && r <= len());
        return _range_sum(_root, _log - 1, l, r);
    }

    /// @brief 区間 [l, r) で upper 未満のキーの個数と重みの総和を返す / O(log(n)log(σ))
    pair<int, W> count_sum_lt(const int l, const int r, const T upper) const {
        assert(0 <= l && l <= r && r <= len());
        if (l == r || upper <= 0) return {0, W(0)};
        if (upper >= _sigma) return {r - l, range_sum(l, r)};
        return _count_sum_lt_node(_root, _log - 1, l, r, upper);
    }

    /// @brief 区間 [l, r) で upper 未満のキーを持つ要素の重みの総和を返す / O(log(n)log(σ))
    W sum_lt(const int l, const int r, const T upper) const {
        return count_sum_lt(l, r, upper).second;
    }

    /// @brief 区間 [l, r) で [lower, upper) のキーを持つ要素の重みの総和を返す / O(log(n)log(σ))
    W sum_range(const int l, const int r, const T lower, const T upper) const {
        assert(0 <= l && l <= r && r <= len());
        if (l == r || lower >= upper || upper <= 0 || lower >= _sigma) return W(0);
        if (lower <= 0) return sum_lt(l, r, upper);
        if (upper >= _sigma) return range_sum(l, r) - sum_lt(l, r, lower);

        int node = _root;
        int ql = l;
        int qr = r;
        for (int bit = _log - 1; bit >= 0; --bit) {
            if (node == _NIL || ql == qr) return W(0);
            auto [l0, r0] = _rank0_pair(node, bit, ql, qr);
            bool lb = (lower >> bit) & 1;
            bool ub = (upper >> bit) & 1;
            if (lb == ub) {
                _descend(lb, ql, qr, l0, r0);
                node = _nodes[node].child[lb];
                continue;
            }

            // lower の bit が 0、upper の bit が 1 の段で分かれる
            W sum0 = l0 < r0 ? _side_sum(node, bit, false, ql, qr, l0, r0) : W(0);
            auto lo = _count_sum_lt_node(_nodes[node].child[0], bit - 1, l0, r0, lower);
            auto hi = _count_sum_lt_node(_nodes[node].child[1], bit - 1, ql - l0, qr - r0, upper);
            return sum0 - lo.second + hi.second;
        }
        return W(0);
    }

    /// @brief 区間 [l, r) のキーが小さい方から k 個の重みの総和を返す / O(log(n)log(σ))
    W sum_k_smallest(const int l, const int r, const int k) const {
        assert(0 <= l && l <= r && r <= len());
        assert(0 <= k && k <= r - l);
        return _sum_k(l, r, k, false);
    }

    /// @brief 区間 [l, r) のキーが大きい方から k 個の重みの総和を返す / O(log(n)log(σ))
    W sum_k_largest(const int l, const int r, const int k) const {
        assert(0 <= l && l <= r && r <= len());
        assert(0 <= k && k <= r - l);
        return _sum_k(l, r, k, true);
    }

    /// @brief 小さいキーから重みを集約し、pred が真である最大範囲を (count, boundary_value, aggregate) で返す / O(log(n)log(σ))
    /// pred(0) == true で、要素を加える順に一度偽になると真へ戻らないことを前提とする。全要素で真なら boundary_value として -1 を返す
    template<class Pred>
    tuple<int, T, W> max_right_smallest(const int l, const int r, Pred pred) const {
        return _max_right(l, r, false, pred);
    }

    /// @brief 大きいキーから重みを集約し、pred が真である最大範囲を (count, boundary_value, aggregate) で返す / O(log(n)log(σ))
    /// pred(0) == true で、要素を加える順に一度偽になると真へ戻らないことを前提とする。全要素で真なら boundary_value として -1 を返す
    template<class Pred>
    tuple<int, T, W> max_right_largest(const int l, const int r, Pred pred) const {
        return _max_right(l, r, true, pred);
    }

    /// @brief 小さいキーから採用し、重みの総和が budget 以下である最大個数を返す / 重みは非負 / O(log(n)log(σ))
    int max_count_smallest_sum_le(const int l, const int r, const W budget) const {
        assert(budget >= W(0));
        auto [count, value, sum] = max_right_smallest(l, r, [&](W sum) { return sum <= budget; });
        return count;
    }

    /// @brief 大きいキーから採用し、重みの総和が budget 以下である最大個数を返す / 重みは非負 / O(log(n)log(σ))
    int max_count_largest_sum_le(const int l, const int r, const W budget) const {
        assert(budget >= W(0));
        auto [count, value, sum] = max_right_largest(l, r, [&](W sum) { return sum <= budget; });
        return count;
    }

    /// @brief 小さいキーからの累積重みが初めて target 以上になるキーを返す / O(log(n)log(σ))
    /// 重みが非負で、0 < target <= range_sum(l, r) であることを前提とする
    T weighted_quantile(const int l, const int r, const W target) const {
        assert(W(0) < target && target <= range_sum(l, r));
        auto [count, value, sum] = max_right_smallest(l, r, [&](W sum) { return sum < target; });
        return value;
    }

    /// @brief 重み付き numerator / denominator 分位点を返す / 重みは非負 / O(log(n)log(σ))
    T weighted_quantile(const int l, const int r, const long long numerator, const long long denominator) const {
        assert(0 < numerator && numerator <= denominator);
        W total = range_sum(l, r);
        assert(total > W(0));
        return weighted_quantile(l, r, _quantile_target(total, numerator, denominator));
    }

    /// @brief 重み付き中央値を返す / 重みは非負 / O(log(n)log(σ))
    T weighted_median(const int l, const int r) const {
        return weighted_quantile(l, r, 1, 2);
    }

    /// @brief 小さいキーから採用し、重みの総和が target 以上になる最小個数を返す / 重みは非負、達成できなければ -1 / O(log(n)log(σ))
    int min_count_smallest_sum_ge(const int l, const int r, const W target) const {
        assert(0 <= l && l <= r && r <= len());
        return _min_count_sum_ge(l, r, target, false);
    }

    /// @brief 大きいキーから採用し、重みの総和が target 以上になる最小個数を返す / 重みは非負、達成できなければ -1 / O(log(n)log(σ))
    int min_count_largest_sum_ge(const int l, const int r, const W target) const {
        assert(0 <= l && l <= r && r <= len());
        return _min_count_sum_ge(l, r, target, true);
    }

    /// @brief 要素数を返す / O(1)
    int len() const { return _size; }

    /// @brief キー列を返す / O(nlog(σ))
    vector<T> tovector() const {
        vector<T> result(_size, 0);
        if (_size == 0) return result;

        vector<int> order(_size), work(_size);
        iota(order.begin(), order.end(), 0);
        auto dfs = [&](auto &&dfs, int node, int bit, int left, int right) -> void {
            if (left == right || bit < 0) return;
            const vector<uint8_t> bits = bit == 0 ? _bottom.tovector(_nodes[node].s).first : _nodes[node].v.tovector();
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

    /// @brief 重み列を返す / O(n)
    vector<W> toweights() const {
        if (_size == 0) return {};
        if (_log == 1) return _bottom.tovector(_nodes[_root].s).second;
        return _weights.tovector(_nodes[_root].w);
    }

    /// @brief (key, weight) の列を返す / O(nlog(σ))
    vector<pair<T, W>> toitems() const {
        const vector<T> keys = tovector();
        const vector<W> weights = toweights();
        vector<pair<T, W>> result(_size);
        for (int i = 0; i < _size; ++i) result[i] = {keys[i], weights[i]};
        return result;
    }

    /// @brief (key, weight) の列を表示する / O(nlog(σ))
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
