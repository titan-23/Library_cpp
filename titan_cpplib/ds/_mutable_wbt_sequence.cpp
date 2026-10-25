#pragma once

#include <algorithm>
#include <cassert>
#include <utility>
#include <vector>
#include "titan_cpplib/ds/_wbt_node_pool.cpp"

namespace titan23::detail {

template<class T, bool Aggregate> struct WBAggregate {
    /// @brief 集約値を持たない補助オブジェクトを構築する / O(1)
    explicit WBAggregate(const T&) {}
};

template<class T> struct WBAggregate<T, true> {
    T data;

    /// @brief 値 value を集約値として保持する / O(1)
    explicit WBAggregate(const T& value) : data(value) {}
};

template<class T, T (*Op)(T, T) = nullptr, T (*E)() = nullptr>
class MutableWBSequence {
protected:
    static constexpr bool aggregate = Op != nullptr;
    static constexpr int DELTA = 3, GAMMA = 2;
    struct Node : WBAggregate<T, aggregate> {
        T key;
        Node *left = nullptr, *right = nullptr;
        int size = 1, left_size = 0;

        /// @brief 値 value を持つ葉ノードを構築する / O(1)
        explicit Node(const T& value) : WBAggregate<T, aggregate>(value), key(value) {}

        /// @brief 子の情報から部分木サイズと集約値を再計算する / O(1)
        void update() {
            left_size = left ? left->size : 0;
            size = 1 + left_size + (right ? right->size : 0);
            if constexpr (aggregate) {
                this->data = left ? Op(left->data, key) : key;
                if (right) this->data = Op(this->data, right->data);
            }
        }

        /// @brief 子の情報から部分木サイズと集約値を再計算する / O(1)
        void _update() { update(); }

        /// @brief 左部分木の重み（要素数 + 1）を返す / O(1)
        int weight_left() const { return left_size + 1; }

        /// @brief 右部分木の重み（要素数 + 1）を返す / O(1)
        int weight_right() const { return size - left_size; }

        /// @brief 左部分木の重みが占める割合を返す / O(1)
        double balance() const { return double(left_size + 1) / (size + 1.0); }

        /// @brief このノードの左右の重みが平衡条件を満たすか検査する / O(1)
        void balance_check() const {
            assert(1LL * weight_left() * DELTA >= weight_right());
            assert(1LL * weight_right() * DELTA >= weight_left());
        }
    };
    using NodePtr = Node*;
    inline static WBNodePool<Node> pool;

    static int size_of(NodePtr node) { return node ? node->size : 0; }

    static int weight(NodePtr node) { return size_of(node) + 1; }

    static NodePtr rotate_left(NodePtr node) {
        NodePtr pivot = node->right;
        node->right = pivot->left;
        pivot->left = node;
        node->update(); pivot->update();
        return pivot;
    }

    static NodePtr rotate_right(NodePtr node) {
        NodePtr pivot = node->left;
        node->left = pivot->right;
        pivot->right = node;
        node->update(); pivot->update();
        return pivot;
    }

    static NodePtr balance(NodePtr node) {
        if (1LL * node->weight_left() * DELTA < node->weight_right()) {
            NodePtr child = node->right;
            if (child->weight_left() >= 1LL * child->weight_right() * GAMMA)
                node->right = rotate_right(child);
            return rotate_left(node);
        }
        if (1LL * node->weight_right() * DELTA < node->weight_left()) {
            NodePtr child = node->left;
            if (child->weight_right() >= 1LL * child->weight_left() * GAMMA)
                node->left = rotate_left(child);
            return rotate_right(node);
        }
        return node;
    }

    static NodePtr join_root(NodePtr left, NodePtr pivot, NodePtr right) {
        if (weight(left) > 1LL * weight(right) * DELTA) {
            left->right = join_root(left->right, pivot, right);
            left->update();
            return balance(left);
        }
        if (weight(right) > 1LL * weight(left) * DELTA) {
            right->left = join_root(left, pivot, right->left);
            right->update();
            return balance(right);
        }
        pivot->left = left; pivot->right = right;
        pivot->update();
        return pivot;
    }

