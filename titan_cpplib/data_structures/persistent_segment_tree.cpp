#include <iostream>
#include <vector>
#include "titan_cpplib/others/print.cpp"
using namespace std;

namespace titan23 {

template<class T, T (*op)(T, T), T (*e)()>
class PersistentSegmentTree {
public:
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
            T key, data;
            Data() {}
            Data(T k, T d) : key(k), data(d) {}
        };
        // #pragma pack(pop)

        vector<Node> tree;
        vector<Data> data;
        size_t ptr, cap;

        MemoeyAllocator() : ptr(1), cap(1) {
            tree.emplace_back(0, 0);
            data.emplace_back(T{}, T{});
        }

        int copy(int node) {
            int idx = new_node(data[node].key);
            tree[idx] = {tree[node].left, tree[node].right};
            data[idx].data = data[node].data;
            return idx;
        }

        int new_node(const T key) {
            if (tree.size() > ptr) {
                tree[ptr] = {0, 0};
                data[ptr] = {key, key};
            } else {
                tree.emplace_back(0, 0);
                data.emplace_back(key, key);
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
    using PLSEG = PersistentSegmentTree<T, op, e>;
    int root;
    int _len;

    void update(int node) {
        ma.data[node].data = ma.data[node].key;
        if (ma.tree[node].left) ma.data[node].data = op(ma.data[ma.tree[node].left].data, ma.data[node].key);
        if (ma.tree[node].right) ma.data[node].data = op(ma.data[node].data, ma.data[ma.tree[node].right].data);
    }

    void _build(vector<T> const &a) {
        auto build = [&] (auto &&build, int l, int r) -> int {
            int mid = (l + r) >> 1;
            int node = ma.new_node(a[mid]);
            if (l != mid) ma.tree[node].left = build(build, l, mid);
            if (mid+1 != r) ma.tree[node].right = build(build, mid+1, r);
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
            if (l <= left && right < r) return ma.data[node].data;
            int mid = (left + right) / 2;
            T res = e();
            if (ma.tree[node].left) res = dfs(dfs, ma.tree[node].left, left, mid);
            if (l <= mid && mid < r) res = op(res, ma.data[node].key);
            if (ma.tree[node].right) res = op(res, dfs(dfs, ma.tree[node].right, mid+1, right));
            return res;
        };
        return dfs(dfs, root, 0, len());
    }

    vector<T> tovector() {
        int node = root;
        stack<int> s;
        vector<T> a;
        a.reserve(len());
        while (!s.empty() || node) {
            if (node) {
                s.emplace(node);
                node = ma.tree[node].left;
            } else {
                node = s.top(); s.pop();
                a.emplace_back(ma.data[node].key);
                node = ma.tree[node].right;
            }
        }
        return a;
    }

    PLSEG copy() {
        return PLSEG(ma.copy(root), len());
    }

    PLSEG set(int k, T v) {
        assert(0 <= k && k < len());
        int node = ma.copy(root);
        int root = node;
        int pnode = 0;
        int d = -1;
        int l = 0, r = len();
        stack<int> path; path.emplace(node);
        while (1) {
            int mid = (l + r) / 2;
            if (k == mid) {
                node = ma.copy(node);
                ma.data[node].key = v;
                if (d == -1) {
                    update(node);
                    return PLSEG(node, len());
                }
                path.emplace(node);
                if (d) ma.tree[pnode].left = node;
                else ma.tree[pnode].right = node;
                while (!path.empty()) {
                    update(path.top());
                    path.pop();
                }
                return PLSEG(root, len());
            }
            pnode = node;
            if (k < mid) {
                node = ma.copy(ma.tree[node].left);
                r = mid;
                d = 1;
            } else {
                node = ma.copy(ma.tree[node].right);
                l = mid+1;
                d = 0;
            }
            path.emplace(node);
            if (d) ma.tree[pnode].left = node;
            else ma.tree[pnode].right = node;
        }
    }

    T get(int k) {
        assert(0 <= k && k < len());
        int node = root;
        int l = 0, r = len();
        while (1) {
            int mid = (l + r) / 2;
            if (r - l == 1) {
                return ma.data[node].key;
            }
            if (k < mid) {
                node = ma.tree[node].left;
                r = mid;
            } else {
                node = ma.tree[node].right;
                l = mid+1;
            }
        }
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
            to = ma.copy(to);
            if (right <= l || r <= left) { return to; }
            if (l <= left && right < r) { return fr; }
            int mid = (left + right) / 2;
            ma.tree[to].left = dfs(dfs, ma.tree[fr].left,  ma.tree[to].left,  left, mid);
            if (l <= mid && mid < r) {
                // data,sizeはこの後updateする
                ma.data[to].key = ma.data[fr].key;
            }
            ma.tree[to].right = dfs(dfs, ma.tree[fr].right, ma.tree[to].right, mid+1, right);
            update(to);
            return to;
        };
        int new_root = dfs(dfs, ma.copy(from.root), ma.copy(root), 0, len());
        return PLSEG(new_root, len());
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

    static void rebuild(PLSEG &tree) {
        PLSEG::ma.reset();
        vector<T> a = tree.tovector();
        tree = PLSEG(a);
    }
};

template<class T, T (*op)(T, T), T (*e)()>
typename PersistentSegmentTree<T, op, e>::MemoeyAllocator PersistentSegmentTree<T, op, e>::ma;

}  // namespace titan23
