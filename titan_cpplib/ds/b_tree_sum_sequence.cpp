/// https://github.com/titan-23/Library_cpp/blob/main/titan_cpplib/ds/b_tree_sum_sequence.cpp
#pragma once

#include <algorithm>
#include <array>
#include <cassert>
#include <cstdint>
#include <utility>
#include <vector>
#include "titan_cpplib/others/simd_int.cpp"
using namespace std;

namespace titan23 {

/// @brief 重みの列を B 木で持ち、位置の挿入・削除と区間和を扱う / 複数の列が節点の領域を共有する
template<typename W>
class BTreeSumSequence {
private:
    static constexpr int _LEAF_CAP = 32;
    static constexpr int _LEAF_MIN = _LEAF_CAP / 2;
    static constexpr int _BRANCH = 16;
    static constexpr int _BRANCH_MIN = _BRANCH / 2;
    static constexpr int _MAX_HEIGHT = 16;

    struct Leaf {
        array<W, _LEAF_CAP> weight;
        W sum;
        int next;
        uint8_t count;

        Leaf() : sum(0), next(0), count(0) {}
    };

    struct Internal {
        array<int, _BRANCH> child;
        array<int, _BRANCH> child_size;
        array<W, _BRANCH> child_sum;
        uint8_t count;

        // SIMD の探索は未使用の要素も読むので、探索に使う累積値を初期化する
        Internal() : child_size{}, count(0) {}
    };

    struct Stats {
        int size;
        W sum;
    };

public:
    /// @brief 1 本の列を表す / 根の番号と高さを持つ
    struct Sequence {
        int root;
        uint8_t height;

        Sequence() : root(0), height(0) {}
    };

private:
    vector<Leaf> _leaves;
    vector<Internal> _internals;
    int _free_leaf;
    int _free_internal;

    static uint64_t _mask(const int n) { return (uint64_t(1) << n) - 1; }

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

    W _sum_before(const Internal &node, const int pos) const { return pos == 0 ? W(0) : node.child_sum[pos - 1]; }

    // child_size[pos] > k となる最初の pos を返す / 未使用の要素は結果に影響しない
    int _find_size(const Internal &node, const int k) const {
        const int pos = simd_find_first_greater_unchecked<_BRANCH>(node.child_size.data(), k);
        assert(pos < node.count);
        return pos;
    }

    Stats _stats(const int node, const int level) const {
        if (level == 0) {
            const Leaf &leaf = _leaves[node];
            return {leaf.count, leaf.sum};
        }
        const Internal &n = _internals[node];
        return {n.child_size[n.count - 1], n.child_sum[n.count - 1]};
    }

    Stats _child_stats(const Internal &node, const int pos) const {
        return {node.child_size[pos] - _size_before(node, pos), node.child_sum[pos] - _sum_before(node, pos)};
    }

    void _pull_leaf(const int leaf) {
        Leaf &node = _leaves[leaf];
        node.sum = 0;
        for (int i = 0; i < node.count; ++i) node.sum += node.weight[i];
    }

    void _append_child(const int node, const int child, const int child_level) {
        Internal &n = _internals[node];
        const Stats stats = _stats(child, child_level);
        int pos = n.count;
        n.child[pos] = child;
        n.child_size[pos] = _size_before(n, pos) + stats.size;
        n.child_sum[pos] = _sum_before(n, pos) + stats.sum;
        ++n.count;
    }

    void _insert_child(const int node, const int pos, const int child, const Stats &stats) {
        Internal &n = _internals[node];
        assert(n.count < _BRANCH);
        for (int i = n.count; i > pos; --i) {
            n.child[i] = n.child[i - 1];
            n.child_size[i] = n.child_size[i - 1] + stats.size;
            n.child_sum[i] = n.child_sum[i - 1] + stats.sum;
        }
        n.child[pos] = child;
        n.child_size[pos] = _size_before(n, pos) + stats.size;
        n.child_sum[pos] = _sum_before(n, pos) + stats.sum;
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
            n.child_sum[i] = n.child_sum[i + 1] - stats.sum;
        }
    }

    // pos の子を child に置き換え、累積値を child の実際の値に合わせる
    void _set_child(const int node, const int pos, const int child, const int child_level) {
        Internal &n = _internals[node];
        const Stats old_stats = _child_stats(n, pos);
        const Stats new_stats = _stats(child, child_level);
        const int size_delta = new_stats.size - old_stats.size;
        const W sum_delta = new_stats.sum - old_stats.sum;
        n.child[pos] = child;
        for (int i = pos; i < n.count; ++i) {
            n.child_size[i] += size_delta;
            n.child_sum[i] += sum_delta;
        }
    }

