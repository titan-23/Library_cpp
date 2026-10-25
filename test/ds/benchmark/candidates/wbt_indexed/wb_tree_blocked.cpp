/// https://github.com/titan-23/Library_cpp/blob/main/titan_cpplib/ds/wb_tree.cpp
#pragma once

#include <cmath>
#include <iostream>
#include <vector>
#include "_mutable_wbt_sequence_indexed.cpp"
using namespace std;

const double ALPHA = 1 - sqrt(2) / 2;
const double BETA = (1 - 2 * ALPHA) / (1 - ALPHA);

namespace titan23 {

template<class T>
class WBTree : private detail::MutableWBSequenceIndexed<T, nullptr, nullptr, false> {
    using Base = detail::MutableWBSequenceIndexed<T, nullptr, nullptr, false>;
    using NodePtr = typename Base::NodePtr;

    static NodePtr build_root(const vector<T>& values) {
        auto& storage = Base::node_pool();
        const size_t needed = storage.size() + values.size();
        if (needed > storage.capacity()) {
            size_t capacity = std::max<size_t>(2, storage.capacity());
            while (capacity < needed) capacity *= 2;
            storage.reserve(capacity);
        }
        return Base::build_nodes(values, 0, static_cast<int>(values.size()));
    }

    explicit WBTree(NodePtr& node) : root(node) {}

public:
    NodePtr root;

    /// @brief 空の列を構築する / O(1)
    WBTree() : root(nullptr) {}

    /// @brief 配列 a から構築する / O(N)
    WBTree(const vector<T>& a) : root(build_root(a)) {}

    /// @brief 配列 a から列を再構築する / O(N)
    void build(vector<T> a) { root = build_root(a); }

    /// @brief other を末尾に連結する / O(log(N + M))
    /// @note 破壊的操作のため、連結後の other は独立に操作できない
    void merge(WBTree& other) {
        assert(!root || root != other.root);
        root = Base::merge_nodes(root, other.root);
    }

    /// @brief 先頭 k 要素と残りに分割して返す / O(log N)
    /// @note 破壊的操作のため、分割前の列は独立に操作できない
    pair<WBTree, WBTree> split(int k) {
        assert(0 <= k && k <= len());
        auto result = Base::split_nodes(root, k);
        return {WBTree(result.first), WBTree(result.second)};
    }

    /// @brief 位置 k に値 key を挿入する / O(log N)
    void insert(int k, const T key) {
        assert(0 <= k && k <= len());
        root = Base::insert_node(root, k, key);
    }

    /// @brief 末尾に値 key を追加する / O(log N)
    void emplace_back(T key) { insert(len(), key); }

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
    T operator[](int k) const {
        assert(0 <= k && k < len());
        return Base::find_node(root, k)->key;
    }

    /// @brief 位置 k の値への参照を返す / O(log N)
    T& operator[](int k) {
        assert(0 <= k && k < len());
        return Base::find_node(root, k)->key;
    }

    /// @brief 列を空にする / O(1)
    /// @note このハンドルのみ初期化し、共有ノードは解放しない
    void clear() { root = nullptr; }

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
    void isok() const { Base::check_node(root); }

    /// @brief 全要素を標準出力に出力する / O(N)
    void print() const {
        const auto values = tovector();
        cout << '[';
        for (size_t i = 0; i < values.size(); ++i) {
            if (i) cout << ", ";
            cout << values[i];
        }
        cout << ']' << endl;
    }
};
} // namespace titan23
