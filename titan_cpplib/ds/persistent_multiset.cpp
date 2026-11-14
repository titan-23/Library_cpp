/// https://github.com/titan-23/Library_cpp/blob/main/titan_cpplib/ds/persistent_multiset.cpp
#pragma once

#include <iostream>
#include <vector>
#include <cmath>
#include <cassert>
#include <stack>
#include <tuple>
#include <memory>
#include <algorithm>
#include "titan_cpplib/others/print.cpp"
using namespace std;

namespace titan23 {

/// @brief 重み平衡木による永続多重集合 / 同じキーは 1 ノードに個数をまとめる
template <typename T>
class PersistentMultiset {
  private:
    class Node;
    using NodePtr = shared_ptr<Node>;
    // using NodePtr = Node*;
    static constexpr int DELTA = 3;
    static constexpr int GAMMA = 2;
    NodePtr root;

    class Node {
      public:
        T key;
        int size, cnt, cnt_subtree;
        NodePtr left;
        NodePtr right;

        Node(T key, int cnt = 1) : key(key), size(1), cnt(cnt), cnt_subtree(cnt), left(nullptr), right(nullptr) {}

        NodePtr copy() const {
            NodePtr node = make_shared<Node>(key);
            node->size = size;
            node->cnt = cnt;
            node->cnt_subtree = cnt_subtree;
            node->left = left;
            node->right = right;
            return node;
        }

        int weight_right() const {
            return right ? right->size + 1 : 1;
        }

        int weight_left() const {
            return left ? left->size + 1 : 1;
        }

        void update() {
            size = 1;
            cnt_subtree = cnt;
            if (left) {
                size += left->size;
                cnt_subtree += left->cnt_subtree;
            }
            if (right) {
                size += right->size;
                cnt_subtree += right->cnt_subtree;
            }
        }

        void balance_check() const {
            if (!(weight_left()*DELTA >= weight_right())) {
                cerr << weight_left() << ", " << weight_right() << endl;
                cerr << "not weight_left()*DELTA >= weight_right()." << endl;
                assert(false);
            }
            if (!(weight_right() * DELTA >= weight_left())) {
                cerr << weight_left() << ", " << weight_right() << endl;
                cerr << "not weight_right() * DELTA >= weight_left()." << endl;
                assert(false);
            }
        }

        void print() const {
            vector<T> a;
            auto dfs = [&] (auto &&dfs, const Node* node) -> void {
                if (!node) return;
                if (node->left)  dfs(dfs, node->left.get());
                a.emplace_back(node->key);
                if (node->right) dfs(dfs, node->right.get());
            };
            dfs(dfs, this);
            cerr << a << endl;
        }

        void debug() const {
            cout << "this : key=" << key << ", size=" << size << ", cnt=" << cnt << endl;
            if (left)  cout << "to-left" << endl;
            if (right) cout << "to-right" << endl;
            cout << endl;
            if (left)  left->print();
            if (right) right->print();
        }
    };

    void _build(vector<T> a) {
        sort(a.begin(), a.end());
        vector<pair<T, int>> b;
        for (const T &x : a) {
            if (!b.empty() && b.back().first == x) {
                b.back().second++;
            } else {
                b.emplace_back(x, 1);
            }
        }
        auto build = [&] (auto &&build, int l, int r) -> NodePtr {
            int mid = (l + r) >> 1;
            NodePtr node = make_shared<Node>(b[mid].first, b[mid].second);
            if (l != mid) node->left = build(build, l, mid);
            if (mid+1 != r) node->right = build(build, mid+1, r);
            node->update();
            return node;
        };
        if (b.empty()) {
            root = nullptr;
            return;
        }
        root = build(build, 0, (int)b.size());
    }

    NodePtr _rotate_right(NodePtr &node) {
        NodePtr u = node->left->copy();
        node->left = u->right;
        u->right = node;
        node->update();
        u->update();
        return u;
    }

    NodePtr _rotate_left(NodePtr &node) {
        NodePtr u = node->right->copy();
        node->right = u->left;
        u->left = node;
        node->update();
        u->update();
        return u;
    }

