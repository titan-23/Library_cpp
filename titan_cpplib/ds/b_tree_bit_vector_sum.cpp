/// https://github.com/titan-23/Library_cpp/blob/main/titan_cpplib/ds/b_tree_bit_vector_sum.cpp
#pragma once

#include <algorithm>
#include <array>
#include <cassert>
#include <cstdint>
#include <utility>
#include <vector>
#include "titan_cpplib/others/bit.cpp"
using namespace std;

// BTreeBitVectorSum
namespace titan23 {

template<typename W>
class BTreeBitVectorSum {
private:
    static constexpr int _LEAF_CAP = 16;
    static constexpr int _LEAF_MIN = _LEAF_CAP / 2;
    static constexpr int _BRANCH = 16;
    static constexpr int _BRANCH_MIN = _BRANCH / 2;
    static constexpr int _MAX_HEIGHT = 16;

    struct Leaf {
        array<W, _LEAF_CAP> weight;
        W sum;
        W sum1;
        int next;
        uint16_t bits;
        uint8_t count;

        Leaf() : sum(0), sum1(0), next(0), bits(0), count(0) {}
    };

    struct Internal {
        array<int, _BRANCH> child;
        array<int, _BRANCH> child_size;
        array<int, _BRANCH> child_ones;
        array<W, _BRANCH> child_sum;
        array<W, _BRANCH> child_sum1;
        uint8_t count;

        Internal() : count(0) {}
    };

    struct Stats {
        int size;
        int ones;
        W sum;
        W sum1;
    };

    struct Prefix {
        int ones;
        W sum;
        W sum1;

        Prefix &operator+=(const Prefix &other) {
            ones += other.ones;
            sum += other.sum;
            sum1 += other.sum1;
            return *this;
        }
    };

public:
    struct Sequence {
        int root;
        uint8_t height;

        Sequence() : root(0), height(0) {}
    };

    struct RangeData {
        int l0;
        int r0;
        W sum0;
        W sum1;
    };

    struct AccessData {
        bool bit;
        int rank1;
        W weight;
    };

private:
    vector<Leaf> _leaves;
    vector<Internal> _internals;
    int _free_leaf;
    int _free_internal;

    uint16_t _mask(const int n) const { return n == 0 ? 0 : (static_cast<uint16_t>(1) << n) - 1; }

    bool _bit_at(const Leaf &leaf, const int k) const { return (leaf.bits >> k) & 1; }

    int _leaf_ones(const Leaf &leaf) const { return popcount(leaf.bits & _mask(leaf.count)); }

    int _make_leaf() {
        if (_free_leaf != 0) {
            int leaf = _free_leaf;
            _free_leaf = _leaves[leaf].next;
            _leaves[leaf] = Leaf();
            return leaf;
        }
        int leaf = _leaves.size();
        _leaves.emplace_back();
        return leaf;
    }

    int _make_internal() {
        if (_free_internal != 0) {
            int node = _free_internal;
            _free_internal = _internals[node].child[0];
            _internals[node].count = 0;
            return node;
        }
        int node = _internals.size();
        _internals.emplace_back();
        return node;
    }

    void _release_leaf(const int leaf) {
        _leaves[leaf].next = _free_leaf;
        _free_leaf = leaf;
    }

    void _release_internal(const int node) {
        _internals[node].child[0] = _free_internal;
        _free_internal = node;
    }

    int _size_before(const Internal &node, const int pos) const { return pos == 0 ? 0 : node.child_size[pos - 1]; }

    int _ones_before(const Internal &node, const int pos) const { return pos == 0 ? 0 : node.child_ones[pos - 1]; }

    W _sum_before(const Internal &node, const int pos) const { return pos == 0 ? W(0) : node.child_sum[pos - 1]; }

    W _sum1_before(const Internal &node, const int pos) const { return pos == 0 ? W(0) : node.child_sum1[pos - 1]; }

    Stats _stats(const int node, const int level) const {
        if (level == 0) {
            const Leaf &leaf = _leaves[node];
            return {leaf.count, _leaf_ones(leaf), leaf.sum, leaf.sum1};
        }
        const Internal &n = _internals[node];
        int last = n.count - 1;
        return {n.child_size[last], n.child_ones[last], n.child_sum[last], n.child_sum1[last]};
    }

    Stats _child_stats(const Internal &node, const int pos) const {
        return {
            node.child_size[pos] - _size_before(node, pos),
            node.child_ones[pos] - _ones_before(node, pos),
            node.child_sum[pos] - _sum_before(node, pos),
            node.child_sum1[pos] - _sum1_before(node, pos)
        };
    }

    Prefix _before(const Internal &node, const int pos) const {
        return {_ones_before(node, pos), _sum_before(node, pos), _sum1_before(node, pos)};
    }

    void _pull_leaf(const int leaf) {
        Leaf &node = _leaves[leaf];
        node.sum = 0;
        node.sum1 = 0;
        for (int i = 0; i < node.count; ++i) {
            node.sum += node.weight[i];
            if (_bit_at(node, i)) node.sum1 += node.weight[i];
        }
    }

    void _append_child(const int node, const int child, const int child_level) {
        Internal &n = _internals[node];
        const Stats stats = _stats(child, child_level);
        int pos = n.count;
        n.child[pos] = child;
        n.child_size[pos] = _size_before(n, pos) + stats.size;
        n.child_ones[pos] = _ones_before(n, pos) + stats.ones;
        n.child_sum[pos] = _sum_before(n, pos) + stats.sum;
        n.child_sum1[pos] = _sum1_before(n, pos) + stats.sum1;
        ++n.count;
    }

