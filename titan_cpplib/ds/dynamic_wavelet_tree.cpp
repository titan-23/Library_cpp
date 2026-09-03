/// https://github.com/titan-23/Library_cpp/blob/main/titan_cpplib/ds/dynamic_wavelet_tree.cpp
#pragma once

#include <array>
#include <cassert>
#include <cstdint>
#include <iostream>
#include <numeric>
#include <queue>
#include <tuple>
#include <utility>
#include <vector>
#include "titan_cpplib/others/bit.cpp"
// #include "titan_cpplib/ds/avl_tree_bit_vector.cpp"
#include "titan_cpplib/ds/b_tree_bit_vector.cpp"
#include "titan_cpplib/others/print.cpp"
using namespace std;

// DynamicWaveletTree
namespace titan23 {

/**
 * @brief 動的ウェーブレット木
 *
 * @tparam T 値の型
 */
template<typename T>
class DynamicWaveletTree {
private:
    static constexpr int _NIL = -1;
    static constexpr int _MAX_LOG = 64;

    struct Node {
        int child[2];
        BTreeBitVector v;

        Node() : child{_NIL, _NIL} {}
        Node(const vector<uint8_t> &a, const int start, const int end) : child{_NIL, _NIL}, v(a, start, end) {}
    };

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
        return max<size_t>(1, ans);
    }

    int _make_node() {
        if (_free != _NIL) {
            int node = _free;
            _free = _nodes[node].child[0];
            _nodes[node].child[0] = _NIL;
            _nodes[node].child[1] = _NIL;
            return node;
        }
        int node = _nodes.size();
        _nodes.emplace_back();
        return node;
    }

    int _make_node(const vector<uint8_t> &a, const int start, const int end) {
        int node = _nodes.size();
        _nodes.emplace_back(a, start, end);
        return node;
    }

    void _release_node(const int node) {
        assert(node != _root);
        assert(_nodes[node].child[0] == _NIL && _nodes[node].child[1] == _NIL);
        _nodes[node].v.clear();
        _nodes[node].child[0] = _free;
        _nodes[node].child[1] = _NIL;
        _free = node;
    }

    void _prune_path(const array<int, _MAX_LOG> &path, const array<uint8_t, _MAX_LOG> &directions, const int depth) {
        for (int i = depth - 1; i > 0; --i) {
            int node = path[i];
            if (!_nodes[node].v.empty()) break;
            int parent = path[i - 1];
            _nodes[parent].child[directions[i - 1]] = _NIL;
            _release_node(node);
        }
    }

