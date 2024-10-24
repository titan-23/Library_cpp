// #include "titan_cpplib/data_structures/wavelet_matrix_cumulative_sum.cpp"
#include <vector>
#include <algorithm>
// #include "titan_cpplib/data_structures/bit_vector.cpp"
#pragma once
#include <iostream>
#include <vector>
#include <cassert>
using namespace std;

// BitVector
namespace titan23 {

    class BitVector {
      private:
        using u64 = unsigned long long;
        int n, bsize;
        vector<u64> bit, acc;

      public:
        BitVector() {}
        BitVector(const int n) :
            n(n), bsize((n+63)>>6), bit(bsize+1, 0), acc(bsize+1, 0) {}

        void set(const int k) {
            bit[k>>6] |= (1ull) << (k & 63);
        }

        void build() {
            for (int i = 0; i < bsize; ++i) {
                acc[i+1] += acc[i] + __builtin_popcountll(bit[i]);
            }
        }

        bool access(const int k) const {
            return (bit[k>>6] >> (k&63)) & 1;
        }

        int rank0(const int r) const {
            return r - (acc[r>>6] + __builtin_popcountll(bit[r>>6] & ((1ull << (r & 63)) - 1)));
        }

        int rank1(const int r) const {
            return acc[r>>6] + __builtin_popcountll(bit[r>>6] & ((1ull << (r & 63)) - 1));
        }

        int rank(const int r, const bool v) const {
            return v ? rank1(r) : rank0(r);
        }

        int select0(int k) const {
            if (k < 0 or rank0(n) <= k) return -1;
            int l = 0, r = bsize+1;
            while (r - l > 1) {
                const int m = (l + r) >> 1;
                if (m*32 - acc[m] > k) {
                    r = m;
                } else {
                    l = m;
                }
            }
            int indx = 32 * l;
            k = k - (l*32 - acc[l]) + rank0(indx);
            l = indx; r = indx+32;
            while (r - l > 1) {
                const int m = (l + r) >> 1;
                if (rank0(m) > k) {
                    r = m;
                } else {
                    l = m;
                }
            }
            return l;
        }

        int select1(int k) const {
            if (k < 0 || rank1(n) <= k) return -1;
            int l = 0, r = bsize+1;
            while (r - l > 1) {
                const int m = (l + r) >> 1;
                if (acc[m] > k) {
                    r = m;
                } else {
                    l = m;
                }
            }
            int indx = 32 * l;
            k = k - acc[l] + rank1(indx);
            l = indx; r = indx+32;
            while (r - l > 1) {
                const int m = (l + r) >> 1;
                if (rank1(m) > k) {
                    r = m;
                } else {
                    l = m;
                }
            }
            return l;
        }

        int select(const int k, const int v) const {
            return v ? select1(k) : select0(k);
        }

        int len() const {
            return n;
        }

        void print() const {
            cout << "[";
            for (int i = 0; i < len()-1; ++i) {
                cout << access(i) << ", ";
            }
            if (len() > 0) {
                cout << access(len()-1);
            }
            cout << "]\n";
        }
    };
}  // namespace titan23

// #include "titan_cpplib/data_structures/cumulative_sum.cpp"
#include <iostream>
#include <vector>
using namespace std;

// CumulativeSum
namespace titan23 {
    template<typename T>
    class CumulativeSum {
      private:
        int n;
        vector<T> acc;

      public:
        CumulativeSum() {}
        CumulativeSum(vector<T> &a, T e) : n((int)a.size()), acc(n+1, e) {
            for (int i = 0; i < n; ++i) {
                acc[i+1] = acc[i] + a[i];
            }
        }

        T pref(const int r) const {
            assert(0 <= r && r <= this->n);
            return acc[r];
        }

        T all_sum() const {
            return acc.back();
        }

        T sum(const int l, const int r) const {
            assert(0 <= l && l <= r && r < this->n);
            return acc[r] - acc[l];
        }

        T prod(const int l, const int r) const {
            assert(0 <= l && l <= r && r < this->n);
            return acc[r] - acc[l];
        }

        T all_prod() const {
            return acc.back();
        }

        int len() const {
            return n;
        }

