/// https://github.com/titan-23/Library_cpp/blob/main/titan_cpplib/ds/binary_trie_multiset.cpp
#pragma once

#include <iostream>
#include <vector>
#include <cassert>
using namespace std;

namespace titan23 {

/// @brief 0 以上 2^bit 未満の整数の多重集合を二分トライで持つ / 全要素への xor を扱う
template<typename T>
class BinaryTrieMultiset {
private:
    vector<int> left, right, par, size;
    int end, root, bit;
    T lim, xor_val;

    int _make_node() {
        if (end >= (int)left.size()) {
            left.emplace_back(0);
            right.emplace_back(0);
            par.emplace_back(0);
            size.emplace_back(0);
        }
        return end++;
    }

    int _find(T key) const {
        key ^= xor_val;
        int node = root;
        for (int i = bit-1; i >= 0; --i) {
            if ((key >> i) & 1) {
                if (!right[node]) return -1;
                node = right[node];
            } else {
                if (!left[node]) return -1;
                node = left[node];
            }
        }
        return node;
    }

    void _remove(int node) {
        int cnt = size[node];
        for (int i = 0; i < bit; ++i) {
            size[node] -= cnt;
            if (left[par[node]] == node) {
                node = par[node];
                left[node] = 0;
                if (right[node]) break;
            } else {
                node = par[node];
                right[node] = 0;
                if (left[node]) break;
            }
        }
        while (node) {
            size[node] -= cnt;
            node = par[node];
        }
    }

    public:
    BinaryTrieMultiset() {}
    /// @brief 0 以上 2^bit 未満の値を扱う空の多重集合を作る
    BinaryTrieMultiset(const int bit) {
        end = 2;
        this->bit = bit;
        root = 1;
        lim = (T)1 << bit;
        xor_val = 0;
        left.resize(2);
        right.resize(2);
        par.resize(2);
        size.resize(2);
    }

    /// @brief ノード n 個分の領域を予約する
    void reserve(const int n) {
        left.reserve(n);
        right.reserve(n);
        par.reserve(n);
        size.reserve(n);
    }

    /// @brief key を cnt 個追加する / O(bit)
    void add(T key, int cnt = 1) {
        assert(0 <= key && key < lim);
        key ^= xor_val;
        int node = root;
        for (int i = bit-1; i >= 0; --i) {
            size[node] += cnt;
            if ((key >> i) & 1) {
                if (!right[node]) {
                    int new_node = _make_node();
                    right[node] = new_node;
                    par[right[node]] = node;
                }
                node = right[node];
            } else {
                if (!left[node]) {
                    int new_node = _make_node();
                    left[node] = new_node;
                    par[left[node]] = node;
                }
                node = left[node];
            }
        }
        size[node] += cnt;
    }

    /// @brief key を含むかを返す / O(bit)
    bool contains(T key) const {
        return _find(key) != -1;
    }

    /// @brief key を最大 cnt 個削除し、key を含んでいたかを返す / O(bit)
    bool discard(T key, int cnt = 1) {
        assert(0 <= key && key < lim);
        int node = _find(key);
        if (node == -1) {
            return false;
        } else if (size[node] <= cnt) {
            _remove(node);
        } else {
            while (node) {
                size[node] -= cnt;
                node = par[node];
            }
        }
        return true;
    }

    /// @brief key をすべて削除し、key を含んでいたかを返す / O(bit)
    bool discard_all(T key) {
        return discard(key, count(key));
    }

    /// @brief key を cnt 個削除する / key が cnt 個以上あること / O(bit)
    void remove(T key, int cnt = 1) {
        key ^= xor_val;
        int node = root;
        for (int i = bit-1; i >= 0; --i) {
            if ((key >> i) & 1) {
                node = right[node];
            } else {
                node = left[node];
            }
            assert(node);
        }
        int c = size[node];
        if (c < cnt) {
            assert(false);
        } else if (c == cnt) {
            _remove(node);
        } else {
            while (node) {
                size[node] -= cnt;
                node = par[node];
            }
        }
    }

    /// @brief key の個数を返す / O(bit)
    int count(T key) const {
        int node = _find(key);
        return node == -1 ? 0 : size[node];
    }

    /// @brief 小さい方から k 番目の値を 1 個削除して返す / k は 0 から数え、負なら後ろから数える / O(bit)
    T pop(int k = -1) {
        if (k < 0) k += len();
        int node = root;
        T key = xor_val;
        T res = 0;
        for (int i = bit-1; i >= 0; --i) {
            res <<= 1;
            if ((key >> i) & 1) {
                int t = size[right[node]];
                if (t <= k) {
                    k -= t;
                    res |= 1;
                    node = left[node];
                } else {
                    node = right[node];
                }
            } else {
                int t = size[left[node]];
                if (t <= k) {
                    k -= t;
                    res |= 1;
                    node = right[node];
                } else {
                    node = left[node];
                }
            }
        }
        if (size[node] == 1) {
            _remove(node);
        } else {
            while (node) {
                --size[node];
                node = par[node];
            }
        }
        return res;
    }

    /// @brief 最小値を 1 個削除して返す / O(bit)
    T pop_min() {
        assert(!empty());
        return pop(0);
    }

