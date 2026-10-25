/// https://github.com/titan-23/Library_cpp/blob/main/titan_cpplib/ds/lazy_wb_tree.cpp
#pragma once

#include <algorithm>
#include <cassert>
#include <iostream>
#include <utility>
#include <vector>
#include "_wbt_indexed_pool.cpp"
using namespace std;

namespace titan23 {

template <class T, class F,
          T (*op)(T, T), T (*mapping)(F, T), F (*composition)(F, F),
          T (*e)(), F (*id)()>
class LazyWBTree {
private:
    class Node;
    using NodePtr = detail::WBIndexHandle<Node, false>;
    using MyLazyWBTree = LazyWBTree<T, F, op, mapping, composition, e, id>;
    static constexpr int DELTA = 3;
    static constexpr int GAMMA = 2;

    class Node {
    public:
        NodePtr left, right;
        T key, data;
        F lazy;
        bool rev;
        int size, left_size;

        /// @brief 1要素のノードを初期化する / O(1)
        Node(const T &value, const F &action)
            : left(nullptr), right(nullptr), key(value), data(value), lazy(action),
              rev(false), size(1), left_size(0) {}

        /// @brief 右の子の部分木サイズに1を加えた重みを返す / O(1)
        int weight_right() const { return size - left_size; }

        /// @brief 左の子の部分木サイズに1を加えた重みを返す / O(1)
        int weight_left() const { return left_size + 1; }

        /// @brief このノードの左右の重みが平衡条件を満たすか検査する / O(1)
        void balance_check() const {
            assert(static_cast<long long>(weight_left()) * DELTA >= weight_right());
            assert(static_cast<long long>(weight_right()) * DELTA >= weight_left());
        }

        /// @brief 子の情報から部分木サイズと総積を更新する / O(1)
        void update() {
            left_size = left ? left->size : 0;
            size = left_size + 1 + (right ? right->size : 0);
            data = left ? op(left->data, key) : key;
            if (right) data = op(data, right->data);
        }

        /// @brief この部分木に作用を適用し、遅延情報へ合成する / O(1)
        void apply(const F &action) {
            key = mapping(action, key);
            data = mapping(action, data);
            lazy = composition(action, lazy);
        }

        /// @brief 遅延作用と反転情報を子へ伝播する / O(1)
        void propagate() {
            if (rev) {
                swap(left, right);
                left_size = size - 1 - left_size;
                if (left) left->rev ^= 1;
                if (right) right->rev ^= 1;
                rev = false;
            }
            const F identity = id();
            if (lazy != identity) {
                if (left) left->apply(lazy);
                if (right) right->apply(lazy);
                lazy = identity;
            }
        }

        /// @brief このノード以下の構造を標準出力へ表示する / O(N)
        void print() const {
            cout << "this : key=" << key << ", size=" << size << endl;
            if (left) cout << "to-left" << endl;
            if (right) cout << "to-right" << endl;
            cout << endl;
            if (left) left->print();
            if (right) right->print();
        }
    };

    static auto &node_pool() { return NodePtr::storage(); }

    static int weight(NodePtr node) { return node ? node->size + 1 : 1; }

    void _build(const vector<T> &values) {
        auto build = [&](auto &&self, int l, int r) -> NodePtr {
            const int mid = l + (r-l)/2;
            NodePtr node = node_pool().emplace(values[mid], id());
            if (l < mid) node->left = self(self, l, mid);
            if (mid+1 < r) node->right = self(self, mid+1, r);
            node->update();
            return node;
        };
        root = values.empty() ? nullptr : build(build, 0, static_cast<int>(values.size()));
    }

    static NodePtr _rotate_right(NodePtr node) {
        NodePtr pivot = node->left;
        node->left = pivot->right;
        pivot->right = node;
        node->update();
        pivot->update();
        return pivot;
    }

    static NodePtr _rotate_left(NodePtr node) {
        NodePtr pivot = node->right;
        node->right = pivot->left;
        pivot->left = node;
        node->update();
        pivot->update();
        return pivot;
    }

