/// https://github.com/titan-23/Library_cpp/blob/main/titan_cpplib/ds/wavelet_matrix.cpp
#pragma once

#include <cassert>
#include <numeric>
#include <vector>
#include <queue>
#include "titan_cpplib/ds/bit_vector.cpp"
#include "titan_cpplib/others/bit.cpp"
using namespace std;

namespace titan23 {

/// @brief 値が [0, sigma) の静的な列で、区間の k 番目の値や頻度を求めるウェーブレット行列
template<typename T>
class WaveletMatrix {
private:
    T sigma;
    int log;
    vector<BitVector> v;
    vector<int> mid;
    int n;

    void build(vector<T> a) {
        vector<T> work(n);
        for (int bit = log-1; bit >= 0; --bit) {
            v[bit] = BitVector(n, [&](const int i) { return (a[i] >> bit) & 1; });
            const int zeros = v[bit].rank0(n);
            mid[bit] = zeros;
            // 0 の要素を前、1 の要素を後ろへ安定に並べる
            int zero = 0, one = zeros;
            for (int i = 0; i < n; ++i) {
                work[(a[i] >> bit) & 1 ? one++ : zero++] = a[i];
            }
            a.swap(work);
        }
    }

    // 段 bit の区間 [l, r) を、値の bit が b である要素の次の段での区間に移す
    void _descend(const int bit, const bool b, int &l, int &r, const int l0, const int r0) const {
        if (b) {
            l += mid[bit] - l0;
            r += mid[bit] - r0;
        } else {
            l = l0;
            r = r0;
        }
    }

    // 段 bit 以下だけを見て、空でない区間 [l, r) の最大値 (largest) か最小値を返す
    // value には上位の段で決まった bit を入れておく
    T _extreme(int bit, int l, int r, T value, const bool largest) const {
        for (; bit >= 0; --bit) {
            const int l0 = v[bit].rank0(l), r0 = v[bit].rank0(r);
            const bool b = largest ? (r - r0 > l - l0) : (l0 == r0);
            if (b) value |= (T)1 << bit;
            _descend(bit, b, l, r, l0, r0);
        }
        return value;
    }

public:
    WaveletMatrix() {}

    /// @brief 値の範囲を [0, sigma) とする空の列を作る
    WaveletMatrix(const T sigma)
        : sigma(sigma), log(bit_length(sigma-1)), v(log), mid(log), n(0) {}

    /// @brief 値が [0, sigma) の列 a から構築する / O(nlog(σ))
    WaveletMatrix(const T sigma, const vector<T> &a)
        : sigma(sigma), log(bit_length(sigma-1)), v(log), mid(log), n(a.size()) {
        build(a);
    }

    /// @brief a[k] を返す / O(log(σ))
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

    /// @brief a[0, r) に含まれる x の個数を返す / O(log(σ))
    int rank(int r, T x) const {
        return range_count(0, r, x);
    }

    /// @brief k 番目の x の位置を返す / k は 0 から数える / O(log(n)log(σ))
    int select(int k, T x) const {
        int s = 0;
        for (int bit = log-1; bit >= 0; --bit) {
            // 段 bit の 0 の個数 rank0(n) は mid[bit] に等しい
            if ((x >> bit) & 1) {
                s = mid[bit] + v[bit].rank1(s);
            } else {
                s = v[bit].rank0(s);
            }
        }
        s += k;
        for (int bit = 0; bit < log; ++bit) {
            if ((x >> bit) & 1) {
                s = v[bit].select1(s - mid[bit]);
            } else {
                s = v[bit].select0(s);
            }
        }
        return s;
    }

    /// @brief 区間 [l, r) にある k 番目の x の位置を返す / k は 0 から数える / O(log(n)log(σ))
    int range_select(const int l, const int r, const int k, const T x) const {
        assert(0 <= k && k < range_count(l, r, x));
        return select(rank(l, x) + k, x);
    }

    /// @brief a[l, r) で k 番目に小さい値を返す / k は 0 から数える / O(log(σ))
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

    /// @brief a[l, r) で k 番目に大きい値を返す / k は 0 から数える / O(log(σ))
    T kth_largest(int l, int r, int k) const {
        return kth_smallest(l, r, r-l-k-1);
    }

    /// @brief 区間 [l, r) に過半数を占める値があるかとその値を返す / なければ {false, 0} / O(log(σ))
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

