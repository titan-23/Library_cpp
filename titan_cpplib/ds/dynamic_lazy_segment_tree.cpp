/// https://github.com/titan-23/Library_cpp/blob/main/titan_cpplib/ds/dynamic_lazy_segment_tree.cpp
#pragma once

#include <algorithm>
#include <cassert>
#include <climits>
#include <new>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <type_traits>
#include <utility>
#include <vector>

using namespace std;

namespace titan23 {

/// @brief 一様な区間の境界をPatricia木で管理する動的遅延セグメント木
/// 管理範囲は[0,N)で、composition(f,g)はgの後にfを適用する作用
/// pow(x,k)はxをk個結合した値を返し、pow(x,0)==e()を満たす
/// Nは列の長さ、Mは保持ノード数、M_otherはコピー元またはムーブ元のノード数
/// TとFの演算・コピー・ムーブ・破棄、pow、述語の評価はO(1)とする
/// ノード追加を含む計算量は償却計算量で、区間数Bに対してノード数は2B-1個
/// B=0ならノード数は0個で、同じ境界への更新と問い合わせはノードを追加しない
/// 葉と分岐を別々のプールに保持し、葉には1要素の値と区間長だけを持たせる
/// 分岐の右端は保存せず、下降時に親から持ち回る
/// 型の整列条件に合わせて小さい分岐配置を選び、余白があれば分岐ビットを保持する
/// ノード番号は0が欠損、奇数が葉、正の偶数が分岐で、右の子の最上位ビットが遅延フラグ
/// 保持ノード数の上限は2^31-1個
/// 探索スタックは添字型のビット幅だけ領域を確保し、生存する探索状態は最大O(logN)個
/// Tの等値比較は不要で、値が等しくなった隣接区間は自動結合しない
template <class IndexType, class T, T (*op)(T, T), T (*e)(),
          class F, T (*mapping)(F, T), F (*composition)(F, F), F (*id)(),
          T (*pow)(T, IndexType)>
class DynamicLazySegmentTree {
    using NodeId = uint32_t;
    static constexpr NodeId lazy_flag = NodeId(1) << 31;
    static constexpr NodeId none = 0;

    struct Leaf {
        T data;
        IndexType length;

        /// @brief 葉の区間長と1要素の値を初期化する / O(1)
        Leaf(IndexType length, T&& value) : data(move(value)), length(length) {}
    };

    struct BranchDataFirst {
        T data;
        F lazy;
        IndexType split;
        NodeId left, right_and_lazy;

        /// @brief 分岐の集約値と子を設定し、遅延作用を初期化する / O(1)
        BranchDataFirst(IndexType split, T&& value, NodeId left, NodeId right)
            : data(move(value)), lazy(id()), split(split), left(left), right_and_lazy(right) {}

        /// @brief 遅延フラグを除いた右の子の番号を返す / O(1)
        NodeId right() const { return right_and_lazy & (lazy_flag - 1); }

        /// @brief 子へ未伝播の作用を保持しているか判定する / O(1)
        bool has_lazy() const { return (right_and_lazy & lazy_flag) != 0; }
    };

    struct BranchFFirstStorage {
        F lazy;
        T data;
        IndexType split;
        NodeId left, right_and_lazy;

        /// @brief 作用を先頭に置いて分岐情報を初期化する / O(1)
        BranchFFirstStorage(IndexType split, T&& value, NodeId left, NodeId right)
            : lazy(id()), data(move(value)), split(split), left(left), right_and_lazy(right) {}
    };

    struct BranchIndexFirstStorage {
        IndexType split;
        T data;
        F lazy;
        NodeId left, right_and_lazy;

        /// @brief 座標を先頭に置いて分岐情報を初期化する / O(1)
        BranchIndexFirstStorage(IndexType split, T&& value, NodeId left, NodeId right)
            : split(split), data(move(value)), lazy(id()), left(left), right_and_lazy(right) {}
    };

