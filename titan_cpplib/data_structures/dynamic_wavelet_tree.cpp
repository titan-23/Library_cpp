#include <iostream>
#include <vector>
#include "titan_cpplib/data_structures/avl_tree_bit_vector.cpp"
#include "titan_cpplib/others/print.cpp"
using namespace std;

// DynamicWaveletTree
namespace titan23 {

/**
 * @brief 動的ウェーブレット木
 * 
 * @tparam T 値の型
 */
template<typename T>
class DynamicWaveletTree {
    private:
    struct Node;
    Node* root;
    T _sigma;
    int _log;
    int _size;

    struct Node {
        Node* left;
        Node* right;
        Node* par;
        AVLTreeBitVector v;
        Node() : left(nullptr), right(nullptr), par(nullptr) {}
        Node(const vector<uint8_t> &a) : left(nullptr), right(nullptr), par(nullptr) {
            v = AVLTreeBitVector(a);
        }
    };

    int bit_length(const int n) const {
        return n > 0 ? 32 - __builtin_clz(n) : 0;
    }

    void _build(const vector<T> &a) {
        vector<int> buff0(a.size()), buff1;
        auto build = [&] (auto &&build,
                            int bit,
                            bool flag01,
                            int s0, int g0,
                            int s1, int g1
                            ) -> Node* {
            int s = flag01 ? s1 : s0;
            int g = flag01 ? g1 : g0;
            if (s == g || bit < 0) return nullptr;
            vector<int> &vec = flag01 ? buff1 : buff0;
            vector<uint8_t> v(g-s, 0);
            int start_0 = buff0.size(), start_1 = buff1.size();
            for (int i = s; i < g; ++i) {
                if (a[vec[i]] >> bit & 1) {
                    v[i-s] = 1;
                    buff1.emplace_back(vec[i]);
                } else {
                    buff0.emplace_back(vec[i]);
                }
            }
            int end_0 = buff0.size(), end_1 = buff1.size();
            Node* node = new Node(v);
            node->left  = build(build, bit-1, 0, start_0, end_0, start_1, end_1);
            if (node->left) node->left->par = node;
            node->right = build(build, bit-1, 1, start_0, end_0, start_1, end_1);
            if (node->right) node->right->par = node;
            return node;
        };
        for (int i = 0; i < a.size(); ++i) {
            buff0[i] = i;
        }
        this->root = build(build, _log-1, 0, 0, a.size(), 0, 0);
        if (this->root == nullptr) {
            this->root = new Node();
        }
    }

    public:
    //! 各要素が `[0, sigma)` の `DynamicWaveletTree` を作成する / `O(1)`
    DynamicWaveletTree(const T sigma)
            : _sigma(sigma), _log(bit_length(sigma)), _size(0) {
        root = new Node();
    }

    //! 各要素が `[0, sigma)` の `DynamicWaveletTree` を作成する / `O(nlog(σ))`
    DynamicWaveletTree(const T sigma, vector<T> &a)
            : _sigma(sigma), _log(bit_length(sigma)), _size(a.size()) {
        _build(a);
    }

    //! 位置 `k` に `x` を挿入する / `O(log(n)log(σ))`
    void insert(int k, T x) {
        assert(0 <= k && k <= len());
        assert(0 <= x && x < _sigma);
        Node* node = root;
        for (int bit = _log-1; bit >= 0; --bit) {
            if ((x >> bit) & 1) {
                k = node->v._insert_and_rank1(k, 1);
                if (!node->right) {
                    node->right = new Node();
                    node->right->par = node;
                }
                node = node->right;
            } else {
                k -= node->v._insert_and_rank1(k, 0);
                if (!node->left) {
                    node->left = new Node();
                    node->left->par = node;
                }
                node = node->left;
            }
        }
        _size++;
    }

