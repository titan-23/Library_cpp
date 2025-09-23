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

        // #pragma pack(push, 1)
        struct Node {
            SizeType left, right, size;
            Node() {}
            Node(SizeType l, SizeType r, SizeType s) : left(l), right(r), size(s) {}
        };
        // #pragma pack(pop)

        vector<Node> d;
        vector<T> keys;
        SizeType ptr;

        MemoeyAllocator() : ptr(1) {
            d.resize(1);
            d[0] = {0, 0, 0};
            keys.resize(1);
        }

        SizeType new_node(const T key) {
            if (d.size() > ptr) {
                keys[ptr] = key;
                d[ptr].left = 0;
                d[ptr].right = 0;
                d[ptr].size = 1;
            } else {
                keys.emplace_back(key);
                d.emplace_back(0, 0, 1);
            }
            ptr++;
            return ptr - 1;
        }

        SizeType copy(SizeType node) {
            SizeType v = new_node(keys[node]);
            d[v].left = d[node].left;
            d[v].right = d[node].right;
            d[v].size = d[node].size;
            return v;
        }

        void reserve(SizeType cap) {
            keys.reserve(cap);
            d.reserve(cap);
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

    inline SizeType weight_right(SizeType node) const { return ma.d[ma.d[node].right].size + 1; }

    inline SizeType weight_left(SizeType node) const { return ma.d[ma.d[node].left].size + 1; }

    inline SizeType weight(SizeType node) const { return ma.d[node].size + 1; }

    void update(SizeType node) {
        ma.d[node].size = 1 + ma.d[ma.d[node].left].size + ma.d[ma.d[node].right].size;
    }

    void _build(vector<T> const &a) {
        auto build = [&] (auto &&build, SizeType l, SizeType r) -> SizeType {
            SizeType mid = (l + r) >> 1;
            SizeType node = ma.new_node(a[mid]);
            if (l != mid) ma.d[node].left = build(build, l, mid);
            if (mid+1 != r) ma.d[node].right = build(build, mid+1, r);
            update(node);
            return node;
        };
        if (a.empty()) {
            root = 0;
            return;
        }
        root = build(build, 0, (SizeType)a.size());
    }

    SizeType _rotate_right(SizeType node) {
        SizeType u = ma.copy(ma.d[node].left);
        ma.d[node].left = ma.d[u].right;
        ma.d[u].right = node;
        update(node);
        update(u);
        return u;
    }

    SizeType _rotate_left(SizeType node) {
        SizeType u = ma.copy(ma.d[node].right);
        ma.d[node].right = ma.d[u].left;
        ma.d[u].left = node;
        update(node);
        update(u);
        return u;
    }

    SizeType _balance_left(SizeType node) {
        SizeType u = ma.d[node].right;
        if (weight_left(u) >= weight_right(u) * GAMMA) {
            ma.d[node].right = _rotate_right(u);
        }
        u = _rotate_left(node);
        return u;
    }

    SizeType _balance_right(SizeType node) {
        SizeType u = ma.d[node].left;
        if (weight_right(u) >= weight_left(u) * GAMMA) {
            ma.d[node].left = _rotate_left(u);
        }
        u = _rotate_right(node);
        return u;
    }

    SizeType _merge_with_root(SizeType l, SizeType root, SizeType r) {
        if (weight(r) * DELTA < weight(l)) {
            l = ma.copy(l);
            ma.d[l].right = _merge_with_root(ma.d[l].right, root, r);
            update(l);
            if (weight(ma.d[l].left) * DELTA < weight(ma.d[l].right)) {
                return _balance_left(l);
            }
            return l;
        } else if (weight(l) * DELTA < weight(r)) {
            r = ma.copy(r);
            ma.d[r].left = _merge_with_root(l, root, ma.d[r].left);
            update(r);
            if (weight(ma.d[r].right) * DELTA < weight(ma.d[r].left)) {
                return _balance_right(r);
            }
            return r;
        }
        root = ma.copy(root);
        ma.d[root].left = l;
        ma.d[root].right = r;
        update(root);
        return root;
    }

    pair<SizeType, SizeType> _pop_right(SizeType node) {
        return _split_node(node, ma.d[node].size-1);
    }

    SizeType _merge_node(SizeType l, SizeType r) {
        if ((!l) && (!r)) { return 0; }
        if (!l) { return ma.copy(r); }
        if (!r) { return ma.copy(l); }
        l = ma.copy(l);
        r = ma.copy(r);
        auto [l_, root_] = _pop_right(l);
        return _merge_with_root(l_, root_, r);
    }

    pair<SizeType, SizeType> _split_node(SizeType node, SizeType k) {
        if (!node) { return {0, 0}; }
        SizeType lch = ma.d[node].left, rch = ma.d[node].right;
        SizeType tmp = lch ? k-ma.d[lch].size : k;
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
        SizeType lch = ma.d[node].left, rch = ma.d[node].right;
        SizeType tmp = lch ? k-ma.d[lch].size : k;
        if (tmp == 0) {
            return lch;
        } else if (tmp < 0) {
            SizeType l = _split_node_left(lch, k);
            return l;
        } else {
            SizeType l = _split_node_left(rch, tmp-1);
            return _merge_with_root(lch, node, l);
        }
    }

    SizeType _split_node_right(SizeType node, SizeType k) {
        if (!node) { return 0; }
        SizeType lch = ma.d[node].left, rch = ma.d[node].right;
        SizeType tmp = lch ? k-ma.d[lch].size : k;
        if (tmp == 0) {
            return _merge_with_root(0, node, rch);
        } else if (tmp < 0) {
            SizeType r = _split_node_right(lch, k);
            return _merge_with_root(r, node, rch);
        } else {
            SizeType r = _split_node_right(rch, tmp-1);
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
        SizeType l = _split_node_left(this->root, k);
        return _new(l);
    }

    PTM split_right(SizeType k) {
        SizeType r = _split_node_right(this->root, k);
        return _new(r);
    }

    PTM insert(SizeType k, T key) {
        assert(0 <= k && k <= len());
        auto [s, t] = _split_node(root, k);
        SizeType v = ma.new_node(key);
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
                node = ma.d[node].left;
            } else {
                node = s.top(); s.pop();
                a.emplace_back(ma.keys[node]);
                node = ma.d[node].right;
            }
        }
        return a;
    }

    PTM copy() const {
        return _new(ma.copy(root));
    }

    T get(SizeType k) const {
        assert(0 <= k && k < len());
        SizeType node = root;
        while (1) {
            SizeType t = ma.d[ma.d[node].left].size;
            if (t == k) {
                return ma.keys[node];
            }
            if (t < k) {
                k -= t + 1;
                node = ma.d[node].right;
            } else {
                node = ma.d[node].left;
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
        return ma.d[root].size;
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

    static void rebuild(PTM &tree) {
        PTM::ma.reset();
        vector<T> a = tree.tovector();
        tree = PTM(a);
    }
};

template <class T, typename SizeType>
typename PersistentWBTree<T, SizeType>::MemoeyAllocator PersistentWBTree<T, SizeType>::ma;

}  // namespace titan23