    NodePtr _balance_left(NodePtr &node) {
        node->right = node->right->copy();
        NodePtr u = node->right;
        if (node->right->weight_left() >= node->right->weight_right() * GAMMA) {
            node->right = _rotate_right(u);
        }
        u = _rotate_left(node);
        return u;
    }

    NodePtr _balance_right(NodePtr &node) {
        node->left = node->left->copy();
        NodePtr u = node->left;
        if (node->left->weight_right() >= node->left->weight_left() * GAMMA) {
            node->left = _rotate_left(u);
        }
        u = _rotate_right(node);
        return u;
    }

    int weight(NodePtr node) const {
        return node ? node->size + 1 : 1;
    }

    NodePtr _merge_with_root(NodePtr l, NodePtr root, NodePtr r) {
        if (weight(r) * DELTA < weight(l)) {
            l = l->copy();
            l->right = _merge_with_root(l->right, root, r);
            l->update();
            if (weight(l->left) * DELTA < weight(l->right)) {
                return _balance_left(l);
            }
            return l;
        } else if (weight(l) * DELTA < weight(r)) {
            r = r->copy();
            r->left = _merge_with_root(l, root, r->left);
            r->update();
            if (weight(r->right) * DELTA < weight(r->left)) {
                return _balance_right(r);
            }
            return r;
        }
        root = root->copy();
        root->left = l;
        root->right = r;
        root->update();
        return root;
    }

    pair<NodePtr, NodePtr> _pop_right(NodePtr &node) {
        return _split_node_idx(node, node->size-1);
    }

    // l の全キー <= r の全キーが前提。境界の等しいキーは cnt を足して 1 ノードにまとめる
    NodePtr _merge_node(NodePtr l, NodePtr r) {
        if ((!l) && (!r)) { return nullptr; }
        if (!l) { return r; }
        if (!r) { return l; }
        auto [l_, root_] = _pop_right(l);
        NodePtr rmin = r;
        while (rmin->left) rmin = rmin->left;
        if (rmin->key == root_->key) {
            NodePtr r2 = _add_leftmost(r, root_->cnt);
            return _merge_node(l_, r2);
        }
        return _merge_with_root(l_, root_, r);
    }

    // 最小キーのノードに cnt を加える。最小キーが加算対象のキーであることが前提
    NodePtr _add_leftmost(NodePtr node, int cnt) {
        node = node->copy();
        node->cnt_subtree += cnt;
        if (node->left) {
            node->left = _add_leftmost(node->left, cnt);
        } else {
            node->cnt += cnt;
        }
        return node;
    }

    pair<NodePtr, NodePtr> _split_node_key(NodePtr &node, const T &key) {
        if (!node) { return {nullptr, nullptr}; }
        if (node->key == key) {
            return {_merge_with_root(node->left, node, nullptr), node->right};
        } else if (node->key > key) {
            auto [l, r] = _split_node_key(node->left, key);
            return {l, _merge_with_root(r, node, node->right)};
        } else {
            auto [l, r] = _split_node_key(node->right, key);
            return {_merge_with_root(node->left, node, l), r};
        }
    }

    // 左に k ノード、右に残りのノードを分ける
    pair<NodePtr, NodePtr> _split_node_idx(NodePtr &node, int k) {
        if (!node) {return {nullptr, nullptr};}
        int tmp = node->left ? k-node->left->size : k;
        if (tmp == 0) {
            return {node->left, _merge_with_root(nullptr, node, node->right)};
        } else if (tmp < 0) {
            auto [l, r] = _split_node_idx(node->left, k);
            return {l, _merge_with_root(r, node, node->right)};
        } else {
            auto [l, r] = _split_node_idx(node->right, tmp-1);
            return {_merge_with_root(node->left, node, l), r};
        }
    }

