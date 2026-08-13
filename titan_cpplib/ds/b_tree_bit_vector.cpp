#pragma once

#include <algorithm>
#include <array>
#include <cassert>
#include <cstdint>
#include <iostream>
#include <memory>
#include <utility>
#include <vector>
#include "titan_cpplib/others/bit.cpp"
using namespace std;

// BTreeBitVector
namespace titan23 {

class BTreeBitVectorCore {
private:
    using uint128 = __uint128_t;

    static constexpr int _W = 127;
    static constexpr int _SPLIT_W = 64;
    static constexpr int _LEAF_CAP = 8;
    static constexpr int _LEAF_MIN = _LEAF_CAP / 2;
    static constexpr int _BRANCH = 12;
    static constexpr int _BRANCH_MIN = _BRANCH / 2;
    static constexpr int _MAX_HEIGHT = 16;

    struct Leaf {
        array<uint128, _LEAF_CAP> key;
        array<uint8_t, _LEAF_CAP> bit_len;
        array<uint8_t, _LEAF_CAP> key_total;
        int size;
        int total;
        int next;
        int count;

        Leaf() : size(0), total(0), next(0), count(0) {}
    };

    struct Internal {
        array<int, _BRANCH> child;
        array<int, _BRANCH> child_size;
        array<int, _BRANCH> child_total;
        int size;
        int total;
        int count;

        Internal() : size(0), total(0), count(0) {}
    };

    vector<Leaf> _leaves;
    vector<Internal> _internals;
    int _root;
    int _first;
    int _free_leaf_head;
    int _free_internal_head;
    uint8_t _height;

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

    int _make_leaf() {
        if (_free_leaf_head != 0) {
            const int leaf = _free_leaf_head;
            _free_leaf_head = _leaves[leaf].next;
            Leaf &node = _leaves[leaf];
            node.size = 0;
            node.total = 0;
            node.next = 0;
            node.count = 0;
            return leaf;
        }
        const int leaf = _leaves.size();
        _leaves.emplace_back();
        return leaf;
    }

    int _make_internal() {
        if (_free_internal_head != 0) {
            const int node = _free_internal_head;
            _free_internal_head = _internals[node].child[0];
            Internal &n = _internals[node];
            n.size = 0;
            n.total = 0;
            n.count = 0;
            return node;
        }
        const int node = _internals.size();
        _internals.emplace_back();
        return node;
    }

    void _release_leaf(const int leaf) {
        _leaves[leaf].next = _free_leaf_head;
        _free_leaf_head = leaf;
    }

    void _release_internal(const int node) {
        _internals[node].child[0] = _free_internal_head;
        _free_internal_head = node;
    }

    int _node_size(const int node, const int level) const {
        return level == 0 ? _leaves[node].size : _internals[node].size;
    }

    int _node_total(const int node, const int level) const {
        return level == 0 ? _leaves[node].total : _internals[node].total;
    }

    int _child_size(const Internal &node, const int pos) const {
        return node.child_size[pos] - (pos == 0 ? 0 : node.child_size[pos - 1]);
    }

    int _child_total(const Internal &node, const int pos) const {
        return node.child_total[pos] - (pos == 0 ? 0 : node.child_total[pos - 1]);
    }

    int _size_before(const Internal &node, const int pos) const { return pos == 0 ? 0 : node.child_size[pos - 1]; }

    int _total_before(const Internal &node, const int pos) const { return pos == 0 ? 0 : node.child_total[pos - 1]; }

    int _count_prefix(const Internal &node, const int pos, const bool bit) const {
        return bit ? node.child_total[pos] : node.child_size[pos] - node.child_total[pos];
    }

    void _pull_leaf(const int leaf) {
        Leaf &node = _leaves[leaf];
        node.size = 0;
        node.total = 0;
        for (int i = 0; i < node.count; ++i) {
            node.size += node.bit_len[i];
            node.total += node.key_total[i];
        }
    }

    void _append_child(const int node, const int child, const int child_level) {
        Internal &n = _internals[node];
        const int pos = n.count;
        const int size = _node_size(child, child_level);
        const int total = _node_total(child, child_level);
        n.child[pos] = child;
        n.size += size;
        n.total += total;
        n.child_size[pos] = n.size;
        n.child_total[pos] = n.total;
        ++n.count;
    }

    void _insert_child(const int node, const int pos, const int child, const int size, const int total) {
        Internal &n = _internals[node];
        assert(n.count < _BRANCH);
        for (int i = n.count; i > pos; --i) {
            n.child[i] = n.child[i - 1];
            n.child_size[i] = n.child_size[i - 1] + size;
            n.child_total[i] = n.child_total[i - 1] + total;
        }
        n.child[pos] = child;
        n.child_size[pos] = _size_before(n, pos) + size;
        n.child_total[pos] = _total_before(n, pos) + total;
        n.size += size;
        n.total += total;
        ++n.count;
    }

    void _insert_child(const int node, const int pos, const int child, const int child_level) {
        _insert_child(node, pos, child, _node_size(child, child_level), _node_total(child, child_level));
    }

    void _erase_child(const int node, const int pos) {
        Internal &n = _internals[node];
        const int size = _child_size(n, pos);
        const int total = _child_total(n, pos);
        --n.count;
        for (int i = pos; i < n.count; ++i) {
            n.child[i] = n.child[i + 1];
            n.child_size[i] = n.child_size[i + 1] - size;
            n.child_total[i] = n.child_total[i + 1] - total;
        }
        n.size -= size;
        n.total -= total;
    }

    void _set_child(const int node, const int pos, const int child, const int child_level) {
        Internal &n = _internals[node];
        const int size = _node_size(child, child_level);
        const int total = _node_total(child, child_level);
        const int size_delta = size - _child_size(n, pos);
        const int total_delta = total - _child_total(n, pos);
        n.child[pos] = child;
        for (int i = pos; i < n.count; ++i) {
            n.child_size[i] += size_delta;
            n.child_total[i] += total_delta;
        }
        n.size += size_delta;
        n.total += total_delta;
    }

