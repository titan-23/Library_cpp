/// https://github.com/titan-23/Library_cpp/blob/main/titan_cpplib/ds/dynamic_segment_tree.cpp
#pragma once

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <limits>
#include <utility>
#include <vector>

using namespace std;

namespace titan23 {

/// @brief [0,N)を管理する経路圧縮付き動的セグメント木
/// clear後に更新した異なる位置がK個なら、ノード数は2K-1個でK=0なら0個
/// 葉と分岐を別々のプールに保持し、葉には値だけを持たせて位置は探索時に持ち回る
/// ノード番号は0が欠損、奇数が葉、正の偶数が分岐で、保持数の上限は2^32-1個
/// Nは列の長さ、Mは保持ノード数、M_otherはコピー元またはムーブ元のノード数
/// Tの演算・コピー・ムーブ・破棄、述語の評価はO(1)とする
/// ノード追加を含む計算量は償却計算量で、問い合わせではノードを追加しない
template <class IndexType, class T, T (*op)(T, T), T (*e)()>
class DynamicSegmentTree {
private:
    using NodeIndex = uint32_t;
    static constexpr NodeIndex null_node = 0;

    struct Leaf {
        T data;

        /// @brief 葉の値を初期化する / O(1)
        Leaf(T value) : data(move(value)) {}
    };

    // 既存のサイズが32バイトになる型だけ整列を強め、分岐のサイズは増やさない
    struct alignas(sizeof(IndexType) == 8 && sizeof(T) == 8
                       ? 32 : max(alignof(NodeIndex), max(alignof(IndexType), alignof(T)))) Branch {
        NodeIndex left, right;
        IndexType split, hi;
        T data;

        /// @brief 分岐の子と区間情報を初期化する / O(1)
        Branch(IndexType split, IndexType hi, T value, NodeIndex left, NodeIndex right)
            : left(left), right(right), split(split), hi(hi), data(move(value)) {}
    };

    vector<Leaf> leaves_;
    vector<Branch> branches_;
    IndexType n_ = 0;
    NodeIndex root_ = null_node;
    IndexType root_min_ = 0;

    /// @brief 番号が葉を表すか判定する / O(1)
    static bool is_leaf(NodeIndex node) { return (node & 1) != 0; }

    /// @brief 分岐のプール内添字を求める / O(1)
    static NodeIndex branch_index(NodeIndex node) { return (node >> 1) - 1; }

    /// @brief 分岐の情報への参照を返す / O(1)
    Branch& branch_node(NodeIndex node) { return branches_[branch_index(node)]; }

    /// @brief 分岐の情報への読み取り専用参照を返す / O(1)
    const Branch& branch_node(NodeIndex node) const { return branches_[branch_index(node)]; }

    /// @brief ノードの集約値への参照を返す / O(1)
    T& node_data(NodeIndex node) {
        return is_leaf(node) ? leaves_[node >> 1].data : branch_node(node).data;
    }

    /// @brief ノードの集約値への読み取り専用参照を返す / O(1)
    const T& node_data(NodeIndex node) const {
        return is_leaf(node) ? leaves_[node >> 1].data : branch_node(node).data;
    }

    /// @brief ノードが持つ最大位置の次の位置を返す / O(1)
    IndexType node_hi(NodeIndex node, IndexType node_min) const {
        return is_leaf(node) ? node_min + 1 : branch_node(node).hi;
    }

    /// @brief 右部分木の開始位置を返す / O(1)
    IndexType node_split(NodeIndex node) const {
        return branch_node(node).split;
    }

    /// @brief 未登録区間の集約値として単位元を返す / O(1)
    T initial_prod(IndexType length) const {
        (void)length;
        return e();
    }

    /// @brief 最上位の1ビットだけが立った値を返す / O(1)
    static IndexType highest_bit(IndexType value) {
        assert(value > 0);
        if constexpr (sizeof(IndexType) <= sizeof(unsigned long long)) {
            const unsigned long long x = static_cast<unsigned long long>(value);
            return IndexType(1) << (63 - __builtin_clzll(x));
        } else {
            const auto x = static_cast<__uint128_t>(value);
            const auto high = static_cast<unsigned long long>(x >> 64);
            if (high) return IndexType(1) << (127 - __builtin_clzll(high));
            return IndexType(1) << (63 - __builtin_clzll(static_cast<unsigned long long>(x)));
        }
    }