    template <class Storage>
    struct BranchAlternative : Storage {
        using Storage::Storage;

        /// @brief 遅延フラグを除いた右の子の番号を返す / O(1)
        NodeId right() const { return this->right_and_lazy & (lazy_flag - 1); }

        /// @brief 子へ未伝播の作用を保持しているか判定する / O(1)
        bool has_lazy() const { return (this->right_and_lazy & lazy_flag) != 0; }
    };

    using BranchFFirst = BranchAlternative<BranchFFirstStorage>;
    using BranchIndexFirst = BranchAlternative<BranchIndexFirstStorage>;
    using SmallerAlternative = conditional_t<(sizeof(BranchFFirst) <= sizeof(BranchIndexFirst)),
                                              BranchFFirst, BranchIndexFirst>;
    // 同サイズなら値を先頭に置く配置を選び、既定型の配置と末尾の余白を維持する
    using BranchBase = conditional_t<(sizeof(SmallerAlternative) < sizeof(BranchDataFirst)),
                                     SmallerAlternative, BranchDataFirst>;

    struct BranchWithBit : BranchBase {
        unsigned char bit;

        /// @brief 分岐情報と生成後に変わらない分岐ビットを初期化する / O(1)
        BranchWithBit(IndexType split, T&& value, NodeId left, NodeId right, unsigned char bit)
            : BranchBase(split, move(value), left, right), bit(bit) {}
    };

    static constexpr bool cache_branch_bit = sizeof(BranchWithBit) == sizeof(BranchBase);
    using Branch = conditional_t<cache_branch_bit, BranchWithBit, BranchBase>;

    // サイズが64バイトの分岐だけ整列を強め、ノードのサイズは増やさない
    struct alignas(sizeof(Branch) == 64 ? 64 : alignof(Branch)) StoredBranch : Branch {
        using Branch::Branch;
    };

    struct Pending {
        NodeId node;
        IndexType key;
    };

    template <class U, class = void> struct HasEquality : false_type {};
    template <class U>
    struct HasEquality<U, void_t<decltype(bool(declval<const U&>() == declval<const U&>()))>> : true_type {};

    IndexType n_ = 0;
    T init_value = e();
    NodeId root = none;
    vector<Leaf> leaves;
    vector<StoredBranch> branches;

    /// @brief 番号が一様な区間を表す葉か判定する / O(1)
    static bool is_leaf(NodeId p) { return (p & 1) != 0; }

    /// @brief 葉の情報への参照を返す / O(1)
    Leaf& leaf(NodeId p) {
        assert(is_leaf(p));
        return leaves[p >> 1];
    }

    /// @brief 葉の情報への読み取り専用参照を返す / O(1)
    const Leaf& leaf(NodeId p) const {
        assert(is_leaf(p));
        return leaves[p >> 1];
    }

    /// @brief 分岐の情報への参照を返す / O(1)
    Branch& branch(NodeId p) {
        assert(p != none && !is_leaf(p));
        return branches[(p >> 1) - 1];
    }

    /// @brief 分岐の情報への読み取り専用参照を返す / O(1)
    const Branch& branch(NodeId p) const {
        assert(p != none && !is_leaf(p));
        return branches[(p >> 1) - 1];
    }

    /// @brief Fが等値比較できる場合に恒等作用か判定する / O(1)
    /// 等値比較できないFではfalseを返す
    static bool is_identity(const F& f) {
        if constexpr (HasEquality<F>::value) return bool(f == id());
        else return false;
    }