    void _insert_child(const int node, const int pos, const int child, const Stats &stats) {
        Internal &n = _internals[node];
        assert(n.count < _BRANCH);
        for (int i = n.count; i > pos; --i) {
            n.child[i] = n.child[i - 1];
            n.child_size[i] = n.child_size[i - 1] + stats.size;
            n.child_ones[i] = n.child_ones[i - 1] + stats.ones;
            n.child_sum[i] = n.child_sum[i - 1] + stats.sum;
            n.child_sum1[i] = n.child_sum1[i - 1] + stats.sum1;
        }
        n.child[pos] = child;
        n.child_size[pos] = _size_before(n, pos) + stats.size;
        n.child_ones[pos] = _ones_before(n, pos) + stats.ones;
        n.child_sum[pos] = _sum_before(n, pos) + stats.sum;
        n.child_sum1[pos] = _sum1_before(n, pos) + stats.sum1;
        ++n.count;
    }

    void _insert_child(const int node, const int pos, const int child, const int child_level) {
        _insert_child(node, pos, child, _stats(child, child_level));
    }

    void _erase_child(const int node, const int pos) {
        Internal &n = _internals[node];
        const Stats stats = _child_stats(n, pos);
        --n.count;
        for (int i = pos; i < n.count; ++i) {
            n.child[i] = n.child[i + 1];
            n.child_size[i] = n.child_size[i + 1] - stats.size;
            n.child_ones[i] = n.child_ones[i + 1] - stats.ones;
            n.child_sum[i] = n.child_sum[i + 1] - stats.sum;
            n.child_sum1[i] = n.child_sum1[i + 1] - stats.sum1;
        }
    }

    void _set_child(const int node, const int pos, const int child, const int child_level) {
        Internal &n = _internals[node];
        const Stats old_stats = _child_stats(n, pos);
        const Stats new_stats = _stats(child, child_level);
        int size_delta = new_stats.size - old_stats.size;
        int ones_delta = new_stats.ones - old_stats.ones;
        W sum_delta = new_stats.sum - old_stats.sum;
        W sum1_delta = new_stats.sum1 - old_stats.sum1;
        n.child[pos] = child;
        for (int i = pos; i < n.count; ++i) {
            n.child_size[i] += size_delta;
            n.child_ones[i] += ones_delta;
            n.child_sum[i] += sum_delta;
            n.child_sum1[i] += sum1_delta;
        }
    }

    void _add_path(const array<int, _MAX_HEIGHT>& path, const array<int, _MAX_HEIGHT>& slots,
                   const int depth, const Stats& delta) {
        for (int i = 0; i < depth; ++i) {
            Internal &node = _internals[path[i]];
            for (int j = slots[i]; j < node.count; ++j) {
                node.child_size[j] += delta.size;
                node.child_ones[j] += delta.ones;
                node.child_sum[j] += delta.sum;
                node.child_sum1[j] += delta.sum1;
            }
        }
    }

    void _add_weight_path(const array<int, _MAX_HEIGHT>& path, const array<int, _MAX_HEIGHT>& slots,
                          const int depth, const W delta, const bool bit) {
        for (int i = 0; i < depth; ++i) {
            Internal& node = _internals[path[i]];
            for (int j = slots[i]; j < node.count; ++j) {
                node.child_sum[j] += delta;
                if (bit) node.child_sum1[j] += delta;
            }
        }
    }

    void _sync_path(const array<int, _MAX_HEIGHT>& path, const array<int, _MAX_HEIGHT>& slots,
                    const int depth, int child, int child_level) {
        for (int i = depth - 1; i >= 0; --i) {
            _set_child(path[i], slots[i], child, child_level);
            child = path[i];
            ++child_level;
        }
    }

    void _insert_leaf(const int leaf, const int pos, const bool bit, const W weight) {
        Leaf &node = _leaves[leaf];
        assert(node.count < _LEAF_CAP);
        for (int i = node.count; i > pos; --i) node.weight[i] = node.weight[i - 1];
        const uint16_t lower = node.bits & _mask(pos);
        const uint16_t upper = (node.bits & ~_mask(pos)) << 1;
        node.bits = lower | upper | (static_cast<uint16_t>(bit) << pos);
        node.weight[pos] = weight;
        node.sum += weight;
        if (bit) node.sum1 += weight;
        ++node.count;
    }

    void _erase_leaf(const int leaf, const int pos) {
        Leaf &node = _leaves[leaf];
        bool bit = _bit_at(node, pos);
        node.sum -= node.weight[pos];
        if (bit) node.sum1 -= node.weight[pos];
        for (int i = pos; i + 1 < node.count; ++i) node.weight[i] = node.weight[i + 1];
        const uint16_t lower = node.bits & _mask(pos);
        const uint16_t upper = (node.bits >> (pos + 1)) << pos;
        node.bits = lower | upper;
        --node.count;
    }

    int _split_leaf(const int leaf) {
        int right = _make_leaf();
        Leaf &left_node = _leaves[leaf];
        Leaf &right_node = _leaves[right];
        int split = _LEAF_CAP / 2;
        right_node.count = left_node.count - split;
        right_node.bits = left_node.bits >> split;
        for (int i = 0; i < right_node.count; ++i) right_node.weight[i] = left_node.weight[split + i];
        left_node.count = split;
        left_node.bits &= _mask(split);
        _pull_leaf(leaf);
        _pull_leaf(right);
        return right;
    }