    //! 位置 `k` の値を削除して返す / `O(log(n)log(σ))`
    T pop(int k) {
        assert(0 <= k && k < len());
        Node* node = root;
        T ans = 0;
        for (int bit = _log-1; node && bit >= 0; --bit) {
            int sb = node->v._access_pop_and_rank1(k);
            if (sb & 1) {
                ans |= (T)1 << bit;
                k = sb >> 1;
                node = node->right;
            } else {
                k -= sb >> 1;
                node = node->left;
            }
        }
        _size--;
        return ans;
    }

    //! 位置 `k` の値を `x` に更新する / `O(log(n)log(σ))`
    void set(int k, T x) {
        assert(0 <= k && k < len());
        assert(0 <= x && x < _sigma);
        pop(k);
        insert(k, x);
    }

    //! 区間 `[0, r)` の `x` の個数を返す / `O(log(n)log(σ))`
    int rank(int r, T x) const {
        assert(0 <= r && r <= len());
        Node* node = root;
        int l = 0;
        for (int bit = _log-1; node && bit >= 0; --bit) {
            if ((x >> bit) & 1) {
                l = node->v.rank1(l);
                r = node->v.rank1(r);
                node = node->right;
            } else {
                l = node->v.rank0(l);
                r = node->v.rank0(r);
                node = node->left;
            }
        }
        return r - l;
    }

    //! 区間 `[l, r)` の `x` の個数を返す / `O(log(n)log(σ))`
    int range_count(int l, int r, T x) const {
        assert(0 <= l && l <= r && r <= len());
        return rank(r, x) - rank(l, x);
    }

    //! `k` 番目の要素を返す / `O(log(n)log(σ))`
    T access(int k) const {
        assert(0 <= k && k < len());
        Node* node = root;
        T s = 0;
        for (int bit = _log-1; bit >= 0; --bit) {
            auto [b, r] = node->v._access_ans_rank1(k);
            if (b) {
                s |= (T)1 << bit;
                k = r;
                node = node->right;
            } else {
                k -= r;
                node = node->left;
            }
        }
        return s;
    }

    //! 区間 `[l, r)` で昇順 `k` 番目の値を返す / `O(log(n)log(σ))`
    T kth_smallest(int l, int r, int k) const {
        assert(0 <= l && l <= r && r <= len());
        assert(0 <= k && k < r-l);
        Node* node = root;
        T s = 0;
        for (int bit = _log-1; node && bit >= 0; --bit) {
            int l0 = node->v.rank0(l);
            int r0 = node->v.rank0(r);
            int cnt = r0 - l0;
            if (cnt <= k) {
                s |= (T)1 << bit;
                k -= cnt;
                l -= l0;
                r -= r0;
                node = node->right;
            } else {
                l = l0;
                r = r0;
                node = node->left;
            }
        }
        return s;
    }

    //! 区間 `[l, r)` で降順 `k` 番目の値を返す / `O(log(n)log(σ))`
    T kth_largest(int l, int r, int k) const {
        return kth_smallest(l, r, r-l-k-1);
    }

    pair<bool, T> has_majority(int l, int r) const {
        Node* node = root;
        int length = (r - l) / 2 + 1;
        T s = 0;
        for (int bit = _log-1; node && bit >= 0; --bit) {
            int l0 = node->v.rank0(l);
            int r0 = node->v.rank0(r);
            int cnt0 = r0 - l0;
            int cnt1 = (r - l) - cnt0;
            if (cnt0 >= length) {
                l = l0;
                r = r0;
                node = node->left;
                continue;
            }
            if (cnt1 >= length) {
                s |= (T)1 << bit;
                l -= l0;
                r -= r0;
                node = node->right;
                continue;
            }
            return {false, 0};
        }
        return {true, s};
    }

    //! 区間 `[l, r)` で `x` 未満の要素の個数を返す / `O(log(n)log(σ))`
    int range_freq(int l, int r, const T &x) const {
        Node* node = root;
        int ans = 0;
        for (int bit = _log-1; node && bit >= 0; --bit) {
            int l0 = node->v.rank0(l);
            int r0 = node->v.rank0(r);
            if ((x >> bit) & 1) {
                ans += r0 - l0;
                l -= l0;
                r -= r0;
                node = node->right;
            } else {
                l = l0;
                r = r0;
                node = node->left;
            }
        }
        return ans;
    }