    /// @brief 最大値を 1 個削除して返す / O(bit)
    T pop_max() {
        assert(!empty());
        return pop(-1);
    }

    /// @brief 全要素に x を xor する / O(1)
    void all_xor(T x) {
        xor_val ^= x;
    }

    /// @brief 最小値を返す / O(bit)
    T get_min() const {
        assert(!empty());
        T key = xor_val;
        T ans = 0;
        int node = root;
        for (int i = bit-1; i >= 0; --i) {
            ans <<= 1;
            if ((key >> i) & 1) {
                if (right[node]) {
                    node = right[node];
                    ans |= 1;
                } else {
                    node = left[node];
                }
            } else {
                if (left[node]) {
                    node = left[node];
                } else {
                    node = right[node];
                    ans |= 1;
                }
            }
        }
        return ans ^ xor_val;
    }

    /// @brief 最大値を返す / O(bit)
    T get_max() const {
        assert(!empty());
        T key = xor_val;
        T ans = 0;
        int node = root;
        for (int i = bit-1; i >= 0; --i) {
            ans <<= 1;
            if ((key >> i) & 1) {
                if (left[node]) {
                    node = left[node];
                } else {
                    node = right[node];
                    ans |= 1;
                }
            } else {
                if (right[node]) {
                    ans |= 1;
                    node = right[node];
                } else {
                    node = left[node];
                }
            }
        }
        return ans ^ xor_val;
    }

    /// @brief key 未満の要素数を返す / O(bit)
    int index(T key) const {
        assert(0 <= key && key < lim);
        int k = 0;
        int node = root;
        for (int i = bit-1; i >= 0; --i) {
            int c = (xor_val >> i) & 1;
            int lo = c ? right[node] : left[node];
            int hi = c ? left[node] : right[node];
            if ((key >> i) & 1) {
                k += size[lo];
                node = hi;
            } else {
                node = lo;
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
            int lo = c ? right[node] : left[node];
            int hi = c ? left[node] : right[node];
            if ((key >> i) & 1) {
                k += size[lo];
                node = hi;
            } else {
                node = lo;
            }
            if (!node) break;
        }
        if (node) k += size[node];
        return k;
    }

    /// @brief [low, high) に含まれる要素数を返す / O(bit)
    int count_range(T low, T high) const {
        int hi = high >= lim ? len() : index(high);
        int lo = low >= lim ? len() : index(low);
        return hi - lo;
    }

    /// @brief 小さい方から k 番目の値を返す / k は 0 から数え、負なら後ろから数える / O(bit)
    T get(int k) const {
        if (k < 0) k += len();
        int node = root;
        T res = 0;
        for (int i = bit-1; i >= 0; --i) {
            if ((xor_val >> i) & 1) {
                int t = size[right[node]];
                res <<= 1;
                if (t <= k) {
                    k -= t;
                    res |= 1;
                    node = left[node];
                } else {
                    node = right[node];
                }
            } else {
                int t = size[left[node]];
                res <<= 1;
                if (t <= k) {
                    k -= t;
                    res |= 1;
                    node = right[node];
                } else {
                    node = left[node];
                }
            }
        }
        return res;
    }

    /// @brief key より大きい最小の値を返す / 無ければ -1 / O(bit)
    T gt(T key) const {
        int i = index_right(key);
        return (i >= size[root]? (-1) : get(i));
    }

    /// @brief key より小さい最大の値を返す / 無ければ -1 / O(bit)
    T lt(T key) const {
        int i = index(key) - 1;
        return (i < 0? -1 : get(i));
    }

    /// @brief key 以上の最小の値を返す / 無ければ -1 / O(bit)
    T ge(T key) const {
        if (key == 0) return (!empty()? get_min() : -1);
        int i = index_right(key - 1);
        return (i >= size[root]? -1 : get(i));
    }

    /// @brief key 以下の最大の値を返す / 無ければ -1 / O(bit)
    T le(T key) const {
        int i = index_right(key) - 1;
        return (i < 0? -1 : get(i));
    }

    /// @brief 全要素を重複込みで昇順に並べて返す
    vector<T> tovector() const {
        vector<T> a;
        if (empty()) return a;
        a.reserve(len());
        auto dfs = [&] (auto &&dfs, int d, int node, T now) -> void {
            if (d < 0) {
                for (int i = 0; i < size[node]; ++i) a.emplace_back(now);
                return;
            }
            int c = (xor_val >> d) & 1;
            int lo = c ? right[node] : left[node];
            int hi = c ? left[node] : right[node];
            if (lo) dfs(dfs, d-1, lo, now<<1);
            if (hi) dfs(dfs, d-1, hi, now<<1|1);
        };
        dfs(dfs, bit-1, root, (T)0);
        return a;
    }

    /// @brief 空かを返す / O(1)
    bool empty() const {
        return size[root] == 0;
    }

    /// @brief 全要素を削除する
    void clear() {
        end = 2;
        root = 1;
        left.assign(2, 0);
        right.assign(2, 0);
        par.assign(2, 0);
        size.assign(2, 0);
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

    /// @brief 要素数を重複込みで返す / O(1)
    int len() const {
        return size[root];
    }
};
}  // namespace titan23