    int _split_internal(const int node) {
        int right = _make_internal();
        Internal &left_node = _internals[node];
        Internal &right_node = _internals[right];
        int split = _BRANCH / 2;
        int left_size = left_node.child_size[split - 1];
        int left_ones = left_node.child_ones[split - 1];
        W left_sum = left_node.child_sum[split - 1];
        W left_sum1 = left_node.child_sum1[split - 1];
        right_node.count = left_node.count - split;
        for (int i = 0; i < right_node.count; ++i) {
            right_node.child[i] = left_node.child[split + i];
            right_node.child_size[i] = left_node.child_size[split + i] - left_size;
            right_node.child_ones[i] = left_node.child_ones[split + i] - left_ones;
            right_node.child_sum[i] = left_node.child_sum[split + i] - left_sum;
            right_node.child_sum1[i] = left_node.child_sum1[split + i] - left_sum1;
        }
        left_node.count = split;
        return right;
    }

    void _propagate_split(Sequence& sequence, const array<int, _MAX_HEIGHT>& path,
                          const array<int, _MAX_HEIGHT>& slots, int depth, int left, int right,
                          int child_level) {
        while (depth > 0) {
            --depth;
            int parent = path[depth];
            int pos = slots[depth];
            _set_child(parent, pos, left, child_level);
            if (_internals[parent].count < _BRANCH) {
                _insert_child(parent, pos + 1, right, child_level);
                _sync_path(path, slots, depth, parent, child_level + 1);
                return;
            }

            int parent_right = _split_internal(parent);
            int left_count = _internals[parent].count;
            if (pos < left_count) {
                _insert_child(parent, pos + 1, right, child_level);
            } else {
                _insert_child(parent_right, pos - left_count + 1, right, child_level);
            }
            left = parent;
            right = parent_right;
            ++child_level;
        }

        int root = _make_internal();
        _append_child(root, left, child_level);
        _append_child(root, right, child_level);
        sequence.root = root;
        ++sequence.height;
    }

    void _borrow_leaf_from_left(const int left, const int leaf) {
        Leaf &left_node = _leaves[left];
        int pos = left_node.count - 1;
        bool bit = _bit_at(left_node, pos);
        W weight = left_node.weight[pos];
        _erase_leaf(left, pos);
        _insert_leaf(leaf, 0, bit, weight);
    }

    void _borrow_leaf_from_right(const int leaf, const int right) {
        Leaf &right_node = _leaves[right];
        bool bit = _bit_at(right_node, 0);
        W weight = right_node.weight[0];
        _erase_leaf(right, 0);
        _insert_leaf(leaf, _leaves[leaf].count, bit, weight);
    }

    void _append_leaf(const int left, const int right) {
        Leaf &left_node = _leaves[left];
        const Leaf &right_node = _leaves[right];
        assert(left_node.count + right_node.count <= _LEAF_CAP);
        int offset = left_node.count;
        for (int i = 0; i < right_node.count; ++i) left_node.weight[offset + i] = right_node.weight[i];
        left_node.bits |= right_node.bits << offset;
        left_node.count += right_node.count;
        left_node.sum += right_node.sum;
        left_node.sum1 += right_node.sum1;
    }

    void _borrow_internal_from_left(const int left, const int node) {
        Internal &left_node = _internals[left];
        int pos = left_node.count - 1;
        int child = left_node.child[pos];
        const Stats stats = _child_stats(left_node, pos);
        _erase_child(left, pos);
        _insert_child(node, 0, child, stats);
    }

    void _borrow_internal_from_right(const int node, const int right) {
        Internal &right_node = _internals[right];
        int child = right_node.child[0];
        const Stats stats = _child_stats(right_node, 0);
        _insert_child(node, _internals[node].count, child, stats);
        _erase_child(right, 0);
    }

    void _append_internal(const int left, const int right) {
        Internal &left_node = _internals[left];
        const Internal &right_node = _internals[right];
        assert(left_node.count + right_node.count <= _BRANCH);
        int size = left_node.count == 0 ? 0 : left_node.child_size[left_node.count - 1];
        int ones = left_node.count == 0 ? 0 : left_node.child_ones[left_node.count - 1];
        W sum = left_node.count == 0 ? W(0) : left_node.child_sum[left_node.count - 1];
        W sum1 = left_node.count == 0 ? W(0) : left_node.child_sum1[left_node.count - 1];
        for (int i = 0; i < right_node.count; ++i) {
            int pos = left_node.count + i;
            left_node.child[pos] = right_node.child[i];
            left_node.child_size[pos] = size + right_node.child_size[i];
            left_node.child_ones[pos] = ones + right_node.child_ones[i];
            left_node.child_sum[pos] = sum + right_node.child_sum[i];
            left_node.child_sum1[pos] = sum1 + right_node.child_sum1[i];
        }
        left_node.count += right_node.count;
    }