    /// @brief 最上位の1ビットの位置を返す / O(1)
    static unsigned char highest_bit(IndexType x) {
        assert(x != 0);
        if constexpr (sizeof(IndexType) <= sizeof(unsigned))
            return static_cast<unsigned char>(31 - __builtin_clz(static_cast<unsigned>(x)));
        else if constexpr (sizeof(IndexType) <= sizeof(unsigned long long))
            return static_cast<unsigned char>(63 - __builtin_clzll(static_cast<unsigned long long>(x)));
        else if constexpr (is_same_v<IndexType, __int128_t> || is_same_v<IndexType, __uint128_t>) {
            const auto value = static_cast<__uint128_t>(x);
            const auto high = static_cast<unsigned long long>(value >> 64);
            if (high) return static_cast<unsigned char>(127 - __builtin_clzll(high));
            return static_cast<unsigned char>(63 - __builtin_clzll(static_cast<unsigned long long>(value)));
        } else {
            unsigned char result = 0;
            while (x >>= 1) ++result;
            return result;
        }
    }

    /// @brief サイズを増やさず保存できる場合はキャッシュした分岐ビットを返す / O(1)
    unsigned char branch_bit(NodeId p, IndexType node_min) const {
        const Branch& b = branch(p);
        if constexpr (cache_branch_bit) {
            assert(b.bit == highest_bit(node_min ^ b.split));
            return b.bit;
        } else return highest_bit(node_min ^ b.split);
    }

    /// @brief 一様な区間を表す葉を追加する / 償却O(1)
    NodeId new_leaf(IndexType lo, IndexType hi, T value) {
        assert(leaves.size() < lazy_flag / 2);
        const NodeId result = (static_cast<NodeId>(leaves.size()) << 1) | 1;
        leaves.emplace_back(hi - lo, move(value));
        return result;
    }

    /// @brief ノードが表す区間全体の集約値を返す / O(1)
    T aggregate(NodeId p) const {
        if (!is_leaf(p)) return branch(p).data;
        const Leaf& a = leaf(p);
        return a.length == 1 ? a.data : pow(a.data, a.length);
    }

    /// @brief 左右の子から集約値を更新する / O(1)
    void pull(NodeId p) {
        Branch& b = branch(p);
        const NodeId right = b.right();
        b.data = op(aggregate(b.left), aggregate(right));
    }

    /// @brief 左右の部分木を結ぶ分岐を追加する / 償却O(1)
    NodeId new_branch(NodeId left, NodeId right, IndexType left_min, IndexType right_min) {
        assert(branches.size() < lazy_flag / 2 - 1);
        T value = op(aggregate(left), aggregate(right));
        const NodeId result = (static_cast<NodeId>(branches.size()) + 1) << 1;
        if constexpr (cache_branch_bit)
            branches.emplace_back(right_min, move(value), left, right, highest_bit(left_min ^ right_min));
        else branches.emplace_back(right_min, move(value), left, right);
        return result;
    }

    /// @brief ノード全体に作用を適用して遅延作用を合成する / O(1)
    void apply_node(NodeId p, const F& f) {
        if (is_leaf(p)) leaf(p).data = mapping(f, leaf(p).data);
        else {
            Branch& b = branch(p);
            b.data = mapping(f, b.data);
            b.lazy = b.has_lazy() ? composition(f, b.lazy) : f;
            b.right_and_lazy |= lazy_flag;
        }
    }

    /// @brief 保持している遅延作用を左右の子へ伝播する / O(1)
    void push(NodeId p) {
        Branch& b = branch(p);
        if (b.has_lazy()) {
            if constexpr (is_trivially_copy_constructible_v<F>) {
                const F f = b.lazy;
                apply_node(b.left, f);
                apply_node(b.right(), f);
            } else {
                apply_node(b.left, b.lazy);
                apply_node(b.right(), b.lazy);
            }
            b.right_and_lazy &= lazy_flag - 1;
        }
    }

