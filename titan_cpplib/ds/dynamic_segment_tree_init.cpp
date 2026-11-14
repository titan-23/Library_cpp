/// https://github.com/titan-23/Library_cpp/blob/main/titan_cpplib/ds/dynamic_segment_tree_init.cpp
#pragma once

#include <algorithm>
#include <cassert>
#include <climits>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <limits>
#include <utility>
#include <vector>

using namespace std;

namespace titan23 {

/// @brief 全要素の初期値を指定できる、[0,N)を管理する経路圧縮付き動的セグメント木 / ノード数の上限は2^32-1個
/// pow(x,n)はxをn個結合した値を返すこと。計算量はpowをO(1)とみなして示す
/// Mは保持ノード数、M_otherはコピー元または移動元のノード数を表す
template <class IndexType, class T, T (*op)(T, T), T (*e)(),
          T (*pow)(T, IndexType)>
class DynamicSegmentTreeInit {
private:
    // ノード番号は0が欠損、奇数が葉、正の偶数が分岐
    using NodeIndex = uint32_t;
    static constexpr NodeIndex null_node = 0;

    struct Leaf {
        T data;

        Leaf(T value) : data(move(value)) {}
    };

    // 既存のサイズが32バイトになる型だけ整列を強め、分岐のサイズは増やさない
    struct alignas(sizeof(IndexType) == 8 && sizeof(T) == 8
                       ? 32 : max(alignof(NodeIndex), max(alignof(IndexType), alignof(T)))) Branch {
        NodeIndex left, right;
        IndexType split, hi;
        T data;

        Branch(IndexType split, IndexType hi, T value, NodeIndex left, NodeIndex right)
            : left(left), right(right), split(split), hi(hi), data(move(value)) {}
    };

    vector<Leaf> leaves_;
    vector<Branch> branches_;
    IndexType n_ = 0;
    NodeIndex root_ = null_node;
    IndexType root_min_ = 0;
    T init_val_ = e();

    static bool is_leaf(NodeIndex node) { return (node & 1) != 0; }

    // 分岐のプール内の添字
    static NodeIndex branch_index(NodeIndex node) { return (node >> 1) - 1; }

    Branch& branch_node(NodeIndex node) { return branches_[branch_index(node)]; }

    const Branch& branch_node(NodeIndex node) const { return branches_[branch_index(node)]; }

    T& node_data(NodeIndex node) {
        return is_leaf(node) ? leaves_[node >> 1].data : branch_node(node).data;
    }

    const T& node_data(NodeIndex node) const {
        return is_leaf(node) ? leaves_[node >> 1].data : branch_node(node).data;
    }

    // ノードが持つ最大位置の次の位置
    IndexType node_hi(NodeIndex node, IndexType node_min) const {
        return is_leaf(node) ? node_min + 1 : branch_node(node).hi;
    }

    // 右部分木の開始位置
    IndexType node_split(NodeIndex node) const {
        return branch_node(node).split;
    }

    // 初期値をlength個結合した値
    T initial_prod(IndexType length) const {
        return length == 0 ? e() : pow(init_val_, length);
    }

    // 最上位の1ビットだけが立った値
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

    // 葉または分岐を対応するプールに追加する
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

    // 初期値の隙間を含めて左右の集約値を結合する
    T join_data(NodeIndex left, IndexType left_min, NodeIndex right, IndexType right_min) const {
        T value = node_data(left);
        const IndexType gap = right_min - node_hi(left, left_min);
        if (gap != 0) value = op(value, initial_prod(gap));
        return op(value, node_data(right));
    }

    // 2つの部分木を座標順に結ぶ分岐を追加する
    [[gnu::noinline]]
    NodeIndex join_nodes(NodeIndex a, IndexType a_min, NodeIndex b, IndexType b_min) {
        if (b_min < a_min) { swap(a, b); swap(a_min, b_min); }
        return new_node(b_min, node_hi(b, b_min), join_data(a, a_min, b, b_min), a, b);
    }

    // 部分木の位置kを更新し、必要な分岐を追加する
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