    /// @brief a[l, r) の値を出現回数の多い順に、回数とともに最大 k 個返す
    vector<pair<T, int>> topk(int l, int r, int k) const {
        // 要素は (区間長, 値, 左端, bit) で、区間長の大きい順に取り出す
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
                int l1 = l - l0 + mid[bit];
                int r1 = r - r0 + mid[bit];
                if (l1 < r1) hq.emplace(r1-l1, x|((T)1<<bit), l1, bit-1);
            }
        }
        return ans;
    }

    /// @brief a[l, r) で x 未満の要素の個数を返す / O(log(σ))
    int range_freq(int l, int r, T x) const {
        if (x <= 0) return 0;
        if (x >= sigma) return r - l;
        int ans = 0;
        for (int bit = log-1; bit >= 0 && l < r; --bit) {
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

    /// @brief a[l, r) で x 以上 y 未満の要素の個数を返す / O(log(σ))
    int range_freq(int l, int r, T x, T y) const {
        return range_freq(l, r, y) - range_freq(l, r, x);
    }

    /// @brief a[l, r) で y 未満の最大の要素を返す / なければ -1 / O(log(σ))
    T prev_value(int l, int r, T y) const {
        if (l == r || y <= 0) return -1;
        if (y >= sigma) return _extreme(log - 1, l, r, 0, true);
        // y の bit をたどり、y 未満へ分かれられる最後の段を候補として覚える
        int candidate_bit = -2, candidate_l = 0, candidate_r = 0;
        T value = 0, candidate_value = 0;
        for (int bit = log-1; bit >= 0 && l < r; --bit) {
            const int l0 = v[bit].rank0(l), r0 = v[bit].rank0(r);
            const bool b = (y >> bit) & 1;
            if (b) {
                if (l0 < r0) {
                    candidate_bit = bit - 1;
                    candidate_l = l0;
                    candidate_r = r0;
                    candidate_value = value;
                }
                value |= (T)1 << bit;
            }
            _descend(bit, b, l, r, l0, r0);
        }
        if (candidate_bit == -2) return -1;
        return _extreme(candidate_bit, candidate_l, candidate_r, candidate_value, true);
    }

    /// @brief a[l, r) で x 以上の最小の要素を返す / なければ -1 / O(log(σ))
    T next_value(int l, int r, T x) const {
        if (l == r || x >= sigma) return -1;
        if (x <= 0) return _extreme(log - 1, l, r, 0, false);
        // x の bit をたどり、x より大きい側へ分かれられる最後の段を候補として覚える
        int candidate_bit = -2, candidate_l = 0, candidate_r = 0;
        T value = 0, candidate_value = 0;
        for (int bit = log-1; bit >= 0 && l < r; --bit) {
            const int l0 = v[bit].rank0(l), r0 = v[bit].rank0(r);
            const bool b = (x >> bit) & 1;
            if (!b && r - r0 > l - l0) {
                candidate_bit = bit - 1;
                candidate_l = l - l0 + mid[bit];
                candidate_r = r - r0 + mid[bit];
                candidate_value = value | ((T)1 << bit);
            }
            if (b) value |= (T)1 << bit;
            _descend(bit, b, l, r, l0, r0);
        }
        // 最後まで残った要素は x に等しい
        if (l < r) return value;
        if (candidate_bit == -2) return -1;
        return _extreme(candidate_bit, candidate_l, candidate_r, candidate_value, false);
    }

    /// @brief a[l, r) に含まれる x の個数を返す / O(log(σ))
    int range_count(int l, int r, T x) const {
        for (int bit = log-1; bit >= 0; --bit) {
            if (l == r) return 0;
            const int l0 = v[bit].rank0(l), r0 = v[bit].rank0(r);
            _descend(bit, (x >> bit) & 1, l, r, l0, r0);
        }
        return r - l;
    }

    /// @brief 区間 [l, r) で値が [lower, upper) にある k 番目の位置を返す / k は 0 から数える / O(log(n)log(σ))
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

    /// @brief 区間 [l, r) で値が [lower, upper) にある最初の位置を返す / なければ -1 / O(log(n)log(σ))
    int next_index_in_value_range(const int l, const int r, const T lower, const T upper) const {
        return range_freq(l, r, lower, upper) == 0 ? -1 : kth_index_in_value_range(l, r, lower, upper, 0);
    }

    /// @brief 区間 [l, r) で値が [lower, upper) にある最後の位置を返す / なければ -1 / O(log(n)log(σ))
    int prev_index_in_value_range(const int l, const int r, const T lower, const T upper) const {
        const int count = range_freq(l, r, lower, upper);
        return count == 0 ? -1 : kth_index_in_value_range(l, r, lower, upper, count - 1);
    }

    /// @brief 列を vector にして返す / O(nlog(σ))
    vector<T> tovector() const {
        vector<T> result(n, 0);
        // order[p] は、今の段の位置 p にある要素の元の位置
        vector<int> order(n), work(n);
        iota(order.begin(), order.end(), 0);
        for (int bit = log-1; bit >= 0; --bit) {
            int zero = 0, one = mid[bit];
            for (int p = 0; p < n; ++p) {
                if (v[bit].access(p)) {
                    result[order[p]] |= (T)1 << bit;
                    work[one++] = order[p];
                } else {
                    work[zero++] = order[p];
                }
            }
            order.swap(work);
        }
        return result;
    }

    /// @brief 列の長さを返す / O(1)
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