    static NodePtr _balance_left(NodePtr node) {
        NodePtr child = node->right;
        child->propagate();
        if (child->weight_left() >= static_cast<long long>(child->weight_right()) * GAMMA) {
            child->left->propagate();
            node->right = _rotate_right(child);
        }
        return _rotate_left(node);
    }

    static NodePtr _balance_right(NodePtr node) {
        NodePtr child = node->left;
        child->propagate();
        if (child->weight_right() >= static_cast<long long>(child->weight_left()) * GAMMA) {
            child->right->propagate();
            node->left = _rotate_left(child);
        }
        return _rotate_right(node);
    }

    static NodePtr _balance(NodePtr node) {
        if (static_cast<long long>(node->weight_left()) * DELTA < node->weight_right())
            return _balance_left(node);
        if (static_cast<long long>(node->weight_right()) * DELTA < node->weight_left())
            return _balance_right(node);
        return node;
    }

    static NodePtr _merge_with_root(NodePtr left, NodePtr pivot, NodePtr right) {
        if (static_cast<long long>(weight(left)) > static_cast<long long>(weight(right)) * DELTA) {
            left->propagate();
            left->right = _merge_with_root(left->right, pivot, right);
            left->update();
            return _balance(left);
        }
        if (static_cast<long long>(weight(right)) > static_cast<long long>(weight(left)) * DELTA) {
            right->propagate();
            right->left = _merge_with_root(left, pivot, right->left);
            right->update();
            return _balance(right);
        }
        pivot->left = left;
        pivot->right = right;
        pivot->update();
        return pivot;
    }

    template <bool Rightmost>
    static pair<NodePtr, NodePtr> _extract_extreme(NodePtr node) {
        node->propagate();
        NodePtr next = Rightmost ? node->right : node->left;
        if (!next) {
            NodePtr rest = Rightmost ? node->left : node->right;
            node->left = node->right = nullptr;
            node->update();
            return {rest, node};
        }
        auto result = _extract_extreme<Rightmost>(next);
        if constexpr (Rightmost) node->right = result.first;
        else node->left = result.first;
        node->update();
        return {_balance(node), result.second};
    }

    static NodePtr _merge_node(NodePtr left, NodePtr right) {
        if (!left) return right;
        if (!right) return left;
        if (right->size < left->size) {
            auto extracted = _extract_extreme<false>(right);
            return _merge_with_root(left, extracted.second, extracted.first);
        }
        auto extracted = _extract_extreme<true>(left);
        return _merge_with_root(extracted.first, extracted.second, right);
    }

    static pair<NodePtr, NodePtr> _split_node(NodePtr node, int k) {
        if (!k) return {nullptr, node};
        if (k == node->size) return {node, nullptr};
        node->propagate();
        NodePtr left = node->left, right = node->right;
        const int left_size = node->left_size;
        if (k <= left_size) {
            auto parts = _split_node(left, k);
            return {parts.first, _merge_with_root(parts.second, node, right)};
        }
        auto parts = _split_node(right, k-left_size-1);
        return {_merge_with_root(left, node, parts.first), parts.second};
    }

    static NodePtr _insert_node(NodePtr node, int k, const T &value) {
        if (!node) return node_pool().emplace(value, id());
        node->propagate();
        if (k <= node->left_size) node->left = _insert_node(node->left, k, value);
        else node->right = _insert_node(node->right, k-node->left_size-1, value);
        node->update();
        return _balance(node);
    }

    template <bool Rightmost>
    static NodePtr _erase_extreme(NodePtr node, T &value) {
        node->propagate();
        NodePtr next = Rightmost ? node->right : node->left;
        if (!next) {
            value = node->key;
            NodePtr rest = Rightmost ? node->left : node->right;
            node_pool().release(node);
            return rest;
        }
        if constexpr (Rightmost) node->right = _erase_extreme<Rightmost>(next, value);
        else node->left = _erase_extreme<Rightmost>(next, value);
        node->update();
        return _balance(node);
    }

