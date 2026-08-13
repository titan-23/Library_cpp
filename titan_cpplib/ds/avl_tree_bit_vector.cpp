#pragma once

#include <algorithm>
#include <array>
#include <cassert>
#include <cstdint>
#include <iostream>
#include <utility>
#include <vector>
#include "titan_cpplib/others/bit.cpp"
using namespace std;

// AVLTreeBitVector
namespace titan23 {

class AVLTreeBitVector {
private:
    using uint128 = __uint128_t;

    static constexpr int _W = 127;
    static constexpr int _MAX_HEIGHT = 64;

    struct Node {
        uint128 key;
        int left;
        int right;
        int size;
        int total;
        uint8_t bit_len;
        uint8_t key_total;
        uint8_t height;

        Node() : key(0), left(0), right(0), size(0), total(0), bit_len(0), key_total(0), height(0) {}

        Node(const uint128 key, const uint8_t bit_len, const uint8_t key_total)
            : key(key), left(0), right(0), size(bit_len), total(key_total), bit_len(bit_len), key_total(key_total), height(1) {}
    };

    vector<Node> _nodes;
    int _root;

    int _prefix_total(const uint128 key, const int bit_len, const int take) const {
        const uint64_t low = key;
        if (bit_len <= 64) return titan23::popcount(low >> ((bit_len - take) & 63)) * (take != 0);
        const int high_len = bit_len - 64;
        const uint64_t high = key >> 64;
        if (take <= high_len) return titan23::popcount(high >> (high_len - take));
        return titan23::popcount(high) + titan23::popcount(low >> (bit_len - take));
    }

    bool _bit_at(const uint128 value, const int k) const {
        const uint64_t word = k < 64 ? static_cast<uint64_t>(value) : static_cast<uint64_t>(value >> 64);
        return (word >> (k & 63)) & 1;
    }

    uint128 _mask(const int bit_len) const { return bit_len == 0 ? 0 : (static_cast<uint128>(1) << bit_len) - 1; }

    uint64_t _mask64(const int bit_len) const { return bit_len == 64 ? UINT64_MAX : bit_len == 0 ? 0 : (static_cast<uint64_t>(1) << bit_len) - 1; }

    uint128 _bit_insert(const uint128 value, const int lower_bit_len, const bool bit) const {
        const uint64_t low = value;
        const uint64_t high = value >> 64;
        if (lower_bit_len < 64) {
            const uint64_t mask = _mask64(lower_bit_len);
            const uint64_t new_low = ((low & ~mask) << 1) | (static_cast<uint64_t>(bit) << lower_bit_len) | (low & mask);
            return (static_cast<uint128>((high << 1) | (low >> 63)) << 64) | new_low;
        }
        const int k = lower_bit_len - 64;
        const uint64_t mask = _mask64(k);
        const uint64_t new_high = ((high & ~mask) << 1) | (static_cast<uint64_t>(bit) << k) | (high & mask);
        return (static_cast<uint128>(new_high) << 64) | low;
    }

    uint128 _bit_pop(const uint128 value, const int lower_bit_len) const {
        const uint64_t low = value;
        const uint64_t high = value >> 64;
        const int k = lower_bit_len - 1;
        if (k < 64) {
            const uint64_t new_low = ((low & ~_mask64(k + 1)) >> 1) | (low & _mask64(k)) | ((high & 1) << 63);
            return (static_cast<uint128>(high >> 1) << 64) | new_low;
        }
        const int high_k = k - 64;
        const uint64_t new_high = ((high & ~_mask64(high_k + 1)) >> 1) | (high & _mask64(high_k));
        return (static_cast<uint128>(new_high) << 64) | low;
    }

    int _make_node(const bool bit, const uint8_t bit_len = 1, const uint128 key = 0) {
        const uint128 value = bit_len == 1 ? bit : key;
        const uint8_t key_total = static_cast<uint8_t>(popcount(value));
        const int node = _nodes.size();
        _nodes.emplace_back(value, bit_len, key_total);
        return node;
    }

    int _size(const int node) const { return _nodes[node].size; }

    int _total(const int node) const { return _nodes[node].total; }

    int _height(const int node) const { return _nodes[node].height; }

    int _balance(const int node) const { return _height(_nodes[node].left) - _height(_nodes[node].right); }

