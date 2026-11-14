/// https://github.com/titan-23/Library_cpp/blob/main/titan_cpplib/ds/dynamic_wavelet_matrix.cpp
#pragma once

#include <cassert>
#include <iostream>
#include <numeric>
#include <vector>
#include <tuple>
#include <queue>
#include "titan_cpplib/ds/b_tree_bit_vector.cpp"
#include "titan_cpplib/others/print.cpp"
#include "titan_cpplib/others/bit.cpp"
using namespace std;

// DynamicWaveletMatrix
namespace titan23 {

/// @brief 動的ウェーブレット行列
template<typename T>
class DynamicWaveletMatrix {
    private:
    T _sigma;
    int _log;
    vector<BTreeBitVector> _v;
    vector<int> _mid;  // 各段の 0 の個数
    int _size;

    void _build(vector<T> a) {
        if (a.empty()) return;
        vector<uint8_t> v(_size);
        vector<T> work(_size);
        for (int bit = _log-1; bit >= 0; --bit) {
            int zeros = 0;
            for (int i = 0; i < _size; ++i) {
                v[i] = (a[i] >> bit) & 1;
                zeros += !v[i];
            }
            _mid[bit] = zeros;
            _v[bit] = BTreeBitVector(v);
            int zero = 0, one = zeros;
            for (int i = 0; i < _size; ++i) work[v[i] ? one++ : zero++] = a[i];
            a.swap(work);
        }
    }

    // 段 bit の区間 [l, r) を、値の bit が b である要素の次の段での区間に移す
    void _descend(const int bit, const bool b, int &l, int &r, const int l0, const int r0) const {
        if (b) {
            l += _mid[bit] - l0;
            r += _mid[bit] - r0;
        } else {
            l = l0;
            r = r0;
        }
    }

    // 段 bit 以下だけを見て、空でない区間 [l, r) の最大値 (largest) か最小値を返す
    // value には上位の段で決まった bit を入れておく
    T _extreme(int bit, int l, int r, T value, const bool largest) const {
        for (; bit >= 0; --bit) {
            const auto [l0, r0] = _v[bit]._rank0_pair(l, r);
            const bool b = largest ? (r - r0 > l - l0) : (l0 == r0);
            if (b) value |= (T)1 << bit;
            _descend(bit, b, l, r, l0, r0);
        }
        return value;
    }

    public:
    /// @brief 各要素が [0, sigma) の空の列を作る / O(log(σ))
    DynamicWaveletMatrix(const T sigma)
            : _sigma(sigma), _log(bit_length(sigma-1)), _v(_log), _mid(_log), _size(0) {
    }

    /// @brief 各要素が [0, sigma) の列 a から作る / O(nlog(σ))
    DynamicWaveletMatrix(const T sigma, vector<T> &a)
            : _sigma(sigma), _log(bit_length(sigma)), _v(_log), _mid(_log), _size(a.size()) {
        _build(a);
    }

    /// @brief 各段のビット列に n 要素分の領域を予約する
    void reserve(const int n) {
        for (int i = 0; i < _log; ++i) {
            _v[i].reserve(n);
        }
    }

    /// @brief 位置 k に x を挿入する / O(log(n)log(σ))
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

    /// @brief 位置 k の値を削除して返す / O(log(n)log(σ))
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

    /// @brief 位置 k の値を x に更新する / O(log(n)log(σ))
    void set(int k, T x) {
        assert(0 <= k && k < _size);
        // 上位の段から元の値と bit が等しい間は、書き換えだけで済む
        int bit = _log - 1;
        int old_k = k, new_k = k;
        for (; bit >= 0; --bit) {
            const bool new_bit = (x >> bit) & 1;
            const auto [old_bit, rank1] = _v[bit]._access_set_and_rank1(k, new_bit);
            if (old_bit == new_bit) {
                k = old_bit ? rank1 + _mid[bit] : k - rank1;
                continue;
            }
            // この段で bit が変わる。下の段の位置は、変更前は old_k、変更後は new_k になる
            old_k = old_bit ? rank1 + _mid[bit] : k - rank1;
            _mid[bit] += new_bit ? -1 : 1;
            new_k = new_bit ? rank1 + _mid[bit] : k - rank1;
            --bit;
            break;
        }
        // 下の段では、変更前の位置から取り除き、変更後の位置へ挿入する
        for (; bit >= 0; --bit) {
            const int sb = _v[bit]._access_pop_and_rank1(old_k);
            const int s = sb >> 1;
            if (sb & 1) {
                old_k = s + _mid[bit];
            } else {
                --_mid[bit];
                old_k -= s;
            }
            const bool new_bit = (x >> bit) & 1;
            const int t = _v[bit]._insert_and_rank1(new_k, new_bit);
            if (new_bit) {
                new_k = t + _mid[bit];
            } else {
                new_k -= t;
                ++_mid[bit];
            }
        }
    }