    void _add_path(const array<int, _MAX_HEIGHT> &path, const array<int, _MAX_HEIGHT> &slots, const int depth,
                   const int size_delta, const W sum_delta) {
        for (int i = 0; i < depth; ++i) {
            Internal &node = _internals[path[i]];
            for (int j = slots[i]; j < node.count; ++j) {
                node.child_size[j] += size_delta;
                node.child_sum[j] += sum_delta;
            }
        }
    }

    void _sync_path(const array<int, _MAX_HEIGHT> &path, const array<int, _MAX_HEIGHT> &slots, const int depth,
                    int child, int child_level) {
        for (int i = depth - 1; i >= 0; --i) {
            _set_child(path[i], slots[i], child, child_level);
            child = path[i];
            ++child_level;
        }
    }

    void _insert_leaf(const int leaf, const int pos, const W weight) {
        Leaf &node = _leaves[leaf];
        assert(node.count < _LEAF_CAP);
        copy_backward(node.weight.begin() + pos, node.weight.begin() + node.count, node.weight.begin() + node.count + 1);
        node.weight[pos] = weight;
        node.sum += weight;
        ++node.count;
    }

    W _erase_leaf(const int leaf, const int pos) {
        Leaf &node = _leaves[leaf];
        const W weight = node.weight[pos];
        node.sum -= weight;
        copy(node.weight.begin() + pos + 1, node.weight.begin() + node.count, node.weight.begin() + pos);
        --node.count;
        return weight;
    }

    int _split_leaf(const int leaf) {
        int right = _make_leaf();
        Leaf &left_node = _leaves[leaf];
        Leaf &right_node = _leaves[right];
        int split = _LEAF_CAP / 2;
        right_node.count = left_node.count - split;
        for (int i = 0; i < right_node.count; ++i) right_node.weight[i] = left_node.weight[split + i];
        left_node.count = split;
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
        W left_sum = left_node.child_sum[split - 1];
        right_node.count = left_node.count - split;
        for (int i = 0; i < right_node.count; ++i) {
            right_node.child[i] = left_node.child[split + i];
            right_node.child_size[i] = left_node.child_size[split + i] - left_size;
            right_node.child_sum[i] = left_node.child_sum[split + i] - left_sum;
        }
        left_node.count = split;
        return right;
    }