    void _pull_aggregate(const int node) {
        Node &n = _nodes[node];
        n.size = _size(n.left) + n.bit_len + _size(n.right);
        n.total = _total(n.left) + n.key_total + _total(n.right);
    }

    void _pull(const int node) {
        Node &n = _nodes[node];
        _pull_aggregate(node);
        const int left_height = _height(n.left);
        const int right_height = _height(n.right);
        n.height = max(left_height, right_height) + 1;
    }

    int _rotate_left(const int node) {
        const int right = _nodes[node].right;
        assert(right != 0);
        _nodes[node].right = _nodes[right].left;
        _nodes[right].left = node;
        _pull(node);
        _pull(right);
        return right;
    }

    int _rotate_right(const int node) {
        const int left = _nodes[node].left;
        assert(left != 0);
        _nodes[node].left = _nodes[left].right;
        _nodes[left].right = node;
        _pull(node);
        _pull(left);
        return left;
    }

    int _rebalance(const int node) {
        _pull(node);
        const int balance = _balance(node);
        if (balance == 2) {
            const int left = _nodes[node].left;
            if (_balance(left) < 0) {
                _nodes[node].left = _rotate_left(left);
            }
            return _rotate_right(node);
        }
        if (balance == -2) {
            const int right = _nodes[node].right;
            if (_balance(right) > 0) {
                _nodes[node].right = _rotate_right(right);
            }
            return _rotate_left(node);
        }
        return node;
    }

    void _push_path(array<int, _MAX_HEIGHT> &path, uint64_t &directions, int &depth, const int node, const bool go_right) const {
        assert(depth < _MAX_HEIGHT);
        path[depth] = node;
        if (go_right) {
            directions |= static_cast<uint64_t>(1) << depth;
        }
        ++depth;
    }

    void _add_path(const array<int, _MAX_HEIGHT> &path, const int begin, const int end, const int size_delta, const int total_delta) {
        for (int i = begin; i < end; ++i) {
            Node &node = _nodes[path[i]];
            node.size += size_delta;
            node.total += total_delta;
        }
    }

    void _rebuild_path(const array<int, _MAX_HEIGHT> &path, const uint64_t directions, int depth, int child) {
        while (depth > 0) {
            --depth;
            const int node = path[depth];
            if ((directions >> depth) & 1) {
                _nodes[node].right = child;
            } else {
                _nodes[node].left = child;
            }
            child = _rebalance(node);
        }
        _root = child;
    }

    void _build(const vector<uint8_t> &a, const int start, const int end) {
        const int n = end - start;
        if (n == 0) return;

        reserve(n);
        const int node_count = (n + _W - 1) / _W;
        const int first_node = _nodes.size();
        for (int i = 0; i < n; i += _W) {
            uint128 key = 0;
            const int bit_len = min(_W, n - i);
            for (int j = 0; j < bit_len; ++j) key = (key << 1) | (a[start + i + j] != 0);
            _nodes.emplace_back(key, bit_len, popcount(key));
        }

        struct BuildTask {
            int left;
            int right;
            int parent;
            bool is_right;
        };

        vector<BuildTask> tasks;
        vector<int> order;
        tasks.reserve(node_count);
        order.reserve(node_count);
        tasks.push_back({first_node, first_node + node_count, 0, false});

        while (!tasks.empty()) {
            const BuildTask task = tasks.back();
            tasks.pop_back();
            const int mid = (task.left + task.right) / 2;
            const int node = mid;
            order.emplace_back(node);

            if (task.parent == 0) {
                _root = node;
            } else if (task.is_right) {
                _nodes[task.parent].right = node;
            } else {
                _nodes[task.parent].left = node;
            }

            if (mid + 1 < task.right) {
                tasks.push_back({mid + 1, task.right, node, true});
            }
            if (task.left < mid) {
                tasks.push_back({task.left, mid, node, false});
            }
        }

        for (auto it = order.rbegin(); it != order.rend(); ++it) {
            _pull(*it);
        }
    }

    int _pref(int r) const {
        int node = _root;
        int result = 0;
        while (r > 0) {
            assert(node != 0);
            const Node &n = _nodes[node];
            const int left_size = _size(n.left);
            if (r <= left_size) {
                node = n.left;
                continue;
            }

            const int node_end = left_size + n.bit_len;
            if (r <= node_end) {
                const int take = r - left_size;
                result += _total(n.left) + _prefix_total(n.key, n.bit_len, take);
                break;
            }

            result += _total(n.left) + n.key_total;
            r -= node_end;
            node = n.right;
        }
        return result;
    }