    /// @brief 区間 [0, r) の x の個数を返す / O(log(n)log(σ))
    int rank(int r, T x) const {
        int l = 0;
        for (int bit = _log-1; bit >= 0; --bit) {
            if (l == r) return 0;
            const auto [l0, r0] = _v[bit]._rank0_pair(l, r);
            _descend(bit, (x >> bit) & 1, l, r, l0, r0);
        }
        return r - l;
    }

    /// @brief 位置 k の値を返す / O(log(n)log(σ))
    T access(int k) const {
        T s = 0;
        for (int bit = _log-1; bit >= 0; --bit) {
            const auto [b, rank1] = _v[bit]._access_ans_rank1(k);
            if (b) {
                s |= (T)1 << bit;
                k = rank1 + _mid[bit];
            } else {
                k -= rank1;
            }
        }
        return s;
    }

    /// @brief 区間 [l, r) で昇順 k 番目の値を返す / O(log(n)log(σ))
    T kth_smallest(int l, int r, int k) const {
        T s = 0;
        for (int bit = _log-1; bit >= 0; --bit) {
            const auto [l0, r0] = _v[bit]._rank0_pair(l, r);
            const int cnt = r0 - l0;
            const bool b = cnt <= k;
            if (b) {
                s |= (T)1 << bit;
                k -= cnt;
            }
            _descend(bit, b, l, r, l0, r0);
        }
        return s;
    }

    /// @brief 区間 [l, r) で降順 k 番目の値を返す / O(log(n)log(σ))
    T kth_largest(int l, int r, int k) const {
        return kth_smallest(l, r, r-l-k-1);
    }

    /// @brief 区間 [l, r) で過半数を占める値があれば {true, 値} を返す / O(log(n)log(σ))
    pair<bool, T> has_majority(int l, int r) const {
        int length = (r - l) / 2 + 1;
        T s = 0;
        for (int bit = _log-1; bit >= 0; --bit) {
            const auto [l0, r0] = _v[bit]._rank0_pair(l, r);
            int cnt0 = r0 - l0;
            int cnt1 = (r - l) - cnt0;
            if (cnt0 >= length) {
                _descend(bit, false, l, r, l0, r0);
                continue;
            }
            if (cnt1 >= length) {
                s |= (T)1 << bit;
                _descend(bit, true, l, r, l0, r0);
                continue;
            }
            return {false, 0};
        }
        return {true, s};
    }