    void _propagate_split(Sequence &sequence, const array<int, _MAX_HEIGHT> &path,
                          const array<int, _MAX_HEIGHT> &slots, int depth, int left, int right, int child_level) {
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

    void _append_leaf(const int left, const int right) {
        Leaf &left_node = _leaves[left];
        const Leaf &right_node = _leaves[right];
        assert(left_node.count + right_node.count <= _LEAF_CAP);
        int offset = left_node.count;
        for (int i = 0; i < right_node.count; ++i) left_node.weight[offset + i] = right_node.weight[i];
        left_node.count += right_node.count;
        left_node.sum += right_node.sum;
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
        W sum = left_node.count == 0 ? W(0) : left_node.child_sum[left_node.count - 1];
        for (int i = 0; i < right_node.count; ++i) {
            int pos = left_node.count + i;
            left_node.child[pos] = right_node.child[i];
            left_node.child_size[pos] = size + right_node.child_size[i];
            left_node.child_sum[pos] = sum + right_node.child_sum[i];
        }
        left_node.count += right_node.count;
    }

    void _rebalance_internal(Sequence &sequence, const array<int, _MAX_HEIGHT> &path,
                             const array<int, _MAX_HEIGHT> &slots, const int node_depth, const int level) {
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

    void _rebalance_leaf(Sequence &sequence, const array<int, _MAX_HEIGHT> &path,
                         const array<int, _MAX_HEIGHT> &slots, const int depth, const int leaf) {
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
                const W weight = _erase_leaf(left, _leaves[left].count - 1);
                _insert_leaf(leaf, 0, weight);
                _set_child(parent, pos - 1, left, 0);
                _set_child(parent, pos, leaf, 0);
                return;
            }
        }
        if (pos + 1 < parent_node.count) {
            int right = parent_node.child[pos + 1];
            if (_leaves[right].count > _LEAF_MIN) {
                const W weight = _erase_leaf(right, 0);
                _insert_leaf(leaf, _leaves[leaf].count, weight);
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

    // 葉の [l, r) の和。マスク付きの読み込みは範囲外の要素を読まない
    W _leaf_range_sum(const int leaf, const int l, const int r) const {
        const W *weight = _leaves[leaf].weight.data();
        if constexpr (simd_int_vectorized_v<W>) {
            return simd_masked_sum<_LEAF_CAP>(weight, _mask(r) & ~_mask(l));
        } else {
            W sum = 0;
            for (int i = l; i < r; ++i) sum += weight[i];
            return sum;
        }
    }

    W _leaf_prefix(const int leaf, const int take) const { return _leaf_range_sum(leaf, 0, take); }

    // 先頭 take 個の和
    W _prefix_node(int node, int level, int take) const {
        if (take == 0) return W(0);
        W result = 0;
        while (level > 0) {
            const Internal &n = _internals[node];
            const int pos = _find_size(n, take - 1);
            if (take == n.child_size[pos]) return result + n.child_sum[pos];
            result += _sum_before(n, pos);
            take -= _size_before(n, pos);
            node = n.child[pos];
            --level;
        }
        return result + _leaf_prefix(node, take);
    }

    void _collect(const int node, const int level, vector<W> &weights) const {
        if (level == 0) {
            const Leaf &leaf = _leaves[node];
            for (int i = 0; i < leaf.count; ++i) weights.emplace_back(leaf.weight[i]);
            return;
        }
        const Internal &n = _internals[node];
        for (int i = 0; i < n.count; ++i) _collect(n.child[i], level - 1, weights);
    }

    // 位置 k の要素がある葉まで降り、途中の節点と子の番号を path, slots に記録する
    int _descend(const Sequence &sequence, int &k, array<int, _MAX_HEIGHT> &path, array<int, _MAX_HEIGHT> &slots,
                 int &depth) const {
        int node = sequence.root;
        int level = sequence.height;
        depth = 0;
        while (level > 0) {
            const Internal &n = _internals[node];
            const int pos = _find_size(n, k);
            k -= _size_before(n, pos);
            path[depth] = node;
            slots[depth++] = pos;
            node = n.child[pos];
            --level;
        }
        return node;
    }

public:
    /// @brief 列を 1 本も持たない状態で作る
    BTreeSumSequence() : _leaves(1), _internals(1), _free_leaf(0), _free_internal(0) {}

    /// @brief 全列の長さの合計 total_size と列の数 sequence_count に合わせて領域を予約する
    void reserve(int total_size, int sequence_count) {
        _leaves.reserve(_leaves.size() + sequence_count + (total_size + _LEAF_CAP - 1) / _LEAF_CAP);
        _internals.reserve(_internals.size() + total_size / (_LEAF_CAP * _BRANCH_MIN) + sequence_count / 32 + 1);
    }

    /// @brief start <= i < end の範囲で weights[order[i]] を並べた列を作る / O(end - start)
    Sequence build(const vector<int> &order, const vector<W> &weights, int start, int end) {
        Sequence sequence;
        int n = end - start;
        if (n == 0) return sequence;

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
                W weight = weights[order[index++]];
                node.weight[j] = weight;
                node.sum += weight;
            }
            node.count = take;
            nodes.emplace_back(leaf);
            remaining -= take;
        }
        if (leaf_count == 1) {
            sequence.root = nodes[0];
            return sequence;
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

    /// @brief 列の長さを返す / O(1)
    int len(const Sequence &sequence) const {
        return sequence.root == 0 ? 0 : _stats(sequence.root, sequence.height).size;
    }

    /// @brief 列が空かを返す / O(1)
    bool empty(const Sequence &sequence) const { return sequence.root == 0; }

    /// @brief 列全体の和を返す / O(1)
    W all_sum(const Sequence &sequence) const {
        return sequence.root == 0 ? W(0) : _stats(sequence.root, sequence.height).sum;
    }

    /// @brief 先頭 r 個の和を返す / O(log n)
    W prefix_sum(const Sequence &sequence, const int r) const {
        assert(0 <= r && r <= len(sequence));
        return _prefix_node(sequence.root, sequence.height, r);
    }

    /// @brief 区間 [l, r) の和を返す / O(log n)
    W range_sum(const Sequence &sequence, int l, int r) const {
        assert(0 <= l && l <= r && r <= len(sequence));
        if (l == r) return W(0);
        // l と r が同じ子に入る間は共通の経路を降り、分かれたら各々の前置和を求める
        int node = sequence.root;
        int level = sequence.height;
        while (level > 0) {
            const Internal &n = _internals[node];
            const int left_pos = _find_size(n, l - 1);
            const int right_pos = _find_size(n, r - 1);
            if (left_pos != right_pos) {
                const W right = _sum_before(n, right_pos) +
                                _prefix_node(n.child[right_pos], level - 1, r - _size_before(n, right_pos));
                const W left = _sum_before(n, left_pos) +
                               _prefix_node(n.child[left_pos], level - 1, l - _size_before(n, left_pos));
                return right - left;
            }
            const int size_before = _size_before(n, left_pos);
            l -= size_before;
            r -= size_before;
            node = n.child[left_pos];
            --level;
        }
        return _leaf_range_sum(node, l, r);
    }

    /// @brief 位置 k の重みを返す / O(log n)
    W access(const Sequence &sequence, int k) const {
        assert(0 <= k && k < len(sequence));
        int node = sequence.root;
        int level = sequence.height;
        while (level > 0) {
            const Internal &n = _internals[node];
            const int pos = _find_size(n, k);
            k -= _size_before(n, pos);
            node = n.child[pos];
            --level;
        }
        return _leaves[node].weight[k];
    }

    /// @brief 位置 k に weight を挿入する / O(log n)
    void insert(Sequence &sequence, int k, const W weight) {
        assert(0 <= k && k <= len(sequence));
        if (sequence.root == 0) {
            int leaf = _make_leaf();
            _insert_leaf(leaf, 0, weight);
            sequence.root = leaf;
            return;
        }

        array<int, _MAX_HEIGHT> path;
        array<int, _MAX_HEIGHT> slots;
        int depth = 0;
        int node = sequence.root;
        int level = sequence.height;
        while (level > 0) {
            Internal &n = _internals[node];
            // k は 0 以上ノードの長さ以下なので、最後の子より後ろには進まない
            const int pos = _find_size(n, k - 1);
            k -= _size_before(n, pos);
            // 祖先の累積値は降りながら更新する
            for (int j = pos; j < n.count; ++j) {
                ++n.child_size[j];
                n.child_sum[j] += weight;
            }
            path[depth] = node;
            slots[depth++] = pos;
            node = n.child[pos];
            --level;
        }

        if (_leaves[node].count < _LEAF_CAP) {
            _insert_leaf(node, k, weight);
            return;
        }

        int right = _split_leaf(node);
        if (k <= _leaves[node].count) {
            _insert_leaf(node, k, weight);
        } else {
            _insert_leaf(right, k - _leaves[node].count, weight);
        }
        _propagate_split(sequence, path, slots, depth, node, right, 0);
    }

    /// @brief 位置 k の重みを削除して返す / O(log n)
    W pop(Sequence &sequence, int k) {
        assert(0 <= k && k < len(sequence));
        array<int, _MAX_HEIGHT> path;
        array<int, _MAX_HEIGHT> slots;
        int depth;
        const int node = _descend(sequence, k, path, slots, depth);
        const W weight = _erase_leaf(node, k);
        const Leaf &leaf = _leaves[node];
        if ((depth == 0 && leaf.count > 0) || (depth > 0 && leaf.count >= _LEAF_MIN)) {
            _add_path(path, slots, depth, -1, -weight);
        } else {
            _sync_path(path, slots, depth, node, 0);
            _rebalance_leaf(sequence, path, slots, depth, node);
        }
        return weight;
    }

    /// @brief 位置 k の重みに delta を足し、足す前の重みを返す / O(log n)
    W add(Sequence &sequence, int k, const W delta) {
        assert(0 <= k && k < len(sequence));
        array<int, _MAX_HEIGHT> path;
        array<int, _MAX_HEIGHT> slots;
        int depth;
        const int node = _descend(sequence, k, path, slots, depth);
        Leaf &leaf = _leaves[node];
        const W old = leaf.weight[k];
        leaf.weight[k] += delta;
        leaf.sum += delta;
        _add_path(path, slots, depth, 0, delta);
        return old;
    }

    /// @brief 位置 k の重みを weight にし、変更前の重みを返す / O(log n)
    W set(Sequence &sequence, int k, const W weight) {
        assert(0 <= k && k < len(sequence));
        array<int, _MAX_HEIGHT> path;
        array<int, _MAX_HEIGHT> slots;
        int depth;
        const int node = _descend(sequence, k, path, slots, depth);
        Leaf &leaf = _leaves[node];
        const W old = leaf.weight[k];
        if (old == weight) return old;
        leaf.weight[k] = weight;
        leaf.sum += weight - old;
        _add_path(path, slots, depth, 0, weight - old);
        return old;
    }

    /// @brief 列を vector にして返す / O(n)
    vector<W> tovector(const Sequence &sequence) const {
        vector<W> weights;
        weights.reserve(len(sequence));
        if (sequence.root != 0) _collect(sequence.root, sequence.height, weights);
        return weights;
    }
};

}  // namespace titan23