    void _add_path(const array<int, _MAX_HEIGHT> &path, const array<int, _MAX_HEIGHT> &slots, const int depth, const int size_delta, const int total_delta) {
        if (size_delta == 0) {
            for (int i = 0; i < depth; ++i) {
                Internal &node = _internals[path[i]];
                for (int j = slots[i]; j < node.count; ++j) node.child_total[j] += total_delta;
                node.total += total_delta;
            }
            return;
        }
        if (total_delta == 0) {
            for (int i = 0; i < depth; ++i) {
                Internal &node = _internals[path[i]];
                for (int j = slots[i]; j < node.count; ++j) node.child_size[j] += size_delta;
                node.size += size_delta;
            }
            return;
        }
        for (int i = 0; i < depth; ++i) {
            Internal &node = _internals[path[i]];
            for (int j = slots[i]; j < node.count; ++j) {
                node.child_size[j] += size_delta;
                node.child_total[j] += total_delta;
            }
            node.size += size_delta;
            node.total += total_delta;
        }
    }

    void _sync_path(const array<int, _MAX_HEIGHT> &path, const array<int, _MAX_HEIGHT> &slots, const int depth, int child, int child_level) {
        for (int i = depth - 1; i >= 0; --i) {
            _set_child(path[i], slots[i], child, child_level);
            child = path[i];
            ++child_level;
        }
    }

    void _insert_leaf_chunk(const int leaf, const int pos, const uint128 key, const uint8_t bit_len, const uint8_t key_total) {
        Leaf &node = _leaves[leaf];
        assert(node.count < _LEAF_CAP);
        for (int i = node.count; i > pos; --i) {
            node.key[i] = node.key[i - 1];
            node.bit_len[i] = node.bit_len[i - 1];
            node.key_total[i] = node.key_total[i - 1];
        }
        node.key[pos] = key;
        node.bit_len[pos] = bit_len;
        node.key_total[pos] = key_total;
        node.size += bit_len;
        node.total += key_total;
        ++node.count;
    }

    void _erase_leaf_chunk(const int leaf, const int pos) {
        Leaf &node = _leaves[leaf];
        node.size -= node.bit_len[pos];
        node.total -= node.key_total[pos];
        --node.count;
        for (int i = pos; i < node.count; ++i) {
            node.key[i] = node.key[i + 1];
            node.bit_len[i] = node.bit_len[i + 1];
            node.key_total[i] = node.key_total[i + 1];
        }
    }

    void _remove_leaf_slot(const int leaf, const int pos) {
        Leaf &node = _leaves[leaf];
        --node.count;
        for (int i = pos; i < node.count; ++i) {
            node.key[i] = node.key[i + 1];
            node.bit_len[i] = node.bit_len[i + 1];
            node.key_total[i] = node.key_total[i + 1];
        }
    }

    void _merge_leaf_chunks(const int leaf, const int left) {
        Leaf &node = _leaves[leaf];
        const int right = left + 1;
        node.key[left] = (node.key[left] << node.bit_len[right]) | node.key[right];
        node.bit_len[left] += node.bit_len[right];
        node.key_total[left] += node.key_total[right];
        _remove_leaf_slot(leaf, right);
    }

    int _split_leaf(const int leaf) {
        const int right = _make_leaf();
        Leaf &left_node = _leaves[leaf];
        Leaf &right_node = _leaves[right];
        const int split = _LEAF_CAP / 2;
        right_node.count = left_node.count - split;
        for (int i = 0; i < right_node.count; ++i) {
            right_node.key[i] = left_node.key[split + i];
            right_node.bit_len[i] = left_node.bit_len[split + i];
            right_node.key_total[i] = left_node.key_total[split + i];
        }
        left_node.count = split;
        right_node.next = left_node.next;
        left_node.next = right;
        _pull_leaf(leaf);
        _pull_leaf(right);
        return right;
    }

    int _split_internal(const int node) {
        const int right = _make_internal();
        Internal &left_node = _internals[node];
        Internal &right_node = _internals[right];
        const int split = _BRANCH / 2;
        const int left_size = left_node.child_size[split - 1];
        const int left_total = left_node.child_total[split - 1];
        right_node.count = left_node.count - split;
        for (int i = 0; i < right_node.count; ++i) {
            right_node.child[i] = left_node.child[split + i];
            right_node.child_size[i] = left_node.child_size[split + i] - left_size;
            right_node.child_total[i] = left_node.child_total[split + i] - left_total;
        }
        right_node.size = left_node.size - left_size;
        right_node.total = left_node.total - left_total;
        left_node.count = split;
        left_node.size = left_size;
        left_node.total = left_total;
        return right;
    }

    void _split_full_chunk(const int leaf, const int chunk, const uint128 value) {
        Leaf &node = _leaves[leaf];
        assert(node.count < _LEAF_CAP);
        for (int i = node.count; i > chunk + 1; --i) {
            node.key[i] = node.key[i - 1];
            node.bit_len[i] = node.bit_len[i - 1];
            node.key_total[i] = node.key_total[i - 1];
        }
        node.key[chunk] = value >> _SPLIT_W;
        node.bit_len[chunk] = _SPLIT_W;
        node.key_total[chunk] = popcount(node.key[chunk]);
        node.key[chunk + 1] = static_cast<uint64_t>(value);
        node.bit_len[chunk + 1] = _SPLIT_W;
        node.key_total[chunk + 1] = popcount(node.key[chunk + 1]);
        ++node.count;
        _pull_leaf(leaf);
    }

