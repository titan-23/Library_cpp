#include <vector>
#include <queue>
#include <array>
#include "titan_cpplib/data_structures/bit_vector.cpp"
using namespace std;

// WaveletMatrix
namespace titan23 {

template<typename T, int log = 31>
class WaveletMatrix {
private:
    int n;
    T sigma;
    array<BitVector, log> v;
    array<int, log> mid;

    void build(vector<T> &a) {
        vector<T> b(n);
        for (int bit = log-1; bit >= 0; --bit) {
            int zero_cnt = 0;
            v[bit] = BitVector(n);
            for (int i = 0; i < n; ++i) {
                if ((a[i] >> bit) & 1) {
                    v[bit].set(i);
                } else {
                    zero_cnt++;
                }
            }
            v[bit].build();
            mid[bit] = zero_cnt;
            int idx0 = 0, idx1 = zero_cnt;
            for (int i = 0; i < n; ++i) {
                if (a[i] >> bit & 1) {
                    b[idx1++] = a[i];
                } else {
                    b[idx0++] = a[i];
                }
            }
            a.swap(b);
        }
    }

public:
    WaveletMatrix() {}
    WaveletMatrix(vector<T> a) : n(a.size()), sigma((1ull<<log)-1) { build(a); }

    T get_sigma() const { return sigma; }

    T access(int k) const {
        T s = 0;
        for (int bit = log-1; bit >= 0; --bit) {
            if (v[bit].access(k)) {
                s |= (T)1 << bit;
                k = v[bit].rank1(k) + mid[bit];
            } else {
                k = v[bit].rank0(k);
            }
        }
        return s;
    }

    //! `a[0, r)` に含まれる `x` の個数を返します。
    int rank(int r, T x) const {
        int l = 0;
        for (int bit = log-1; bit >= 0; --bit) {
            if ((x >> bit) & 1) {
                l = v[bit].rank1(l) + mid[bit];
                r = v[bit].rank1(r) + mid[bit];
            } else {
                l = v[bit].rank0(l);
                r = v[bit].rank0(r);
            }
        }
        return r - l;
    }

    // `k` 番目の `v` のインデックスを返す。
    int select(int k, T x) const {
        int s = 0;
        for (int bit = log-1; bit >= 0; --bit) {
            if ((x >> bit) & 1) {
                s = v[bit].rank0(n) + v[bit].rank1(s);
            } else {
                s = v[bit].rank0(s);
            }
        }
        s += k;
        for (int bit = 0; bit < log; ++bit) {
            if ((x >> bit) & 1) {
                s = v[bit].select1(s - v[bit].rank0(n));
            } else {
                s = v[bit].select0(s);
            }
        }
        return s;
    }

    // `a[l, r)` の中で k 番目に **小さい** 値を返します。
    T kth_smallest(int l, int r, int k) const {
        T s = 0;
        #pragma unroll(log)
        for (int bit = log-1; bit >= 0; --bit) {
            const int r0 = v[bit].rank0(r), l0 = v[bit].rank0(l);
            const int cnt = r0 - l0;
            if (cnt <= k) {
                s |= (T)1 << bit;
                k -= cnt;
                l = l - l0 + mid[bit];
                r = r - r0 + mid[bit];
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

    // `a[l, r)` の中で、要素を出現回数が多い順にその頻度とともに `k` 個返します。
    vector<pair<int, int>> topk(int l, int r, int k) {
        // heap[-length, x, l, bit]
        priority_queue<tuple<int, T, int, int>> hq;
        hq.emplace(r-l, 0, l, log-1);
        vector<pair<T, int>> ans;
        while (!hq.empty()) {
            auto [length, x, l, bit] = hq.top();
            hq.pop();
            if (bit == -1) {
                ans.emplace_back(x, length);
                k -= 1;
                if (k == 0) break;
            } else {
                r = l + length;
                int l0 = v[bit].rank0(l);
                int r0 = v[bit].rank0(r);
                if (l0 < r0) hq.emplace(r0-l0, x, l0, bit-1);
                int l1 = v[bit].rank1(l) + mid[bit];
                int r1 = v[bit].rank1(r) + mid[bit];
                if (l1 < r1) hq.emplace(r1-l1, x|((T)1<<bit), l1, bit-1);
            }
        }
        return ans;
    }

    T sum(int l, int r) const {
        assert(false);
        T s = 0;
        for (const auto &[sum, cnt]: topk(l, r, r-l)) {
            s += sum * cnt;
        }
        return s;
    }

    // a[l, r) で x 未満の要素の数を返す'''
    int range_freq(int l, int r, T x) const {
        int ans = 0;
        #pragma unroll(log)
        for (int bit = log-1; bit >= 0; --bit) {
            int l0 = v[bit].rank0(l), r0 = v[bit].rank0(r);
            if ((x >> bit) & 1) {
                ans += r0 - l0;
                l += mid[bit] - l0;
                r += mid[bit] - r0;
            } else {
                l = l0;
                r = r0;
            }
        }
        return ans;
    }

    //`a[l, r)` に含まれる、 `x` 以上 `y` 未満である要素の個数を返します。
    int range_freq(int l, int r, T x, T y) const {
        return range_freq(l, r, y) - range_freq(l, r, x);
    }

    //`a[l, r)` で、`y`未満であるような要素のうち最大の要素を返します。
    T prev_value(int l, int r, T y) const {
        int x = range_freq(l, r, y);
        if (x == 0) {
            return -1;
        }
        return kth_smallest(l, r, x-1);
    }

    T next_value(int l, int r, T x) const {
        return kth_smallest(l, r, range_freq(l, r, x));
    }

    //`a[l, r)` に含まれる `x` の個数を返します。
    int range_count(int l, int r, T x) const {
        return rank(r, x) - rank(l, x);
    }

    int len() const {
        return n;
    }

    friend ostream& operator<<(ostream& os, const titan23::WaveletMatrix<T>& wm) {
        int n = wm.len();
        os << "[";
        for (int i = 0; i < n; ++i) {
            os << wm.access(i);
            if (i != n-1) os << ", ";
        }
        os << "]";
        return os;
    }
};
}  // namespace titan23




int main() {
    ios::sync_with_stdio(false);
    cin.tie(0);
    int n, q;
    cin >> n >> q;
    vector<int> A(n);
    for (int i = 0; i < n; ++i) cin >> A[i];
    titan23::WaveletMatrix<int> wm(A);
    for (int i = 0; i < q; ++i) {
        int l, r, k;
        cin >> l >> r >> k;
        cout << wm.kth_smallest(l, r, k) << '\n';
    }
    return 0;
}
