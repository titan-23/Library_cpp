#include <iostream>
#include <vector>
#include <cmath>
#include <cassert>
#include <stack>
#include <memory>
#include "titan_cpplib/others/print.cpp"
using namespace std;

namespace titan23 {

template <class T, typename SizeType=int>
class PersistentWBTree {
public:
    struct MemoeyAllocator {
        vector<SizeType> left, right, size;
        vector<T> keys;
        size_t ptr;

        MemoeyAllocator() : ptr(1) {
            left.emplace_back(0);
            right.emplace_back(0);
            size.emplace_back(0);
            keys.emplace_back(T{});
        }

        SizeType new_node(T key) {
            if (left.size() > ptr) {
                left[ptr] = 0;
                right[ptr] = 0;
                size[ptr] = 1;
                keys[ptr] = key;
            } else {
                left.emplace_back(0);
                right.emplace_back(0);
                size.emplace_back(1);
                keys.emplace_back(key);
            }
            ptr++;
            return ptr - 1;
        }

        void reserve(SizeType cap) {
            left.reserve(cap);
            right.reserve(cap);
            keys.reserve(cap);
            size.reserve(cap);
        }

        void reset() {
            ptr = 1;
        }
    };

    static MemoeyAllocator ma;

private:
    using PTM = PersistentWBTree<T, SizeType>;
    static constexpr int DELTA = 3;
    static constexpr int GAMMA = 2;
    SizeType root;

    static SizeType new_node(T key) {
        if (ma.left.size() > ma.ptr) {
            ma.left[ma.ptr] = 0;
            ma.right[ma.ptr] = 0;
            ma.size[ma.ptr] = 1;
            ma.keys[ma.ptr] = key;
        } else {
            ma.left.emplace_back(0);
            ma.right.emplace_back(0);
            ma.size.emplace_back(1);
            ma.keys.emplace_back(key);
        }
        ma.ptr++;
        return ma.ptr-1;
    }

    SizeType copy(SizeType node) const {
        SizeType v = new_node(ma.keys[node]);
        ma.left[v] = ma.left[node];
        ma.right[v] = ma.right[node];
        ma.size[v] = ma.size[node];
        return v;
    }

    SizeType weight_right(SizeType node) const {
        return ma.size[ma.right[node]];
    }

    SizeType weight_left(SizeType node) const {
        return ma.size[ma.left[node]];
    }

    void update(SizeType node) {
        ma.size[node] = 1 + ma.size[ma.left[node]] + ma.size[ma.right[node]];
    }

    void _build(vector<T> const &a) {
        auto build = [&] (auto &&build, SizeType l, SizeType r) -> SizeType {
            SizeType mid = (l + r) >> 1;
            SizeType node = new_node(a[mid]);
            if (l != mid) ma.left[node] = build(build, l, mid);
            if (mid+1 != r) ma.right[node] = build(build, mid+1, r);
            update(node);
            return node;
        };
        root = build(build, 0, (SizeType)a.size());
    }

    SizeType _rotate_right(SizeType node) {
        SizeType u = copy(ma.left[node]);
        ma.left[node] = ma.right[u];
        ma.right[u] = node;
        update(node);
        update(u);
        return u;
    }

    SizeType _rotate_left(SizeType node) {
        SizeType u = copy(ma.right[node]);
        ma.right[node] = ma.left[u];
        ma.left[u] = node;
        update(node);
        update(u);
        return u;
    }

    SizeType _balance_left(SizeType node) {
        SizeType u = ma.right[node];
        if (weight_left(ma.right[node]) >= weight_right(ma.right[node]) * GAMMA) {
            ma.right[node] = _rotate_right(u);
        }
        u = _rotate_left(node);
        return u;
    }

    SizeType _balance_right(SizeType node) {
        SizeType u = ma.left[node];
        if (weight_right(ma.left[node]) >= weight_left(ma.left[node]) * GAMMA) {
            ma.left[node] = _rotate_left(u);
        }
        u = _rotate_right(node);
        return u;
    }

    SizeType weight(SizeType node) const {
        return ma.size[node] + 1;
    }

    SizeType _merge_with_root(SizeType l, SizeType root, SizeType r) {
        if (weight(r) * DELTA < weight(l)) {
            l = copy(l);
            ma.right[l] = _merge_with_root(ma.right[l], root, r);
            update(l);
            if (weight(ma.left[l]) * DELTA < weight(ma.right[l])) {
                return _balance_left(l);
            }
            return l;
        } else if (weight(l) * DELTA < weight(r)) {
            r = copy(r);
            ma.left[r] = _merge_with_root(l, root, ma.left[r]);
            update(r);
            if (weight(ma.right[r]) * DELTA < weight(ma.left[r])) {
                return _balance_right(r);
            }
            return r;
        }
        root = copy(root);
        ma.left[root] = l;
        ma.right[root] = r;
        update(root);
        return root;
    }

    pair<SizeType, SizeType> _pop_right(SizeType node) {
        return _split_node(node, ma.size[node]-1);
    }

