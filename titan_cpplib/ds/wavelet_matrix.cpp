#pragma once

#include <vector>
#include <queue>
#include "titan_cpplib/ds/bit_vector.cpp"
using namespace std;

// WaveletMatrix
namespace titan23 {

template<typename T>
class WaveletMatrix {
private:
    T sigma;
    int log;
    vector<BitVector> v;
    vector<int> mid;
    int n;

    int bit_length(T n) const {
        unsigned long long x = static_cast<unsigned long long>(n);
        return x == 0 ? 0 : 64 - __builtin_clzll(x);
    }

    void build(vector<T> a) {
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
            assert(a.size() == n);
        }
    }

public:
    WaveletMatrix() {}

    WaveletMatrix(const T sigma)
        : sigma(sigma), log(bit_length(sigma-1)), v(log), mid(log), n(0) {}

    WaveletMatrix(const T sigma, const vector<T> &a)
        : sigma(sigma), log(bit_length(sigma-1)), v(log), mid(log), n(a.size()) {
        build(a);
    }

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

    /// `a[0, r)` に含まれる `x` の個数を返します。
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

    /// 区間 `[l, r)` にある `k` 番目の `x` の位置を返す / `O(log(n)log(σ))`
    /// 例: `[5, 1, 4, 1, 9]` で `range_select(1, 5, 1, 1)` は `3`
    int range_select(const int l, const int r, const int k, const T x) const {
        assert(0 <= k && k < range_count(l, r, x));
        return select(rank(l, x) + k, x);
    }

    // `a[l, r)` の中で k 番目に **小さい** 値を返します。
    T kth_smallest(int l, int r, int k) const {
        T s = 0;
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

    /// 区間 `[l, r)` に過半数を占める値があるか判定する / `O(log(σ))`
    /// 例: `[2, 1, 2, 2]` では `{true, 2}`、`[1, 2, 3]` では `{false, 0}`
    pair<bool, T> has_majority(int l, int r) const {
        assert(0 <= l && l < r && r <= len());
        const int majority = (r - l) / 2 + 1;
        T result = 0;
        for (int bit = log - 1; bit >= 0; --bit) {
            const int l0 = v[bit].rank0(l);
            const int r0 = v[bit].rank0(r);
            const int count0 = r0 - l0;
            const int count1 = (r - l) - count0;
            if (count0 >= majority) {
                l = l0;
                r = r0;
            } else if (count1 >= majority) {
                result |= static_cast<T>(1) << bit;
                l = mid[bit] + l - l0;
                r = mid[bit] + r - r0;
            } else {
                return {false, 0};
            }
        }
        return {true, result};
    }

    // `a[l, r)` の中で、要素を出現回数が多い順にその頻度とともに `k` 個返します。
    vector<pair<T, int>> topk(int l, int r, int k) const {
        // heap[-length, x, l, bit]
        priority_queue<tuple<int, T, int, int>> hq;
        vector<pair<T, int>> ans;
        if (l == r || k <= 0) return ans;
        hq.emplace(r-l, 0, l, log-1);
        while (!hq.empty() && k > 0) {
            auto [length, x, l, bit] = hq.top();
            hq.pop();
            if (bit == -1) {
                ans.emplace_back(x, length);
                --k;
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

    // a[l, r) で x 未満の要素の数を返す'''
    int range_freq(int l, int r, T x) const {
        if (x <= 0) return 0;
        if (x >= sigma) return r - l;
        int ans = 0;
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
        if (x == 0) return -1;
        return kth_smallest(l, r, x-1);
    }

    T next_value(int l, int r, T x) const {
        int c = range_freq(l, r, x);
        if (c == r - l) return -1;
        return kth_smallest(l, r, c);
    }

    //`a[l, r)` に含まれる `x` の個数を返します。
    int range_count(int l, int r, T x) const {
        return rank(r, x) - rank(l, x);
    }

    /// 区間 `[l, r)` で値が `[lower, upper)` にある `k` 番目の位置を返す / `O(log(n)log(σ))`
    /// 例: `[5, 1, 4, 1, 9]`, `[lower, upper) = [1, 5)`, `k = 2` なら `3`
    int kth_index_in_value_range(const int l, const int r, const T lower, const T upper, const int k) const {
        assert(0 <= k && k < range_freq(l, r, lower, upper));
        int left = l;
        int right = r;
        while (right - left > 1) {
            const int middle = (left + right) / 2;
            if (range_freq(l, middle, lower, upper) <= k) {
                left = middle;
            } else {
                right = middle;
            }
        }
        return right - 1;
    }

    /// 区間 `[l, r)` で値が `[lower, upper)` にある最初の位置を返す / `O(log(n)log(σ))`
    /// 例: `[5, 1, 4, 1, 9]`, `[lower, upper) = [1, 5)` なら `1`
    int next_index_in_value_range(const int l, const int r, const T lower, const T upper) const {
        return range_freq(l, r, lower, upper) == 0 ? -1 : kth_index_in_value_range(l, r, lower, upper, 0);
    }

    /// 区間 `[l, r)` で値が `[lower, upper)` にある最後の位置を返す / `O(log(n)log(σ))`
    /// 例: `[5, 1, 4, 1, 9]`, `[lower, upper) = [1, 5)` なら `3`
    int prev_index_in_value_range(const int l, const int r, const T lower, const T upper) const {
        const int count = range_freq(l, r, lower, upper);
        return count == 0 ? -1 : kth_index_in_value_range(l, r, lower, upper, count - 1);
    }

    /// 配列を返す / `O(nlog(σ))`
    vector<T> tovector() const {
        vector<T> result(n);
        for (int i = 0; i < n; ++i) result[i] = access(i);
        return result;
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
