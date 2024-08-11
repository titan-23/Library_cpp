#include <iostream>
#include <vector>
#include <tuple>
#include <queue>
#include "titan_cpplib/data_structures/avl_tree_bit_vector.cpp"
using namespace std;

// DynamicWaveletMatrix
namespace titan23 {

    /**
     * @brief 動的ウェーブレット行列
     *
     * @note `DynamicWavletTree` の方が平均的には高速かもしれません
     *
     * @tparam T
     */
    template<typename T>
    class DynamicWaveletMatrix {
      private:
        T _sigma;
        int _log;
        vector<AVLTreeBitVector> _v;
        vector<int> _mid;
        int _size;

        int bit_length(const int n) const {
            return 32 - __builtin_clz(n);
        }

        int bit_length(const long long n) const {
            return 64 - __builtin_clz(n);
        }

        int bit_length(const unsigned long long n) const {
            return 64 - __builtin_clz(n);
        }

        void _build(vector<T> a) {
            if (a.empty()) return;
            vector<uint8_t> v(_size);
            for (int bit = _log-1; bit >= 0; --bit) {
                vector<T> zero, one;
                for (int i = 0; i < _size; ++i) {
                    if ((a[i] >> bit) & 1) {
                        v[i] = 1;
                        one.emplace_back(a[i]);
                    } else {
                        v[i] = 0;
                        zero.emplace_back(a[i]);
                    }
                }
                _mid[bit] = zero.size();
                _v[bit] = AVLTreeBitVector(v);
                a = zero;
                a.insert(a.end(), one.begin(), one.end());
            }
        }

      public:
        DynamicWaveletMatrix(const T sigma)
                : _sigma(sigma), _log(bit_length(sigma-1)), _v(_log), _mid(_log), _size(0) {
        }

        DynamicWaveletMatrix(const T sigma, vector<T> &a)
                : _sigma(sigma), _log(bit_length(sigma)), _v(_log), _mid(_log), _size(a.size()) {
            _build(a);
        }

        void reserve(const int n) {
            for (int i = 0; i < _log; ++i) {
                _v[i].reserve(n);
            }
        }

        void insert(int k, T x) {
            for (int bit = _log-1; bit >= 0; --bit) {
                if ((x >> bit) & 1) {
                    int s = _v[bit]._insert_and_rank1(k, 1);
                    k = s + _mid[bit];
                } else {
                    int s = _v[bit]._insert_and_rank1(k, 0);
                    k -= s;
                    ++_mid[bit];
                }
            }
            _size++;
        }

        T pop(int k) {
            T ans = 0;
            for (int bit = _log-1; bit >= 0; --bit) {
                int sb = _v[bit]._access_pop_and_rank1(k);
                int s = sb >> 1;
                if (sb & 1) {
                    ans |= (T)1 << bit;
                    k = s + _mid[bit];
                } else {
                    --_mid[bit];
                    k -= s;
                }
            }
            _size--;
            return ans;
        }

        void set(int k, T x) {
            assert(0 <= k && k < _size);
            pop(k);
            insert(k, x);
        }

        int rank(int r, T x) const {
            int l = 0;
            for (int bit = _log-1; bit >= 0; --bit) {
                if ((x >> bit) & 1) {
                    l = _v[bit].rank1(l) + _mid[bit];
                    r = _v[bit].rank1(r) + _mid[bit];
                } else {
                    l = _v[bit].rank0(l);
                    r = _v[bit].rank0(r);
                }
            }
            return r - l;
        }

        T access(int k) const {
            T s = 0;
            for (int bit = _log-1; bit >= 0; --bit) {
                if (_v[bit].access(k)) {
                    s |= (T)1 << bit;
                    k = _v[bit].rank1(k) + _mid[bit];
                } else {
                    k = _v[bit].rank0(k);
                }
            }
            return s;
        }

        T kth_smallest(int l, int r, int k) const {
            T s = 0;
            for (int bit = _log-1; bit >= 0; --bit) {
                int l0 = _v[bit].rank0(l);
                int r0 = _v[bit].rank0(r);
                int cnt = r0 - l0;
                if (cnt <= k) {
                    s |= (T)1 << bit;
                    k -= cnt;
                    l = l - l0 + _mid[bit];
                    r = r - r0 + _mid[bit];
                } else {
                    l = l0;
                    r = r0;
                }
            }
            return s;
        }

        T kth_largest(int l, int r, int k) const {
            return kth_smallest(l, r, r-l-k-1);
        }

        vector<pair<T, int>> topk(int l, int r, int k) {
            priority_queue<tuple<int, T, int, char>> hq;
            hq.emplace(r-l, 0, l, _log-1);
            vector<pair<T, int>> ans;
            while (!hq.empty()) {
                auto [length, x, l, bit] = hq.top();
                hq.pop();
                if (bit == -1) {
                    ans.emplace_back(x, length);
                    --k;
                    if (k == 0) break;
                } else {
                    int r = l + length;
                    int l0 = _v[bit].rank0(l);
                    int r0 = _v[bit].rank0(r);
                    if (l0 < r0) hq.emplace(r0-l0, x, l0, bit-1);
                    int l1 = _v[bit].rank1(l) + _mid[bit];
                    int r1 = _v[bit].rank1(r) + _mid[bit];
                    if (l1 < r1) hq.emplace(r1-l1, x|((T)1<<(T)bit), l1, bit-1);
                }
            }
            return ans;
        }

        int select(int k, T x) const {
            T s = 0;
            for (int bit = _log-1; bit >= 0; --bit) {
                if ((x >> bit) & 1) {
                    s = _v[bit].rank0(_size) + _v[bit].rank1(s);
                } else {
                    s = _v[bit].rank0(s);
                }
            }
            s += k;
            for (int bit = 0; bit < _log; ++bit) {
                if ((x >> bit) & 1) {
                    s = _v[bit].select1(s - _v[bit].rank0(_size));
                } else {
                    s = _v[bit].select0(s);
                }
            }
            return s;
        }

        int range_freq(int l, int r, T x) const {
            int ans = 0;
            for (int bit = _log-1; bit >= 0; --bit) {
                int l0 = _v[bit].rank0(l);
                int r0 = _v[bit].rank0(r);
                if ((x >> bit) & 1) {
                    ans += r0 - l0;
                    l += _mid[bit] - l0;
                    r += _mid[bit] - r0;
                } else {
                    l = l0;
                    r = r0;
                }
            }
            return ans;
        }

        int range_freq(int l, int r, int x, int y) const {
            return range_freq(l, r, y) - range_freq(l, r, x);
        }

        T prev_value(int l, int r, T x) const {
            return kth_smallest(l, r, _range_freq(l, r, x)-1);
        }

        T next_value(int l, int r, T x) const {
            return kth_smallest(l, r, _range_freq(l, r, x));
        }

        int range_count(int l, int r, T x) const {
            return rank(r, x) - rank(l, x);
        }

        vector<T> tovector() const {

            vector<T> a(_size);
            for (int i = 0; i < _size; ++i) {
                a[i] = access(i);
            }
            return a;
        }

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
    };
} // namespace titan23
