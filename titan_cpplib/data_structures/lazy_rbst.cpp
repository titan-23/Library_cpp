#include <iostream>
#include <stack>
#include "titan_cpplib/algorithm/random.cpp"
#include "titan_cpplib/data_structures/fast_stack.cpp"
// #include "titan_cpplib/others/print.cpp"
using namespace std;

namespace titan23 {

template <class T, T (*op)(T, T), T (*e)(),
          class F, T (*mapping)(F, T), F (*composition)(F, F), F (*id)()>
class LazyRBST {
    class Node;
    using NodePtr = Node*;
    using RBST = LazyRBST<T, op, e, F, mapping, composition, id>;
    NodePtr root;
    static titan23::Random trnd;
    static titan23::FastStack<NodePtr> left_path, right_path, path;

    class Node {
    public:
        NodePtr left, right;
        T key, data;
        F lazy;
        bool rev, has_lazy;
        int size;

        Node(T key, F lazy) : left(nullptr), right(nullptr), key(key), data(key), lazy(lazy), rev(false), has_lazy(false), size(1) {}

        void update() {
            size = 1;
            data = key;
            if (left) {
                size += left->size;
                data = op(left->data, data);
            }
            if (right) {
                size += right->size;
                data = op(data, right->data);
            }
        }

        void push(F f) {
            key = mapping(f, key);
            data = mapping(f, data);
            lazy = composition(f, lazy);
            has_lazy = true;
        }

        void propagate() {
            if (rev) {
                swap(left, right);
                if (left) left->rev ^= 1;
                if (right) right->rev ^= 1;
                rev = 0;
            }
            if (has_lazy) {
                if (left) { left->push(lazy); }
                if (right) { right->push(lazy); }
                lazy = id();
                has_lazy = false;
            }
        }
    };

    void _build(vector<T> const &a) {
        auto build = [&] (auto &&build, int l, int r) -> NodePtr {
            int mid = (l + r) >> 1;
            NodePtr node = new Node(a[mid], id());
            if (l != mid) node->left = build(build, l, mid);
            if (mid+1 != r) node->right = build(build, mid+1, r);
            node->update();
            return node;
        };
        if (a.empty()) {
            this->root = nullptr;
            return;
        }
        this->root = build(build, 0, (int)a.size());
    }

    void _update_lr(NodePtr l, NodePtr r) {
        l->size += r->size;
        l->data = op(l->data, r->data);
    }

    NodePtr _merge_node(NodePtr l, NodePtr r) {
        NodePtr root = nullptr, r_root = nullptr;
        int d = -1;
        while (l && r) {
            int nd = trnd.randrange(l->size + r->size) < l->size;
            NodePtr node = nd ? l : r;
            node->propagate();
            if (!root) {
                r_root = node;
            } else if (d == 0) {
                root->left = node;
            } else {
                root->right = node;
            }
            root = node;
            d = nd;
            if (d) {
                _update_lr(l, r);
                l = l->right;
            } else {
                _update_lr(r, l);
                r = r->left;
            }
        }
        if (!root) {
            return l ? l : r;
        }
        (d ? root->right : root->left) = l ? l : r;
        return r_root;
    }

    pair<NodePtr, NodePtr> _split_node(NodePtr node, int k) {
        left_path.clear(); right_path.clear();
        while (node) {
            node->propagate();
            int s = node->left ? k-node->left->size : k;
            if (s <= 0) {
                right_path.emplace(node);
                node = node->left;
            } else {
                k = s-1;
                left_path.emplace(node);
                node = node->right;
            }
        }
        NodePtr l = nullptr, r = nullptr;
        while (!left_path.empty()) {
            NodePtr node = left_path.top(); left_path.pop();
            node->right = l;
            l = node;
            l->update();
        }
        while (!right_path.empty()) {
            NodePtr node = right_path.top(); right_path.pop();
            node->left = r;
            r = node;
            r->update();
        }
        return {l, r};
    }