    void _propagate_split(const array<int, _MAX_HEIGHT> &path, const array<int, _MAX_HEIGHT> &slots, int depth, int left, int right, int child_level) {
        while (depth > 0) {
            --depth;
            const int parent = path[depth];
            const int pos = slots[depth];
            _set_child(parent, pos, left, child_level);
            if (_internals[parent].count < _BRANCH) {
                _insert_child(parent, pos + 1, right, child_level);
                _sync_path(path, slots, depth, parent, child_level + 1);
                return;
            }

            const int parent_right = _split_internal(parent);
            const int left_count = _internals[parent].count;
            if (pos < left_count) {
                _insert_child(parent, pos + 1, right, child_level);
            } else {
                _insert_child(parent_right, pos - left_count + 1, right, child_level);
            }
            left = parent;
            right = parent_right;
            ++child_level;
        }

        const int root = _make_internal();
        _append_child(root, left, child_level);
        _append_child(root, right, child_level);
        _root = root;
        ++_height;
    }

    void _borrow_leaf_from_left(const int left, const int leaf) {
        Leaf &left_node = _leaves[left];
        Leaf &node = _leaves[leaf];
        for (int i = node.count; i > 0; --i) {
            node.key[i] = node.key[i - 1];
            node.bit_len[i] = node.bit_len[i - 1];
            node.key_total[i] = node.key_total[i - 1];
        }
        --left_node.count;
        node.key[0] = left_node.key[left_node.count];
        node.bit_len[0] = left_node.bit_len[left_node.count];
        node.key_total[0] = left_node.key_total[left_node.count];
        ++node.count;
        _pull_leaf(left);
        _pull_leaf(leaf);
    }

    void _borrow_leaf_from_right(const int leaf, const int right) {
        Leaf &node = _leaves[leaf];
        Leaf &right_node = _leaves[right];
        const int pos = node.count;
        node.key[pos] = right_node.key[0];
        node.bit_len[pos] = right_node.bit_len[0];
        node.key_total[pos] = right_node.key_total[0];
        ++node.count;
        --right_node.count;
        for (int i = 0; i < right_node.count; ++i) {
            right_node.key[i] = right_node.key[i + 1];
            right_node.bit_len[i] = right_node.bit_len[i + 1];
            right_node.key_total[i] = right_node.key_total[i + 1];
        }
        _pull_leaf(leaf);
        _pull_leaf(right);
    }

    void _append_leaf(const int left, const int right) {
        Leaf &left_node = _leaves[left];
        const Leaf &right_node = _leaves[right];
        assert(left_node.count + right_node.count <= _LEAF_CAP);
        for (int i = 0; i < right_node.count; ++i) {
            const int pos = left_node.count + i;
            left_node.key[pos] = right_node.key[i];
            left_node.bit_len[pos] = right_node.bit_len[i];
            left_node.key_total[pos] = right_node.key_total[i];
        }
        left_node.count += right_node.count;
        left_node.next = right_node.next;
        _pull_leaf(left);
    }

    void _borrow_internal_from_left(const int left, const int node) {
        Internal &left_node = _internals[left];
        const int pos = left_node.count - 1;
        const int child = left_node.child[pos];
        const int size = _child_size(left_node, pos);
        const int total = _child_total(left_node, pos);
        _erase_child(left, pos);
        _insert_child(node, 0, child, size, total);
    }

    void _borrow_internal_from_right(const int node, const int right) {
        Internal &right_node = _internals[right];
        const int child = right_node.child[0];
        const int size = right_node.child_size[0];
        const int total = right_node.child_total[0];
        _insert_child(node, _internals[node].count, child, size, total);
        _erase_child(right, 0);
    }

    void _append_internal(const int left, const int right) {
        Internal &left_node = _internals[left];
        const Internal &right_node = _internals[right];
        assert(left_node.count + right_node.count <= _BRANCH);
        const int size = left_node.size;
        const int total = left_node.total;
        for (int i = 0; i < right_node.count; ++i) {
            const int pos = left_node.count + i;
            left_node.child[pos] = right_node.child[i];
            left_node.child_size[pos] = size + right_node.child_size[i];
            left_node.child_total[pos] = total + right_node.child_total[i];
        }
        left_node.count += right_node.count;
        left_node.size += right_node.size;
        left_node.total += right_node.total;
    }

    void _rebalance_internal(const array<int, _MAX_HEIGHT> &path, const array<int, _MAX_HEIGHT> &slots, const int node_depth, const int level) {
        const int node = path[node_depth];
        if (node_depth == 0) {
            const int count = _internals[node].count;
            if (count == 0) {
                _release_internal(node);
                _root = 0;
                _first = 0;
                _height = 0;
            } else if (count == 1) {
                _root = _internals[node].child[0];
                _release_internal(node);
                --_height;
            }
            return;
        }
        if (_internals[node].count >= _BRANCH_MIN) return;

        const int parent = path[node_depth - 1];
        const int pos = slots[node_depth - 1];
        Internal &parent_node = _internals[parent];
        if (pos > 0) {
            const int left = parent_node.child[pos - 1];
            if (_internals[left].count > _BRANCH_MIN) {
                _borrow_internal_from_left(left, node);
                _set_child(parent, pos - 1, left, level);
                _set_child(parent, pos, node, level);
                return;
            }
        }
        if (pos + 1 < parent_node.count) {
            const int right = parent_node.child[pos + 1];
            if (_internals[right].count > _BRANCH_MIN) {
                _borrow_internal_from_right(node, right);
                _set_child(parent, pos, node, level);
                _set_child(parent, pos + 1, right, level);
                return;
            }
        }

        if (pos > 0) {
            const int left = parent_node.child[pos - 1];
            _append_internal(left, node);
            _set_child(parent, pos - 1, left, level);
            _erase_child(parent, pos);
            _release_internal(node);
        } else {
            const int right = parent_node.child[1];
            _append_internal(node, right);
            _set_child(parent, 0, node, level);
            _erase_child(parent, 1);
            _release_internal(right);
        }
        _rebalance_internal(path, slots, node_depth - 1, level + 1);
    }