    /// @brief 葉または分岐を対応するプールに追加する / 償却O(1)
    [[gnu::noinline]]
    NodeIndex new_node(IndexType lo, IndexType hi, T value,
                       NodeIndex left = null_node, NodeIndex right = null_node) {
        assert(node_count() < static_cast<size_t>(numeric_limits<NodeIndex>::max()));
        if (left == null_node) {
            assert(right == null_node && hi == lo + 1);
            const NodeIndex index = static_cast<NodeIndex>(leaves_.size());
            assert(leaves_.size() <= (numeric_limits<NodeIndex>::max() >> 1));
            leaves_.emplace_back(move(value));
            return (index << 1) | 1;
        }
        const NodeIndex index = static_cast<NodeIndex>(branches_.size());
        assert(branches_.size() < (numeric_limits<NodeIndex>::max() >> 1));
        branches_.emplace_back(lo, hi, move(value), left, right);
        return (index + 1) << 1;
    }

    /// @brief 左右の集約値を順に結合する / O(1)
    T join_data(NodeIndex left, IndexType left_min, NodeIndex right, IndexType right_min) const {
        (void)left_min;
        (void)right_min;
        return op(node_data(left), node_data(right));
    }

    /// @brief 2つの部分木を座標順に結ぶ分岐を追加する / 償却O(1)
    [[gnu::noinline]]
    NodeIndex join_nodes(NodeIndex a, IndexType a_min, NodeIndex b, IndexType b_min) {
        if (b_min < a_min) { swap(a, b); swap(a_min, b_min); }
        return new_node(b_min, node_hi(b, b_min), join_data(a, a_min, b, b_min), a, b);
    }

    /// @brief 部分木の指定位置を更新し、必要な分岐を追加する / O(logN)
    NodeIndex inner_set(NodeIndex node, IndexType node_min, IndexType k, T& value) {
        if (node == null_node) return new_node(k, k + 1, move(value));
        if (is_leaf(node)) {
            if (node_min == k) {
                node_data(node) = move(value);
                return node;
            }
            return join_nodes(node, node_min, new_node(k, k + 1, move(value)), k);
        }
        const IndexType branch = highest_bit(node_min ^ (node_hi(node, node_min) - 1));
        if ((node_min ^ k) > (branch | (branch - 1))) {
            return join_nodes(node, node_min, new_node(k, k + 1, move(value)), k);
        }
        if ((k & branch) == 0) {
            const NodeIndex child = inner_set(branch_node(node).left, node_min, k, value);
            branch_node(node).left = child;
            node_min = min(node_min, k);
        } else {
            const NodeIndex child = inner_set(branch_node(node).right, node_split(node), k, value);
            branch_node(node).right = child;
            branch_node(node).split = min(branch_node(node).split, k);
        }
        const NodeIndex left = branch_node(node).left, right = branch_node(node).right;
        branch_node(node).hi = node_hi(right, node_split(node));
        node_data(node) = join_data(left, node_min, right, node_split(node));
        return node;
    }

    /// @brief 区間に含まれる部分木の集約値を返す / O(1)
    T covered_prod(NodeIndex node, IndexType node_min, IndexType l, IndexType r) const {
        (void)node_min;
        (void)l;
        (void)r;
        return node_data(node);
    }

    /// @brief 葉の座標を再読せず、前後の初期値を含む区間を集約する / O(1)
    T leaf_prod(NodeIndex node, IndexType node_min, IndexType l, IndexType r) const {
        (void)node_min;
        (void)l;
        (void)r;
        return leaves_[node >> 1].data;
    }

    /// @brief 部分木の先頭からr未満の範囲を集約する / O(logN)
    T prefix_prod(NodeIndex node, IndexType node_min, IndexType r) const {
        T acc = e();
        while (true) {
            if (r <= node_min) return acc;
            if (is_leaf(node)) return op(acc, leaf_prod(node, node_min, node_min, r));
            const Branch& b = branch_node(node);
            if (b.hi <= r) return op(acc, covered_prod(node, node_min, node_min, r));
            if (r <= b.split) node = b.left;
            else {
                acc = op(acc, node_data(b.left));
                node_min = b.split;
                node = b.right;
            }
        }
    }