    static NodePtr _erase_node(NodePtr node, int k, T &value) {
        node->propagate();
        const int left_size = node->left_size;
        if (k < left_size) node->left = _erase_node(node->left, k, value);
        else if (k > left_size) node->right = _erase_node(node->right, k-left_size-1, value);
        else {
            value = node->key;
            if (!node->left || !node->right) {
                NodePtr rest = node->left ? node->left : node->right;
                node_pool().release(node);
                return rest;
            }
            T replacement;
            if (node->left_size >= node->size-node->left_size-1)
                node->left = _erase_extreme<true>(node->left, replacement);
            else node->right = _erase_extreme<false>(node->right, replacement);
            node->key = std::move(replacement);
        }
        node->update();
        return _balance(node);
    }

    static NodePtr _reverse_node(NodePtr node, int l, int r) {
        if (r-l <= 1) return node;
        if (!l && r == node->size) {
            node->rev ^= 1;
            return node;
        }
        node->propagate();
        const int left_size = node->left_size;
        if (r <= left_size) {
            node->left = _reverse_node(node->left, l, r);
            node->update();
            return node;
        }
        if (l > left_size) {
            node->right = _reverse_node(node->right, l-left_size-1, r-left_size-1);
            node->update();
            return node;
        }
        auto tail = _split_node(node, r);
        auto head = _split_node(tail.first, l);
        head.second->rev ^= 1;
        return _merge_node(_merge_node(head.first, head.second), tail.second);
    }

    explicit LazyWBTree(NodePtr node) : root(node) {}

    static MyLazyWBTree _new(NodePtr node) { return MyLazyWBTree(node); }

public:
    NodePtr root;

    /// @brief 空の木を構築する / O(1)
    LazyWBTree() : root(nullptr) {}

    /// @brief valuesの要素を順番に格納した木を構築する / O(N)
    LazyWBTree(const vector<T> &values) { _build(values); }

    /// @brief otherを末尾へ破壊的に結合する / O(log N + log M)
    /// @note 入力の木どうしでノードを共有しないこと
    /// @note other.rootは残るが、otherを独立した木として再利用できない
    void merge(MyLazyWBTree &other) { root = _merge_node(root, other.root); }

    /// @brief 先頭k要素と残りに破壊的に分割する / O(log N)
    /// @note 元のrootは消去しないが、分割前のハンドルは独立した木として再利用できない
    pair<MyLazyWBTree, MyLazyWBTree> split(const int k) {
        assert(0 <= k && k <= len());
        auto result = _split_node(root, k);
        return {_new(result.first), _new(result.second)};
    }

    /// @brief 全要素に作用actionを適用する / O(1)
    void all_apply(const F action) {
        if (root && action != id()) root->apply(action);
    }

    /// @brief 区間 [l, r) に作用actionを適用する / O(log N)
    void apply(const int l, const int r, const F action) {
        assert(0 <= l && l <= r && r <= len());
        if (l == r || !(action != id())) return;
        auto dfs = [&](auto &&self, NodePtr node, int l, int r) -> void {
            if (!l && r == node->size) { node->apply(action); return; }
            node->propagate();
            const int left_size = node->left_size;
            if (l < left_size) self(self, node->left, l, min(r, left_size));
            if (l <= left_size && left_size < r) node->key = mapping(action, node->key);
            if (r > left_size+1) self(self, node->right, l > left_size+1 ? l-left_size-1 : 0, r-left_size-1);
            node->update();
        };
        dfs(dfs, root, l, r);
    }

    /// @brief 全要素の総積を返す / O(1)
    /// @note 空の木ではe()を返す
    T all_prod() const { return root ? root->data : e(); }

    /// @brief 区間 [l, r) の総積を返す / O(log N)
    /// @note 空区間ではe()を返す
    T prod(const int l, const int r) {
        assert(0 <= l && l <= r && r <= len());
        if (l == r) return e();
        const F identity = id();
        auto dfs = [&](auto &&self, NodePtr node, int l, int r, F acc) -> T {
            if (!l && r == node->size) return acc != identity ? mapping(acc, node->data) : node->data;
            if (node->rev) {
                const int old_l = l;
                l = node->size-r;
                r = node->size-old_l;
            }
            const int left_size = node->left_size;
            F next = node->lazy != identity ? composition(acc, node->lazy) : acc;
            if (r <= left_size) return self(self, node->left, l, r, next);
            if (l > left_size) return self(self, node->right, l-left_size-1, r-left_size-1, next);
            T result = acc != identity ? mapping(acc, node->key) : node->key;
            if (l < left_size) result = op(self(self, node->left, l, left_size, next), result);
            if (r > left_size+1) result = op(result, self(self, node->right, 0, r-left_size-1, next));
            return result;
        };
        return dfs(dfs, root, l, r, identity);
    }

