/// https://github.com/titan-23/Library_cpp/blob/main/titan_cpplib/ds/binary_trie_set.cpp
#pragma once

#include <cassert>
#include <iostream>
#include <vector>
#include <stack>
using namespace std;

namespace titan23 {

/// @brief 0 以上 2^bit 未満の整数の集合を二分トライで持つ / 全要素への xor を扱う
template<typename T>
class BinaryTrieSet {
private:
    int root, bit;
    T lim, xor_val;

    struct MemoryAllocator {

        struct Node {
            int ch[2];
            int par, size;
            Node() : par(0), size(0) { ch[0] = ch[1] = 0; }

            void clear() {
                ch[0] = ch[1] = 0;
                par = 0; size = 0;
            }
        };

        vector<Node> tree;
        stack<int> erased;
        size_t ptr;

        MemoryAllocator() : ptr(2) {
            tree.emplace_back(Node{});
            tree.emplace_back(Node{});
        }

        int new_node() {
            if (!erased.empty()) {
                int idx = erased.top(); erased.pop();
                tree[idx].clear();
                return idx;
            }
            if (tree.size() > ptr) {
                tree[ptr].clear();
            } else {
                tree.emplace_back(Node{});
            }
            ptr++;
            return ptr - 1;
        }

        void erase(int node) {
            erased.emplace(node);
        }

        void reserve(int n) {
            tree.reserve(n);
        }
    } ma;

    inline int _find(T key) const {
        key ^= xor_val;
        int node = root;
        for (int i = bit-1; i >= 0; --i) {
            int c = key >> i & 1;
            if (!ma.tree[node].ch[c]) return -1;
            node = ma.tree[node].ch[c];
        }
        return node;
    }

public:
    /// @brief 0 以上 2^bit 未満の値を扱う空の集合を作る
    BinaryTrieSet(int bit) : bit(bit) {
        root = 1;
        lim = (T)1 << bit;
        xor_val = 0;
    }

    /// @brief ノード n 個分の領域を予約する
    void reserve(const int n) {
        ma.reserve(n);
    }

    /// @brief key を追加し、追加できたかを返す / O(bit)
    bool add(T key) {
        assert(0 <= key && key < lim);
        key ^= xor_val;
        int node = root;
        for (int i = bit-1; i >= 0; --i) {
            int c = key >> i & 1;
            if (!ma.tree[node].ch[c]) {
                int new_node = ma.new_node();
                ma.tree[node].ch[c] = new_node;
                ma.tree[ma.tree[node].ch[c]].par = node;
            }
            node = ma.tree[node].ch[c];
        }
        if (ma.tree[node].size) return false;
        ma.tree[node].size = 1;
        for (int i = 0; i < bit; ++i) {
            node = ma.tree[node].par;
            ma.tree[node].size++;
        }
        return true;
    }

    /// @brief key を含むかを返す / O(bit)
    bool contains(const T key) const {
        return _find(key) != -1;
    }

    void _discard(int node) {
        for (int i = 0; i < bit; ++i) {
            int c = ma.tree[ma.tree[node].par].ch[0] == node;
            ma.tree[node].size--;
            ma.erase(node);
            node = ma.tree[node].par;
            ma.tree[node].ch[c^1] = 0;
            if (ma.tree[node].ch[c]) break;
        }
        while (node) {
            ma.tree[node].size--;
            node = ma.tree[node].par;
        }
    }

    /// @brief key を削除し、削除できたかを返す / O(bit)
    bool discard(T key) {
        int node = _find(key);
        if (node == -1) return false;
        _discard(node);
        return true;
    }

    /// @brief 小さい方から k 番目の値を削除して返す / k は 0 から数え、負なら後ろから数える / O(bit)
    T pop(int k) {
        if (k < 0) k += len();
        assert(0 <= k && k < len());
        int node = root;
        T res = 0;
        for (int i = bit-1; i >= 0; --i) {
            int c = xor_val >> i & 1;
            int t = ma.tree[ma.tree[node].ch[c]].size;
            res <<= 1;
            if (t <= k) {
                k -= t;
                res |= 1;
                node = ma.tree[node].ch[c^1];
            } else {
                node = ma.tree[node].ch[c];
            }
        }
        _discard(node);
        return res;
    }

    /// @brief 最小値を削除して返す / O(bit)
    T pop_min() {
        assert(!empty());
        return pop(0);
    }

    /// @brief 最大値を削除して返す / O(bit)
    T pop_max() {
        assert(!empty());
        return pop(len()-1);
    }

    /// @brief 全要素に x を xor する / O(1)
    void all_xor(T x) {
        xor_val ^= x;
    }

    /// @brief 最小値を返す / O(bit)
    T get_min() const {
        T key = xor_val;
        T ans = 0;
        int node = root;
        for (int i = bit-1; i >= 0; --i) {
            ans <<= 1;
            int c = key >> i & 1;
            if (ma.tree[node].ch[c]) {
                node = ma.tree[node].ch[c];
                ans |= c;
            } else {
                node = ma.tree[node].ch[c^1];
                ans |= (c^1);
            }
        }
        return ans ^ xor_val;
    }