    void _rebalance_internal(Sequence& sequence, const array<int, _MAX_HEIGHT>& path,
                             const array<int, _MAX_HEIGHT>& slots, const int node_depth, const int level) {
        int node = path[node_depth];
        if (node_depth == 0) {
            int count = _internals[node].count;
            if (count == 0) {
                _release_internal(node);
                sequence.root = 0;
                sequence.height = 0;
            } else if (count == 1) {
                sequence.root = _internals[node].child[0];
                _release_internal(node);
                --sequence.height;
            }
            return;
        }
        if (_internals[node].count >= _BRANCH_MIN) return;

        int parent = path[node_depth - 1];
        int pos = slots[node_depth - 1];
        Internal &parent_node = _internals[parent];
        if (pos > 0) {
            int left = parent_node.child[pos - 1];
            if (_internals[left].count > _BRANCH_MIN) {
                _borrow_internal_from_left(left, node);
                _set_child(parent, pos - 1, left, level);
                _set_child(parent, pos, node, level);
                return;
            }
        }
        if (pos + 1 < parent_node.count) {
            int right = parent_node.child[pos + 1];
            if (_internals[right].count > _BRANCH_MIN) {
                _borrow_internal_from_right(node, right);
                _set_child(parent, pos, node, level);
                _set_child(parent, pos + 1, right, level);
                return;
            }
        }

        if (pos > 0) {
            int left = parent_node.child[pos - 1];
            _append_internal(left, node);
            _set_child(parent, pos - 1, left, level);
            _erase_child(parent, pos);
            _release_internal(node);
        } else {
            int right = parent_node.child[1];
            _append_internal(node, right);
            _set_child(parent, 0, node, level);
            _erase_child(parent, 1);
            _release_internal(right);
        }
        _rebalance_internal(sequence, path, slots, node_depth - 1, level + 1);
    }

    void _rebalance_leaf(Sequence& sequence, const array<int, _MAX_HEIGHT>& path,
                         const array<int, _MAX_HEIGHT>& slots, const int depth, const int leaf) {
        if (depth == 0) {
            if (_leaves[leaf].count == 0) {
                _release_leaf(leaf);
                sequence.root = 0;
            }
            return;
        }
        if (_leaves[leaf].count >= _LEAF_MIN) return;

        int parent_depth = depth - 1;
        int parent = path[parent_depth];
        int pos = slots[parent_depth];
        Internal &parent_node = _internals[parent];
        if (pos > 0) {
            int left = parent_node.child[pos - 1];
            if (_leaves[left].count > _LEAF_MIN) {
                _borrow_leaf_from_left(left, leaf);
                _set_child(parent, pos - 1, left, 0);
                _set_child(parent, pos, leaf, 0);
                return;
            }
        }
        if (pos + 1 < parent_node.count) {
            int right = parent_node.child[pos + 1];
            if (_leaves[right].count > _LEAF_MIN) {
                _borrow_leaf_from_right(leaf, right);
                _set_child(parent, pos, leaf, 0);
                _set_child(parent, pos + 1, right, 0);
                return;
            }
        }

        if (pos > 0) {
            int left = parent_node.child[pos - 1];
            _append_leaf(left, leaf);
            _set_child(parent, pos - 1, left, 0);
            _erase_child(parent, pos);
            _release_leaf(leaf);
        } else {
            int right = parent_node.child[1];
            _append_leaf(leaf, right);
            _set_child(parent, 0, leaf, 0);
            _erase_child(parent, 1);
            _release_leaf(right);
        }
        _rebalance_internal(sequence, path, slots, parent_depth, 1);
    }

    Prefix _leaf_prefix(const int leaf, const int take) const {
        const Leaf &node = _leaves[leaf];
        Prefix result{popcount(node.bits & _mask(take)), 0, 0};
        for (int i = 0; i < take; ++i) {
            result.sum += node.weight[i];
            if (_bit_at(node, i)) result.sum1 += node.weight[i];
        }
        return result;
    }

    Prefix _prefix_node(int node, int level, int take) const {
        if (take == 0) return {0, 0, 0};
        const Stats all = _stats(node, level);
        if (take == all.size) return {all.ones, all.sum, all.sum1};
        Prefix result{0, 0, 0};
        while (level > 0) {
            const Internal &n = _internals[node];
            int pos = 0;
            while (take > n.child_size[pos]) ++pos;
            result += _before(n, pos);
            take -= _size_before(n, pos);
            node = n.child[pos];
            --level;
        }
        result += _leaf_prefix(node, take);
        return result;
    }

    int _ones_node(int node, int level, int take) const {
        if (take == 0) return 0;
        if (level == 0) return popcount(_leaves[node].bits & _mask(take));
        const Internal& root = _internals[node];
        if (take == root.child_size[root.count - 1]) return root.child_ones[root.count - 1];
        int ans = 0;
        while (level > 0) {
            const Internal& n = _internals[node];
            int pos = 0;
            while (take > n.child_size[pos]) ++pos;
            if (take == n.child_size[pos]) return ans + n.child_ones[pos];
            ans += _ones_before(n, pos);
            take -= _size_before(n, pos);
            node = n.child[pos];
            --level;
        }
        return ans + popcount(_leaves[node].bits & _mask(take));
    }

    pair<Prefix, Prefix> _prefix_pair(const Sequence &sequence, int l, int r) const {
        if (l == r) {
            const Prefix result = _prefix_node(sequence.root, sequence.height, l);
            return {result, result};
        }
        Prefix left_result{0, 0, 0};
        Prefix right_result{0, 0, 0};
        int node = sequence.root;
        int level = sequence.height;
        while (level > 0) {
            const Internal &n = _internals[node];
            int left_pos = 0;
            while (l > n.child_size[left_pos]) ++left_pos;
            int right_pos = left_pos;
            while (r > n.child_size[right_pos]) ++right_pos;
            if (left_pos != right_pos) {
                left_result += _before(n, left_pos);
                right_result += _before(n, right_pos);
                left_result += _prefix_node(n.child[left_pos], level - 1, l - _size_before(n, left_pos));
                right_result += _prefix_node(n.child[right_pos], level - 1, r - _size_before(n, right_pos));
                return {left_result, right_result};
            }
            const Prefix before = _before(n, left_pos);
            left_result += before;
            right_result += before;
            int size_before = _size_before(n, left_pos);
            l -= size_before;
            r -= size_before;
            node = n.child[left_pos];
            --level;
        }
        left_result += _leaf_prefix(node, l);
        right_result += _leaf_prefix(node, r);
        return {left_result, right_result};
    }

