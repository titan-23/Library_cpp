#include <iostream>
#include <vector>
#include <cassert>
#include "titan_cpplib/others/print.cpp"
#include "titan_cpplib/ds/fast_stack.cpp"
using namespace std;

namespace titan23 {

template <class T, class F,
        T (*op)(T, T),
        T (*mapping)(F, T),
        F (*composition)(F, F),
        T (*e)(),
        F (*id)()>
class PersistentLazySegmentTree {
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

        // #pragma pack(push, 1)
        struct Data {
            T data; F lazy;;
            Data() {}
            Data(T d, F l) : data(std::move(d)), lazy(std::move(l)) {}
        };
        // #pragma pack(pop)

        vector<Node> tree;
        vector<Data> data;
        size_t ptr, cap;

        MemoeyAllocator() : ptr(1), cap(1) {
            tree.emplace_back(0, 0);
            data.emplace_back(e(), id());
        }

        int copy(int node) {
            int idx = new_node(data[node].data, data[node].lazy);
            tree[idx].left = tree[node].left;
            tree[idx].right = tree[node].right;
            return idx;
        }

        int new_node(const T key, const F f) {
            if (tree.size() > ptr) {
                tree[ptr] = {0, 0};
                data[ptr] = {key, f};
            } else {
                tree.emplace_back(0, 0);
                data.emplace_back(key, f);
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
    using PLSEG = PersistentLazySegmentTree<T, F, op, mapping, composition, e, id>;
    int root;
    int _len;

    void update(int node) {
        ma.data[node].data = op(ma.data[ma.tree[node].left].data, ma.data[ma.tree[node].right].data);
    }

    void push(int node, F f) {
        ma.data[node].data = mapping(f, ma.data[node].data);
        if (ma.tree[node].left) {
            ma.data[node].lazy = composition(f, ma.data[node].lazy);  // 葉なら不要
        }
    }

    void propagate(int node) {
        if (ma.data[node].lazy == id()) return;
        if (ma.tree[node].left) {
            ma.tree[node].left = ma.copy(ma.tree[node].left);
            push(ma.tree[node].left, ma.data[node].lazy);
        }
        if (ma.tree[node].right) {
            ma.tree[node].right = ma.copy(ma.tree[node].right);
            push(ma.tree[node].right, ma.data[node].lazy);
        }
        ma.data[node].lazy = id();
    }

    void _build(vector<T> const &a) {
        auto build = [&] (auto &&build, int l, int r) -> int {
            if (r - l == 1) {
                int node = ma.new_node(a[l], id());
                return node;
            }
            int mid = (l + r) / 2;
            int node = ma.new_node(e(), id());
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

    PersistentLazySegmentTree(int root, int l) : root(root), _len(l) {}

  public:
    PersistentLazySegmentTree() : root(0), _len(0) {}
    PersistentLazySegmentTree(vector<T> &a) { _build(a); }

    PLSEG apply(int l, int r, F f) {
        assert(0 <= l && l <= r && r <= len());
        if (l == r) return PLSEG(ma.copy(root), len());
        auto dfs = [&] (auto &&dfs, int node, int left, int right) -> int {
            if (right <= l || r <= left) return node;
            propagate(node);
            int nnode = ma.copy(node);
            if (l <= left && right <= r) {
                push(nnode, f);
                return nnode;
            }
            int mid = (left + right) / 2;
            if (ma.tree[nnode].left) ma.tree[nnode].left = dfs(dfs, ma.tree[nnode].left, left, mid);
            if (ma.tree[nnode].right) ma.tree[nnode].right = dfs(dfs, ma.tree[nnode].right, mid, right);
            update(nnode);
            return nnode;
        };
        return PLSEG(dfs(dfs, root, 0, len()), len());
    }

    T prod(int l, int r) {
        assert(0 <= l && l <= r && r <= len());
        if (l == r) return e();
        auto dfs = [&] (auto &&dfs, int node, int left, int right) -> T {
            if (right <= l || r <= left) return e();
            if (l <= left && right <= r) return ma.data[node].data;
            propagate(node);
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
            propagate(node);
            if (r - l == 1) {
                a[l] = ma.data[node].data;
                return;
            }
            int mid = (l + r) / 2;
            dfs(dfs, ma.tree[node].left, l, mid);
            dfs(dfs, ma.tree[node].right, mid, r);
        };
        dfs(dfs, root, 0, _len);
        return a;
    }

    PLSEG copy() {
        return PLSEG(ma.copy(root), len());
    }

    PLSEG set(int k, T v) {
        assert(0 <= k && k < len());
        propagate(root);
        int new_root = ma.copy(root);
        if (len() <= 1) {
            ma.data[new_root].data = v;
            return PLSEG(new_root, len());
        }
        int node = new_root;
        int l = 0, r = len();
        path.clear(); path.emplace(node);
        while (r - l > 1) {
            propagate(node);
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
        ma.data[node].data = v;
        path.pop();
        while (!path.empty()) {
            update(path.top());
            path.pop();
        }
        return PLSEG(new_root, len());
    }

    T get(int k) {
        assert(0 <= k && k < len());
        int node = root;
        int l = 0, r = len();
        while (r - l > 1) {
            propagate(node);
            int mid = (l + r) / 2;
            if (k < mid) {
                node = ma.tree[node].left;
                r = mid;
            } else {
                node = ma.tree[node].right;
                l = mid;
            }
        }
        return ma.data[node].data;
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
    PLSEG copy_from(PLSEG &from, int l, int r) {
        assert(0 <= l && l <= r && r <= len());
        auto dfs = [&] (auto &&dfs, int fr, int to, int left, int right) -> int {
            if (!fr && !to) return fr;
            propagate(fr); propagate(to);
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
        return PLSEG(new_root, len());
    }

    int len() const {
        return _len;
    }

    friend ostream& operator<<(ostream& os, PersistentLazySegmentTree<T, F, op, mapping, composition, e, id> &tree) {
        vector<T> a = tree.tovector();
        os << "[";
        for (int i = 0; i < (int)a.size()-1; ++i) {
            os << a[i] << ", ";
        }
        if (!a.empty()) os << a.back();
        os << "]";
        return os;
    }

    static void rebuild(PLSEG &tree) {
        PLSEG::ma.reset();
        vector<T> a = tree.tovector();
        tree = PLSEG(a);
    }
};

template<class T, class F,
        T (*op)(T, T),
        T (*mapping)(F, T),
        F (*composition)(F, F),
        T (*e)(),
        F (*id)()>
typename PersistentLazySegmentTree<T, F, op, mapping, composition, e, id>::MemoeyAllocator PersistentLazySegmentTree<T, F, op, mapping, composition, e, id>::ma;

template<class T, class F,
        T (*op)(T, T),
        T (*mapping)(F, T),
        F (*composition)(F, F),
        T (*e)(),
        F (*id)()>
FastStack<int> PersistentLazySegmentTree<T, F, op, mapping, composition, e, id>::path;

}  // namespace titan23