    /// @brief 最大値を返す / O(bit)
    T get_max() const {
        T key = xor_val;
        T ans = 0;
        int node = root;
        for (int i = bit-1; i >= 0; --i) {
            ans <<= 1;
            int c = key >> i & 1;
            if (ma.tree[node].ch[c^1]) {
                node = ma.tree[node].ch[c^1];
                ans |= (c^1);
            } else {
                node = ma.tree[node].ch[c];
                ans |= c;
            }
        }
        return ans ^ xor_val;
    }

    /// @brief key 未満の要素数を返す / O(bit)
    int index(T key) const {
        int k = 0;
        int node = root;
        for (int i = bit-1; i >= 0; --i) {
            int c = (xor_val >> i) & 1;
            if (key >> i & 1) {
                k += ma.tree[ma.tree[node].ch[c]].size;
                node = ma.tree[node].ch[c^1];
            } else {
                node = ma.tree[node].ch[c];
            }
            if (!node) break;
        }
        return k;
    }

    /// @brief key 以下の要素数を返す / O(bit)
    int index_right(T key) const {
        int k = 0;
        int node = root;
        for (int i = bit-1; i >= 0; --i) {
            int c = (xor_val >> i) & 1;
            if (key >> i & 1) {
                k += ma.tree[ma.tree[node].ch[c]].size;
                node = ma.tree[node].ch[c^1];
            } else {
                node = ma.tree[node].ch[c];
            }
            if (!node) break;
        }
        if (node) k += ma.tree[node].size;
        return k;
    }

    /// @brief [low, high) に含まれる要素数を返す / O(bit)
    int count_range(T low, T high) const {
        int hi = high >= lim ? len() : index(high);
        int lo = low >= lim ? len() : index(low);
        return hi - lo;
    }

    /// @brief 小さい方から k 番目の値を返す / k は 0 から数える / O(bit)
    T get(int k) const {
        assert(0 <= k && k < len());
        int node = root;
        T res = 0;
        for (int i = bit-1; i >= 0; --i) {
            if ((xor_val >> i) & 1) {
                int t = ma.tree[ma.tree[node].ch[1]].size;
                res <<= 1;
                if (t <= k) {
                    k -= t;
                    res |= 1;
                    node = ma.tree[node].ch[0];
                } else {
                    node = ma.tree[node].ch[1];
                }
            } else {
                int t = ma.tree[ma.tree[node].ch[0]].size;
                res <<= 1;
                if (t <= k) {
                    k -= t;
                    res |= 1;
                    node = ma.tree[node].ch[1];
                } else {
                    node = ma.tree[node].ch[0];
                }
            }
        }
        return res;
    }

    /// @brief key より大きい最小の値を返す / 無ければ -1 / O(bit)
    T gt(T key) const {
        int i = index_right(key);
        return (i >= ma.tree[root].size ? (-1) : get(i));
    }

    /// @brief key より小さい最大の値を返す / 無ければ -1 / O(bit)
    T lt(T key) const {
        int i = index(key) - 1;
        return (i < 0 ? -1 : get(i));
    }

    /// @brief key 以上の最小の値を返す / 無ければ -1 / O(bit)
    T ge(T key) const {
        if (key == 0) return (!empty() ? get_min() : -1);
        int i = index_right(key - 1);
        return (i >= ma.tree[root].size ? -1 : get(i));
    }

    /// @brief key 以下の最大の値を返す / 無ければ -1 / O(bit)
    T le(T key) const {
        int i = index_right(key) - 1;
        return (i < 0 ? -1 : get(i));
    }

    /// @brief 全要素を昇順に並べて返す
    vector<T> tovector() const {
        vector<T> a;
        if (empty()) return a;
        a.reserve(len());
        auto dfs = [&] (auto &&dfs, int d, int node, T now) -> void {
            if (d < 0) {
                a.emplace_back(now);
                return;
            }
            int c = xor_val >> d & 1;
            if (ma.tree[node].ch[c])   dfs(dfs, d-1, ma.tree[node].ch[c], now<<1);
            if (ma.tree[node].ch[c^1]) dfs(dfs, d-1, ma.tree[node].ch[c^1], now<<1|1);
        };
        dfs(dfs, bit-1, root, (T)0);
        return a;
    }

    /// @brief 空かを返す / O(1)
    bool empty() const {
        return ma.tree[root].size == 0;
    }

    void print() const {
        cout << "{";
        vector<T> a = tovector();
        for (int i = 0; i < len()-1; ++i) {
            cout << a[i] << ", ";
        }
        if (!a.empty()) {
            cout << a.back();
        }
        cout << "}" << endl;
    }

    /// @brief 要素数を返す / O(1)
    int len() const {
        return ma.tree[root].size;
    }
};
}  // namespace titan23
