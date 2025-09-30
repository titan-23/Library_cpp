#include <vector>
#include "titan_cpplib/data_structures/fast_stack.cpp"
using namespace std;

// PersistentArray
namespace titan23 {

/**
 * @brief 永続配列
 */
template<typename T>
class PersistentArray {
private:
    static FastStack<int> path;

    struct MemoeyAllocator {

        struct Node {
            int left, right;
            Node() : left(0), right(0) {}
            Node(int l, int r) : left(l), right(r) {}
        };

        vector<Node> tree;
        vector<T> keys;
        size_t ptr;

        MemoeyAllocator() : ptr(1) {
            tree.emplace_back(0, 0);
            keys.emplace_back(T{});
        }

        int copy(int node) {
            int idx = new_node(keys[node]);
            tree[idx].left = tree[node].left;
            tree[idx].right = tree[node].right;
            return idx;
        }

        int new_node(const T key) {
            if (tree.size() > ptr) {
                tree[ptr] = {0, 0};
                keys[ptr] = key;
            } else {
                tree.emplace_back(0, 0);
                keys.emplace_back(key);
            }
            ptr++;
            return ptr - 1;
        }

        void reserve(int n) {
            tree.reserve(n);
            keys.reserve(n);
        }

        void reset() {
            ptr = 1;
        }
    };

    static MemoeyAllocator ma;

    int root;
    int n;

    int bit_length(const int n) const {
        return n == 0 ? 0 : 32 - __builtin_clz(n);
    }

    void _build(vector<T> &a) {
        if (n == 0) {
            root = 0;
            return;
        }
        vector<int> pool(n);
        for (int i = 0; i < n; ++i) {
            pool[i] = ma.new_node(a[i]);
        }
        for (int i = 1; i < n+1; ++i) {
            if (2*i-1 < n) {
                ma.tree[pool[i-1]].left = pool[2*i-1];
            }
            if (2*i < n) {
                ma.tree[pool[i-1]].right = pool[2*i];
            } else {
                break;
            }
        }
        root = pool[0];
    }

    PersistentArray<T> _new(int root) const {
        PersistentArray<T> res;
        res.root = root;
        res.n = n;
        return res;
    }

public:
    PersistentArray() : root(0), n(0) {}

    //! 配列 `a` をもとに構築する / `O(n)`
    PersistentArray(vector<T> a) : n(a.size()) {
        _build(a);
    }

    //! 複数の位置を一括変更した永続配列を返す
    PersistentArray<T> multiset(vector<pair<int, T>> indices) const {
        if (indices.empty()) return copy();
        sort(indices.begin(), indices.end());

        auto dfs = [&](auto&& dfs, int node, int l, int r, int li, int ri) -> int {
            if (li >= ri) return node;
            if (r - l == 1) {
                int idx = ma.copy(node);
                ma.keys[idx] = indices[li].second;
                return idx;
            }
            int mid = (l + r) / 2;
            int mi = lower_bound(indices.begin() + li, indices.begin() + ri, mid, [&] (const pair<int, T> &p) {
                return p.first < mid;
            }) - indices.begin();
            int idx = ma.copy(node);
            if (li < mi) {
                ma.tree[idx].left = dfs(dfs, ma.tree[node].left, l, mid, li, mi);
            }
            if (mi < ri) {
                ma.tree[idx].right = dfs(dfs, ma.tree[node].right, mid, r, mi, ri);
            }
            return idx;
        };

        int new_root = dfs(dfs, root, 0, n, 0, (int)indices.size());
        return _new(new_root);
    }

    //! 複数の位置を一括変更した永続配列を返す
    //! unique前提注意!
    PersistentArray<T> multiset_uf(vector<int> indices, T v) const {
        if (indices.empty()) return copy();
        sort(indices.begin(), indices.end());

        auto dfs = [&](auto&& dfs, int node, int l, int r, int li, int ri) -> int {
            if (li >= ri) return node;
            if (r - l == 1) {
                int idx = ma.copy(node);
                ma.keys[idx] = v;
                return idx;
            }
            int mid = (l + r) / 2;
            int mi = lower_bound(indices.begin() + li, indices.begin() + ri, mid) - indices.begin();
            int idx = ma.copy(node);
            if (li < mi) {
                ma.tree[idx].left = dfs(dfs, ma.tree[node].left, l, mid, li, mi);
            }
            if (mi < ri) {
                ma.tree[idx].right = dfs(dfs, ma.tree[node].right, mid, r, mi, ri);
            }
            return idx;
        };

        int new_root = dfs(dfs, root, 0, n, 0, (int)indices.size());
        return _new(new_root);
    }

    //! 位置 `k` を `v` に変更した永続配列を返す / `O(logn)` time, `O(logn)` space
    PersistentArray<T> set(int k, T v) const{
        assert(0 <= k && k < n);
        assert(root);
        int node = root;
        int new_node = ma.copy(node);
        PersistentArray<T> res = _new(new_node);
        k++;
        int b = bit_length(k);
        for (int i = b-2; i >= 0; --i) {
            if ((k >> i) & 1) {
                node = ma.tree[node].right;
                ma.tree[new_node].right = ma.copy(node);
                new_node = ma.tree[new_node].right;
            } else {
                node = ma.tree[node].left;
                ma.tree[new_node].left = ma.copy(node);
                new_node = ma.tree[new_node].left;
            }
        }
        ma.keys[new_node] = v;
        return res;
    }

    //! 位置 `k` の値を返す / `O(logn)` time, `O(1)` space
    T get(int k) const {
        assert(0 <= k && k < n);
        assert(root);
        int node = root;
        k++;
        int b = bit_length(k);
        for (int i = b-2; i >= 0; --i) {
            if ((k >> i) & 1) {
                node = ma.tree[node].right;
            } else {
                node = ma.tree[node].left;
            }
        }
        return ma.keys[node];
    }

    //! 永続配列全体をコピーして返す / `O(1)` time, `O(1)` space
    PersistentArray<T> copy() const {
        return _new(root ? ma.copy(root) : 0);
    }

    //! `vector` にして返す / `O(n)`
    vector<T> tovector() const {
        vector<T> a(n);
        vector<int> q = {root};
        for (int i = 0; i < (int)q.size(); ++i) {
            int node = q[i];
            a[i] = ma.keys[node];
            if (ma.tree[node].left) q.emplace_back(ma.tree[node].left);
            if (ma.tree[node].right) q.emplace_back(ma.tree[node].right);
        }
        return a;
    }

    //! 要素数を返す / `O(1)`
    int len() const {
        return n;
    }

    void print() const {
        vector<T> a = tovector();
        cout << "[";
        for (int i = 0; i < (int)a.size(); ++i) {
            cout << a[i];
            if (i != (int)a.size()-1) {
                cout << ", ";
            }
        }
        cout << "]" << endl;
    }
};

template<typename T> typename PersistentArray<T>::MemoeyAllocator PersistentArray<T>::ma;
template<class T> FastStack<int> PersistentArray<T>::path;

}  // namespace titan23