    LazyRBST(NodePtr node) : root(node) {}

public:
    LazyRBST() : root(nullptr) {}
    LazyRBST(const vector<T> &a) : root(nullptr) { _build(a); }

    void merge(RBST &other) {
        root = _merge_node(root, other.root);
    }

    pair<RBST, RBST> split(int k) {
        auto [l, r] = _split_node(root, k);
        return {RBST(l), RBST(r)};
    }

    void apply(int l, int r, F f) {
        assert(0 <= l && l <= r && r <= len());
        if (l == r) return;
        auto dfs = [&] (auto &&dfs, NodePtr node, int left, int right) -> void {
            if (right <= l || r <= left) return;
            if (l <= left && right < r) {
                node->push(f);
                return;
            }
            node->propagate();
            int lsize = node->left ? node->left->size : 0;
            if (node->left) dfs(dfs, node->left, left, left+lsize);
            if (l <= left+lsize && left+lsize < r) node->key = mapping(f, node->key);
            if (node->right) dfs(dfs, node->right, left+lsize+1, right);
            node->update();
        };
        dfs(dfs, root, 0, len());
    }

    T all_prod() const {
        return this->root ? this->root->data : e();
    }

    T prod(int l, int r) {
        assert(0 <= l && l <= r && r <= len());
        if (l == r) return e();
        auto dfs = [&] (auto &&dfs, NodePtr node, int left, int right) -> T {
            if (right <= l || r <= left) return e();
            if (l <= left && right < r) return node->data;
            node->propagate();
            int lsize = node->left ? node->left->size : 0;
            T res = e();
            if (node->left) res = dfs(dfs, node->left, left, left+lsize);
            if (l <= left+lsize && left+lsize < r) res = op(res, node->key);
            if (node->right) res = op(res, dfs(dfs, node->right, left+lsize+1, right));
            return res;
        };
        return dfs(dfs, root, 0, len());
    }

    void insert(int k, T key) {
        assert(0 <= k && k <= len());
        auto [s, t] = _split_node(root, k);
        NodePtr node = new Node(key, id());
        root = _merge_node(_merge_node(s, node), t);
    }

    T pop(int k) {
        assert(0 <= k && k < len());
        auto [s, t] = _split_node(root, k + 1);
        auto [l, tmp] = _split_node(s, s->size - 1);
        T res = tmp->key;
        root = _merge_node(l, t);
        return res;
    }

    void reverse(int l, int r) {
        assert(0 <= l && l <= r && r <= len());
        if (l == r) return;
        auto [s2, t] = _split_node(root, r);
        auto [u, s] = _split_node(s2, l);
        s->rev ^= 1;
        root = _merge_node(_merge_node(u, s), t);
    }

    vector<T> tovector() {
        NodePtr node = root;
        stack<NodePtr> s;
        vector<T> a; a.reserve(len());
        while (!s.empty() || node) {
            if (node) {
                node->propagate();
                s.emplace(node);
                node = node->left;
            } else {
                node = s.top(); s.pop();
                a.emplace_back(node->key);
                node = node->right;
            }
        }
        return a;
    }

    T get(int k) {
        assert(0 <= k && k < len());
        NodePtr node = root;
        while (1) {
            node->propagate();
            int t = node->left ? node->left->size : 0;
            if (t == k) {
                return node->key;
            } else if (t < k) {
                k -= t + 1;
                node = node->right;
            } else {
                node = node->left;
            }
        }
    }

    void set(int k, T key) {
        assert(0 <= k && k < len());
        NodePtr node = root;
        path.clear();
        while (1) {
            node->propagate(node);
            path.emplace(node);
            int t = node->left ? node->left->size : 0;
            if (t == k) {
                node->key = key;
                break;
            } else if (t < k) {
                k -= t + 1;
                node = node->right;
            } else {
                node = node->left;
            }
        }
        while (!path.empty()) {
            NodePtr node = path.top(); path.pop();
            node->update();
        }
    }

