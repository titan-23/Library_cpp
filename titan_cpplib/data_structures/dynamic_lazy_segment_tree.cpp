#include <iostream>
#include <cassert>
#include <memory>

#include "titan_cpplib/others/print.cpp"
using namespace std;

// DynamicLazySegmentTree
namespace titan23 {

    /**
     * @brief `[0, u)` の列を管理する、必要なところだけ作る遅延セグ木
     *
     * - apply / set : `O(logu)` time, `O(logu)` space
     *
     * - prod  / get : `O(logu)` time, `O(1)` space
     *
     * @tparam IndexType 添え字を表すインデックス long long を推奨 和がオーバーフローしないことが条件かな
     * @tparam T モノイドの型
     * @tparam (*op)(T, T) モノイドの2項演算
     * @tparam (*e)() モノイドの単位元
     * @tparam F 作用素の型
     * @tparam (*mapping)(F, T) 遅延セグ木のアレ
     * @tparam (*composition)(F, F) 遅延セグ木のアレ
     * @tparam (*id)() 遅延セグ木のアレ
     * @tparam (*pow)(T, IndexType) T=op(T,T)をk回繰り返した結果を返す関数
     */
    template <class IndexType,
            class T,
            T (*op)(T, T),
            T (*e)(),
            class F,
            T (*mapping)(F, T),
            F (*composition)(F, F),
            F (*id)(),
            T (*pow)(T, IndexType)>
    class DynamicLazySegmentTree {
      private:
        static int bit_length(long long x) {
            return x == 0 ? 0 : 64 - __builtin_clzll(x);
        }

        struct Node;
        using NodePtr = Node*;
        // using NodePtr = shared_ptr<Node>;
        NodePtr root;
        IndexType u;

        struct Node {
            NodePtr left, right;
            IndexType l, r;
            T key, data; // 子がないとき、data=pow(key, r-l)が成り立つ
            F lazy;

            Node() {}

            Node(IndexType l, IndexType r, T key) :
                    left(nullptr), right(nullptr),
                    l(l), r(r),
                    key(key), data(pow(key, r-l)) {
                    // key(key), data(r-l == 1 ? key : pow(key, r-l)) {
                assert(l < r);
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
                    this->left = new Node(l, mid(), this->key);
                }
                if (this->right) {
                    this->right->apply(this->lazy);
                } else {
                    this->right = new Node(mid(), r, this->key);
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

            IndexType mid() const {
                return (this->l + this->r) / 2;
            }
        };

      private:
        T inner_prod(NodePtr node, const IndexType l, const IndexType r) {
            if (!node || l >= r || r <= node->l || node->r <= l) return e();
            if (l <= node->l && node->r <= r) return node->data;
            if ((!node->left) && (!node->right)) return pow(node->key, r - l);
            node->propagate();
            return op(
                inner_prod(node->left, l, min(r, node->mid())),
                inner_prod(node->right, max(l, node->mid()), r)
            );
        }

        T inner_prod2(NodePtr node, const IndexType l, const IndexType r, const F f) const {
            // cerr << PRINT_RED << "titan_cpplib-Error: this method has not been impremented yet.\n";
            // cerr << "Do not use prod2(), use prod()." << PRINT_NONE << endl;
            // assert(false); // NotImeprementedError
            if (!node || l >= r || r <= node->l || node->r <= l) return mapping(f, e());
            if (l <= node->l && node->r <= r) return mapping(f, node->data);
            if ((!node->left) && (!node->right)) return mapping(f, pow(node->key, r - l));
            return op(
                inner_prod2(node->left, l, min(r, node->mid()), composition(f, node->lazy)),
                inner_prod2(node->right, max(l, node->mid()), r, composition(f, node->lazy))
            );
        }

        void inner_apply(NodePtr node, IndexType l, IndexType r, F f) {
            if (!node || l >= r || r <= node->l || node->r <= l) return;
            if (l <= node->l && node->r <= r) {
                node->apply(f);
                return;
            }
            node->propagate();
            inner_apply(node->left, l, min(r, node->mid()), f);
            inner_apply(node->right, max(l, node->mid()), r, f);
            node->update();
        }

        void inner_set(NodePtr node, IndexType k, T val) {
            assert(node);
            if (node->is_leaf()) {
                assert(node->l == k && node->r == k+1);
                node->key = val;
                node->data = val;
                return;
            }
            node->propagate();
            if (k < node->mid()) {
                inner_set(node->left, k, val);
            } else {
                inner_set(node->right, k, val);
            }
            node->update();
        }

        T inner_get(NodePtr node, IndexType k) {
            assert(node);
            while (true) {
                if (node->is_leaf()) {
                    assert(node->l == k && node->r == k+1);
                    return node->key;
                }
                if (k < node->mid()) {
                    if (!node->left) return node->key;
                    node->propagate();
                    node = node->left;
                } else {
                    if (!node->right) return node->key;
                    node->propagate();
                    node = node->right;
                }
            }
        }

      public:

        DynamicLazySegmentTree() : root(nullptr), u(0) {}

        //! 初期値 `e()` , `[0, u)` の区間を管理する `DynamicLazySegmentTree` を作成する
        DynamicLazySegmentTree(const IndexType u_) {
            assert(u_ > 0);
            this->u = (IndexType)1 << bit_length(u_);
            this->root = new Node(0, this->u, e());
        }

        //! 初期値 `init` , `[0, u)` の区間を管理する `DynamicLazySegmentTree` を作成する
        DynamicLazySegmentTree(const IndexType u_, const T init) {
            assert(u_ > 0);
            this->u = (IndexType)1 << bit_length(u_);
            this->root = new Node(0, this->u, init);
        }

        //! `[l, r)` の集約値を返す / `O(logu)` time, `O(logu)` space
        T prod(IndexType l, IndexType r) {
            assert(0 <= l && l <= r && r <= u);
            return inner_prod(this->root, l, r);
        }

        //! `[l, r)` の集約値を返す / `O(logu)` time, `O(1)` space
        T prod2(IndexType l, IndexType r) const {
            assert(0 <= l && l <= r && r <= u);
            return inner_prod2(this->root, l, r, id());
        }

        //! `[l, r)` に `f` を作用させる / `O(logu)` time, `O(logu)` space
        void apply(IndexType l, IndexType r, F f) {
            assert(0 <= l && l <= r && r <= u);
            inner_apply(this->root, l, r, f);
        }

        //! `k` 番目の値を取得する / `O(logu)` time, `O(1)` space
        T get(IndexType k) {
            return inner_get(this->root, k);
        }

        //! `k` 番目の値を `val` に更新する / `O(logu)` time, `O(logu)` space
        void set(IndexType k, T val) {
            assert(0 <= k && k < u);
            inner_set(this->root, k, val);
        }

        //! 適当に表示する
        void print() {
            for (IndexType i = 0; i < u; ++id) {
                cout << get(i) << ", ";
            }
            cout << endl;
        }
    };
}  // namespace titan23