    // 小さい方から k 要素を左に分ける。同じキーの途中で切るときはノードを 2 つに分ける
    pair<NodePtr, NodePtr> _split_node_cnt(NodePtr node, int k) {
        if (!node) { return {nullptr, nullptr}; }
        int left_cnt = node->left ? node->left->cnt_subtree : 0;
        if (k <= left_cnt) {
            auto [l, r] = _split_node_cnt(node->left, k);
            return {l, _merge_with_root(r, node, node->right)};
        } else if (k >= left_cnt + node->cnt) {
            auto [l, r] = _split_node_cnt(node->right, k - left_cnt - node->cnt);
            return {_merge_with_root(node->left, node, l), r};
        } else {
            int a = k - left_cnt; // 1 <= a <= cnt-1
            NodePtr node_l = make_shared<Node>(node->key, a);
            NodePtr node_r = make_shared<Node>(node->key, node->cnt - a);
            NodePtr lt = _merge_with_root(node->left, node_l, nullptr);
            NodePtr rt = _merge_with_root(nullptr, node_r, node->right);
            return {lt, rt};
        }
    }

    NodePtr _find_node(const T &key) const {
        NodePtr node = root;
        while (node) {
            if (key == node->key) return node;
            node = key < node->key ? node->left : node->right;
        }
        return nullptr;
    }

    PersistentMultiset<T> _new(NodePtr root) const {
        return PersistentMultiset<T>(root);
    }

    PersistentMultiset(NodePtr root) : root(root) {}

  public:
    /// @brief 空の多重集合を構築する / O(1)
    PersistentMultiset() : root(nullptr) {}

    /// @brief 列 a の要素からなる多重集合を構築する / O(n log n)
    PersistentMultiset(const vector<T> &a) { _build(a); }

    /// @brief other の要素を合わせた多重集合を返す / this の全要素 <= other の全要素が前提 / O(log n)
    PersistentMultiset<T> merge(PersistentMultiset<T> other) {
        NodePtr root = _merge_node(this->root, other.root);
        return _new(root);
    }

    /// @brief 小さい方から k 要素と残りに分けて返す / 多重度を数える / O(log n)
    pair<PersistentMultiset<T>, PersistentMultiset<T>> split(int k) {
        assert(0 <= k && k <= len());
        auto [l, r] = _split_node_cnt(this->root, k);
        return {_new(l), _new(r)};
    }

    /// @brief key を cnt 個追加した多重集合を返す / cnt >= 1 / O(log n)
    PersistentMultiset<T> add(T key, int cnt = 1) {
        assert(cnt >= 1);
        NodePtr it = _find_node(key);
        if (it == nullptr) {
            auto [s, t] = _split_node_key(root, key);
            NodePtr new_node = make_shared<Node>(key, cnt);
            return _new(_merge_with_root(s, new_node, t));
        }
        NodePtr new_root = root->copy();
        NodePtr node = new_root;
        while (true) {
            node->cnt_subtree += cnt;
            if (key == node->key) {
                node->cnt += cnt;
                break;
            }
            if (key < node->key) {
                node->left = node->left->copy();
                node = node->left;
            } else {
                node->right = node->right->copy();
                node = node->right;
            }
        }
        return _new(new_root);
    }

    /// @brief key を cnt 個削除した多重集合を返す / cnt >= 1 / O(log n)
    /// cnt が key の個数以上なら key をすべて削除する。key が無ければ変更しない
    PersistentMultiset<T> remove(T key, int cnt = 1) {
        assert(cnt >= 1);
        NodePtr it = _find_node(key);
        if (it == nullptr) {
            return _new(root ? root->copy() : nullptr);
        }
        if (it->cnt > cnt) {
            NodePtr new_root = root->copy();
            NodePtr node = new_root;
            while (true) {
                node->cnt_subtree -= cnt;
                if (key == node->key) {
                    node->cnt -= cnt;
                    break;
                }
                if (key < node->key) {
                    node->left = node->left->copy();
                    node = node->left;
                } else {
                    node->right = node->right->copy();
                    node = node->right;
                }
            }
            return _new(new_root);
        }
        auto [s_, t] = _split_node_key(this->root, key);
        auto [s, tmp] = _pop_right(s_);
        assert(tmp->key == key);
        NodePtr new_root = _merge_node(s, t);
        return _new(new_root);
    }

    /// @brief key を含むか判定する / O(log n)
    bool contains(const T &key) const {
        return _find_node(key) != nullptr;
    }