    /// @brief 一様な区間を表す葉を部分木へ挿入する / O(logN)
    /// 先頭に挿入した場合はnode_minも更新する
    NodeId insert_run(NodeId p, IndexType& node_min, Pending added) {
        const IndexType key = added.key;
        if (is_leaf(p)) {
            if (key < node_min) {
                const NodeId result = new_branch(added.node, p, key, node_min);
                node_min = key;
                return result;
            }
            return new_branch(p, added.node, node_min, key);
        }
        const unsigned char bit = branch_bit(p, node_min);
        if (((key ^ node_min) >> bit) > 1) {
            if (key < node_min) {
                const NodeId result = new_branch(added.node, p, key, node_min);
                node_min = key;
                return result;
            }
            return new_branch(p, added.node, node_min, key);
        }
        push(p);
        if ((key >> bit) & 1) {
            IndexType right_min = branch(p).split;
            NodeId right = insert_run(branch(p).right(), right_min, added);
            branch(p).right_and_lazy = right;
            branch(p).split = right_min;
        } else {
            NodeId left = insert_run(branch(p).left, node_min, added);
            branch(p).left = left;
        }
        pull(p);
        return p;
    }

    /// @brief 必要な境界だけを分割して区間更新または点代入を行う / O(logN)
    template <bool Replace>
    NodeId inner_update(NodeId p, IndexType& node_min, IndexType node_hi, IndexType l, IndexType r,
                        const F& f, const T& value, Pending (&added)[2],
                        unsigned& count, unsigned char parent_bit) {
        if (l <= node_min && node_hi <= r) {
            if constexpr (Replace) {
                assert(is_leaf(p));
                leaf(p).data = value;
            } else apply_node(p, f);
            return p;
        }
        if (is_leaf(p)) {
            const IndexType lo = node_min, hi = lo + leaf(p).length;
            const IndexType a = max(l, lo), b = min(r, hi);
            T updated = [&]() {
                if constexpr (Replace) return value;
                else return mapping(f, leaf(p).data);
            }();
            Pending parts[2]{};
            unsigned parts_count = 0;
            if (a != lo) parts[parts_count++] = {new_leaf(a, b, move(updated)), a};
            if (b != hi) parts[parts_count++] = {new_leaf(b, hi, leaf(p).data), b};
            leaf(p).length = (a == lo ? b : a) - lo;
            if (a == lo) leaf(p).data = move(updated);
            NodeId result = p;
            for (unsigned i = 0; i < parts_count; ++i) {
                if (parent_bit == 255 || ((parts[i].key ^ lo) >> parent_bit) == 0) {
                    result = insert_run(result, node_min, parts[i]);
                } else {
                    assert(count < 2);
                    added[count++] = parts[i];
                }
            }
            return result;
        }
        push(p);
        const NodeId right = branch(p).right();
        const IndexType middle = branch(p).split;
        const unsigned char bit = branch_bit(p, node_min);
        if (r <= middle) {
            const NodeId child = inner_update<Replace>(branch(p).left, node_min, middle, l, r, f, value, added, count, bit);
            branch(p).left = child;
        } else if (middle <= l) {
            IndexType right_min = middle;
            const NodeId child = inner_update<Replace>(right, right_min, node_hi, l, r, f, value, added, count, bit);
            branch(p).right_and_lazy = child;
            branch(p).split = right_min;
        } else {
            NodeId child = inner_update<Replace>(branch(p).left, node_min, middle, l, r, f, value, added, count, bit);
            branch(p).left = child;
            IndexType right_min = middle;
            child = inner_update<Replace>(right, right_min, node_hi, l, r, f, value, added, count, bit);
            branch(p).right_and_lazy = child;
            branch(p).split = right_min;
        }
        pull(p);
        unsigned kept = 0;
        for (unsigned i = 0; i < count; ++i) {
            if (parent_bit == 255 || ((added[i].key ^ node_min) >> parent_bit) == 0) {
                p = insert_run(p, node_min, added[i]);
            } else added[kept++] = added[i];
        }
        count = kept;
        return p;
    }