        void print() const {
            cout << '[';
            for (int i = 0; i < n-1; ++i) {
                cout << acc[i] << ", ";
            }
            if (n > 0) cout << acc.back();
            cout << ']' << endl;
        }
    };
}  // namespace titan23

using namespace std;

// WaveletMatrixCumulativeSum
namespace titan23 {

    /**
     * @brief 
     * 
     * @tparam T 点の座標を表す型
     * @tparam W 重みを表す型
     */
    template<typename T, typename W>
    class WaveletMatrixCumulativeSum {

      private:
        T sigma;
        int log;
        vector<tuple<T, T, W>> pos;
        vector<BitVector> v;
        vector<pair<T, T>> xy;
        vector<T> y;
        vector<int> mid;
        vector<titan23::CumulativeSum<W>> cumsum;
        int n;

        int bit_length(T n) const {
            int b = 0;
            while (n) {
                n >>= 1;
                ++b;
            }
            return b;
        }

        void _build(vector<T> a) {
            for (int bit = log-1; bit >= 0; --bit) {
                vector<T> zero, one;
                v[bit] = BitVector(n);
                for (int i = 0; i < n; ++i) {
                    if ((a[i] >> bit) & 1) {
                        v[bit].set(i);
                        one.emplace_back(a[i]);
                    } else {
                        zero.emplace_back(a[i]);
                    }
                }
                v[bit].build();
                mid[bit] = zero.size();
                a = zero;
                a.insert(a.end(), one.begin(), one.end());
            }
        }

        template<typename S>
        static void sort_unique(vector<S> &a) {
            std::sort(a.begin(), a.end());
            a.erase(std::unique(a.begin(), a.end()), a.end());
        }

        W _sum(int l, int r, int x) const {
            W ans = 0;
            for (int bit = log-1; bit >= 0; --bit) {
                int l0 = v[bit].rank0(l);
                int r0 = v[bit].rank0(r);
                if ((x>>bit) & 1) {
                    l += mid[bit] - l0;
                    r += mid[bit] - r0;
                    ans += cumsum[bit].sum(l0, r0);
                } else {
                    l = l0;
                    r = r0;
                }
            }
            return ans;
        }

      public:
        WaveletMatrixCumulativeSum() {}
        WaveletMatrixCumulativeSum(const T sigma)
            : sigma(sigma), log(bit_length(sigma-1)), v(log), mid(log), cumsum(log) {
        }

        void reserve(const int cap) {
            pos.reserve(cap);
        }

        void set_point(T x, T y, W w) {
            pos.emplace_back(x, y, w);
        }

        void build() {
            xy.reserve(pos.size());
            for (const auto &[x, y, w]: pos) {
                xy.emplace_back(x, y);
            }
            sort_unique(xy);

            this->n = xy.size();

            y.reserve(n);
            for (const auto &[x, y_]: xy) {
                y.emplace_back(y_);
            }
            sort_unique(y);

            vector<int> a;
            for (const auto &[x, y_]: xy) {
                a.emplace_back(lower_bound(y.begin(), y.end(), y_) - y.begin());
            }
            _build(a);

            vector<vector<W>> ws(log, vector<W>(n, 0));
            for (const auto [x, y_, w]: pos) {
                int k = lower_bound(xy.begin(), xy.end(), make_pair(x, y_)) - xy.begin();
                int i_y = lower_bound(y.begin(), y.end(), y_) - y.begin();
                for (int bit = log-1; bit >= 0; --bit) {
                    if ((i_y >> bit) & 1) {
                        k = v[bit].rank1(k) + mid[bit];
                    } else {
                        k = v[bit].rank0(k);
                    }
                    ws[bit][k] += w;
                }
            }

            for (int i = 0; i < log; ++i) {
                cumsum[i] = titan23::CumulativeSum<W>(ws[i], 0);
            }
        }

        W sum(int w1, int w2, int h1, int h2) const {
            int l = lower_bound(xy.begin(), xy.end(), make_pair(w1, 0)) - xy.begin();
            int r = lower_bound(xy.begin(), xy.end(), make_pair(w2, 0)) - xy.begin();
            return _sum(l, r, lower_bound(y.begin(), y.end(), h2) - y.begin())
                    - _sum(l, r, lower_bound(y.begin(), y.end(), h1) - y.begin());
        }
    };
}  // namespace titan23