    pair<int, int> _ones_pair(const Sequence& seq, int l, int r) const {
        if (l == r) {
            int one = _ones_node(seq.root, seq.height, l);
            return {one, one};
        }
        if (seq.height == 0) {
            const Leaf& leaf = _leaves[seq.root];
            return {popcount(leaf.bits & _mask(l)), popcount(leaf.bits & _mask(r))};
        }
        int lo = 0;
        int ro = 0;
        int node = seq.root;
        int level = seq.height;
        while (level > 0) {
            const Internal& n = _internals[node];
            int lp = 0;
            while (l > n.child_size[lp]) ++lp;
            int rp = lp;
            while (r > n.child_size[rp]) ++rp;
            int ls = _size_before(n, lp);
            int rs = _size_before(n, rp);
            if (lp != rp) {
                lo += _ones_before(n, lp) + _ones_node(n.child[lp], level - 1, l - ls);
                ro += _ones_before(n, rp) + _ones_node(n.child[rp], level - 1, r - rs);
                return {lo, ro};
            }
            int before = _ones_before(n, lp);
            lo += before;
            ro += before;
            l -= ls;
            r -= rs;
            node = n.child[lp];
            --level;
        }
        const Leaf& leaf = _leaves[node];
        return {lo + popcount(leaf.bits & _mask(l)), ro + popcount(leaf.bits & _mask(r))};
    }

    int _branch_count(const Stats &stats, const bool bit) const { return bit ? stats.ones : stats.size - stats.ones; }

    W _branch_sum(const Stats &stats, const bool bit) const { return bit ? stats.sum1 : stats.sum - stats.sum1; }

    bool _consume(int node, const int level, int l, int r, const bool bit, W &target, int &count) const {
        const Stats all = _stats(node, level);
        if (l == 0 && r == all.size) {
            W sum = _branch_sum(all, bit);
            if (sum < target) {
                target -= sum;
                count += _branch_count(all, bit);
                return false;
            }
        }

        if (level == 0) {
            const Leaf &leaf = _leaves[node];
            for (int i = l; i < r; ++i) {
                if (_bit_at(leaf, i) != bit) continue;
                ++count;
                if (leaf.weight[i] >= target) {
                    target = 0;
                    return true;
                }
                target -= leaf.weight[i];
            }
            return false;
        }

        const Internal &n = _internals[node];
        for (int pos = 0; pos < n.count; ++pos) {
            int begin = _size_before(n, pos);
            int end = n.child_size[pos];
            if (r <= begin) break;
            if (end <= l) continue;
            int child_l = max(0, l - begin);
            int child_r = min(end - begin, r - begin);
            if (_consume(n.child[pos], level - 1, child_l, child_r, bit, target, count)) return true;
        }
        return false;
    }

    template<class Pred>
    bool _consume_pred(int node, const int level, int l, int r, const bool bit, W& aggregate, int& count,
                       Pred& pred) const {
        const Stats all = _stats(node, level);
        if (l == 0 && r == all.size) {
            W next = aggregate + _branch_sum(all, bit);
            if (pred(next)) {
                aggregate = next;
                count += _branch_count(all, bit);
                return false;
            }
        }

        if (level == 0) {
            const Leaf &leaf = _leaves[node];
            for (int i = l; i < r; ++i) {
                if (_bit_at(leaf, i) != bit) continue;
                W next = aggregate + leaf.weight[i];
                if (!pred(next)) return true;
                aggregate = next;
                ++count;
            }
            return false;
        }

        const Internal &n = _internals[node];
        for (int pos = 0; pos < n.count; ++pos) {
            int begin = _size_before(n, pos);
            int end = n.child_size[pos];
            if (r <= begin) break;
            if (end <= l) continue;
            int child_l = max(0, l - begin);
            int child_r = min(end - begin, r - begin);
            if (_consume_pred(n.child[pos], level - 1, child_l, child_r, bit, aggregate, count, pred)) return true;
        }
        return false;
    }

    void _collect(const int node, const int level, vector<uint8_t> &bits, vector<W> &weights) const {
        if (level == 0) {
            const Leaf &leaf = _leaves[node];
            for (int i = 0; i < leaf.count; ++i) {
                bits.emplace_back(_bit_at(leaf, i));
                weights.emplace_back(leaf.weight[i]);
            }
            return;
        }
        const Internal &n = _internals[node];
        for (int i = 0; i < n.count; ++i) _collect(n.child[i], level - 1, bits, weights);
    }

public:
    BTreeBitVectorSum() : _leaves(1), _internals(1), _free_leaf(0), _free_internal(0) {}

    void reserve(int total_size, int sequence_count) {
        _leaves.reserve(_leaves.size() + sequence_count + (total_size + _LEAF_CAP - 1) / _LEAF_CAP);
        _internals.reserve(_internals.size() + total_size / (_LEAF_CAP * _BRANCH_MIN) + sequence_count / 32 + 1);
    }