    void _rebalance_leaf(const array<int, _MAX_HEIGHT> &path, const array<int, _MAX_HEIGHT> &slots, const int depth, const int leaf) {
        if (depth == 0) {
            if (_leaves[leaf].count == 0) {
                _release_leaf(leaf);
                _root = 0;
                _first = 0;
            }
            return;
        }
        if (_leaves[leaf].count >= _LEAF_MIN) return;

        const int parent_depth = depth - 1;
        const int parent = path[parent_depth];
        const int pos = slots[parent_depth];
        Internal &parent_node = _internals[parent];
        if (pos > 0) {
            const int left = parent_node.child[pos - 1];
            if (_leaves[left].count > _LEAF_MIN) {
                _borrow_leaf_from_left(left, leaf);
                _set_child(parent, pos - 1, left, 0);
                _set_child(parent, pos, leaf, 0);
                return;
            }
        }
        if (pos + 1 < parent_node.count) {
            const int right = parent_node.child[pos + 1];
            if (_leaves[right].count > _LEAF_MIN) {
                _borrow_leaf_from_right(leaf, right);
                _set_child(parent, pos, leaf, 0);
                _set_child(parent, pos + 1, right, 0);
                return;
            }
        }

        if (pos > 0) {
            const int left = parent_node.child[pos - 1];
            _append_leaf(left, leaf);
            _set_child(parent, pos - 1, left, 0);
            _erase_child(parent, pos);
            _release_leaf(leaf);
        } else {
            const int right = parent_node.child[1];
            _append_leaf(leaf, right);
            _set_child(parent, 0, leaf, 0);
            _erase_child(parent, 1);
            _release_leaf(right);
        }
        _rebalance_internal(path, slots, parent_depth, 1);
    }

    void _pop_leaf_bit(const array<int, _MAX_HEIGHT> &path, const array<int, _MAX_HEIGHT> &slots, const int depth, const int leaf, const int chunk, const int k, const bool bit) {
        Leaf &node = _leaves[leaf];
        if (node.bit_len[chunk] == 1) {
            _erase_leaf_chunk(leaf, chunk);
        } else {
            node.key[chunk] = _bit_pop(node.key[chunk], node.bit_len[chunk] - k);
            --node.bit_len[chunk];
            node.key_total[chunk] -= bit;
            --node.size;
            node.total -= bit;
            if (chunk + 1 < node.count && node.bit_len[chunk] + node.bit_len[chunk + 1] <= _W) {
                _merge_leaf_chunks(leaf, chunk);
            } else if (chunk > 0 && node.bit_len[chunk - 1] + node.bit_len[chunk] <= _W) {
                _merge_leaf_chunks(leaf, chunk - 1);
            }
        }

        if ((depth == 0 && node.count > 0) || (depth > 0 && node.count >= _LEAF_MIN)) {
            _add_path(path, slots, depth, -1, -bit);
            return;
        }
        _sync_path(path, slots, depth, leaf, 0);
        _rebalance_leaf(path, slots, depth, leaf);
    }

    void _build(const vector<uint8_t> &a, const int start, const int end) {
        const int n = end - start;
        if (n == 0) return;

        reserve(n);
        const int chunk_count = (n + _W - 1) / _W;
        const int leaf_count = (chunk_count + _LEAF_CAP - 1) / _LEAF_CAP;
        vector<int> current;
        current.reserve(leaf_count);
        int index = start;
        int remaining_chunks = chunk_count;
        int previous = 0;

        for (int i = 0; i < leaf_count; ++i) {
            const int leaves_left = leaf_count - i;
            const int take = (remaining_chunks + leaves_left - 1) / leaves_left;
            const int leaf = _make_leaf();
            Leaf &node = _leaves[leaf];
            for (int j = 0; j < take; ++j) {
                const int bit_len = min(_W, end - index);
                uint128 key = 0;
                for (int k = 0; k < bit_len; ++k) key = (key << 1) | (a[index + k] != 0);
                node.key[j] = key;
                node.bit_len[j] = bit_len;
                node.key_total[j] = popcount(key);
                index += bit_len;
            }
            node.count = take;
            _pull_leaf(leaf);
            if (previous == 0) {
                _first = leaf;
            } else {
                _leaves[previous].next = leaf;
            }
            previous = leaf;
            current.emplace_back(leaf);
            remaining_chunks -= take;
        }

        int child_level = 0;
        while (current.size() > 1) {
            const int count = current.size();
            const int parent_count = (count + _BRANCH - 1) / _BRANCH;
            vector<int> next;
            next.reserve(parent_count);
            int index = 0;
            int remaining = count;
            for (int i = 0; i < parent_count; ++i) {
                const int parents_left = parent_count - i;
                const int take = (remaining + parents_left - 1) / parents_left;
                const int parent = _make_internal();
                for (int j = 0; j < take; ++j) _append_child(parent, current[index++], child_level);
                next.emplace_back(parent);
                remaining -= take;
            }
            current.swap(next);
            ++child_level;
        }

        _root = current[0];
        _height = child_level;
    }

    int _leaf_pref(const int leaf, int r) const {
        int result = 0;
        const Leaf &node = _leaves[leaf];
        for (int i = 0; i < node.count; ++i) {
            if (r <= node.bit_len[i]) return result + _prefix_total(node.key[i], node.bit_len[i], r);
            result += node.key_total[i];
            r -= node.bit_len[i];
        }
        return result;
    }

    int _pref_node(int node, int level, int r) const {
        if (r == 0) return 0;
        if (level == 0) {
            const Leaf &leaf = _leaves[node];
            return r == leaf.size ? leaf.total : _leaf_pref(node, r);
        }
        if (r == _internals[node].size) return _internals[node].total;
        int result = 0;
        while (level > 0) {
            const Internal &n = _internals[node];
            int pos = 0;
            while (r > n.child_size[pos]) ++pos;
            if (r == n.child_size[pos]) return result + n.child_total[pos];
            result += _total_before(n, pos);
            r -= _size_before(n, pos);
            node = n.child[pos];
            --level;
        }
        return result + _leaf_pref(node, r);
    }

    int _pref(const int r) const {
        if (_root == 0) return 0;
        return _pref_node(_root, _height, r);
    }