    int len() const {
        return root ? root->size : 0;
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

    int height() {
        auto dfs = [&] (auto &&dfs, NodePtr node) -> int {
            if (!node) return 0;
            return max(dfs(dfs, node->left), dfs(dfs, node->right)) + 1;
        };
        return dfs(dfs, root);
    }

    void rebuild() {
        vector<T> a = tovector();
        _build(a);
    }
};

template <class T, T (*op)(T, T), T (*e)(), class F, T (*mapping)(F, T), F (*composition)(F, F), F (*id)()>
Random LazyRBST<T, op, e, F, mapping, composition, id>::trnd;

template <class T, T (*op)(T, T), T (*e)(), class F, T (*mapping)(F, T), F (*composition)(F, F), F (*id)()>
FastStack<typename LazyRBST<T, op, e, F, mapping, composition, id>::Node*> LazyRBST<T, op, e, F, mapping, composition, id>::left_path;

template <class T, T (*op)(T, T), T (*e)(), class F, T (*mapping)(F, T), F (*composition)(F, F), F (*id)()>
FastStack<typename LazyRBST<T, op, e, F, mapping, composition, id>::Node*> LazyRBST<T, op, e, F, mapping, composition, id>::right_path;

template <class T, T (*op)(T, T), T (*e)(), class F, T (*mapping)(F, T), F (*composition)(F, F), F (*id)()>
FastStack<typename LazyRBST<T, op, e, F, mapping, composition, id>::Node*> LazyRBST<T, op, e, F, mapping, composition, id>::path;
} // namespace titan23




// ------------------------

#include <bits/stdc++.h>
using ll = long long;
#define rep(i, n) for (int i = 0; i < (ll)(n); ++i)

static constexpr ll mod = 998244353;
static constexpr ll bit = 30;
static constexpr ll msk = (1<<bit)-1;

ll op(const ll s, const ll t) {
    ll s1 = s>>bit, s2 = s&msk;
    ll t1 = t>>bit, t2 = t&msk;
    ll c1 = (s1 + t1) % mod;
    ll c2 = (s2 + t2) % mod;
    return (c1 << 30) + c2;
}

ll mapping(const ll f, const ll s) {
    ll f1 = f>>bit, f2 = f&msk;
    ll s1 = s>>bit, s2 = s&msk;
    return (((s1 * f1 + s2 * f2) % mod) << 30) + s2;
}

ll composition(const ll f, const ll g) {
    ll f1 = f>>bit, f2 = f&msk;
    ll g1 = g>>bit, g2 = g&msk;
    ll z1 = (f1 * g1) % mod;
    ll z2 = (f1 * g2 + f2) % mod;
    return (z1 << 30) + z2;
}

ll e() {
    return 0;
}

ll id() {
    return 1<<bit;
}

void dynamic_sequence_range_affine_range_sum() {
    int n, q;
    cin >> n >> q;
    vector<ll> A(n);
    rep(i, n) {
        ll a;
        cin >> a;
        A[i] = a<<bit|1ll;
    }

    using Tree = titan23::LazyRBST<ll, op, e, ll, mapping, composition, id>;
    Tree s(A);

    rep(qdx, q) {
        int com; cin >> com;
        if (com == 0) {
            int i;
            ll x;
            cin >> i >> x;
            s.insert(i, x<<bit|1ll);
        } else if (com == 1) {
            int i;
            cin >> i;
            s.pop(i);
        } else if (com == 2) {
            int l, r;
            cin >> l >> r;
            s.reverse(l, r);
        } else if (com == 3) {
            int l, r;
            ll b, c;
            cin >> l >> r >> b >> c;
            s.apply(l, r, b<<bit|c);
        } else {
            int l, r;
            cin >> l >> r;
            ll res = s.prod(l, r)>>bit;
            cout << res << '\n';
        }
    }
    // cerr << s.height() << endl;
}

int main() {
    ios::sync_with_stdio(false);
    cin.tie(0);
    dynamic_sequence_range_affine_range_sum();
    return 0;
}