    /// @brief 読み取り中に持ち回る作用へノードの遅延作用を合成する / O(1)
    void carry_lazy(NodeId p, F& carry, bool& has_carry) const {
        const Branch& b = branch(p);
        if (b.has_lazy()) {
            carry = has_carry ? composition(carry, b.lazy) : b.lazy;
            has_carry = true;
        }
    }

    /// @brief 部分木の先頭からr未満の範囲を集約する / O(logN)
    T prefix_prod(NodeId p, IndexType node_min, IndexType node_hi, IndexType r, F carry, bool has_carry) const {
        T acc = e();
        while (true) {
            if (is_leaf(p) || node_hi <= r) {
                T part = is_leaf(p) ? pow(leaf(p).data, r - node_min) : branch(p).data;
                if (has_carry) part = mapping(carry, part);
                return op(acc, part);
            }
            carry_lazy(p, carry, has_carry);
            if (r <= branch(p).split) {
                node_hi = branch(p).split;
                p = branch(p).left;
            }
            else {
                T part = aggregate(branch(p).left);
                if (has_carry) part = mapping(carry, part);
                acc = op(acc, part);
                node_min = branch(p).split;
                p = branch(p).right();
            }
        }
    }

    /// @brief l以上から部分木の末尾までを集約する / O(logN)
    T suffix_prod(NodeId p, IndexType node_min, IndexType l, F carry, bool has_carry) const {
        T acc = e();
        while (true) {
            if (is_leaf(p) || l <= node_min) {
                T part = is_leaf(p) ? pow(leaf(p).data, node_min + leaf(p).length - l) : branch(p).data;
                if (has_carry) part = mapping(carry, part);
                return op(part, acc);
            }
            carry_lazy(p, carry, has_carry);
            if (branch(p).split <= l) {
                node_min = branch(p).split;
                p = branch(p).right();
            } else {
                T part = aggregate(branch(p).right());
                if (has_carry) part = mapping(carry, part);
                acc = op(part, acc);
                p = branch(p).left;
            }
        }
    }

    /// @brief 部分木内の[l,r)を遅延伝播せずに集約する / O(logN)
    T inner_prod(NodeId p, IndexType node_min, IndexType node_hi, IndexType l, IndexType r, F carry, bool has_carry) const {
        while (true) {
            if (is_leaf(p) || (l <= node_min && node_hi <= r)) {
                T part = is_leaf(p) ? pow(leaf(p).data, r - l) : branch(p).data;
                return has_carry ? mapping(carry, part) : part;
            }
            carry_lazy(p, carry, has_carry);
            const IndexType mid = branch(p).split;
            if (r <= mid) {
                node_hi = mid;
                p = branch(p).left;
            }
            else if (mid <= l) {
                node_min = mid;
                p = branch(p).right();
            }
            else {
                T lv = suffix_prod(branch(p).left, node_min, l, carry, has_carry);
                T rv = prefix_prod(branch(p).right(), mid, node_hi, r, move(carry), has_carry);
                return op(lv, rv);
            }
        }
    }

    /// @brief 一様な区間で判定を満たす長さを二分探索する / O(logN)
    template <bool Forward, class Pred>
    IndexType search_uniform(const T& key, IndexType length, const T& acc, Pred& pred) const {
        IndexType good = 0, bad = length;
        while (bad - good > 1) {
            const IndexType mid = good + (bad - good) / 2;
            T part = pow(key, mid);
            T next = [&]() {
                if constexpr (Forward) return op(acc, part);
                else return op(part, acc);
            }();
            if (pred(next)) good = mid;
            else bad = mid;
        }
        return good;
    }