    // 部分木の前後の初期値を含めて区間を集約する
    T covered_prod(NodeIndex node, IndexType node_min, IndexType l, IndexType r) const {
        T value = node_data(node);
        if (l < node_min) value = op(initial_prod(node_min - l), value);
        if (node_hi(node, node_min) < r) value = op(value, initial_prod(r - node_hi(node, node_min)));
        return value;
    }

    // 葉の座標を再読せず、前後の初期値を含む区間を集約する
    T leaf_prod(NodeIndex node, IndexType node_min, IndexType l, IndexType r) const {
        T value = leaves_[node >> 1].data;
        if (l < node_min) value = op(initial_prod(node_min - l), value);
        const IndexType hi = node_min + 1;
        if (hi < r) value = op(value, initial_prod(r - hi));
        return value;
    }

    // 部分木内の[l,r)を集約する。呼び出し元に展開すると遅くなるため展開させない
    [[gnu::noinline]]
    T inner_prod(NodeIndex node, IndexType node_min, IndexType l, IndexType r) const {
        while (true) {
            if (is_leaf(node)) {
                if (node_min < l || r <= node_min) return initial_prod(r - l);
                return leaf_prod(node, node_min, l, r);
            }
            const Branch& b = branch_node(node);
            if (r <= node_min || b.hi <= l) return initial_prod(r - l);
            if (l <= node_min && b.hi <= r) return covered_prod(node, node_min, l, r);
            const IndexType split = b.split;
            if ((l < split) & (split < r)) break;
            // 子の選択を条件付き転送で書ける形にする
            const bool right = split <= l;
            node_min = right ? split : node_min;
            node = right ? b.right : b.left;
        }
        const Branch& top = branch_node(node);
        NodeIndex left = top.left, right = top.right;
        IndexType left_min = node_min, right_min = top.split;
        // 左の子の末尾から右の子の先頭までの初期値
        const IndexType middle_start = max(l, node_hi(left, left_min));
        T left_acc = e(), right_acc = e();
        bool left_done = false, right_done = false;
        // 左端側と右端側の経路を同じループで下る
        while (!(left_done & right_done)) {
            // 左の子ではl以上を右から結合する。lより前の初期値は含めない
            if (!left_done) {
                if (is_leaf(left)) {
                    if (l <= left_min) left_acc = op(leaf_prod(left, left_min, l, left_min + 1), left_acc);
                    left_done = true;
                } else {
                    const Branch& b = branch_node(left);
                    if (b.hi <= l) left_done = true;
                    else if (l <= left_min) {
                        left_acc = op(covered_prod(left, left_min, l, b.hi), left_acc);
                        left_done = true;
                    } else if (b.split <= l) {
                        left_min = b.split;
                        left = b.right;
                    } else {
                        left_acc = op(node_data(b.right), left_acc);
                        const IndexType left_hi = node_hi(b.left, left_min);
                        const IndexType gap_start = max(l, left_hi);
                        if (gap_start < b.split) left_acc = op(initial_prod(b.split - gap_start), left_acc);
                        if (left_hi <= l) left_done = true;
                        else left = b.left;
                    }
                }
            }
            // 右の子ではr未満を左から結合する
            if (!right_done) {
                if (is_leaf(right)) {
                    right_acc = op(right_acc, leaf_prod(right, right_min, right_min, r));
                    right_done = true;
                } else {
                    const Branch& b = branch_node(right);
                    if (b.hi <= r) {
                        right_acc = op(right_acc, covered_prod(right, right_min, right_min, r));
                        right_done = true;
                    } else if (r <= b.split) right = b.left;
                    else {
                        right_acc = op(right_acc, node_data(b.left));
                        const IndexType gap = b.split - node_hi(b.left, right_min);
                        if (gap != 0) right_acc = op(right_acc, initial_prod(gap));
                        right_min = b.split;
                        right = b.right;
                    }
                }
            }
        }
        if (middle_start < top.split) left_acc = op(left_acc, initial_prod(top.split - middle_start));
        return op(left_acc, right_acc);
    }

