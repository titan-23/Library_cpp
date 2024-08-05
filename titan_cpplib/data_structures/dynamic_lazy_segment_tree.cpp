#pragma GCC target("avx2")
#pragma GCC optimize("O3")
#pragma GCC optimize("unroll-loops")

#include <bits/stdc++.h>
using namespace std;

#include <ext/pb_ds/assoc_container.hpp>

#define rep(i, n) for (int i = 0; i < (n); ++i)

#include <iostream>
#include <cassert>
#include <memory>
using namespace std;

int cnt = 0;

namespace titan23 {

    /**
     * @brief 必要なところだけ作る遅延セグ木 / type: split
     *
     * [0, u)の列を管理
     *
     * - apply : O(logu) time, O(logu) space
     *
     * - prod  : O(logu) time, O(1) space
     *
     * - get, set は未verify
     *
     * @tparam T モノイドの型
     * @tparam (*op)(T, T) モノイドの2項演算
     * @tparam (*e)() モノイドの単位元
     * @tparam F 作用素の型
     * @tparam (*mapping)(F, T) 遅延セグ木のアレ
     * @tparam (*composition)(F, F) 遅延セグ木のアレ
     * @tparam (*id)() 遅延セグ木のアレ
     * @tparam (*pow)(T, long long) T=op(T,T)をk回繰り返した結果を返す関数
     * @tparam (*split)(T) T=op(S,S)を満たすSを返す関数
     */
    template <class T,
            T (*op)(T, T),
            T (*e)(),
            class F,
            T (*mapping)(F, T),
            F (*composition)(F, F),
            F (*id)(),
            T (*pow)(T, long long),
            T (*split)(T),
            bool use_split>
    class DynamicLazySegmentTree {
      private:
        static int bit_length(long long x) {
            return x == 0 ? 0 : 64 - __builtin_clzll(x);
        }

        struct Node;
        using NodePtr = Node*;
        // using NodePtr = shared_ptr<Node>;
        NodePtr root;
        long long u;

        struct Node {
            NodePtr left, right;
            long long l, r;
            T key, data; // 子がないときに限り、data=pow(key, r-l)が成り立つ
            F lazy;

            Node() {}

            Node(long long l, long long r, T key) :
                    left(nullptr), right(nullptr),
                    l(l), r(r),
                    key(key), data(pow(key, r-l)) {
                ++cnt;
                assert(l < r);
            }

            Node(long long l, long long r, T key, T p_data) :
                    left(nullptr), right(nullptr),
                    l(l), r(r),
                    key(key) {
                ++cnt;
                assert(l < r);
                this->data = use_split ? split(p_data) : pow(key, r-l);
            }

            bool is_leaf() const {
                return this->r - this->l == 1;
            }

            void apply(const F f) {
                if (f == id()) return;
                this->key = mapping(f, this->key);
                this->data = mapping(f, this->data);
                if (!is_leaf()) this->lazy = composition(f, this->lazy);
            }

            void propagate() {
                if (is_leaf()) return;
                if (this->left) {
                    this->left->apply(this->lazy);
                } else {
                    this->left = new Node(l, mid(), this->key, this->data);
                }
                if (this->right) {
                    this->right->apply(this->lazy);
                } else {
                    this->right = new Node(mid(), r, this->key, this->data);
                }
                this->lazy = id();
            }

            void update() {
                if (this->left) {
                    this->data = this->left->data;
                } else {
                    this->data = e();
                }
                if (this->right) {
                    this->data = op(this->data, this->right->data);
                }
            }

            long long mid() const {
                return (this->l + this->r) / 2;
            }

            void print() const {
                printf("  node : [%lld, %lld), data.val=%lld, lazy=%d\n", l, r, data.val, lazy);
            }
        };

      private:
        T inner_prod(NodePtr node, long long l, long long r) {
            if (!node || l >= r || r <= node->l || node->r <= l) {
                return e();
            }
            if (l <= node->l && node->r <= r) {
                return node->data;
            }
            if ((!node->left) && (!node->right)) {
                return pow(node->key, r - l);
            }
            assert(node->left && node->right);
            node->propagate();
            return op(
                inner_prod(node->left, l, min(r, node->mid())),
                inner_prod(node->right, max(l, node->mid()), r)
            );
        }