    void _build(const vector<T> &a) {
        if (a.empty() || _log == 0) {
            _root = _make_node();
            return;
        }

        int n = a.size();
        _nodes.reserve(_estimated_node_count(n));
        vector<int> order(n), work(n);
        vector<uint8_t> bits(n);
        iota(order.begin(), order.end(), 0);

        auto build = [&](auto&& self, int bit, int l, int r, vector<int>& src, vector<int>& dst) -> int {
            if (l == r || bit < 0) return _NIL;

            int zeros = 0;
            for (int i = l; i < r; ++i) {
                bool b = (a[src[i]] >> bit) & 1;
                bits[i] = b;
                zeros += !b;
            }

            int node = _make_node(bits, l, r);
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

    int _range_freq_node(int node, int bit, int l, int r, const T x) const {
        int result = 0;
        while (node != _NIL && bit >= 0 && l < r) {
            auto [l0, r0] = _nodes[node].v._rank0_pair(l, r);
            bool b = (x >> bit) & 1;
            if (b) {
                result += r0 - l0;
                l -= l0;
                r -= r0;
            } else {
                l = l0;
                r = r0;
            }
            node = _nodes[node].child[b];
            --bit;
        }
        return result;
    }

    T _extreme(int node, int bit, int l, int r, T value, const bool largest) const {
        while (bit >= 0) {
            auto [l0, r0] = _nodes[node].v._rank0_pair(l, r);
            int count0 = r0 - l0;
            int count1 = (r - l) - count0;
            bool b = largest ? count1 > 0 : count0 == 0;
            if (b) {
                value |= (T)1 << bit;
                l -= l0;
                r -= r0;
            } else {
                l = l0;
                r = r0;
            }
            node = _nodes[node].child[b];
            --bit;
        }
        return value;
    }

public:
    /// 各要素が `[0, sigma)` の `DynamicWaveletTree` を作成する / `O(1)`
    DynamicWaveletTree(const T sigma)
        : _root(_NIL), _free(_NIL), _sigma(sigma),
          _log(sigma <= 1 ? 0 : bit_length((unsigned long long)(sigma - 1))), _size(0) {
        assert(sigma > 0);
        _root = _make_node();
    }

    /// 各要素が `[0, sigma)` の `DynamicWaveletTree` を作成する / `O(nlog(σ))`
    DynamicWaveletTree(const T sigma, const vector<T> &a)
        : _root(_NIL), _free(_NIL), _sigma(sigma),
          _log(sigma <= 1 ? 0 : bit_length((unsigned long long)(sigma - 1))), _size(a.size()) {
        assert(sigma > 0);
        _build(a);
    }

    /// 最終的な要素数を見積もってNodeと根の動的ビット列の領域を予約する
    /// 最悪 `O(nlog(σ))`
    void reserve(int expected_size) {
        assert(len() <= expected_size);
        _nodes.reserve(_estimated_node_count(expected_size));
        if (_log > 0) _nodes[_root].v.reserve(expected_size);
    }

    /// 位置 `k` に `x` を挿入する / `O(log(n)log(σ))`
    void insert(int k, const T x) {
        assert(0 <= k && k <= len());
        assert(0 <= x && x < _sigma);
        int node = _root;
        for (int bit = _log - 1; bit >= 0; --bit) {
            bool b = (x >> bit) & 1;
            int rank1 = _nodes[node].v._insert_and_rank1(k, b);
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

    /// 位置 `k` の値を削除して返す / `O(log(n)log(σ))`
    T pop(int k) {
        assert(0 <= k && k < len());
        array<int, _MAX_LOG> path;
        array<uint8_t, _MAX_LOG> directions;
        int depth = 0;
        int node = _root;
        T result = 0;

        for (int bit = _log - 1; bit >= 0; --bit) {
            path[depth] = node;
            int sb = _nodes[node].v._access_pop_and_rank1(k);
            bool b = sb & 1;
            int rank1 = sb >> 1;
            if (b) {
                result |= (T)1 << bit;
                k = rank1;
            } else {
                k -= rank1;
            }
            if (bit > 0) {
                directions[depth] = b;
                node = _nodes[node].child[b];
            }
            ++depth;
        }

        _prune_path(path, directions, depth);
        --_size;
        return result;
    }

    /// 位置 `k` の値を `x` に更新する / 共通prefixを除き `O(log(n)log(σ))`
    void set(int k, const T x) {
        assert(0 <= k && k < len());
        assert(0 <= x && x < _sigma);
        int node = _root;

        for (int bit = _log - 1; bit >= 0; --bit) {
            bool new_bit = (x >> bit) & 1;
            auto [old_bit, rank1] = _nodes[node].v._access_set_and_rank1(k, new_bit);
            int old_k = old_bit ? rank1 : k - rank1;
            if (old_bit == new_bit) {
                k = old_k;
                if (bit > 0) node = _nodes[node].child[old_bit];
                continue;
            }

            int new_k = new_bit ? rank1 : k - rank1;
            if (bit == 0) return;

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
                int sb = _nodes[old_node].v._access_pop_and_rank1(old_pos);
                bool old_lower_bit = sb & 1;
                int old_rank1 = sb >> 1;
                old_pos = old_lower_bit ? old_rank1 : old_pos - old_rank1;

                bool new_lower_bit = (x >> lower) & 1;
                int new_rank1 = _nodes[new_node].v._insert_and_rank1(new_pos, new_lower_bit);
                new_pos = new_lower_bit ? new_rank1 : new_pos - new_rank1;

                if (lower > 0) {
                    old_directions[old_depth] = old_lower_bit;
                    old_node = _nodes[old_node].child[old_lower_bit];
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

    /// 位置 `k` の値を `x` に更新する / 共通prefixを除き `O(log(n)log(σ))`
    void set_key(int k, T x) { set(k, x); }

    /// 区間 `[0, r)` の `x` の個数を返す / `O(log(n)log(σ))`
    int rank(int r, const T x) const {
        assert(0 <= r && r <= len());
        assert(0 <= x && x < _sigma);
        int node = _root;
        for (int bit = _log - 1; bit >= 0; --bit) {
            if (node == _NIL) return 0;
            bool b = (x >> bit) & 1;
            r = _nodes[node].v.rank(r, b);
            node = _nodes[node].child[b];
        }
        return r;
    }

    /// 区間 `[l, r)` の `x` の個数を返す / `O(log(n)log(σ))`
    int range_count(int l, int r, const T x) const {
        assert(0 <= l && l <= r && r <= len());
        assert(0 <= x && x < _sigma);
        int node = _root;
        for (int bit = _log - 1; bit >= 0; --bit) {
            if (node == _NIL) return 0;
            bool b = (x >> bit) & 1;
            auto [l0, r0] = _nodes[node].v._rank0_pair(l, r);
            if (b) {
                l -= l0;
                r -= r0;
            } else {
                l = l0;
                r = r0;
            }
            node = _nodes[node].child[b];
        }
        return r - l;
    }

    /// 区間 `[l, r)` で値が `[lower, upper)` にある `k` 番目の位置を返す / `O(log^2(n)log(σ))`
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

    /// 区間 `[l, r)` で値が `[lower, upper)` にある最初の位置を返す / `O(log^2(n)log(σ))`
    /// 例: `[5, 1, 4, 1, 9]`, `[lower, upper) = [1, 5)` なら `1`
    int next_index_in_value_range(const int l, const int r, const T lower, const T upper) const {
        return range_freq(l, r, lower, upper) == 0 ? -1 : kth_index_in_value_range(l, r, lower, upper, 0);
    }

    /// 区間 `[l, r)` で値が `[lower, upper)` にある最後の位置を返す / `O(log^2(n)log(σ))`
    /// 例: `[5, 1, 4, 1, 9]`, `[lower, upper) = [1, 5)` なら `3`
    int prev_index_in_value_range(const int l, const int r, const T lower, const T upper) const {
        int count = range_freq(l, r, lower, upper);
        return count == 0 ? -1 : kth_index_in_value_range(l, r, lower, upper, count - 1);
    }

    /// `k` 番目の要素を返す / `O(log(n)log(σ))`
    T access(int k) const {
        assert(0 <= k && k < len());
        int node = _root;
        T result = 0;
        for (int bit = _log - 1; bit >= 0; --bit) {
            auto [b, rank1] = _nodes[node].v._access_ans_rank1(k);
            if (b) {
                result |= (T)1 << bit;
                k = rank1;
            } else {
                k -= rank1;
            }
            node = _nodes[node].child[b];
        }
        return result;
    }

    /// 区間 `[l, r)` で昇順 `k` 番目の値を返す / `O(log(n)log(σ))`
    T kth_smallest(int l, int r, int k) const {
        assert(0 <= l && l <= r && r <= len());
        assert(0 <= k && k < r - l);
        int node = _root;
        T result = 0;
        for (int bit = _log - 1; bit >= 0; --bit) {
            auto [l0, r0] = _nodes[node].v._rank0_pair(l, r);
            int count0 = r0 - l0;
            bool b = count0 <= k;
            if (b) {
                result |= (T)1 << bit;
                k -= count0;
                l -= l0;
                r -= r0;
            } else {
                l = l0;
                r = r0;
            }
            node = _nodes[node].child[b];
        }
        return result;
    }

    /// 区間 `[l, r)` で降順 `k` 番目の値を返す / `O(log(n)log(σ))`
    T kth_largest(int l, int r, const int k) const {
        return kth_smallest(l, r, r - l - k - 1);
    }

    /// 区間 `[l, r)` で頻度が高い値を最大 `k` 種類返す
    /// 訪問Node数を `p` として `O(p(log(n)+log(p)))`
    /// 例: `[1, 2, 1, 3, 1, 2]` で `topk(0, 6, 2)` は `{(1, 3), (2, 2)}`
    vector<pair<T, int>> topk(int l, int r, int k) const {
        assert(0 <= l && l <= r && r <= len());
        vector<pair<T, int>> ans;
        if (l == r || k <= 0) return ans;
        priority_queue<tuple<int, T, int, int, int, int>> hq;
        hq.emplace(r - l, 0, _root, _log - 1, l, r);
        while (!hq.empty() && k > 0) {
            auto [length, x, node, bit, ql, qr] = hq.top();
            hq.pop();
            if (bit < 0) {
                ans.emplace_back(x, length);
                --k;
                continue;
            }
            auto [l0, r0] = _nodes[node].v._rank0_pair(ql, qr);
            int cnt0 = r0 - l0;
            int cnt1 = length - cnt0;
            if (cnt0 > 0) hq.emplace(cnt0, x, _nodes[node].child[0], bit - 1, l0, r0);
            if (cnt1 > 0) {
                hq.emplace(cnt1, x | (static_cast<T>(1) << bit), _nodes[node].child[1], bit - 1, ql - l0,
                           qr - r0);
            }
        }
        return ans;
    }

    pair<bool, T> has_majority(int l, int r) const {
        assert(0 <= l && l < r && r <= len());
        int node = _root;
        int length = (r - l) / 2 + 1;
        T result = 0;
        for (int bit = _log - 1; bit >= 0; --bit) {
            auto [l0, r0] = _nodes[node].v._rank0_pair(l, r);
            int count0 = r0 - l0;
            int count1 = (r - l) - count0;
            if (count0 >= length) {
                l = l0;
                r = r0;
                node = _nodes[node].child[0];
            } else if (count1 >= length) {
                result |= (T)1 << bit;
                l -= l0;
                r -= r0;
                node = _nodes[node].child[1];
            } else {
                return {false, 0};
            }
        }
        return {true, result};
    }

    /// 区間 `[l, r)` で `x` 未満の要素の個数を返す / `O(log(n)log(σ))`
    int range_freq(const int l, const int r, const T x) const {
        assert(0 <= l && l <= r && r <= len());
        if (x <= 0) return 0;
        if (x >= _sigma) return r - l;
        return _range_freq_node(_root, _log - 1, l, r, x);
    }

    /// 区間 `[l, r)` で `lower` 以上 `upper` 未満の要素の個数を返す / `O(log(n)log(σ))`
    int range_freq(const int l, const int r, T lower, T upper) const {
        assert(0 <= l && l <= r && r <= len());
        if (lower >= upper || upper <= 0 || lower >= _sigma) return 0;
        if (lower <= 0) return range_freq(l, r, upper);
        if (upper >= _sigma) return r - l - range_freq(l, r, lower);

        int node = _root;
        int left = l;
        int right = r;
        for (int bit = _log - 1; bit >= 0; --bit) {
            if (node == _NIL || left == right) return 0;
            auto [l0, r0] = _nodes[node].v._rank0_pair(left, right);
            bool lower_bit = (lower >> bit) & 1;
            bool upper_bit = (upper >> bit) & 1;
            if (lower_bit == upper_bit) {
                if (lower_bit) {
                    left -= l0;
                    right -= r0;
                } else {
                    left = l0;
                    right = r0;
                }
                node = _nodes[node].child[lower_bit];
                continue;
            }

            int left_count = r0 - l0;
            int lower_part = left_count - _range_freq_node(_nodes[node].child[0], bit - 1, l0, r0, lower);
            int upper_part = _range_freq_node(_nodes[node].child[1], bit - 1, left - l0, right - r0, upper);
            return lower_part + upper_part;
        }
        return 0;
    }

    /// `k` 番目の `x` の位置を返す / `O(log(n)log(σ))`
    int select(int k, const T x) const {
        array<int, _MAX_LOG> path;
        int depth = 0;
        int node = _root;
        for (int bit = _log - 1; bit >= 0; --bit) {
            path[depth++] = node;
            if (bit > 0) node = _nodes[node].child[(x >> bit) & 1];
        }
        for (int bit = 0; bit < _log; ++bit) {
            int p = path[_log - bit - 1];
            k = _nodes[p].v.select(k, (x >> bit) & 1);
        }
        return k;
    }

    /// 区間 `[l, r)` にある `k` 番目の `x` の位置を返す / `O(log(n)log(σ))`
    /// 例: `[5, 1, 4, 1, 9]` で `range_select(1, 5, 1, 1)` は `3`
    int range_select(int l, int r, int k, T x) const {
        assert(0 <= k && k < range_count(l, r, x));
        return select(rank(l, x) + k, x);
    }

    /// `k` 番目の `x` の位置を返して削除する / `O(log(n)log(σ))`
    int select_remove(int k, const T x) {
        array<int, _MAX_LOG> path;
        array<uint8_t, _MAX_LOG> directions;
        int depth = 0;
        int node = _root;
        for (int bit = _log - 1; bit >= 0; --bit) {
            path[depth] = node;
            bool b = (x >> bit) & 1;
            if (bit > 0) {
                directions[depth] = b;
                node = _nodes[node].child[b];
            }
            ++depth;
        }
        for (int bit = 0; bit < _log; ++bit) {
            int p = path[_log - bit - 1];
            k = _nodes[p].v._select_pop(k, (x >> bit) & 1);
        }
        _prune_path(path, directions, depth);
        --_size;
        return k;
    }

    /// 区間 `[l, r)` で `x` 未満のうち最大の要素を返す
    T prev_value(int l, int r, const T x) const {
        if (l == r || x <= 0) return -1;
        if (x >= _sigma) return _extreme(_root, _log - 1, l, r, 0, true);

        int node = _root;
        int candidate = _NIL;
        int candidate_bit = -1;
        int candidate_l = 0;
        int candidate_r = 0;
        T value = 0;
        T candidate_value = 0;

        for (int bit = _log - 1; node != _NIL && bit >= 0 && l < r; --bit) {
            auto [l0, r0] = _nodes[node].v._rank0_pair(l, r);
            bool b = (x >> bit) & 1;
            if (b) {
                if (l0 < r0) {
                    candidate = _nodes[node].child[0];
                    candidate_bit = bit - 1;
                    candidate_l = l0;
                    candidate_r = r0;
                    candidate_value = value;
                }
                value |= (T)1 << bit;
                l -= l0;
                r -= r0;
            } else {
                l = l0;
                r = r0;
            }
            node = _nodes[node].child[b];
        }

        if (candidate_bit < 0) return candidate_r > candidate_l ? candidate_value : (T)-1;
        if (candidate == _NIL) return -1;
        return _extreme(candidate, candidate_bit, candidate_l, candidate_r, candidate_value, true);
    }

    /// 区間 `[l, r)` で `x` 以上のうち最小の要素を返す
    T next_value(int l, int r, const T x) const {
        if (l == r || x >= _sigma) return -1;
        if (x <= 0) return _extreme(_root, _log - 1, l, r, 0, false);

        int node = _root;
        int candidate = _NIL;
        int candidate_bit = -1;
        int candidate_l = 0;
        int candidate_r = 0;
        T value = 0;
        T candidate_value = 0;

        for (int bit = _log - 1; node != _NIL && bit >= 0 && l < r; --bit) {
            auto [l0, r0] = _nodes[node].v._rank0_pair(l, r);
            bool b = (x >> bit) & 1;
            if (!b && r - r0 > l - l0) {
                candidate = _nodes[node].child[1];
                candidate_bit = bit - 1;
                candidate_l = l - l0;
                candidate_r = r - r0;
                candidate_value = value | ((T)1 << bit);
            }

            if (b) {
                value |= (T)1 << bit;
                l -= l0;
                r -= r0;
            } else {
                l = l0;
                r = r0;
            }
            node = _nodes[node].child[b];
        }

        if (l < r) return value;
        if (candidate_bit < 0) return candidate_r > candidate_l ? candidate_value : (T)-1;
        if (candidate == _NIL) return -1;
        return _extreme(candidate, candidate_bit, candidate_l, candidate_r, candidate_value, false);
    }

    /// 要素数を返す / `O(1)`
    int len() const { return _size; }

    /// `vector` にして返す / `O(nlog(σ))`
    vector<T> tovector() const {
        vector<T> result(_size, 0);
        if (_size == 0 || _log == 0) return result;

        vector<int> order(_size), work(_size);
        iota(order.begin(), order.end(), 0);
        auto dfs = [&](auto &&dfs, int node, int bit, int left, int right) -> void {
            if (left == right || bit < 0) return;

            int zeros = 0;
            {
                const vector<uint8_t> bits = _nodes[node].v.tovector();
                for (int i = left; i < right; ++i) {
                    bool b = bits[i - left];
                    if (b) result[order[i]] |= (T)1 << bit;
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
            }

            int mid = left + zeros;
            dfs(dfs, _nodes[node].child[0], bit - 1, left, mid);
            dfs(dfs, _nodes[node].child[1], bit - 1, mid, right);
        };
        dfs(dfs, _root, _log - 1, 0, _size);
        return result;
    }

    /// 表示する / `O(nlog(σ))`
    void print() const {
        const vector<T> a = tovector();
        cout << "[";
        bool first = true;
        for (T value : a) {
            if (!first) cout << ", ";
            first = false;
            cout << value;
        }
        cout << "]" << endl;
    }

    friend ostream& operator<<(ostream& os, const DynamicWaveletTree<T> &dwt) {
        os << dwt.tovector();
        return os;
    }
};

} // namespace titan23
