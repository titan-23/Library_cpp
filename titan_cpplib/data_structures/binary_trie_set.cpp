#include <iostream>
#include <vector>
#include <stack>
using namespace std;

namespace titan23 {

template<typename T>
class BinaryTrieSet {
private:
    int root, bit;
    T lim, xor_val;

    struct MemoeyAllocator {

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

        MemoeyAllocator() : ptr(2) {
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
    BinaryTrieSet(int bit) : bit(bit) {
        root = 1;
        lim = (T)1 << bit;
        xor_val = 0;
    }

    void reserve(const int n) {
        ma.reserve(n);
    }

    bool add(T key) {
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

    bool discard(T key) {
        int node = _find(key);
        if (node == -1) return false;
        _discard(node);
        return true;
    }

    T pop(int k) {
        assert(0 <= k && k < len());
        if (k < 0) k += len();
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
        return res ^ xor_val;
    }

    T pop_min() {
        assert(!empty());
        return pop(0);
    }

    T pop_max() {
        assert(!empty());
        return pop(len()-1);
    }

    void all_xor(T x) {
        xor_val ^= x;
    }

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

    int index(T key) const {
        int k = 0;
        int node = root;
        key ^= xor_val;
        for (int i = bit-1; i >= 0; --i) {
            if (key >> i & 1) {
                k += ma.tree[ma.tree[node].ch[0]].size;
                node = ma.tree[node].ch[1];
            } else {
                node = ma.tree[node].ch[0];
            }
            if (!node) break;
        }
        return k;
    }

    int index_right(T key) const {
        int k = 0;
        int node = root;
        key ^= xor_val;
        for (int i = bit-1; i >= 0; --i) {
            if (key >> i & 1) {
                k += ma.tree[ma.tree[node].ch[0]].size;
                node = ma.tree[node].ch[1];
            } else {
                node = ma.tree[node].ch[0];
            }
            if (!node) break;
        }
        if (node) k++;
        return k;
    }

    T get(int k) {
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

    T gt(T key) {
        int i = index_right(key);
        return (i >= ma.tree[root].size ? (-1) : get(i));
    }

    T lt(T key) {
        int i = index(key) - 1;
        return (i < 0 ? -1 : get(i));
    }

    T ge(T key) {
        if (key == 0) return (len() ? get_min() : -1);
        int i = index_right(key - 1);
        return (i >= ma.tree[root].size ? -1 : get(i));
    }

    T le(T key) {
        int i = index(key + 1) - 1;
        return (i < 0 ? -1 : get(i));
    }

    vector<T> tovector() const {
        vector<T> a;
        if (empty()) return a;
        a.reserve(len());
        auto dfs = [&] (auto &&dfs, int d, int node, T now) -> void {
            assert(node);
            if (d < 0) {
                assert(ma.tree[node].size);
                a.emplace_back(now);
                return;
            }
            int c = xor_val >> d & 1;
            if (ma.tree[node].ch[c])   dfs(dfs, d-1, ma.tree[node].ch[c], now<<1|c);
            if (ma.tree[node].ch[c^1]) dfs(dfs, d-1, ma.tree[node].ch[c^1], now<<1|(c^1));
        };
        dfs(dfs, bit-1, root, (T)0);
        return a;
    }

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

    int len() const {
        return ma.tree[root].size;
    }
};
}  // namespace titan23