    // 初期値だけの区間で右端の境界を探す
    template <class Predicate>
    bool uniform_max_right(IndexType lo, IndexType hi, IndexType l,
                           Predicate& pred, T& acc, IndexType& answer) const {
        lo = max(lo, l);
        if (lo >= hi) return false;
        T candidate = op(acc, initial_prod(hi - lo));
        if (pred(candidate)) {
            acc = move(candidate);
            return false;
        }
        while (hi - lo > 1) {
            const IndexType mid = lo + (hi - lo) / 2;
            candidate = op(acc, initial_prod(mid - lo));
            if (pred(candidate)) {
                acc = move(candidate);
                lo = mid;
            } else hi = mid;
        }
        answer = lo;
        return true;
    }

    // 初期値だけの区間で左端の境界を探す
    template <class Predicate>
    bool uniform_min_left(IndexType lo, IndexType hi, IndexType r,
                          Predicate& pred, T& acc, IndexType& answer) const {
        hi = min(hi, r);
        if (lo >= hi) return false;
        T candidate = op(initial_prod(hi - lo), acc);
        if (pred(candidate)) {
            acc = move(candidate);
            return false;
        }
        while (hi - lo > 1) {
            const IndexType mid = lo + (hi - lo) / 2;
            candidate = op(initial_prod(hi - mid), acc);
            if (pred(candidate)) {
                acc = move(candidate);
                hi = mid;
            } else lo = mid;
        }
        answer = hi;
        return true;
    }

    // 判定が失敗する部分木を下り、子の間の初期値も含めて境界の位置を求める。Forwardなら左から結合する
    template <bool Forward, class Predicate>
    IndexType descend_failed(NodeIndex node, IndexType node_min, IndexType boundary, Predicate& pred, T& acc) const {
        IndexType answer = 0;
        while (!is_leaf(node)) {
            const Branch& b = branch_node(node);
            const NodeIndex first = Forward ? b.left : b.right, second = Forward ? b.right : b.left;
            T candidate = Forward ? op(acc, node_data(first)) : op(node_data(first), acc);
            if (!pred(candidate)) {
                if constexpr (!Forward) node_min = b.split;
                node = first;
                continue;
            }
            acc = move(candidate);
            const IndexType left_hi = node_hi(b.left, node_min);
            if constexpr (Forward) {
                if (uniform_max_right(left_hi, b.split, boundary, pred, acc, answer)) return answer;
                node_min = b.split;
            } else {
                if (uniform_min_left(left_hi, b.split, boundary, pred, acc, answer)) return answer;
            }
            node = second;
        }
        return Forward ? node_min : node_min + 1;
    }

    // 部分木または初期値だけの区間[lo,hi)。nodeがnull_nodeなら初期値だけの区間
    struct Pending {
        NodeIndex node;
        IndexType lo, hi;
    };

    // 木の中で境界を探し、見つかればanswerに入れてtrueを返す。木の外側の初期値は呼び出し元が調べる
    template <bool Forward, class Predicate>
    bool scan_boundary(IndexType boundary, Predicate& pred, T& acc, IndexType& answer) const {
        Pending pending[2 * (sizeof(IndexType) * CHAR_BIT + 1)];
        int count = 0;
        NodeIndex node = root_;
        IndexType node_min = root_min_;
        bool has_first = false;  // 下りた先のnode全体が最初の候補か
        // min_leftで左の子へ下りるとき、左の子の末尾からboundaryまでの初期値は左の子より先に調べる
        IndexType lead_lo = 0, lead_hi = 0;
        while (true) {
            if (is_leaf(node)) {
                has_first = Forward ? boundary <= node_min : node_min < boundary;
                break;
            }
            const Branch& b = branch_node(node);
            if constexpr (Forward) {
                if (b.hi <= boundary) break;
                if (boundary <= node_min) {
                    has_first = true;
                    break;
                }
                if (boundary < b.split) {
                    pending[count++] = {b.right, b.split, b.hi};
                    pending[count++] = {null_node, node_hi(b.left, node_min), b.split};
                    node = b.left;
                } else {
                    node_min = b.split;
                    node = b.right;
                }
            } else {
                if (boundary <= node_min) break;
                if (b.hi <= boundary) {
                    has_first = true;
                    break;
                }
                const IndexType left_hi = node_hi(b.left, node_min);
                if (b.split < boundary) {
                    pending[count++] = {b.left, node_min, left_hi};
                    pending[count++] = {null_node, left_hi, b.split};
                    node_min = b.split;
                    node = b.right;
                } else {
                    node = b.left;
                    if (left_hi < boundary) {
                        lead_lo = left_hi;
                        lead_hi = boundary;
                        has_first = true;
                        break;
                    }
                }
            }
        }
        if constexpr (!Forward) {
            if (lead_lo < lead_hi && uniform_min_left(lead_lo, lead_hi, boundary, pred, acc, answer)) return true;
        }
        while (true) {
            if (has_first) {
                T candidate = Forward ? op(acc, node_data(node)) : op(node_data(node), acc);
                if (!pred(candidate)) {
                    answer = descend_failed<Forward>(node, node_min, boundary, pred, acc);
                    return true;
                }
                acc = move(candidate);
            }
            if (count == 0) return false;
            const Pending item = pending[--count];
            if (item.node == null_node) {
                const bool found = Forward ? uniform_max_right(item.lo, item.hi, boundary, pred, acc, answer)
                                           : uniform_min_left(item.lo, item.hi, boundary, pred, acc, answer);
                if (found) return true;
                has_first = false;
            } else {
                node = item.node;
                node_min = item.lo;
                has_first = true;
            }
        }
    }

public:
    /// @brief 長さ0の木を構築する / O(1)
    DynamicSegmentTreeInit() = default;

