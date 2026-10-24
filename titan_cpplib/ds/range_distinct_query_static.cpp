/// https://github.com/titan-23/Library_cpp/blob/main/titan_cpplib/ds/range_distinct_query_static.cpp
#pragma once

#include <algorithm>
#include <cassert>
#include <limits>
#include <map>
#include <tuple>
#include <utility>
#include <vector>
#include "titan_cpplib/ds/wavelet_matrix.cpp"
using namespace std;

namespace titan23 {

/**
 * @brief 静的な列の区間に含まれる異なる値の個数
 *
 * 構築 O(n log n)、クエリ O(log n)、保持する索引は O(n log n) bits
 */
template<typename T>
class StaticRangeDistinctQuery {
private:
    WaveletMatrix<int> wm;

    static WaveletMatrix<int> build(const vector<T> &a) {
        // BitVector の切り上げ計算 n+63 が int に収まる範囲
        assert(a.size() <= static_cast<size_t>(numeric_limits<int>::max() - 63));
        const int n = static_cast<int>(a.size());
        // prev[i] は直前の同値要素の位置+1（なければ0）
        vector<int> prev(n);
        map<T, int> last;
        for (int i = 0; i < n; ++i) {
            auto it = last.try_emplace(a[i], 0).first;
            prev[i] = it->second;
            it->second = i + 1;
        }
        return WaveletMatrix<int>(max(1, n), prev);
    }

public:
    StaticRangeDistinctQuery() : wm(1) {}

    explicit StaticRangeDistinctQuery(const vector<T> &a) : wm(build(a)) {}

    /// 区間 [l, r) に含まれる異なる値の個数を返す / O(log n)
    int count(const int l, const int r) const {
        assert(0 <= l && l <= r && r <= len());
        if (l == r) return 0;
        // prev[i] <= l の要素が、区間内でその値の最初の出現となる
        return wm.range_freq(l, r, l + 1);
    }

    int range_distinct(const int l, const int r) const { return count(l, r); }

    int len() const { return wm.len(); }

    bool empty() const { return len() == 0; }
};

} // namespace titan23