    SizeType _merge_node(SizeType l, SizeType r) {
        if ((!l) && (!r)) { return 0; }
        if (!l) { return copy(r); }
        if (!r) { return copy(l); }
        l = copy(l);
        r = copy(r);
        auto [l_, root_] = _pop_right(l);
        return _merge_with_root(l_, root_, r);
    }

    pair<SizeType, SizeType> _split_node(SizeType node, SizeType k) {
        if (!node) { return {0, 0}; }
        SizeType lch = ma.left[node], rch = ma.right[node];
        SizeType tmp = lch ? k-ma.size[lch] : k;
        if (tmp == 0) {
            return {lch, _merge_with_root(0, node, rch)};
        } else if (tmp < 0) {
            auto [l, r] = _split_node(lch, k);
            return {l, _merge_with_root(r, node, rch)};
        } else {
            auto [l, r] = _split_node(rch, tmp-1);
            return {_merge_with_root(lch, node, l), r};
        }
    }

    SizeType _split_node_left(SizeType node, SizeType k) {
        if (!node) { return 0; }
        SizeType lch = ma.left[node], rch = ma.right[node];
        SizeType tmp = lch ? k-ma.size[lch] : k;
        if (tmp == 0) {
            return lch;
        } else if (tmp < 0) {
            auto l = _split_node_left(lch, k);
            return l;
        } else {
            auto l = _split_node_left(rch, tmp-1);
            return _merge_with_root(lch, node, l);
        }
    }

    SizeType _split_node_right(SizeType node, SizeType k) {
        if (!node) { return 0; }
        SizeType lch = ma.left[node], rch = ma.right[node];
        SizeType tmp = lch ? k-ma.size[lch] : k;
        if (tmp == 0) {
            return _merge_with_root(0, node, rch);
        } else if (tmp < 0) {
            auto r = _split_node_right(lch, k);
            return _merge_with_root(r, node, rch);
        } else {
            auto r = _split_node_right(rch, tmp-1);
            return r;
        }
    }

    PTM _new(SizeType root) {
        return PTM(root);
    }

    PersistentWBTree(SizeType root) : root(root) {}

  public:
    PersistentWBTree() : root(0) {}

    PersistentWBTree(vector<T> &a) { _build(a); }

    PTM merge(PTM other) {
        SizeType root = _merge_node(this->root, other.root);
        return _new(root);
    }

    pair<PTM, PTM> split(SizeType k) {
        auto [l, r] = _split_node(this->root, k);
        return {_new(l), _new(r)};
    }

    PTM split_left(SizeType k) {
        auto l = _split_node_left(this->root, k);
        return _new(l);
    }

    PTM split_right(SizeType k) {
        auto r = _split_node_right(this->root, k);
        return _new(r);
    }

    PTM insert(SizeType k, T key) {
        assert(0 <= k && k <= len());
        auto [s, t] = _split_node(root, k);
        SizeType v = new_node(key);
        return _new(_merge_with_root(s, v, t));
    }

    pair<PTM, T> pop(SizeType k) {
        assert(0 <= k && k < len());
        auto [s_, t] = _split_node(this->root, k+1);
        auto [s, tmp] = _pop_right(s_);
        SizeType root = _merge_node(s, t);
        return {_new(root), ma.keys[tmp]};
    }

    vector<T> tovector() {
        SizeType node = root;
        stack<SizeType> s;
        vector<T> a;
        a.reserve(len());
        while (!s.empty() || node) {
            if (node) {
                s.emplace(node);
                node = ma.left[node];
            } else {
                node = s.top(); s.pop();
                a.emplace_back(ma.keys[node]);
                node = ma.right[node];
            }
        }
        return a;
    }

    PTM copy() {
        return _new(copy(root));
    }

    T get(SizeType k) {
        assert(0 <= k && k < len());
        SizeType node = root;
        while (1) {
            SizeType t = ma.size[ma.left[node]];
            if (t == k) {
                return ma.keys[node];
            }
            if (t < k) {
                k -= t + 1;
                node = ma.right[node];
            } else {
                node = ma.left[node];
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

    SizeType len() const {
        return ma.size[root];
    }

    static void reserve(SizeType cap) {
        ma.reserve(cap);
    }

    friend ostream& operator<<(ostream& os, PTM &tree) {
        vector<T> a = tree.tovector();
        os << "[";
        for (int i = 0; i < (int)a.size()-1; ++i) {
            os << a[i] << ", ";
        }
        if (!a.empty()) os << a.back();
        os << "]";
        return os;
    }
};

template <class T, typename SizeType>
typename PersistentWBTree<T, SizeType>::MemoeyAllocator PersistentWBTree<T, SizeType>::ma;

template <class T, typename SizeType>
void rebuild_ptm(PersistentWBTree<T, SizeType> &tree) {
    vector<T> a = tree.tovector();
    PersistentWBTree<T, SizeType>::ma.reset();
    tree = PersistentWBTree<T, SizeType>(a);
}

}  // namespace titan23