    /// @brief 全要素がinitの長さnの木を構築する / O(1)
    DynamicSegmentTreeInit(const IndexType n, T init) : n_(n), init_val_(move(init)) {
        assert(n >= 0);
    }

    /// @brief 独立した木としてコピーする / O(M_other)
    DynamicSegmentTreeInit(const DynamicSegmentTreeInit&) = default;

    /// @brief 現在の内容を独立したコピーで置き換える / O(M+M_other)
    DynamicSegmentTreeInit& operator=(const DynamicSegmentTreeInit&) = default;

    /// @brief 所有するノードを移し、移動元を空にする / O(1)
    DynamicSegmentTreeInit(DynamicSegmentTreeInit&& other)
        : leaves_(move(other.leaves_)), branches_(move(other.branches_)), n_(exchange(other.n_, 0)),
          root_(exchange(other.root_, null_node)), root_min_(exchange(other.root_min_, 0)), init_val_(move(other.init_val_)) {
        other.leaves_.clear();
        other.branches_.clear();
    }

    /// @brief 現在の内容を破棄してノードを移し、移動元を空にする / O(M)
    DynamicSegmentTreeInit& operator=(DynamicSegmentTreeInit&& other) {
        if (this != &other) {
            leaves_ = move(other.leaves_);
            branches_ = move(other.branches_);
            n_ = exchange(other.n_, 0);
            root_ = exchange(other.root_, null_node);
            root_min_ = exchange(other.root_min_, 0);
            init_val_ = move(other.init_val_);
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
        if (k < root_min_) return init_val_;
        NodeIndex node = root_;
        IndexType node_min = root_min_;
        while (node != null_node) {
            if (is_leaf(node)) return node_min == k ? leaves_[node >> 1].data : init_val_;
            const Branch& b = branch_node(node);
            if (b.hi <= k) break;
            if (k < b.split) node = b.left;
            else {
                node_min = b.split;
                node = b.right;
            }
        }
        return init_val_;
    }

    /// @brief k番目の値をvalueに置き換える / 償却O(logN)
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
        if (root_ == null_node) {
            uniform_max_right(l, n_, l, pred, acc, answer);
            return answer;
        }
        if (uniform_max_right(0, root_min_, l, pred, acc, answer)) return answer;
        if (scan_boundary<true>(l, pred, acc, answer)) return answer;
        uniform_max_right(node_hi(root_, root_min_), n_, l, pred, acc, answer);
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
        if (root_ == null_node) {
            uniform_min_left(0, r, r, pred, acc, answer);
            return answer;
        }
        if (uniform_min_left(node_hi(root_, root_min_), n_, r, pred, acc, answer)) return answer;
        if (scan_boundary<false>(r, pred, acc, answer)) return answer;
        uniform_min_left(0, root_min_, r, pred, acc, answer);
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