    /// @brief l以上から部分木の末尾までを集約する / O(logN)
    T suffix_prod(NodeIndex node, IndexType node_min, IndexType l) const {
        T acc = e();
        while (true) {
            if (is_leaf(node)) {
                if (node_min < l) return acc;
                return op(leaf_prod(node, node_min, l, node_min + 1), acc);
            }
            const Branch& b = branch_node(node);
            if (b.hi <= l) return acc;
            if (l <= node_min) return op(covered_prod(node, node_min, l, b.hi), acc);
            if (b.split <= l) {
                node_min = b.split;
                node = b.right;
            } else {
                acc = op(node_data(b.right), acc);
                if (node_hi(b.left, node_min) <= l) return acc;
                node = b.left;
            }
        }
    }

    /// @brief 部分木内の[l,r)を集約する / O(logN)
    T inner_prod(NodeIndex node, IndexType node_min, IndexType l, IndexType r) const {
        while (true) {
            if (is_leaf(node)) {
                if (node_min < l || r <= node_min) return initial_prod(r - l);
                return leaf_prod(node, node_min, l, r);
            }
            const Branch& b = branch_node(node);
            if (r <= node_min || b.hi <= l) return initial_prod(r - l);
            if (l <= node_min && b.hi <= r) return covered_prod(node, node_min, l, r);
            if (r <= b.split) node = b.left;
            else if (b.split <= l) {
                node_min = b.split;
                node = b.right;
            } else {
                T left_value = suffix_prod(b.left, node_min, l);
                return op(left_value, prefix_prod(b.right, b.split, r));
            }
        }
    }

    /// @brief 部分木を左から結合して右端の境界を探す / O(logN)
    template <class Predicate>
    bool inner_max_right(NodeIndex node, IndexType node_min, IndexType l, Predicate& pred,
                         T& acc, IndexType& answer) const {
        if (node_hi(node, node_min) <= l) return false;
        if (l <= node_min) {
            T candidate = op(acc, node_data(node));
            if (pred(candidate)) {
                acc = move(candidate);
                return false;
            }
            if (is_leaf(node)) {
                answer = node_min;
                return true;
            }
        }
        const NodeIndex left = branch_node(node).left, right = branch_node(node).right;
        if (inner_max_right(left, node_min, l, pred, acc, answer)) return true;
        return inner_max_right(right, node_split(node), l, pred, acc, answer);
    }

    /// @brief 部分木を右から結合して左端の境界を探す / O(logN)
    template <class Predicate>
    bool inner_min_left(NodeIndex node, IndexType node_min, IndexType r, Predicate& pred,
                        T& acc, IndexType& answer) const {
        if (r <= node_min) return false;
        if (node_hi(node, node_min) <= r) {
            T candidate = op(node_data(node), acc);
            if (pred(candidate)) {
                acc = move(candidate);
                return false;
            }
            if (is_leaf(node)) {
                answer = node_hi(node, node_min);
                return true;
            }
        }
        const NodeIndex left = branch_node(node).left, right = branch_node(node).right;
        if (inner_min_left(right, node_split(node), r, pred, acc, answer)) return true;
        return inner_min_left(left, node_min, r, pred, acc, answer);
    }

public:
    /// @brief 長さ0の木を構築する / O(1)
    DynamicSegmentTree() = default;

    /// @brief 全要素が単位元の長さnの木を構築する / O(1)
    DynamicSegmentTree(const IndexType n) : n_(n) {
        assert(n >= 0);
    }

    /// @brief 独立した木としてコピーする / O(M_other)
    DynamicSegmentTree(const DynamicSegmentTree&) = default;

    /// @brief 現在の内容を独立したコピーで置き換える / O(M+M_other)
    DynamicSegmentTree& operator=(const DynamicSegmentTree&) = default;

    /// @brief 所有するノードを移し、移動元を空にする / O(1)
    DynamicSegmentTree(DynamicSegmentTree&& other) noexcept
        : leaves_(move(other.leaves_)), branches_(move(other.branches_)), n_(exchange(other.n_, 0)),
          root_(exchange(other.root_, null_node)), root_min_(exchange(other.root_min_, 0)) {
        other.leaves_.clear();
        other.branches_.clear();
    }