    static std::pair<NodePtr, NodePtr> extract(NodePtr node, bool rightmost) {
        NodePtr next = rightmost ? node->right : node->left;
        if (!next) {
            NodePtr rest = rightmost ? node->left : node->right;
            node->left = node->right = nullptr;
            node->update();
            return {rest, node};
        }
        auto result = extract(next, rightmost);
        if (rightmost) node->right = result.first;
        else node->left = result.first;
        node->update();
        return {balance(node), result.second};
    }

    static NodePtr merge_nodes(NodePtr left, NodePtr right) {
        if (!left) return right;
        if (!right) return left;
        if (left->size > right->size) {
            auto result = extract(right, false);
            return join_root(left, result.second, result.first);
        }
        auto result = extract(left, true);
        return join_root(result.first, result.second, right);
    }

    static std::pair<NodePtr, NodePtr> split_nodes(NodePtr node, int k) {
        if (k == 0) return {nullptr, node};
        if (k == size_of(node)) return {node, nullptr};
        NodePtr left = node->left, right = node->right;
        if (k <= node->left_size) {
            auto result = split_nodes(left, k);
            return {result.first, join_root(result.second, node, right)};
        }
        auto result = split_nodes(right, k-node->left_size-1);
        return {join_root(left, node, result.first), result.second};
    }

    static NodePtr insert_node(NodePtr node, int k, const T& value) {
        if (!node) return pool.emplace(value);
        if (k <= node->left_size) node->left = insert_node(node->left, k, value);
        else node->right = insert_node(node->right, k-node->left_size-1, value);
        node->update();
        return balance(node);
    }

    static NodePtr erase_node(NodePtr node, int k, T& value) {
        if (k < node->left_size) node->left = erase_node(node->left, k, value);
        else if (k > node->left_size) node->right = erase_node(node->right, k-node->left_size-1, value);
        else {
            value = node->key;
            if (!node->left || !node->right) {
                NodePtr child = node->left ? node->left : node->right;
                pool.release(node);
                return child;
            }
            const bool from_left = node->left_size >= node->size-node->left_size-1;
            auto result = extract(from_left ? node->left : node->right, from_left);
            node->key = result.second->key;
            if (from_left) node->left = result.first;
            else node->right = result.first;
            pool.release(result.second);
        }
        node->update();
        return balance(node);
    }

    static NodePtr find_node(NodePtr node, int k) {
        while (k != node->left_size) {
            if (k < node->left_size) node = node->left;
            else { k -= node->left_size + 1; node = node->right; }
        }
        return node;
    }

    static void set_node(NodePtr node, int k, const T& value) {
        if constexpr (!aggregate) {
            find_node(node, k)->key = value;
        } else {
            if (k == node->left_size) node->key = value;
            else if (k < node->left_size) set_node(node->left, k, value);
            else set_node(node->right, k-node->left_size-1, value);
            node->update();
        }
    }

    static T prod_node(NodePtr node, int l, int r) {
        if (l == 0 && r == node->size) return node->data;
        const int left_size = node->left_size;
        if (r <= left_size) return prod_node(node->left, l, r);
        if (l > left_size) return prod_node(node->right, l-left_size-1, r-left_size-1);
        T result = node->key;
        if (l < left_size) result = Op(prod_node(node->left, l, left_size), result);
        if (left_size+1 < r) result = Op(result, prod_node(node->right, 0, r-left_size-1));
        return result;
    }

    static NodePtr build_nodes(const std::vector<T>& values, int l, int r) {
        if (l == r) return nullptr;
        const int mid = l + (r-l)/2;
        NodePtr node = pool.emplace(values[mid]);
        node->left = build_nodes(values, l, mid);
        node->right = build_nodes(values, mid+1, r);
        node->update();
        return node;
    }

    static void append_values(NodePtr node, std::vector<T>& values) {
        if (!node) return;
        append_values(node->left, values);
        values.push_back(node->key);
        append_values(node->right, values);
    }

    static int check_node(NodePtr node) {
        if (!node) return 0;
        const int ls = check_node(node->left), rs = check_node(node->right);
        assert(node->left_size == ls && node->size == ls+rs+1);
        node->balance_check();
        return node->size;
    }
};

} // namespace titan23::detail