    /// @brief key の個数を返す / O(log n)
    int count(const T &key) const {
        NodePtr it = _find_node(key);
        return it ? it->cnt : 0;
    }

    /// @brief 小さい方から k 番目の要素を返す / 0-indexed で多重度を数える / O(log n)
    T get(int k) const {
        assert(0 <= k && k < len());
        NodePtr node = root;
        while (true) {
            assert(node);
            int left_cnt = node->left ? node->left->cnt_subtree : 0;
            if (left_cnt <= k && k < left_cnt + node->cnt) return node->key;
            if (k < left_cnt) {
                node = node->left;
            } else {
                k -= left_cnt + node->cnt;
                node = node->right;
            }
        }
    }

    /// @brief key 未満の要素数を返す / O(log n)
    int index(const T &key) const {
        int k = 0;
        NodePtr node = root;
        while (node) {
            if (key == node->key) {
                k += node->left ? node->left->cnt_subtree : 0;
                break;
            }
            if (key < node->key) {
                node = node->left;
            } else {
                k += node->left ? (node->left->cnt_subtree + node->cnt) : node->cnt;
                node = node->right;
            }
        }
        return k;
    }

    /// @brief key 以下の要素数を返す / O(log n)
    int index_right(const T &key) const {
        int k = 0;
        NodePtr node = root;
        while (node) {
            if (key == node->key) {
                k += node->left ? (node->left->cnt_subtree + node->cnt) : node->cnt;
                break;
            }
            if (key < node->key) {
                node = node->left;
            } else {
                k += node->left ? (node->left->cnt_subtree + node->cnt) : node->cnt;
                node = node->right;
            }
        }
        return k;
    }

    /// @brief 小さい方から k 番目の要素を 1 つ削除した多重集合とその要素を返す / O(log n)
    pair<PersistentMultiset<T>, T> pop(int k) {
        assert(0 <= k && k < len());
        T key = get(k);
        return {remove(key), key};
    }

    /// @brief 全要素を昇順の配列にして返す / O(n)
    vector<T> tovector() const {
        NodePtr node = root;
        stack<NodePtr> s;
        vector<T> a;
        a.reserve(len());
        while (!s.empty() || node) {
            if (node) {
                s.emplace(node);
                node = node->left;
            } else {
                node = s.top(); s.pop();
                for (int i = 0; i < node->cnt; ++i) a.emplace_back(node->key);
                node = node->right;
            }
        }
        return a;
    }

    /// @brief コピーを返す / O(1)
    PersistentMultiset<T> copy() const {
        return _new(this->root ? this->root->copy() : nullptr);
    }

    /// @brief 多重度を含む要素数を返す / O(1)
    int len() const {
        return root ? root->cnt_subtree : 0;
    }

    /// @brief 平衡条件と部分木の集計値を検査する / O(n)
    void check() const {
        auto rec = [&] (auto &&rec, NodePtr node) -> tuple<int, int, int> {
            int ls = 0, rs = 0, lc = 0, rc = 0;
            int height = 0;
            if (node->left) {
                auto [s, c, h] = rec(rec, node->left);
                ls = s; lc = c;
                height = max(height, h);
            }
            if (node->right) {
                auto [s, c, h] = rec(rec, node->right);
                rs = s; rc = c;
                height = max(height, h);
            }
            node->balance_check();
            assert(node->cnt >= 1);
            int size = ls + rs + 1;
            int cnt_subtree = lc + rc + node->cnt;
            assert(size == node->size);
            assert(cnt_subtree == node->cnt_subtree);
            return {size, cnt_subtree, height+1};
        };
        if (root == nullptr) return;
        auto [_, c, h] = rec(rec, root);
        cerr << PRINT_GREEN << "OK : height=" << h << PRINT_NONE << endl;
    }

    /// @brief 全要素をストリームへ出力する / O(n)
    friend ostream& operator<<(ostream& os, const PersistentMultiset<T> &tree) {
        vector<T> a = tree.tovector();
        os << "{";
        for (int i = 0; i < (int)a.size()-1; ++i) {
            os << a[i] << ", ";
        }
        if (!a.empty()) os << a.back();
        os << "}";
        return os;
    }
};
}  // namespace titan23