        void inner_apply(NodePtr node, long long l, long long r, F f) {
            if (!node || l >= r || r <= node->l || node->r <= l) {
                return;
            }
            if (l <= node->l && node->r <= r) {
                node->apply(f);
                return;
            }
            node->propagate();
            if (l < min(r, node->mid())) {
                inner_apply(node->left, l, min(r, node->mid()), f);
            }
            if (max(l, node->mid()) < r) {
                inner_apply(node->right, max(l, node->mid()), r, f);
            }
            node->update();
        }

        void inner_set(NodePtr node, long long k, T val) {
            assert(node);
            node->propagate();
            if (node->is_leaf()) {
                node->data = val;
                return;
            }
            if (k < node->mid()) {
                inner_set(node->left, k, val);
            } else {
                inner_set(node->right, k, val);
            }
            node->update();
        }

      public:

        DynamicLazySegmentTree() : root(nullptr) {}

        DynamicLazySegmentTree(const long long u_) : root(nullptr), u(1ll<<bit_length(u_)) {
            root = new Node(0, u, e());
        }

        DynamicLazySegmentTree(const long long u_, T init) : root(nullptr), u(1ll<<bit_length(u_)) {
            root = new Node(0, u, init);
        }

        // `[l, r)` の集約値を返す / `O(logu)` time, `O(1)` space
        T prod(long long l, long long r) {
            assert(0 <= l && l <= r && r < u);
            return inner_prod(root, l, r);
        }

        // `[l, r)` に `f` を作用させる / `O(logu)` time, `O(logu)` space
        void apply(long long l, long long r, F f) {
            assert(0 <= l && l <= r && r < u);
            inner_apply(root, l, r, f);
        }

        T get(long long k) {
            return prod(k, k+1);
        }

        void set(long long k, T val) {
            inner_set(root, k, val);
        }

        void print() {
            rep(i, u) cout << get(i) << ", ";
            cout << endl;
        }
    };
}  // namespace titan23


vector<int> P[200000];

using S = long long;
using F = long long;

const int bit = 30;
const int msk = (1ll << bit) - 1;

S op(S a, S b) {
    long long a0 = a >> bit, a1 = a & msk;
    long long b0 = b >> bit, b1 = b & msk;
    return (a0 + b0) << bit | (a1 + b1);
}

S e() {
    return 0;
}

S mapping(F f, S s) {
    long long s0 = s >> bit, s1 = s & msk;
    return (s0 + f*s1) << bit | s1;
}

F composition(F f, F g) {
    return f + g;
}

F id() {
    return 0;
}

S split(S s) {
    assert(false);
    // return {s.first / 2, s.second / 2};
}

S pow(S s, long long k) {
    long long s0 = s >> bit, s1 = s & msk;
    return (s0 * k) << bit | (s1 * k);
}

titan23::DynamicLazySegmentTree<S, op, e, F, mapping, composition, id, pow, split, false> PST[200000];

int main() {
  ios::sync_with_stdio(false);
  cin.tie(0);

  int n, m;
  cin >> n >> m;
  rep(i, m) {
    int t, p;
    cin >> t >> p;
    --p;
    P[p].emplace_back(t);
  }

  rep(i, n) {
    PST[i] = titan23::DynamicLazySegmentTree<S, op, e, F, mapping, composition, id, pow, split, false>(1e9+1, 1);

    bool is_valid = false;
    rep(j, int(P[i].size())-1) {
      is_valid = !is_valid;
      if (!is_valid) continue;
      int l = P[i][j], r = P[i][j+1];
      PST[i].apply(l, r, 1);
    }
  }

  int q;
  cin >> q;
  __gnu_pbds::gp_hash_table<long long, long long> memo;

  rep(_, q) {
    int a, b;
    cin >> a >> b;
    --a, --b;

    auto solve = [&] (int a, int b) -> long long {
      if (P[a].size() > P[b].size()) {
        swap(a, b);
      }

      if (memo.find((long long)a * n + b) != memo.end()) {
        return memo[(long long)a * n + b];
      }

      // a側を走査する
      long long ans = 0;
      bool is_valid_a = false;
      rep(i, int(P[a].size())-1) {
        is_valid_a = !is_valid_a;
        if (!is_valid_a) continue;
        ans += PST[b].prod(P[a][i], P[a][i+1]) >> bit;
      }

      return memo[(long long)a * n + b] = ans;
    };

    long long ans = solve(a, b);
    cout << ans << '\n';
  }

  cerr << "cnt=" << cnt << endl;
  return 0;
}