    /// @brief 全体では判定が失敗する部分木から境界の位置を求める / O(logN)
    template <bool Forward, class Pred>
    IndexType descend_failed(NodeId p, IndexType node_min, F carry, bool has_carry, T& acc, Pred& pred) const {
        while (!is_leaf(p)) {
            carry_lazy(p, carry, has_carry);
            const Branch& b = branch(p);
            const NodeId first = Forward ? b.left : b.right();
            T part = aggregate(first);
            if (has_carry) part = mapping(carry, part);
            T next = [&]() {
                if constexpr (Forward) return op(acc, part);
                else return op(part, acc);
            }();
            if (pred(next)) {
                acc = move(next);
                if constexpr (Forward) {
                    node_min = b.split;
                    p = b.right();
                }
                else p = b.left;
            } else {
                if constexpr (!Forward) node_min = b.split;
                p = first;
            }
        }
        const T key = has_carry ? mapping(carry, leaf(p).data) : leaf(p).data;
        const IndexType accepted = search_uniform<Forward>(key, leaf(p).length, acc, pred);
        if constexpr (Forward) return node_min + accepted;
        else return node_min + leaf(p).length - accepted;
    }

    struct SearchFrame {
        IndexType node_min;
        NodeId node;
        bool has_carry;
        F carry;
    };

    /// Pは保存している探索状態数で、Fの既定構築を要求せず必要な状態だけを構築する
    class SearchStack {
        alignas(SearchFrame) unsigned char storage[sizeof(SearchFrame) * sizeof(IndexType) * CHAR_BIT];
        unsigned count = 0;

        /// @brief 構築済みの探索状態へのポインタを返す / O(1)
        SearchFrame* entry(unsigned i) {
            return launder(reinterpret_cast<SearchFrame*>(storage + i * sizeof(SearchFrame)));
        }

    public:
        /// @brief 探索状態を構築せず空のスタックを用意する / O(1)
        SearchStack() = default;

        /// @brief 探索状態のコピー構築を禁止する
        SearchStack(const SearchStack&) = delete;

        /// @brief 探索状態のコピー代入を禁止する
        SearchStack& operator=(const SearchStack&) = delete;

        /// @brief 保存している探索状態を全て破棄する / O(P)
        ~SearchStack() {
            while (count != 0) entry(--count)->~SearchFrame();
        }

        /// @brief 探索状態が残っていないか判定する / O(1)
        bool empty() const { return count == 0; }

        /// @brief 未初期化領域に探索状態を構築して追加する / O(1)
        void push(NodeId node, IndexType node_min, const F& carry, bool has_carry) {
            assert(count < sizeof(IndexType) * CHAR_BIT);
            ::new (static_cast<void*>(storage + count * sizeof(SearchFrame))) SearchFrame{node_min, node, has_carry, carry};
            ++count;
        }

        /// @brief 末尾の探索状態を取り出して破棄する / O(1)
        void pop(NodeId& node, IndexType& node_min, F& carry, bool& has_carry) {
            SearchFrame* frame = entry(count - 1);
            carry = move(frame->carry);
            node = frame->node;
            node_min = frame->node_min;
            has_carry = frame->has_carry;
            frame->~SearchFrame();
            --count;
        }
    };