    /// @brief 区間 [l, r) で頻度が高い値を最大 k 種類、(値, 個数) の組で返す
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
                const auto [l0, r0] = _v[bit]._rank0_pair(l, r);
                if (l0 < r0) hq.emplace(r0-l0, x, l0, bit-1);
                int l1 = l - l0 + _mid[bit];
                int r1 = r - r0 + _mid[bit];
                if (l1 < r1) hq.emplace(r1-l1, x|((T)1<<(T)bit), l1, bit-1);
            }
        }
        return ans;
    }

    /// @brief k 番目の x の位置を返す / O(log(n)log(σ))
    int select(int k, T x) const {
        // 0 の個数 rank0(_size) は _mid に等しい
        int s = 0;
        for (int bit = _log-1; bit >= 0; --bit) {
            if ((x >> bit) & 1) {
                s = _mid[bit] + _v[bit].rank1(s);
            } else {
                s = _v[bit].rank0(s);
            }
        }
        s += k;
        for (int bit = 0; bit < _log; ++bit) {
            if ((x >> bit) & 1) {
                s = _v[bit].select1(s - _mid[bit]);
            } else {
                s = _v[bit].select0(s);
            }
        }
        return s;
    }

    /// @brief 区間 [l, r) で x 未満の要素の個数を返す / O(log(n)log(σ))
    int range_freq(int l, int r, T x) const {
        if (x <= 0) return 0;
        if (x >= _sigma) return r - l;
        int ans = 0;
        for (int bit = _log-1; bit >= 0; --bit) {
            if (l == r) break;
            const auto [l0, r0] = _v[bit]._rank0_pair(l, r);
            const bool b = (x >> bit) & 1;
            if (b) ans += r0 - l0;
            _descend(bit, b, l, r, l0, r0);
        }
        return ans;
    }

    /// @brief 区間 [l, r) で x 以上 y 未満の要素の個数を返す / O(log(n)log(σ))
    int range_freq(int l, int r, T x, T y) const {
        return range_freq(l, r, y) - range_freq(l, r, x);
    }

    /// @brief 区間 [l, r) で x 未満のうち最大の要素を返す / なければ -1 / O(log(n)log(σ))
    T prev_value(int l, int r, T x) const {
        if (l == r || x <= 0) return -1;
        if (x >= _sigma) return _extreme(_log - 1, l, r, 0, true);
        // x の bit をたどり、x 未満へ分かれられる最後の段を候補として覚える
        int candidate_bit = -2, candidate_l = 0, candidate_r = 0;
        T value = 0, candidate_value = 0;
        for (int bit = _log-1; bit >= 0 && l < r; --bit) {
            const auto [l0, r0] = _v[bit]._rank0_pair(l, r);
            const bool b = (x >> bit) & 1;
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

    /// @brief 区間 [l, r) で x 以上のうち最小の要素を返す / なければ -1 / O(log(n)log(σ))
    T next_value(int l, int r, T x) const {
        if (l == r || x >= _sigma) return -1;
        if (x <= 0) return _extreme(_log - 1, l, r, 0, false);
        // x の bit をたどり、x より大きい側へ分かれられる最後の段を候補として覚える
        int candidate_bit = -2, candidate_l = 0, candidate_r = 0;
        T value = 0, candidate_value = 0;
        for (int bit = _log-1; bit >= 0 && l < r; --bit) {
            const auto [l0, r0] = _v[bit]._rank0_pair(l, r);
            const bool b = (x >> bit) & 1;
            if (!b && r - r0 > l - l0) {
                candidate_bit = bit - 1;
                candidate_l = l - l0 + _mid[bit];
                candidate_r = r - r0 + _mid[bit];
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

    /// @brief 区間 [l, r) の x の個数を返す / O(log(n)log(σ))
    int range_count(int l, int r, T x) const {
        for (int bit = _log-1; bit >= 0; --bit) {
            if (l == r) return 0;
            const auto [l0, r0] = _v[bit]._rank0_pair(l, r);
            _descend(bit, (x >> bit) & 1, l, r, l0, r0);
        }
        return r - l;
    }

    /// @brief 区間 [l, r) で値が [lower, upper) にある k 番目の位置を返す / O(log^2(n)log(σ))
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

    /// @brief 区間 [l, r) で値が [lower, upper) にある最初の位置を返す / なければ -1 / O(log^2(n)log(σ))
    int next_index_in_value_range(const int l, const int r, const T lower, const T upper) const {
        return range_freq(l, r, lower, upper) == 0 ? -1 : kth_index_in_value_range(l, r, lower, upper, 0);
    }

    /// @brief 区間 [l, r) で値が [lower, upper) にある最後の位置を返す / なければ -1 / O(log^2(n)log(σ))
    int prev_index_in_value_range(const int l, const int r, const T lower, const T upper) const {
        const int count = range_freq(l, r, lower, upper);
        return count == 0 ? -1 : kth_index_in_value_range(l, r, lower, upper, count - 1);
    }

    /// @brief 値の列を返す / O(nlog(σ))
    vector<T> tovector() const {
        vector<T> a(_size, 0);
        // order[p] は、今の段の位置 p にある要素の元の位置
        vector<int> order(_size), work(_size);
        iota(order.begin(), order.end(), 0);
        for (int bit = _log-1; bit >= 0; --bit) {
            const vector<uint8_t> bits = _v[bit].tovector();
            int zero = 0, one = _mid[bit];
            for (int p = 0; p < _size; ++p) {
                if (bits[p]) {
                    a[order[p]] |= (T)1 << bit;
                    work[one++] = order[p];
                } else {
                    work[zero++] = order[p];
                }
            }
            order.swap(work);
        }
        return a;
    }

    /// @brief 値の列を表示する / O(nlog(σ))
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

    friend ostream& operator<<(ostream& os, const titan23::DynamicWaveletMatrix<T> &dwm) {
        vector<T> a = dwm.tovector();
        os << a;
        return os;
    }
};
} // namespace titan23