    /// @brief 現在の内容を破棄してノードを移し、移動元を空にする / O(M)
    DynamicSegmentTree& operator=(DynamicSegmentTree&& other) noexcept {
        if (this != &other) {
            leaves_ = move(other.leaves_);
            branches_ = move(other.branches_);
            n_ = exchange(other.n_, 0);
            root_ = exchange(other.root_, null_node);
            root_min_ = exchange(other.root_min_, 0);
            other.leaves_.clear();
            other.branches_.clear();
        }
        return *this;
    }

    /// @brief 列の長さを返す / O(1)
    IndexType size() const { return n_; }

    /// @brief 列全体の集約値を返す / O(1)
    T all_prod() const { return root_ == null_node ? initial_prod(n_) : covered_prod(root_, root_min_, 0, n_); }

    /// @brief 保持しているノード数を返す / O(1)
    size_t node_count() const { return leaves_.size() + branches_.size(); }

    /// @brief ノードプールの容量を予約する / O(M) 再確保しなければO(1)
    void reserve_nodes(size_t count) {
        assert(count <= static_cast<size_t>(numeric_limits<NodeIndex>::max()));
        leaves_.reserve(count / 2 + count % 2);
        branches_.reserve(count / 2);
    }

    /// @brief 長さと確保済み容量を維持して構築時の初期値に戻す / O(M)
    void clear() {
        leaves_.clear();
        branches_.clear();
        root_ = null_node;
        root_min_ = 0;
    }

    /// @brief 半開区間[l,r)の集約値を返す / O(logN)
    T prod(IndexType l, IndexType r) const {
        assert(0 <= l && l <= r && r <= n_);
        if (l == r) return e();
        return root_ == null_node ? initial_prod(r - l) : inner_prod(root_, root_min_, l, r);
    }

    /// @brief k番目の値を返す / O(logN)
    T get(IndexType k) const {
        assert(0 <= k && k < n_);
        if (k < root_min_) return e();
        NodeIndex node = root_;
        IndexType node_min = root_min_;
        while (node != null_node) {
            if (is_leaf(node)) return node_min == k ? leaves_[node >> 1].data : e();
            const Branch& b = branch_node(node);
            if (b.hi <= k) break;
            if (k < b.split) node = b.left;
            else {
                node_min = b.split;
                node = b.right;
            }
        }
        return e();
    }

    /// @brief k番目の値をvalueに置き換える / O(logN)
    void set(IndexType k, T value) {
        assert(0 <= k && k < n_);
        const bool empty = root_ == null_node;
        root_ = inner_set(root_, root_min_, k, value);
        root_min_ = empty ? k : min(root_min_, k);
    }

    /// @brief pred(prod(l,r))が真となる最大のrを求める / O(logN)
    /// pred(e())==trueと、区間を右へ伸ばしたときの判定の単調性が必要
    template <class Predicate>
    IndexType max_right(IndexType l, Predicate&& pred) const {
        assert(0 <= l && l <= n_);
        T acc = e();
        assert(pred(acc));
        if (l == n_) return n_;
        IndexType answer = n_;
        if (root_ != null_node) inner_max_right(root_, root_min_, l, pred, acc, answer);
        return answer;
    }

    /// @brief pred(prod(l,r))が真となる最小のlを求める / O(logN)
    /// pred(e())==trueと、区間を左へ伸ばしたときの判定の単調性が必要
    template <class Predicate>
    IndexType min_left(IndexType r, Predicate&& pred) const {
        assert(0 <= r && r <= n_);
        T acc = e();
        assert(pred(acc));
        if (r == 0) return 0;
        IndexType answer = 0;
        if (root_ != null_node) inner_min_left(root_, root_min_, r, pred, acc, answer);
        return answer;
    }

    /// @brief 関数ポインタpredを使って右端の境界を探す / O(logN)
    template <bool (*pred)(T)>
    IndexType max_right(IndexType l) const { return max_right(l, pred); }

    /// @brief 関数ポインタpredを使って左端の境界を探す / O(logN)
    template <bool (*pred)(T)>
    IndexType min_left(IndexType r) const { return min_left(r, pred); }

    /// @brief 全要素を添字順に出力する / O(NlogN)
    void print() const {
        for (IndexType i = 0; i < n_; ++i) cout << get(i) << ", ";
        cout << '\n';
    }
};

}  // namespace titan23