    pair<int, int> _pref_pair(int l, int r) const {
        if (l == r) {
            const int result = _pref(l);
            return {result, result};
        }
        if (_height == 0) return {_leaf_pref(_root, l), _leaf_pref(_root, r)};
        int left_result = 0;
        int right_result = 0;
        int node = _root;
        int level = _height;
        while (level > 0) {
            const Internal &n = _internals[node];
            int left_pos = 0;
            while (l > n.child_size[left_pos]) ++left_pos;
            int right_pos = left_pos;
            while (r > n.child_size[right_pos]) ++right_pos;
            const int left_size = _size_before(n, left_pos);
            const int right_size = _size_before(n, right_pos);
            const int left_total = _total_before(n, left_pos);
            const int right_total = _total_before(n, right_pos);
            if (left_pos != right_pos) {
                left_result += left_total + _pref_node(n.child[left_pos], level - 1, l - left_size);
                right_result += right_total + _pref_node(n.child[right_pos], level - 1, r - right_size);
                return {left_result, right_result};
            }
            left_result += left_total;
            right_result += right_total;
            l -= left_size;
            r -= right_size;
            node = n.child[left_pos];
            --level;
        }
        return {left_result + _leaf_pref(node, l), right_result + _leaf_pref(node, r)};
    }

    int _select64(uint64_t word, int bit_len, int k) const {
        int offset = 0;
        while (bit_len > 1) {
            const int left_len = (bit_len + 1) / 2;
            const int right_len = bit_len - left_len;
            const uint64_t left = word >> right_len;
            const int count = titan23::popcount(left);
            if (k < count) {
                word = left;
                bit_len = left_len;
            } else {
                word &= _mask64(right_len);
                k -= count;
                offset += left_len;
                bit_len = right_len;
            }
        }
        return offset;
    }

    int _select_in_key(const uint128 key, const int bit_len, int k, const bool bit) const {
        const int high_len = max(0, bit_len - 64);
        const int low_len = min(64, bit_len);
        uint64_t high = key >> 64;
        uint64_t low = key;
        if (!bit) {
            high ^= _mask64(high_len);
            low ^= _mask64(low_len);
        }
        const int high_count = titan23::popcount(high);
        if (k < high_count) return _select64(high, high_len, k);
        return high_len + _select64(low, low_len, k - high_count);
    }

    int _select_position(int k, const bool bit) const {
        int node = _root;
        int level = _height;
        int offset = 0;
        while (level > 0) {
            const Internal &n = _internals[node];
            int pos = 0;
            while (k >= _count_prefix(n, pos, bit)) ++pos;
            if (pos > 0) k -= _count_prefix(n, pos - 1, bit);
            offset += _size_before(n, pos);
            node = n.child[pos];
            --level;
        }

        const Leaf &leaf = _leaves[node];
        for (int i = 0; i < leaf.count; ++i) {
            const int count = bit ? leaf.key_total[i] : leaf.bit_len[i] - leaf.key_total[i];
            if (k < count) return offset + _select_in_key(leaf.key[i], leaf.bit_len[i], k, bit);
            k -= count;
            offset += leaf.bit_len[i];
        }
        assert(false);
        return -1;
    }

public:
    BTreeBitVectorCore() : _leaves(1), _internals(1), _root(0), _first(0), _free_leaf_head(0), _free_internal_head(0), _height(0) {}

    BTreeBitVectorCore(const vector<uint8_t> &a) : BTreeBitVectorCore() { _build(a, 0, a.size()); }

    BTreeBitVectorCore(const vector<uint8_t> &a, const int start, const int end) : BTreeBitVectorCore() { _build(a, start, end); }

    void clear() {
        _leaves.resize(1);
        _internals.resize(1);
        _root = 0;
        _first = 0;
        _free_leaf_head = 0;
        _free_internal_head = 0;
        _height = 0;
    }

    void reserve(const int n) {
        const int chunks = (n + _SPLIT_W - 1) / _SPLIT_W;
        const int leaves = max(1, (chunks + _LEAF_MIN - 1) / _LEAF_MIN);
        int internals = 0;
        int count = leaves;
        while (count > 1) {
            count = (count + _BRANCH_MIN - 1) / _BRANCH_MIN;
            internals += count;
        }
        _leaves.reserve(_leaves.size() + leaves);
        _internals.reserve(_internals.size() + internals);
    }

    void insert(const int k, const bool bit) { _insert_and_rank1(k, bit); }

    bool pop(const int k) { return _access_pop_and_rank1(k) & 1; }

    void set(int k, const bool bit) {
        assert(0 <= k && k < len());
        array<int, _MAX_HEIGHT> path;
        array<int, _MAX_HEIGHT> slots;
        int depth = 0;
        int node = _root;
        int level = _height;

        while (level > 0) {
            const Internal &n = _internals[node];
            int pos = 0;
            while (k >= n.child_size[pos]) ++pos;
            k -= _size_before(n, pos);
            path[depth] = node;
            slots[depth++] = pos;
            node = n.child[pos];
            --level;
        }

        Leaf &leaf = _leaves[node];
        int chunk = 0;
        while (k >= leaf.bit_len[chunk]) k -= leaf.bit_len[chunk++];
        const int shift = leaf.bit_len[chunk] - k - 1;
        const bool old = _bit_at(leaf.key[chunk], shift);
        if (old == bit) return;
        const int delta = bit ? 1 : -1;
        leaf.key[chunk] ^= static_cast<uint128>(1) << shift;
        leaf.key_total[chunk] += delta;
        leaf.total += delta;
        _add_path(path, slots, depth, 0, delta);
    }

    vector<uint8_t> tovector() const {
        vector<uint8_t> result(len());
        int index = 0;
        int leaf = _first;
        while (leaf != 0) {
            const Leaf &node = _leaves[leaf];
            for (int i = 0; i < node.count; ++i) {
                for (int j = node.bit_len[i] - 1; j >= 0; --j) result[index++] = (node.key[i] >> j) & 1;
            }
            leaf = node.next;
        }
        return result;
    }