    Sequence build(const vector<uint8_t>& bits, const vector<int>& order, const vector<W>& weights,
                   int start, int end) {
        Sequence sequence;
        int n = end - start;
        if (n == 0) return sequence;

        if (n <= _LEAF_CAP) {
            int leaf = _make_leaf();
            Leaf& node = _leaves[leaf];
            for (int i = 0; i < n; ++i) {
                bool bit = bits[start + i];
                W weight = weights[order[start + i]];
                node.weight[i] = weight;
                node.bits |= static_cast<uint16_t>(bit) << i;
                node.sum += weight;
                if (bit) node.sum1 += weight;
            }
            node.count = n;
            sequence.root = leaf;
            return sequence;
        }

        int leaf_count = (n + _LEAF_CAP - 1) / _LEAF_CAP;
        vector<int> nodes;
        nodes.reserve(leaf_count);
        int index = start;
        int remaining = n;
        for (int i = 0; i < leaf_count; ++i) {
            int take = (remaining + leaf_count - i - 1) / (leaf_count - i);
            int leaf = _make_leaf();
            Leaf &node = _leaves[leaf];
            for (int j = 0; j < take; ++j) {
                bool bit = bits[index];
                W weight = weights[order[index]];
                node.weight[j] = weight;
                node.bits |= static_cast<uint16_t>(bit) << j;
                node.sum += weight;
                if (bit) node.sum1 += weight;
                ++index;
            }
            node.count = take;
            nodes.emplace_back(leaf);
            remaining -= take;
        }

        int child_level = 0;
        while (nodes.size() > 1) {
            int count = nodes.size();
            int parent_count = (count + _BRANCH - 1) / _BRANCH;
            vector<int> next;
            next.reserve(parent_count);
            int p = 0;
            int remaining_nodes = count;
            for (int i = 0; i < parent_count; ++i) {
                int take = (remaining_nodes + parent_count - i - 1) / (parent_count - i);
                int parent = _make_internal();
                for (int j = 0; j < take; ++j) _append_child(parent, nodes[p++], child_level);
                next.emplace_back(parent);
                remaining_nodes -= take;
            }
            nodes.swap(next);
            ++child_level;
        }

        sequence.root = nodes[0];
        sequence.height = child_level;
        return sequence;
    }

    int len(const Sequence& sequence) const {
        return sequence.root == 0 ? 0 : _stats(sequence.root, sequence.height).size;
    }

    bool empty(const Sequence &sequence) const { return sequence.root == 0; }

    W all_sum(const Sequence& sequence) const {
        return sequence.root == 0 ? W(0) : _stats(sequence.root, sequence.height).sum;
    }

    RangeData range_data(const Sequence &sequence, const int l, const int r) const {
        assert(sequence.root != 0);
        assert(0 <= l && l <= r && r <= len(sequence));
        auto [left, right] = _prefix_pair(sequence, l, r);
        W sum = right.sum - left.sum;
        W sum1 = right.sum1 - left.sum1;
        return {l - left.ones, r - right.ones, sum - sum1, sum1};
    }

    int rank0(const Sequence& seq, const int r) const {
        assert(seq.root != 0);
        assert(0 <= r && r <= len(seq));
        return r - _ones_node(seq.root, seq.height, r);
    }

    pair<int, int> rank0_pair(const Sequence& seq, const int l, const int r) const {
        assert(seq.root != 0);
        assert(0 <= l && l <= r && r <= len(seq));
        auto [lo, ro] = _ones_pair(seq, l, r);
        return {l - lo, r - ro};
    }

    AccessData access(const Sequence &sequence, int k) const {
        assert(0 <= k && k < len(sequence));
        int node = sequence.root;
        int level = sequence.height;
        int rank1 = 0;
        while (level > 0) {
            const Internal &n = _internals[node];
            int pos = 0;
            while (k >= n.child_size[pos]) ++pos;
            rank1 += _ones_before(n, pos);
            k -= _size_before(n, pos);
            node = n.child[pos];
            --level;
        }
        const Leaf &leaf = _leaves[node];
        rank1 += popcount(leaf.bits & _mask(k));
        return {_bit_at(leaf, k), rank1, leaf.weight[k]};
    }

    pair<bool, int> access_bit(const Sequence& seq, int k) const {
        assert(0 <= k && k < len(seq));
        int node = seq.root;
        int level = seq.height;
        int rank1 = 0;
        while (level > 0) {
            const Internal& n = _internals[node];
            int pos = 0;
            while (k >= n.child_size[pos]) ++pos;
            rank1 += _ones_before(n, pos);
            k -= _size_before(n, pos);
            node = n.child[pos];
            --level;
        }
        const Leaf& leaf = _leaves[node];
        rank1 += popcount(leaf.bits & _mask(k));
        return {_bit_at(leaf, k), rank1};
    }

    W access_weight(const Sequence& seq, int k) const {
        assert(0 <= k && k < len(seq));
        int node = seq.root;
        int level = seq.height;
        while (level > 0) {
            const Internal& n = _internals[node];
            int pos = 0;
            while (k >= n.child_size[pos]) ++pos;
            k -= _size_before(n, pos);
            node = n.child[pos];
            --level;
        }
        return _leaves[node].weight[k];
    }

