/// https://github.com/titan-23/Library_cpp/blob/main/titan_cpplib/ds/wb_tree_seg.cpp
#pragma once

#include <iostream>
#include <vector>
#include "_mutable_wbt_sequence.cpp"
#include "titan_cpplib/others/print.cpp"
using namespace std;

namespace titan23 {

template<class T, T (*op)(T, T), T (*e)()>
class WBSegTree : private detail::MutableWBSequence<T, op, e> {
    using Base = detail::MutableWBSequence<T, op, e>;
    using NodePtr = typename Base::NodePtr;

    explicit WBSegTree(NodePtr& node) : root(node) {}

public:
    NodePtr root;

    /// @brief 空の列を構築する / O(1)
    WBSegTree() : root(nullptr) {}

    /// @brief 配列 a から構築する / O(N)
    WBSegTree(const vector<T>& a) : root(Base::build_nodes(a, 0, static_cast<int>(a.size()))) {}

    /// @brief other を末尾に連結する / O(log(N + M))
    /// @note 破壊的操作のため、連結後の other は独立に操作できない
    void merge(WBSegTree& other) {
        assert(!root || root != other.root);
        root = Base::merge_nodes(root, other.root);
    }

    /// @brief 先頭 k 要素と残りに分割して返す / O(log N)
    /// @note 破壊的操作のため、分割前の列は独立に操作できない
    pair<WBSegTree, WBSegTree> split(int k) {
        assert(0 <= k && k <= len());
        auto result = Base::split_nodes(root, k);
        return {WBSegTree(result.first), WBSegTree(result.second)};
    }

    /// @brief 列全体の総積を返す / O(1)
    T all_prod() const { return root ? root->data : e(); }

    /// @brief 区間 [l, r) の総積を返す / O(log N)
    T prod(int l, int r) const {
        assert(0 <= l && l <= r && r <= len());
        return l == r ? e() : Base::prod_node(root, l, r);
    }

    /// @brief 位置 k に値 key を挿入する / O(log N)
    void insert(int k, const T key) {
        assert(0 <= k && k <= len());
        root = Base::insert_node(root, k, key);
    }

    /// @brief 位置 k の値を削除して返す / O(log N)
    T pop(int k) {
        assert(0 <= k && k < len());
        T result;
        root = Base::erase_node(root, k, result);
        return result;
    }

    /// @brief 位置 k の値を value に変更する / O(log N)
    void set(int k, T value) {
        assert(0 <= k && k < len());
        Base::set_node(root, k, value);
    }

    /// @brief 位置 k の値を返す / O(log N)
    T get(int k) const {
        assert(0 <= k && k < len());
        return Base::find_node(root, k)->key;
    }

    /// @brief 全要素を配列にして返す / O(N)
    vector<T> tovector() const {
        vector<T> values;
        values.reserve(len());
        Base::append_values(root, values);
        return values;
    }

    /// @brief 要素数を返す / O(1)
    int len() const { return Base::size_of(root); }

    /// @brief 部分木サイズと平衡条件を検査する / O(N)
    void check() const { Base::check_node(root); }

    /// @brief 全要素を標準出力に出力する / O(N)
    void print() const { cout << *this << endl; }

    /// @brief 全要素をストリームへ出力する / O(N)
    friend ostream& operator<<(ostream& out, const WBSegTree& tree) {
        const auto values = tree.tovector();
        out << '[';
        for (size_t i = 0; i < values.size(); ++i) {
            if (i) out << ", ";
            out << values[i];
        }
        return out << ']';
    }
};
} // namespace titan23