    bool access(int k) const {
        assert(0 <= k && k < len());
        int node = _root;
        int level = _height;
        while (level > 0) {
            const Internal &n = _internals[node];
            int pos = 0;
            while (k >= n.child_size[pos]) ++pos;
            k -= _size_before(n, pos);
            node = n.child[pos];
            --level;
        }
        const Leaf &leaf = _leaves[node];
        int chunk = 0;
        while (k >= leaf.bit_len[chunk]) k -= leaf.bit_len[chunk++];
        return _bit_at(leaf.key[chunk], leaf.bit_len[chunk] - k - 1);
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
        const auto [l1, r1] = _pref_pair(l, r);
        return {l - l1, r - r1};
    }

    int select0(const int k) const {
        assert(0 <= k && k < len() - (_root == 0 ? 0 : _node_total(_root, _height)));
        return _select_position(k, false);
    }

    int select1(const int k) const {
        assert(0 <= k && k < (_root == 0 ? 0 : _node_total(_root, _height)));
        return _select_position(k, true);
    }

    int select(const int k, const bool bit) const { return bit ? select1(k) : select0(k); }

    int _select_pop(int k, const bool bit) {
        const int total = _root == 0 ? 0 : _node_total(_root, _height);
        assert(0 <= k && k < (bit ? total : len() - total));
        array<int, _MAX_HEIGHT> path;
        array<int, _MAX_HEIGHT> slots;
        int depth = 0;
        int node = _root;
        int level = _height;
        int offset = 0;

        while (level > 0) {
            const Internal &n = _internals[node];
            int pos = 0;
            while (k >= _count_prefix(n, pos, bit)) ++pos;
            if (pos > 0) k -= _count_prefix(n, pos - 1, bit);
            offset += _size_before(n, pos);
            path[depth] = node;
            slots[depth++] = pos;
            node = n.child[pos];
            --level;
        }

        const Leaf &leaf = _leaves[node];
        int chunk = 0;
        while (true) {
            const int count = bit ? leaf.key_total[chunk] : leaf.bit_len[chunk] - leaf.key_total[chunk];
            if (k < count) break;
            k -= count;
            offset += leaf.bit_len[chunk++];
        }
        const int key_index = _select_in_key(leaf.key[chunk], leaf.bit_len[chunk], k, bit);
        const int result = offset + key_index;
        _pop_leaf_bit(path, slots, depth, node, chunk, key_index, bit);
        return result;
    }

    int _insert_and_rank1(int k, const bool bit) {
        assert(0 <= k && k <= len());
        if (_root == 0) {
            const int leaf = _make_leaf();
            _insert_leaf_chunk(leaf, 0, bit, 1, bit);
            _root = leaf;
            _first = leaf;
            return 0;
        }

        array<int, _MAX_HEIGHT> path;
        array<int, _MAX_HEIGHT> slots;
        int depth = 0;
        int result = 0;
        int node = _root;
        int level = _height;

        while (level > 0) {
            const Internal &n = _internals[node];
            int pos = 0;
            while (pos + 1 < n.count && k > n.child_size[pos]) ++pos;
            result += _total_before(n, pos);
            k -= _size_before(n, pos);
            path[depth] = node;
            slots[depth++] = pos;
            node = n.child[pos];
            --level;
        }

        Leaf &leaf = _leaves[node];
        int chunk = 0;
        while (chunk + 1 < leaf.count && k > leaf.bit_len[chunk]) {
            result += leaf.key_total[chunk];
            k -= leaf.bit_len[chunk++];
        }
        result += _prefix_total(leaf.key[chunk], leaf.bit_len[chunk], k);

        if (leaf.bit_len[chunk] < _W) {
            leaf.key[chunk] = _bit_insert(leaf.key[chunk], leaf.bit_len[chunk] - k, bit);
            ++leaf.bit_len[chunk];
            leaf.key_total[chunk] += bit;
            ++leaf.size;
            leaf.total += bit;
            _add_path(path, slots, depth, 1, bit);
            return result;
        }

        const uint128 value = _bit_insert(leaf.key[chunk], _W - k, bit);
        if (leaf.count < _LEAF_CAP) {
            _split_full_chunk(node, chunk, value);
            _add_path(path, slots, depth, 1, bit);
            return result;
        }

        const int right = _split_leaf(node);
        const int left_count = _leaves[node].count;
        if (chunk < left_count) {
            _split_full_chunk(node, chunk, value);
        } else {
            _split_full_chunk(right, chunk - left_count, value);
        }
        _propagate_split(path, slots, depth, node, right, 0);
        return result;
    }

    int _access_pop_and_rank1(int k) {
        assert(0 <= k && k < len());
        array<int, _MAX_HEIGHT> path;
        array<int, _MAX_HEIGHT> slots;
        int depth = 0;
        int result = 0;
        int node = _root;
        int level = _height;

        while (level > 0) {
            const Internal &n = _internals[node];
            int pos = 0;
            while (k >= n.child_size[pos]) ++pos;
            result += _total_before(n, pos);
            k -= _size_before(n, pos);
            path[depth] = node;
            slots[depth++] = pos;
            node = n.child[pos];
            --level;
        }

        const Leaf &leaf = _leaves[node];
        int chunk = 0;
        while (k >= leaf.bit_len[chunk]) {
            result += leaf.key_total[chunk];
            k -= leaf.bit_len[chunk++];
        }
        result += _prefix_total(leaf.key[chunk], leaf.bit_len[chunk], k);
        const bool bit = _bit_at(leaf.key[chunk], leaf.bit_len[chunk] - k - 1);
        _pop_leaf_bit(path, slots, depth, node, chunk, k, bit);
        return (result << 1) | bit;
    }

    pair<bool, int> _access_ans_rank1(int k) const {
        assert(0 <= k && k < len());
        int result = 0;
        int node = _root;
        int level = _height;
        while (level > 0) {
            const Internal &n = _internals[node];
            int pos = 0;
            while (k >= n.child_size[pos]) ++pos;
            result += _total_before(n, pos);
            k -= _size_before(n, pos);
            node = n.child[pos];
            --level;
        }

        const Leaf &leaf = _leaves[node];
        int chunk = 0;
        while (k >= leaf.bit_len[chunk]) {
            result += leaf.key_total[chunk];
            k -= leaf.bit_len[chunk++];
        }
        result += _prefix_total(leaf.key[chunk], leaf.bit_len[chunk], k);
        const bool bit = _bit_at(leaf.key[chunk], leaf.bit_len[chunk] - k - 1);
        return {bit, result};
    }