    /// @brief 位置kにvalueを挿入する / O(log N)
    void insert(int k, const T value) {
        assert(0 <= k && k <= len());
        root = _insert_node(root, k, value);
    }

    /// @brief 位置kの要素を削除し、その値を返す / O(log N)
    /// @note 削除されたノードへのポインタは無効になる
    T pop(int k) {
        assert(0 <= k && k < len());
        T value;
        root = _erase_node(root, k, value);
        return value;
    }

    /// @brief 区間 [l, r) の要素順を反転する / O(log N)
    /// @note 総積が反転で変わらない場合に利用できる
    /// @note 木全体の反転はO(1)
    void reverse(const int l, const int r) {
        assert(0 <= l && l <= r && r <= len());
        root = _reverse_node(root, l, r);
    }

    /// @brief 要素を現在の順番でvectorに格納して返す / O(N)
    vector<T> tovector() {
        vector<T> result;
        result.reserve(len());
        const F identity = id();
        auto dfs = [&](auto &&self, NodePtr node, F acc, bool rev) -> void {
            if (!node) return;
            rev ^= node->rev;
            F next = node->lazy != identity ? composition(acc, node->lazy) : acc;
            self(self, rev ? node->right : node->left, next, rev);
            result.emplace_back(acc != identity ? mapping(acc, node->key) : node->key);
            self(self, rev ? node->left : node->right, next, rev);
        };
        dfs(dfs, root, identity, false);
        return result;
    }

    /// @brief 位置kの要素をvalueに置き換える / O(log N)
    void set(int k, T value) {
        assert(0 <= k && k < len());
        auto dfs = [&](auto &&self, NodePtr node, int k) -> void {
            node->propagate();
            const int left_size = node->left_size;
            if (k == left_size) node->key = value;
            else if (k < left_size) self(self, node->left, k);
            else self(self, node->right, k-left_size-1);
            node->update();
        };
        dfs(dfs, root, k);
    }

    /// @brief 位置kの要素を返す / O(log N)
    T get(int k) {
        assert(0 <= k && k < len());
        const F identity = id();
        F acc = identity;
        NodePtr node = root;
        while (true) {
            if (node->rev) k = node->size-1-k;
            const int left_size = node->left_size;
            if (k == left_size) return acc != identity ? mapping(acc, node->key) : node->key;
            if (node->lazy != identity) acc = composition(acc, node->lazy);
            if (k < left_size) node = node->left;
            else { k -= left_size+1; node = node->right; }
        }
    }

    /// @brief 要素列を標準出力へ表示する / O(N)
    void print() { cout << *this << endl; }

    /// @brief 要素列を出力ストリームへ書き込む / O(N)
    friend ostream& operator<<(ostream &out, LazyWBTree &tree) {
        vector<T> values = tree.tovector();
        out << '[';
        for (size_t i = 0; i < values.size(); ++i) {
            if (i) out << ", ";
            out << values[i];
        }
        return out << ']';
    }

    /// @brief 要素数を返す / O(1)
    int len() const { return root ? root->size : 0; }

    /// @brief 部分木サイズ、キャッシュ、平衡条件をassertで検査する / O(N)
    void check() const {
        auto dfs = [&](auto &&self, NodePtr node) -> int {
            if (!node) return 0;
            const int left = self(self, node->left), right = self(self, node->right);
            assert(node->left_size == left);
            assert(node->size == left+right+1);
            node->balance_check();
            return node->size;
        };
        dfs(dfs, root);
    }
};
}  // namespace titan23
