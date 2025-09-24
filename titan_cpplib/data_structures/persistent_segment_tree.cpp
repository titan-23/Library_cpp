#include <iostream>
#include <vector>
#include "titan_cpplib/others/print.cpp"
#include "titan_cpplib/data_structures/fast_stack.cpp"
using namespace std;

namespace titan23 {

template<class T, T (*op)(T, T), T (*e)()>
class PersistentSegmentTree {
public:
    static FastStack<int> path;

    struct MemoeyAllocator {

        // #pragma pack(push, 1)
        struct Node {
            int left, right;
            Node() : left(0), right(0) {}
            Node(int l, int r) : left(l), right(r) {}
        };
        // #pragma pack(pop)

        vector<Node> tree;
        vector<T> data;
        size_t ptr, cap;

        MemoeyAllocator() : ptr(1), cap(1) {
            tree.emplace_back(0, 0);
            data.emplace_back(e());
        }

        int copy(int node) {
            int idx = new_node(data[node]);
            tree[idx] = {tree[node].left, tree[node].right};
            return idx;
        }

        int new_node(const T key) {
            if (tree.size() > ptr) {
                tree[ptr] = {0, 0};
                data[ptr] = key;
            } else {
                tree.emplace_back(0, 0);
                data.emplace_back(key);
                if (tree.size() >= cap) {
                    cap++;
                }
            }
            ptr++;
            return ptr - 1;
        }

        void reserve(int n) {
            tree.reserve(n);
            data.reserve(n);
            cap += n;
        }

        void reset() {
            ptr = 1;
        }

        bool almost_full() const {
            return (double)ptr > max(1.0, cap*0.99);
        }
    };

    static MemoeyAllocator ma;

private:
    using PSEG = PersistentSegmentTree<T, op, e>;
    int root;
    int _len;

    int bit_length(int x) {
        return x ? 32 - __builtin_clz(x) : 0;
    }

    void update(int node) {
        ma.data[node] = op(ma.data[ma.tree[node].left], ma.data[ma.tree[node].right]);
    }

    void _build(vector<T> const &a) {
        auto build = [&] (auto &&build, int l, int r) -> int {
            if (r - l == 1) {
                int node = ma.new_node(a[l]);
                return node;
            }
            int mid = (l + r) / 2;
            int node = ma.new_node(e());
            ma.tree[node].left = build(build, l, mid);
            ma.tree[node].right = build(build, mid, r);
            update(node);
            return node;
        };
        _len = a.size();
        if (a.empty()) {
            root = 0;
            return;
        }
        root = build(build, 0, (int)a.size());
    }

    PersistentSegmentTree(int root, int l) : root(root), _len(l) {}

  public:
    PersistentSegmentTree() : root(0), _len(0) {}

    PersistentSegmentTree(vector<T> &a) { _build(a); }

    T prod(int l, int r) {
        assert(0 <= l && l <= r && r <= len());
        if (l == r) return e();
        auto dfs = [&] (auto &&dfs, int node, int left, int right) -> T {
            if (right <= l || r <= left) return e();
            if (l <= left && right <= r) return ma.data[node];
            int mid = (left + right) / 2;
            T res = e();
            if (ma.tree[node].left) {
                res = dfs(dfs, ma.tree[node].left, left, mid);
            }
            if (ma.tree[node].right) {
                res = op(res, dfs(dfs, ma.tree[node].right, mid, right));
            }
            return res;
        };
        return dfs(dfs, root, 0, len());
    }

    vector<T> tovector() {
        if (len() == 0) return {};
        vector<T> a(len());
        a.resize(_len);
        auto dfs = [&](auto &&dfs, int node, int l, int r) -> void {
            if (r - l == 1) {
                a[l] = ma.data[node];
                return;
            }
            int mid = (l + r) / 2;
            dfs(dfs, ma.tree[node].left, l, mid);
            dfs(dfs, ma.tree[node].right, mid, r);
        };
        dfs(dfs, root, 0, _len);
        return a;
    }

    PSEG copy() {
        return PSEG(ma.copy(root), len());
    }

    PSEG set(int k, T v) {
        assert(0 <= k && k < len());
        int new_root = ma.copy(root);
        if (len() <= 1) {
            ma.data[new_root] = v;
            return PSEG(new_root, len());
        }
        int node = new_root;
        int l = 0, r = len();
        path.clear(); path.emplace(node);
        while (r - l > 1) {
            int pnode = node;
            int mid = (l + r) / 2;
            if (k < mid) {
                node = ma.copy(ma.tree[pnode].left);
                ma.tree[pnode].left = node;
                r = mid;
            } else {
                node = ma.copy(ma.tree[pnode].right);
                ma.tree[pnode].right = node;
                l = mid;
            }
            path.emplace(node);
        }
        ma.data[node] = v;
        path.pop();
        while (!path.empty()) {
            update(path.top());
            path.pop();
        }
        return PSEG(new_root, len());
    }

    T get(int k) {
        assert(0 <= k && k < len());
        int node = root;
        int l = 0, r = len();
        while (r - l > 1) {
            int mid = (l + r) / 2;
            if (k < mid) {
                node = ma.tree[node].left;
                r = mid;
            } else {
                node = ma.tree[node].right;
                l = mid;
            }
        }
        return ma.data[node];
    }

    void print() {
        vector<T> a = tovector();
        cout << "[";
        for (int i = 0; i < (int)a.size()-1; ++i) {
            cout << a[i] << ", ";
        }
        if (!a.empty()) cout << a.back();
        cout << "]" << endl;
    }

    // fromの[l, r)を自身の[l, r)にコピーしたものを返す(自身は不変)
    PSEG copy_from(PSEG &from, int l, int r) {
        assert(0 <= l && l <= r && r <= len());
        auto dfs = [&] (auto &&dfs, int fr, int to, int left, int right) -> int {
            if (!fr && !to) return fr;
            to = ma.copy(to);
            if (right <= l || r <= left) { return to; }
            if (l <= left && right <= r) { return fr; }
            int mid = (left + right) / 2;
            // data[node].dataはこの後updateする / .lazyは伝播済み
            ma.tree[to].left = dfs(dfs, ma.tree[fr].left, ma.tree[to].left, left, mid);
            ma.tree[to].right = dfs(dfs, ma.tree[fr].right, ma.tree[to].right, mid, right);
            update(to);
            return to;
        };
        int new_root = dfs(dfs, ma.copy(from.root), ma.copy(root), 0, len());
        return PSEG(new_root, len());
    }

    int len() const {
        return _len;
    }

    friend ostream& operator<<(ostream& os, PersistentSegmentTree<T, op, e> &tree) {
        vector<T> a = tree.tovector();
        os << "[";
        for (int i = 0; i < (int)a.size()-1; ++i) {
            os << a[i] << ", ";
        }
        if (!a.empty()) os << a.back();
        os << "]";
        return os;
    }

    static void rebuild(PSEG &tree) {
        PSEG::ma.reset();
        vector<T> a = tree.tovector();
        tree = PSEG(a);
    }
};

template<class T, T (*op)(T, T), T (*e)()>
typename PersistentSegmentTree<T, op, e>::MemoeyAllocator PersistentSegmentTree<T, op, e>::ma;

template<class T, T (*op)(T, T), T (*e)()>
FastStack<int> PersistentSegmentTree<T, op, e>::path;

}  // namespace titan23