    int insert(Sequence &sequence, int k, const bool bit, const W weight) {
        assert(0 <= k && k <= len(sequence));
        if (sequence.root == 0) {
            int leaf = _make_leaf();
            _insert_leaf(leaf, 0, bit, weight);
            sequence.root = leaf;
            return 0;
        }

        array<int, _MAX_HEIGHT> path;
        array<int, _MAX_HEIGHT> slots;
        int depth = 0;
        int rank1 = 0;
        int node = sequence.root;
        int level = sequence.height;
        while (level > 0) {
            const Internal &n = _internals[node];
            int pos = 0;
            while (pos + 1 < n.count && k > n.child_size[pos]) ++pos;
            rank1 += _ones_before(n, pos);
            k -= _size_before(n, pos);
            path[depth] = node;
            slots[depth++] = pos;
            node = n.child[pos];
            --level;
        }

        Leaf &leaf = _leaves[node];
        rank1 += popcount(leaf.bits & _mask(k));
        if (leaf.count < _LEAF_CAP) {
            _insert_leaf(node, k, bit, weight);
            _add_path(path, slots, depth, {1, bit, weight, bit ? weight : W(0)});
            return rank1;
        }

        int right = _split_leaf(node);
        if (k <= _leaves[node].count) {
            _insert_leaf(node, k, bit, weight);
        } else {
            _insert_leaf(right, k - _leaves[node].count, bit, weight);
        }
        _propagate_split(sequence, path, slots, depth, node, right, 0);
        return rank1;
    }

    AccessData pop(Sequence &sequence, int k) {
        assert(0 <= k && k < len(sequence));
        array<int, _MAX_HEIGHT> path;
        array<int, _MAX_HEIGHT> slots;
        int depth = 0;
        int rank1 = 0;
        int node = sequence.root;
        int level = sequence.height;
        while (level > 0) {
            const Internal &n = _internals[node];
            int pos = 0;
            while (k >= n.child_size[pos]) ++pos;
            rank1 += _ones_before(n, pos);
            k -= _size_before(n, pos);
            path[depth] = node;
            slots[depth++] = pos;
            node = n.child[pos];
            --level;
        }

        Leaf &leaf = _leaves[node];
        rank1 += popcount(leaf.bits & _mask(k));
        bool bit = _bit_at(leaf, k);
        W weight = leaf.weight[k];
        _erase_leaf(node, k);
        if ((depth == 0 && leaf.count > 0) || (depth > 0 && leaf.count >= _LEAF_MIN)) {
            _add_path(path, slots, depth, {-1, -static_cast<int>(bit), -weight, bit ? -weight : W(0)});
        } else {
            _sync_path(path, slots, depth, node, 0);
            _rebalance_leaf(sequence, path, slots, depth, node);
        }
        return {bit, rank1, weight};
    }

    AccessData set(Sequence &sequence, int k, const bool bit, const W weight) {
        assert(0 <= k && k < len(sequence));
        array<int, _MAX_HEIGHT> path;
        array<int, _MAX_HEIGHT> slots;
        int depth = 0;
        int rank1 = 0;
        int node = sequence.root;
        int level = sequence.height;
        while (level > 0) {
            const Internal &n = _internals[node];
            int pos = 0;
            while (k >= n.child_size[pos]) ++pos;
            rank1 += _ones_before(n, pos);
            k -= _size_before(n, pos);
            path[depth] = node;
            slots[depth++] = pos;
            node = n.child[pos];
            --level;
        }

        Leaf &leaf = _leaves[node];
        rank1 += popcount(leaf.bits & _mask(k));
        bool old_bit = _bit_at(leaf, k);
        W old_weight = leaf.weight[k];
        if (old_bit == bit && old_weight == weight) return {old_bit, rank1, old_weight};
        if (old_bit != bit) leaf.bits ^= static_cast<uint16_t>(1) << k;
        leaf.weight[k] = weight;
        leaf.sum += weight - old_weight;
        leaf.sum1 += (bit ? weight : W(0)) - (old_bit ? old_weight : W(0));
        _add_path(path, slots, depth,
                  {0, static_cast<int>(bit) - static_cast<int>(old_bit), weight - old_weight,
                   (bit ? weight : W(0)) - (old_bit ? old_weight : W(0))});
        return {old_bit, rank1, old_weight};
    }

    AccessData set_bit(Sequence& seq, int k, const bool bit) {
        assert(0 <= k && k < len(seq));
        array<int, _MAX_HEIGHT> path;
        array<int, _MAX_HEIGHT> slots;
        int depth = 0;
        int rank1 = 0;
        int node = seq.root;
        int level = seq.height;
        while (level > 0) {
            const Internal& n = _internals[node];
            int pos = 0;
            while (k >= n.child_size[pos]) ++pos;
            rank1 += _ones_before(n, pos);
            k -= _size_before(n, pos);
            path[depth] = node;
            slots[depth++] = pos;
            node = n.child[pos];
            --level;
        }

        Leaf& leaf = _leaves[node];
        rank1 += popcount(leaf.bits & _mask(k));
        bool old = _bit_at(leaf, k);
        W weight = leaf.weight[k];
        if (old == bit) return {old, rank1, weight};
        leaf.bits ^= static_cast<uint16_t>(1) << k;
        W delta = bit ? weight : -weight;
        leaf.sum1 += delta;
        _add_path(path, slots, depth, {0, bit ? 1 : -1, W(0), delta});
        return {old, rank1, weight};
    }