    /// @brief 開始位置まで降り、保存した部分木を順に調べて境界を探す / O(logN)
    template <bool Forward, class Pred>
    IndexType scan_boundary(NodeId p, IndexType node_min, IndexType node_hi, IndexType boundary,
                            F carry, bool has_carry, T& acc, Pred& pred) const {
        const IndexType end = Forward ? node_hi : node_min;
        SearchStack pending;
        while (!is_leaf(p) && (Forward ? node_min < boundary : boundary < node_hi)) {
            carry_lazy(p, carry, has_carry);
            const IndexType split = branch(p).split;
            if constexpr (Forward) {
                if (boundary < split) {
                    pending.push(branch(p).right(), split, carry, has_carry);
                    node_hi = split;
                    p = branch(p).left;
                } else {
                    node_min = split;
                    p = branch(p).right();
                }
            } else {
                if (split < boundary) {
                    pending.push(branch(p).left, node_min, carry, has_carry);
                    node_min = split;
                    p = branch(p).right();
                } else {
                    node_hi = split;
                    p = branch(p).left;
                }
            }
        }
        if (is_leaf(p)) {
            const IndexType lo = Forward ? max(boundary, node_min) : node_min;
            const IndexType hi = Forward ? node_hi : min(boundary, node_hi);
            const T key = has_carry ? mapping(carry, leaf(p).data) : leaf(p).data;
            T part = pow(key, hi - lo);
            T next = [&]() {
                if constexpr (Forward) return op(acc, part);
                else return op(part, acc);
            }();
            if (!pred(next)) {
                const IndexType accepted = search_uniform<Forward>(key, hi - lo, acc, pred);
                if constexpr (Forward) return lo + accepted;
                else return hi - accepted;
            }
            acc = move(next);
        } else {
            T part = has_carry ? mapping(carry, branch(p).data) : branch(p).data;
            T next = [&]() {
                if constexpr (Forward) return op(acc, part);
                else return op(part, acc);
            }();
            if (!pred(next)) return descend_failed<Forward>(p, node_min, move(carry), has_carry, acc, pred);
            acc = move(next);
        }
        while (!pending.empty()) {
            pending.pop(p, node_min, carry, has_carry);
            T part = aggregate(p);
            if (has_carry) part = mapping(carry, part);
            T next = [&]() {
                if constexpr (Forward) return op(acc, part);
                else return op(part, acc);
            }();
            if (!pred(next)) return descend_failed<Forward>(p, node_min, move(carry), has_carry, acc, pred);
            acc = move(next);
        }
        return end;
    }

    /// @brief 根から右端の境界を探す / O(logN)
    template <class Pred>
    IndexType inner_max_right(NodeId p, IndexType node_min, IndexType l, F carry, bool has_carry,
                             T& acc, Pred& pred) const {
        return scan_boundary<true>(p, node_min, n_, l, move(carry), has_carry, acc, pred);
    }

    /// @brief 根から左端の境界を探す / O(logN)
    template <class Pred>
    IndexType inner_min_left(NodeId p, IndexType node_min, IndexType r, F carry, bool has_carry,
                            T& acc, Pred& pred) const {
        return scan_boundary<false>(p, node_min, n_, r, move(carry), has_carry, acc, pred);
    }

public:
    /// @brief 長さ0の木を構築する / O(1)
    DynamicLazySegmentTree() = default;

    /// @brief 全要素が単位元の長さnの木を構築する / O(1)
    DynamicLazySegmentTree(IndexType n) : DynamicLazySegmentTree(n, e()) {}

    /// @brief 全要素がinitの長さnの木を構築する / O(1)
    DynamicLazySegmentTree(IndexType n, T init) : n_(n), init_value(move(init)) {
        assert(n >= 0);
        if (n_ > 0) root = new_leaf(0, n_, init_value);
    }

    /// @brief 独立した木としてコピーする / O(M_other)
    DynamicLazySegmentTree(const DynamicLazySegmentTree&) = default;

    /// @brief 現在の内容を独立したコピーで置き換える / O(M+M_other)
    DynamicLazySegmentTree& operator=(const DynamicLazySegmentTree&) = default;

    /// @brief 所有するノードを移し、移動元を空にする / O(1)
    DynamicLazySegmentTree(DynamicLazySegmentTree&& other)
        : n_(exchange(other.n_, 0)), init_value(move(other.init_value)), root(exchange(other.root, none)),
          leaves(move(other.leaves)), branches(move(other.branches)) {
        other.leaves.clear();
        other.branches.clear();
    }

    /// @brief 現在の内容を破棄してノードを移し、移動元を空にする / O(M)
    DynamicLazySegmentTree& operator=(DynamicLazySegmentTree&& other) {
        if (this != &other) {
            n_ = exchange(other.n_, 0);
            init_value = move(other.init_value);
            root = exchange(other.root, none);
            leaves = move(other.leaves);
            branches = move(other.branches);
            other.leaves.clear();
            other.branches.clear();
        }
        return *this;
    }