    int _select_in_key(const Node &node, const int k, const bool bit) const {
        int left = 0;
        int right = node.bit_len;
        while (right - left > 1) {
            const int mid = (left + right) / 2;
            const int ones = _prefix_total(node.key, node.bit_len, mid);
            const int count = bit ? ones : mid - ones;
            if (count > k) {
                right = mid;
            } else {
                left = mid;
            }
        }
        return left;
    }

    void _pop_at_node(array<int, _MAX_HEIGHT> &path, uint64_t directions, int depth, const int node, const int k, const bool removed_bit) {
        Node &target = _nodes[node];
        if (target.bit_len > 1) {
            target.key = _bit_pop(target.key, target.bit_len - k);
            --target.bit_len;
            target.key_total -= removed_bit;
            --target.size;
            target.total -= removed_bit;
            _add_path(path, 0, depth, -1, -removed_bit);
            return;
        }

        if (target.left == 0 || target.right == 0) {
            const int child = target.left == 0 ? target.right : target.left;
            _rebuild_path(path, directions, depth, child);
            return;
        }

        _push_path(path, directions, depth, node, false);
        int predecessor = target.left;
        while (_nodes[predecessor].right != 0) {
            _push_path(path, directions, depth, predecessor, true);
            predecessor = _nodes[predecessor].right;
        }

        target.key = _nodes[predecessor].key;
        target.bit_len = _nodes[predecessor].bit_len;
        target.key_total = _nodes[predecessor].key_total;
        const int child = _nodes[predecessor].left;
        _rebuild_path(path, directions, depth, child);
    }

public:
    AVLTreeBitVector() : _nodes(1), _root(0) {}

    AVLTreeBitVector(const vector<uint8_t> &a) : _nodes(1), _root(0) {
        _build(a, 0, a.size());
    }

    AVLTreeBitVector(const vector<uint8_t> &a, const int start, const int end) : _nodes(1), _root(0) {
        _build(a, start, end);
    }

    void clear() {
        _nodes.resize(1);
        _root = 0;
    }

    void reserve(const int n) {
        _nodes.reserve((n + _W - 1) / _W + 1);
    }

    void insert(const int k, const bool bit) { _insert_and_rank1(k, bit); }

    bool pop(const int k) { return _access_pop_and_rank1(k) & 1; }

    void set(int k, const bool bit) {
        assert(0 <= k && k < len());
        array<int, _MAX_HEIGHT> path;
        int depth = 0;
        int node = _root;

        while (true) {
            Node &n = _nodes[node];
            const int left_size = _size(n.left);
            const int node_end = left_size + n.bit_len;
            if (left_size <= k && k < node_end) {
                k -= left_size;
                const int shift = n.bit_len - k - 1;
                const bool old = _bit_at(n.key, shift);
                if (old == bit) return;
                const int delta = bit ? 1 : -1;
                n.key ^= static_cast<uint128>(1) << shift;
                n.key_total += delta;
                n.total += delta;
                _add_path(path, 0, depth, 0, delta);
                return;
            }

            const bool go_right = node_end <= k;
            assert(depth < _MAX_HEIGHT);
            path[depth++] = node;
            if (go_right) {
                k -= node_end;
                node = n.right;
            } else {
                node = n.left;
            }
        }
    }

    vector<uint8_t> tovector() const {
        vector<uint8_t> result(len());
        if (_root == 0) return result;

        array<int, _MAX_HEIGHT> stack;
        int depth = 0;
        int index = 0;
        int node = _root;
        while (node != 0 || depth > 0) {
            while (node != 0) {
                assert(depth < _MAX_HEIGHT);
                stack[depth++] = node;
                node = _nodes[node].left;
            }
            node = stack[--depth];
            const Node &n = _nodes[node];
            for (int i = n.bit_len - 1; i >= 0; --i) {
                result[index++] = (n.key >> i) & 1;
            }
            node = n.right;
        }
        return result;
    }

    bool access(int k) const {
        assert(0 <= k && k < len());
        int node = _root;
        while (true) {
            const Node &n = _nodes[node];
            const int left_size = _size(n.left);
            const int node_end = left_size + n.bit_len;
            if (left_size <= k && k < node_end) {
                k -= left_size;
                return _bit_at(n.key, n.bit_len - k - 1);
            }
            if (k < left_size) {
                node = n.left;
            } else {
                k -= node_end;
                node = n.right;
            }
        }
    }