    AccessData add_weight(Sequence &sequence, int k, W delta) {
        assert(0 <= k && k < len(sequence));
        array<int, _MAX_HEIGHT> path;
        array<int, _MAX_HEIGHT> slots;
        int depth = 0;
        int rank1 = 0;
        int node = sequence.root;
        int level = sequence.height;
        while (level > 0) {
            const Internal &n = _internals[node];
            int pos = 0;
            while (k >= n.child_size[pos]) ++pos;
            rank1 += _ones_before(n, pos);
            k -= _size_before(n, pos);
            path[depth] = node;
            slots[depth++] = pos;
            node = n.child[pos];
            --level;
        }

        Leaf &leaf = _leaves[node];
        rank1 += popcount(leaf.bits & _mask(k));
        bool bit = _bit_at(leaf, k);
        W old_weight = leaf.weight[k];
        leaf.weight[k] += delta;
        leaf.sum += delta;
        if (bit) leaf.sum1 += delta;
        _add_weight_path(path, slots, depth, delta, bit);
        return {bit, rank1, old_weight};
    }

    int select(const Sequence &sequence, int k, const bool bit) const {
        assert(sequence.root != 0);
        const Stats root_stats = _stats(sequence.root, sequence.height);
        assert(0 <= k && k < _branch_count(root_stats, bit));
        int node = sequence.root;
        int level = sequence.height;
        int offset = 0;
        while (level > 0) {
            const Internal &n = _internals[node];
            int pos = 0;
            while (true) {
                int count = bit ? n.child_ones[pos] : n.child_size[pos] - n.child_ones[pos];
                if (k < count) break;
                ++pos;
            }
            if (pos > 0) k -= bit ? n.child_ones[pos - 1] : n.child_size[pos - 1] - n.child_ones[pos - 1];
            offset += _size_before(n, pos);
            node = n.child[pos];
            --level;
        }
        const Leaf &leaf = _leaves[node];
        for (int i = 0; i < leaf.count; ++i) {
            if (_bit_at(leaf, i) != bit) continue;
            if (k-- == 0) return offset + i;
        }
        assert(false);
        return -1;
    }

    int select_pop(Sequence& seq, int k, const bool bit) {
        assert(seq.root != 0);
        const Stats root = _stats(seq.root, seq.height);
        assert(0 <= k && k < _branch_count(root, bit));
        array<int, _MAX_HEIGHT> path;
        array<int, _MAX_HEIGHT> slots;
        int depth = 0;
        int node = seq.root;
        int level = seq.height;
        int off = 0;
        while (level > 0) {
            const Internal& n = _internals[node];
            int pos = 0;
            while (true) {
                int cnt = bit ? n.child_ones[pos] : n.child_size[pos] - n.child_ones[pos];
                if (k < cnt) break;
                ++pos;
            }
            if (pos > 0) {
                k -= bit ? n.child_ones[pos - 1] : n.child_size[pos - 1] - n.child_ones[pos - 1];
            }
            off += _size_before(n, pos);
            path[depth] = node;
            slots[depth++] = pos;
            node = n.child[pos];
            --level;
        }

        Leaf& leaf = _leaves[node];
        int pos = 0;
        while (_bit_at(leaf, pos) != bit || k-- > 0) ++pos;
        int ans = off + pos;
        W weight = leaf.weight[pos];
        _erase_leaf(node, pos);
        if ((depth == 0 && leaf.count > 0) || (depth > 0 && leaf.count >= _LEAF_MIN)) {
            _add_path(path, slots, depth, {-1, -static_cast<int>(bit), -weight, bit ? -weight : W(0)});
        } else {
            _sync_path(path, slots, depth, node, 0);
            _rebalance_leaf(seq, path, slots, depth, node);
        }
        return ans;
    }

    int min_count_sum_ge(const Sequence &sequence, const int l, const int r, const bool bit, W target) const {
        if (target <= W(0)) return 0;
        RangeData data = range_data(sequence, l, r);
        W sum = bit ? data.sum1 : data.sum0;
        if (sum < target) return -1;
        int count = 0;
        bool found = _consume(sequence.root, sequence.height, l, r, bit, target, count);
        assert(found);
        return count;
    }

    template<class Pred>
    int max_count(const Sequence &sequence, const int l, const int r, const bool bit, W &aggregate, Pred pred) const {
        assert(0 <= l && l <= r && r <= len(sequence));
        assert(pred(aggregate));
        if (l == r) return 0;
        int count = 0;
        _consume_pred(sequence.root, sequence.height, l, r, bit, aggregate, count, pred);
        return count;
    }

    W sum_first_k(const Sequence &sequence, const int l, const int r, const bool bit, const int k) const {
        if (k == 0) return W(0);
        RangeData data = range_data(sequence, l, r);
        int count = bit ? (r - l) - (data.r0 - data.l0) : data.r0 - data.l0;
        assert(0 < k && k <= count);
        if (k == count) return bit ? data.sum1 : data.sum0;
        int before = bit ? l - data.l0 : data.l0;
        int end = select(sequence, before + k - 1, bit) + 1;
        RangeData prefix = range_data(sequence, l, end);
        return bit ? prefix.sum1 : prefix.sum0;
    }

    pair<vector<uint8_t>, vector<W>> tovector(const Sequence &sequence) const {
        vector<uint8_t> bits;
        vector<W> weights;
        bits.reserve(len(sequence));
        weights.reserve(len(sequence));
        if (sequence.root != 0) _collect(sequence.root, sequence.height, bits, weights);
        return {move(bits), move(weights)};
    }
};

} // namespace titan23