    /// @brief 列の長さを返す / O(1)
    IndexType size() const { return n_; }

    /// @brief 保持しているノード数を返す / O(1)
    size_t node_count() const { return leaves.size() + branches.size(); }

    /// @brief ノードプールの容量を予約する / O(M) 再確保しなければO(1)
    void reserve_nodes(size_t capacity) {
        assert(capacity < lazy_flag);
        leaves.reserve((capacity + 1) / 2);
        branches.reserve(capacity / 2);
    }

    /// @brief 長さと確保済み容量を維持して構築時の初期値に戻す / O(M)
    void clear() {
        leaves.clear();
        branches.clear();
        root = n_ > 0 ? new_leaf(0, n_, init_value) : none;
    }

    /// @brief 列全体の集約値を返す / O(1)
    T all_prod() const { return root == none ? e() : aggregate(root); }

    /// @brief 半開区間[l,r)の集約値を返す / O(logN)
    T prod(IndexType l, IndexType r) const {
        assert(0 <= l && l <= r && r <= n_);
        return l == r ? e() : inner_prod(root, 0, n_, l, r, id(), false);
    }

    /// @brief k番目の値を返す / O(logN)
    T get(IndexType k) const {
        assert(0 <= k && k < n_);
        NodeId p = root;
        F carry = id();
        bool has_carry = false;
        while (!is_leaf(p)) {
            carry_lazy(p, carry, has_carry);
            p = k < branch(p).split ? branch(p).left : branch(p).right();
        }
        return has_carry ? mapping(carry, leaf(p).data) : leaf(p).data;
    }

    /// @brief k番目の値をvalueに置き換える / O(logN)
    void set(IndexType k, T value) {
        assert(0 <= k && k < n_);
        Pending added[2]{};
        unsigned count = 0;
        IndexType root_min = 0;
        root = inner_update<true>(root, root_min, n_, k, k + 1, id(), value, added, count, 255);
        for (unsigned i = 0; i < count; ++i) root = insert_run(root, root_min, added[i]);
    }

    /// @brief 半開区間[l,r)の各要素にfを適用する / O(logN)
    void apply(IndexType l, IndexType r, F f) {
        assert(0 <= l && l <= r && r <= n_);
        if (l == r || is_identity(f)) return;
        Pending added[2]{};
        unsigned count = 0;
        IndexType root_min = 0;
        root = inner_update<false>(root, root_min, n_, l, r, f, init_value, added, count, 255);
        for (unsigned i = 0; i < count; ++i) root = insert_run(root, root_min, added[i]);
    }

    /// @brief k番目の値にfを適用する / O(logN)
    void apply(IndexType k, F f) {
        assert(0 <= k && k < n_);
        apply(k, k + 1, f);
    }

    /// @brief 全要素にfを適用する / O(1)
    void all_apply(F f) {
        if (root != none && !is_identity(f)) apply_node(root, f);
    }

    /// @brief pred(prod(l,r))が真となる最大のrを求める / O(logN)
    /// pred(e())==trueと、区間を右へ伸ばしたときの判定の単調性が必要
    template <class Pred>
    IndexType max_right(IndexType l, Pred&& pred) const {
        assert(0 <= l && l <= n_);
        assert(pred(e()));
        if (l == n_) return n_;
        T acc = e();
        return inner_max_right(root, 0, l, id(), false, acc, pred);
    }

    /// @brief pred(prod(l,r))が真となる最小のlを求める / O(logN)
    /// pred(e())==trueと、区間を左へ伸ばしたときの判定の単調性が必要
    template <class Pred>
    IndexType min_left(IndexType r, Pred&& pred) const {
        assert(0 <= r && r <= n_);
        assert(pred(e()));
        if (r == 0) return 0;
        T acc = e();
        return inner_min_left(root, 0, r, id(), false, acc, pred);
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