    int rank0(const int r) const {
        assert(0 <= r && r <= len());
        return r - _pref(r);
    }

    int rank1(const int r) const {
        assert(0 <= r && r <= len());
        return _pref(r);
    }

    int rank(const int r, const bool bit) const { return bit ? rank1(r) : rank0(r); }

    pair<int, int> _rank0_pair(const int l, const int r) const {
        assert(0 <= l && l <= r && r <= len());
        return {l - _pref(l), r - _pref(r)};
    }

    int select0(const int k) const {
        assert(0 <= k && k < len() - _total(_root));
        int node = _root;
        int offset = 0;
        int target = k;
        while (true) {
            const Node &n = _nodes[node];
            const int left_zero = _size(n.left) - _total(n.left);
            const int key_zero = n.bit_len - n.key_total;
            if (target < left_zero) {
                node = n.left;
            } else if (target < left_zero + key_zero) {
                return offset + _size(n.left) + _select_in_key(n, target - left_zero, false);
            } else {
                target -= left_zero + key_zero;
                offset += _size(n.left) + n.bit_len;
                node = n.right;
            }
        }
    }

    int select1(const int k) const {
        assert(0 <= k && k < _total(_root));
        int node = _root;
        int offset = 0;
        int target = k;
        while (true) {
            const Node &n = _nodes[node];
            const int left_one = _total(n.left);
            if (target < left_one) {
                node = n.left;
            } else if (target < left_one + n.key_total) {
                return offset + _size(n.left) + _select_in_key(n, target - left_one, true);
            } else {
                target -= left_one + n.key_total;
                offset += _size(n.left) + n.bit_len;
                node = n.right;
            }
        }
    }

    int select(const int k, const bool bit) const { return bit ? select1(k) : select0(k); }

    int _select_pop(int k, const bool bit) {
        assert(0 <= k && k < (bit ? _total(_root) : len() - _total(_root)));
        array<int, _MAX_HEIGHT> path;
        uint64_t directions = 0;
        int depth = 0;
        int offset = 0;
        int node = _root;

        while (true) {
            const Node &n = _nodes[node];
            const int left_count = bit ? _total(n.left) : _size(n.left) - _total(n.left);
            const int key_count = bit ? n.key_total : n.bit_len - n.key_total;
            if (k < left_count) {
                _push_path(path, directions, depth, node, false);
                node = n.left;
            } else if (k < left_count + key_count) {
                const int key_index = _select_in_key(n, k - left_count, bit);
                const int result = offset + _size(n.left) + key_index;
                _pop_at_node(path, directions, depth, node, key_index, bit);
                return result;
            } else {
                k -= left_count + key_count;
                offset += _size(n.left) + n.bit_len;
                _push_path(path, directions, depth, node, true);
                node = n.right;
            }
        }
    }

    int _insert_and_rank1(int k, const bool bit) {
        assert(0 <= k && k <= len());
        if (_root == 0) {
            _root = _make_node(bit);
            return 0;
        }

        array<int, _MAX_HEIGHT> path;
        uint64_t directions = 0;
        int depth = 0;
        int result = 0;
        int node = _root;

        while (true) {
            const Node &n = _nodes[node];
            const int left_size = _size(n.left);
            const int node_end = left_size + n.bit_len;
            if (left_size <= k && k <= node_end) break;

            const bool go_right = node_end < k;
            _push_path(path, directions, depth, node, go_right);
            if (go_right) {
                result += _total(n.left) + n.key_total;
                k -= node_end;
                node = n.right;
            } else {
                node = n.left;
            }
        }

        Node &target = _nodes[node];
        const int left_size = _size(target.left);
        k -= left_size;
        result += _total(target.left) + _prefix_total(target.key, target.bit_len, k);

        if (target.bit_len < _W) {
            target.key = _bit_insert(target.key, target.bit_len - k, bit);
            ++target.bit_len;
            target.key_total += bit;
            ++target.size;
            target.total += bit;
            _add_path(path, 0, depth, 1, bit);
            return result;
        }

        const uint128 value = _bit_insert(target.key, _W - k, bit);
        const bool overflow_bit = value >> _W;
        target.key = value & _mask(_W);
        target.key_total += bit;
        target.key_total -= overflow_bit;

        const int target_depth = depth;
        _push_path(path, directions, depth, node, false);
        int left = target.left;
        if (left == 0) {
            const int new_node = _make_node(overflow_bit);
            _rebuild_path(path, directions, depth, new_node);
            return result;
        }

        while (_nodes[left].right != 0) {
            _push_path(path, directions, depth, left, true);
            left = _nodes[left].right;
        }

        if (_nodes[left].bit_len < _W) {
            Node &left_node = _nodes[left];
            left_node.key = (left_node.key << 1) | overflow_bit;
            ++left_node.bit_len;
            left_node.key_total += overflow_bit;
            ++left_node.size;
            left_node.total += overflow_bit;
            _add_path(path, 0, target_depth + 1, 1, bit);
            _add_path(path, target_depth + 1, depth, 1, overflow_bit);
            return result;
        }

        _push_path(path, directions, depth, left, true);
        const int new_node = _make_node(overflow_bit);
        _rebuild_path(path, directions, depth, new_node);
        return result;
    }