    pair<bool, int> _access_set_and_rank1(int k, const bool bit) {
        assert(0 <= k && k < len());
        array<int, _MAX_HEIGHT> path;
        array<int, _MAX_HEIGHT> slots;
        int depth = 0;
        int result = 0;
        int node = _root;
        int level = _height;
        while (level > 0) {
            const Internal &n = _internals[node];
            int pos = 0;
            while (k >= n.child_size[pos]) ++pos;
            result += _total_before(n, pos);
            k -= _size_before(n, pos);
            path[depth] = node;
            slots[depth++] = pos;
            node = n.child[pos];
            --level;
        }

        Leaf &leaf = _leaves[node];
        int chunk = 0;
        while (k >= leaf.bit_len[chunk]) {
            result += leaf.key_total[chunk];
            k -= leaf.bit_len[chunk++];
        }
        result += _prefix_total(leaf.key[chunk], leaf.bit_len[chunk], k);
        const int shift = leaf.bit_len[chunk] - k - 1;
        const bool old = _bit_at(leaf.key[chunk], shift);
        if (old != bit) {
            const int delta = bit ? 1 : -1;
            leaf.key[chunk] ^= static_cast<uint128>(1) << shift;
            leaf.key_total[chunk] += delta;
            leaf.total += delta;
            _add_path(path, slots, depth, 0, delta);
        }
        return {old, result};
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

    int len() const { return _root == 0 ? 0 : _node_size(_root, _height); }
};

class BTreeBitVector {
private:
    using uint128 = __uint128_t;

    static constexpr int _W = 127;
    static constexpr uint8_t _TREE_MODE = UINT8_MAX;

    struct Key {
        uint64_t low;
        uint64_t high;
    };

    union Storage {
        Key key;
        BTreeBitVectorCore *tree;

        Storage() : key{0, 0} {}
    };

    Storage _data;
    uint8_t _bit_len;
    uint8_t _total;

    bool _is_tree() const { return _bit_len == _TREE_MODE; }

    uint128 _key() const { return (static_cast<uint128>(_data.key.high) << 64) | _data.key.low; }

    void _set_key(const uint128 key) {
        _data.key.low = key;
        _data.key.high = key >> 64;
    }

    void _flip_bit(const int k) {
        if (k < 64) {
            _data.key.low ^= static_cast<uint64_t>(1) << k;
        } else {
            _data.key.high ^= static_cast<uint64_t>(1) << (k - 64);
        }
    }

    int _prefix_total(const int take) const {
        const uint64_t low = _data.key.low;
        if (_bit_len <= 64) return titan23::popcount(low >> ((_bit_len - take) & 63)) * (take != 0);
        const int high_len = _bit_len - 64;
        const uint64_t high = _data.key.high;
        if (take <= high_len) return titan23::popcount(high >> (high_len - take));
        return titan23::popcount(high) + titan23::popcount(low >> (_bit_len - take));
    }

    bool _bit_at(const int k) const {
        const uint64_t word = k < 64 ? _data.key.low : _data.key.high;
        return (word >> (k & 63)) & 1;
    }

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

    int _select_in_key(const int k, const bool bit) const {
        int left = 0;
        int right = _bit_len;
        while (right - left > 1) {
            const int mid = (left + right) / 2;
            const int ones = _prefix_total(mid);
            const int count = bit ? ones : mid - ones;
            if (count > k) {
                right = mid;
            } else {
                left = mid;
            }
        }
        return left;
    }

    void _build_small(const vector<uint8_t> &a, const int start, const int end) {
        uint128 key = 0;
        _total = 0;
        _bit_len = end - start;
        for (int i = start; i < end; ++i) {
            const bool bit = a[i] != 0;
            key = (key << 1) | bit;
            _total += bit;
        }
        _set_key(key);
    }

    void _promote(const int capacity = 0) {
        const int bit_len = _bit_len;
        vector<uint8_t> a(_bit_len);
        for (int i = _bit_len - 1, j = 0; i >= 0; --i, ++j) a[j] = _bit_at(i);
        unique_ptr<BTreeBitVectorCore> tree = make_unique<BTreeBitVectorCore>(a);
        if (capacity > bit_len) tree->reserve(capacity);
        _data.tree = tree.release();
        _bit_len = _TREE_MODE;
        _total = 0;
    }

    void _try_demote() {
        if (_data.tree->len() > _W) return;
        const vector<uint8_t> a = _data.tree->tovector();
        delete _data.tree;
        _data.key = {0, 0};
        _bit_len = 0;
        _build_small(a, 0, a.size());
    }

public:
    BTreeBitVector() : _data(), _bit_len(0), _total(0) {}

    BTreeBitVector(const vector<uint8_t> &a) : BTreeBitVector() {
        if (a.size() <= _W) {
            _build_small(a, 0, a.size());
        } else {
            _data.tree = new BTreeBitVectorCore(a);
            _bit_len = _TREE_MODE;
        }
    }

    BTreeBitVector(const vector<uint8_t> &a, const int start, const int end) : BTreeBitVector() {
        if (end - start <= _W) {
            _build_small(a, start, end);
        } else {
            _data.tree = new BTreeBitVectorCore(a, start, end);
            _bit_len = _TREE_MODE;
        }
    }

    BTreeBitVector(const BTreeBitVector &other) : _data(), _bit_len(other._bit_len), _total(other._total) {
        if (other._is_tree()) {
            _data.tree = new BTreeBitVectorCore(*other._data.tree);
        } else {
            _data.key = other._data.key;
        }
    }