    //! 区間 `[l, r)` で `x` 以上 `y` 未満の要素の個数を返す / `O(log(n)log(σ))`
    int range_freq(int l, int r, int x, int y) const {
        return range_freq(l, r, y) - range_freq(l, r, x);
    }

    //! `k` 番目の `x` の位置を返す / `O(log(n)log(σ))`
    int select(int k, T x) const {
        Node* node = root;
        for (int bit = _log-1; bit > 0; --bit) {
            if ((x >> bit) & 1) {
                node = node->right;
            } else {
                node = node->left;
            }
        }
        for (int bit = 0; bit < _log; ++bit) {
            if ((x >> bit) & 1) {
                k = node->v.select1(k);
            } else {
                k = node->v.select0(k);
            }
            node = node->par;
        }
        return k;
    }

    //! `k` 番目の `x` の位置を返して削除する / `O(log(n)log(σ))`
    int select_remove(int k, T x) {
        Node* node = root;
        for (int bit = _log-1; bit > 0; --bit) {
            if ((x >> bit) & 1) {
                node = node->right;
            } else {
                node = node->left;
            }
        }
        for (int bit = 0; bit < _log; ++bit) {
            if ((x >> bit) & 1) {
                k = node->v.select1(k);
            } else {
                k = node->v.select0(k);
            }
            node->v.pop(k);
            node = node->par;
        }
        _size--;
        return k;
    }

    //! 区間[l, r)で、x未満のうち最大の要素を返す
    T prev_value(int l, int r, T x) const {
        int k = range_freq(l, r, x)-1;
        if (k < 0) return -1;
        return kth_smallest(l, r, k);
    }

    //! 区間[l, r)で、x以上のうち最小の要素を返す
    T next_value(int l, int r, T x) const {
        int k = range_freq(l, r, x);
        if (k >= r-l) return -1;
        return kth_smallest(l, r, k);
    }

    //! 要素数を返す / `O(1)`
    int len() const {
        return _size;
    }

    //! `vector` にして返す / `O(nlog(σ))`
    //! (n 回 access するよりも高速)
    vector<T> tovector() const {
        vector<T> a(len(), 0);
        vector<int> buff0(a.size()), buff1;
        auto dfs = [&] (auto &&dfs,
                        Node* node,
                        int bit,
                        bool flag01,
                        int s0, int g0,
                        int s1, int g1
                        ) -> void {
            int s = flag01 ? s1 : s0;
            int g = flag01 ? g1 : g0;
            if (s == g || bit < 0) return;
            vector<int> &vec = flag01 ? buff1 : buff0;
            const vector<uint8_t> &v = node->v.tovector();
            int start_0 = buff0.size(), start_1 = buff1.size();
            for (int i = s; i < g; ++i) {
                if (v[i-s]) {
                    a[vec[i]] |= (T)1 << bit;
                    buff1.emplace_back(vec[i]);
                } else {
                    buff0.emplace_back(vec[i]);
                }
            }
            int end_0 = buff0.size(), end_1 = buff1.size();
            dfs(dfs, node->left,  bit-1, 0, start_0, end_0, start_1, end_1);
            dfs(dfs, node->right, bit-1, 1, start_0, end_0, start_1, end_1);
        };
        for (int i = 0; i < a.size(); ++i) {
            buff0[i] = i;
        }
        dfs(dfs, this->root, _log-1, 0, 0, a.size(), 0, 0);
        return a;
    }

    //! 表示する / `O(nlog(σ))`
    void print() const {
        vector<T> a = tovector();
        int n = (int)a.size();
        cout << "[";
        for (int i = 0; i < n-1; ++i) {
            cout << a[i] << ", ";
        }
        if (n > 0) {
            cout << a.back();
        }
        cout << "]";
        cout << endl;
    }

    friend ostream& operator<<(ostream& os, const titan23::DynamicWaveletTree<T> &dwm) {
        vector<T> a = dwm.tovector();
        os << a;
        return os;
    }
};
} // namespace titan23