    int _access_pop_and_rank1(int k) {
        assert(0 <= k && k < len());
        array<int, _MAX_HEIGHT> path;
        uint64_t directions = 0;
        int depth = 0;
        int result = 0;
        int node = _root;

        while (true) {
            const Node &n = _nodes[node];
            const int left_size = _size(n.left);
            const int node_end = left_size + n.bit_len;
            if (left_size <= k && k < node_end) break;

            const bool go_right = node_end <= k;
            _push_path(path, directions, depth, node, go_right);
            if (go_right) {
                result += _total(n.left) + n.key_total;
                k -= node_end;
                node = n.right;
            } else {
                node = n.left;
            }
        }

        Node &target = _nodes[node];
        const int left_size = _size(target.left);
        k -= left_size;
        result += _total(target.left) + _prefix_total(target.key, target.bit_len, k);
        const bool removed_bit = _bit_at(target.key, target.bit_len - k - 1);
        _pop_at_node(path, directions, depth, node, k, removed_bit);
        return (result << 1) | removed_bit;
    }

    pair<bool, int> _access_ans_rank1(int k) const {
        assert(0 <= k && k < len());
        int node = _root;
        int result = 0;
        while (true) {
            const Node &n = _nodes[node];
            const int left_size = _size(n.left);
            const int node_end = left_size + n.bit_len;
            if (left_size <= k && k < node_end) {
                k -= left_size;
                result += _total(n.left) + _prefix_total(n.key, n.bit_len, k);
                const bool bit = _bit_at(n.key, n.bit_len - k - 1);
                return {bit, result};
            }
            if (k < left_size) {
                node = n.left;
            } else {
                result += _total(n.left) + n.key_total;
                k -= node_end;
                node = n.right;
            }
        }
    }

    pair<bool, int> _access_set_and_rank1(int k, const bool bit) {
        assert(0 <= k && k < len());
        array<int, _MAX_HEIGHT> path;
        int depth = 0;
        int node = _root;
        int result = 0;
        while (true) {
            Node &n = _nodes[node];
            const int left_size = _size(n.left);
            const int node_end = left_size + n.bit_len;
            if (left_size <= k && k < node_end) {
                k -= left_size;
                result += _total(n.left) + _prefix_total(n.key, n.bit_len, k);
                const int shift = n.bit_len - k - 1;
                const bool old = _bit_at(n.key, shift);
                if (old != bit) {
                    const int delta = bit ? 1 : -1;
                    n.key ^= static_cast<uint128>(1) << shift;
                    n.key_total += delta;
                    n.total += delta;
                    _add_path(path, 0, depth, 0, delta);
                }
                return {old, result};
            }
            path[depth++] = node;
            if (k < left_size) {
                node = n.left;
            } else {
                result += _total(n.left) + n.key_total;
                k -= node_end;
                node = n.right;
            }
        }
    }

    void print() const {
        const vector<uint8_t> a = tovector();
        cout << "[";
        bool first = true;
        for (const uint8_t bit : a) {
            if (!first) cout << ", ";
            first = false;
            cout << +bit;
        }
        cout << "]" << endl;
    }

    bool empty() const { return _root == 0; }

    int len() const { return _size(_root); }
};

} // namespace titan23