    BTreeBitVector(BTreeBitVector &&other) noexcept : _data(), _bit_len(other._bit_len), _total(other._total) {
        if (other._is_tree()) {
            _data.tree = other._data.tree;
            other._data.key = {0, 0};
            other._bit_len = 0;
            other._total = 0;
        } else {
            _data.key = other._data.key;
        }
    }

    ~BTreeBitVector() {
        if (_is_tree()) delete _data.tree;
    }

    BTreeBitVector &operator=(const BTreeBitVector &other) {
        if (this == &other) return *this;
        BTreeBitVectorCore *tree = other._is_tree() ? new BTreeBitVectorCore(*other._data.tree) : nullptr;
        if (_is_tree()) delete _data.tree;
        _bit_len = other._bit_len;
        _total = other._total;
        if (other._is_tree()) {
            _data.tree = tree;
        } else {
            _data.key = other._data.key;
        }
        return *this;
    }

    BTreeBitVector &operator=(BTreeBitVector &&other) noexcept {
        if (this == &other) return *this;
        if (_is_tree()) delete _data.tree;
        _bit_len = other._bit_len;
        _total = other._total;
        if (other._is_tree()) {
            _data.tree = other._data.tree;
            other._data.key = {0, 0};
            other._bit_len = 0;
            other._total = 0;
        } else {
            _data.key = other._data.key;
        }
        return *this;
    }

    void clear() {
        if (_is_tree()) delete _data.tree;
        _data.key = {0, 0};
        _bit_len = 0;
        _total = 0;
    }

    void reserve(const int n) {
        if (_is_tree()) {
            _data.tree->reserve(n);
        } else if (n > _W) {
            _promote(n);
        }
    }

    void insert(const int k, const bool bit) { _insert_and_rank1(k, bit); }

    bool pop(const int k) { return _access_pop_and_rank1(k) & 1; }

    void set(const int k, const bool bit) {
        if (_is_tree()) {
            _data.tree->set(k, bit);
            return;
        }
        assert(0 <= k && k < _bit_len);
        const int shift = _bit_len - k - 1;
        const bool old = _bit_at(shift);
        if (old == bit) return;
        _flip_bit(shift);
        _total += bit ? 1 : -1;
    }

    vector<uint8_t> tovector() const {
        if (_is_tree()) return _data.tree->tovector();
        vector<uint8_t> result(_bit_len);
        for (int i = _bit_len - 1, j = 0; i >= 0; --i, ++j) result[j] = _bit_at(i);
        return result;
    }

    bool access(const int k) const {
        if (_is_tree()) return _data.tree->access(k);
        assert(0 <= k && k < _bit_len);
        return _bit_at(_bit_len - k - 1);
    }

    int rank0(const int r) const {
        if (_is_tree()) return _data.tree->rank0(r);
        assert(0 <= r && r <= _bit_len);
        return r - _prefix_total(r);
    }

    int rank1(const int r) const {
        if (_is_tree()) return _data.tree->rank1(r);
        assert(0 <= r && r <= _bit_len);
        return _prefix_total(r);
    }

    int rank(const int r, const bool bit) const { return bit ? rank1(r) : rank0(r); }

    pair<int, int> _rank0_pair(const int l, const int r) const {
        if (_is_tree()) return _data.tree->_rank0_pair(l, r);
        assert(0 <= l && l <= r && r <= _bit_len);
        return {l - _prefix_total(l), r - _prefix_total(r)};
    }

    int select0(const int k) const {
        if (_is_tree()) return _data.tree->select0(k);
        assert(0 <= k && k < _bit_len - _total);
        return _select_in_key(k, false);
    }

    int select1(const int k) const {
        if (_is_tree()) return _data.tree->select1(k);
        assert(0 <= k && k < _total);
        return _select_in_key(k, true);
    }

    int select(const int k, const bool bit) const { return bit ? select1(k) : select0(k); }

    int _select_pop(const int k, const bool bit) {
        if (!_is_tree()) {
            assert(0 <= k && k < (bit ? _total : _bit_len - _total));
            const int position = _select_in_key(k, bit);
            _access_pop_and_rank1(position);
            return position;
        }
        const int result = _data.tree->_select_pop(k, bit);
        _try_demote();
        return result;
    }

    int _insert_and_rank1(const int k, const bool bit) {
        if (_is_tree()) return _data.tree->_insert_and_rank1(k, bit);
        assert(0 <= k && k <= _bit_len);
        if (_bit_len == _W) {
            _promote();
            return _data.tree->_insert_and_rank1(k, bit);
        }
        const int result = _prefix_total(k);
        _set_key(_bit_insert(_key(), _bit_len - k, bit));
        ++_bit_len;
        _total += bit;
        return result;
    }

    int _access_pop_and_rank1(const int k) {
        if (_is_tree()) {
            const int result = _data.tree->_access_pop_and_rank1(k);
            _try_demote();
            return result;
        }
        assert(0 <= k && k < _bit_len);
        const int result = _prefix_total(k);
        const bool bit = _bit_at(_bit_len - k - 1);
        _set_key(_bit_pop(_key(), _bit_len - k));
        --_bit_len;
        _total -= bit;
        return (result << 1) | bit;
    }

    pair<bool, int> _access_ans_rank1(const int k) const {
        if (_is_tree()) return _data.tree->_access_ans_rank1(k);
        assert(0 <= k && k < _bit_len);
        return {_bit_at(_bit_len - k - 1), _prefix_total(k)};
    }

    pair<bool, int> _access_set_and_rank1(const int k, const bool bit) {
        if (_is_tree()) return _data.tree->_access_set_and_rank1(k, bit);
        assert(0 <= k && k < _bit_len);
        const int result = _prefix_total(k);
        const int shift = _bit_len - k - 1;
        const bool old = _bit_at(shift);
        if (old != bit) {
            _flip_bit(shift);
            _total += bit ? 1 : -1;
        }
        return {old, result};
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

    bool empty() const { return _is_tree() ? _data.tree->empty() : _bit_len == 0; }

    int len() const { return _is_tree() ? _data.tree->len() : _bit_len; }
};

} // namespace titan23
