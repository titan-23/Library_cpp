/// https://github.com/titan-23/Library_cpp/blob/main/titan_cpplib/ds/merge_sort_tree.cpp
#pragma once

#include <algorithm>
#include <iostream>
#include <vector>
#include "titan_cpplib/others/bit.cpp"
using namespace std;

namespace titan23 {

/// @brief 各ノードが区間の要素を昇順に並べた列と、それに func を適用した値を持つセグメント木
template<class T, class F, F func(const vector<T>&)>
class MergeSortTree {
private:
    int n, size, log;

    struct Data {
        vector<T> data;
        F func_data;
    };

    vector<Data> data;

public:
    MergeSortTree() {}
    /// @brief 列 a から構築する / func の計算を除いて O(n log n)
    MergeSortTree(vector<T> a) {
        n = a.size();
        log = bit_length(n);
        size = 1 << log;
        data.resize(size<<1);
        for (int i = size; i < size+n; ++i) {
            data[i].data.emplace_back(a[i-size]);
        }
        for (int i = size-1; i > 0; --i) {
            const auto &l = data[i<<1].data;
            const auto &r = data[i<<1|1].data;
            auto &now = data[i].data;
            now.resize(l.size() + r.size());
            std::merge(l.begin(), l.end(), r.begin(), r.end(), now.begin());
        }
        for (int i = 1; i < data.size(); ++i) {
            data[i].func_data = func(data[i].data);
        }
    }

    /// @brief 区間 [l, r) を覆うノードごとに g(data, func_data) を呼ぶ / 呼ぶ回数は O(log n)
    /// g は [&] (vector<T> &data, F &func_data) -> void の形にする。引数の & を付け忘れないこと
    template <class G>
    void prod(int l, int r, G &&g) {
        l += size; r += size;
        while (l < r) {
            if (l & 1) {
                g(data[l].data, data[l].func_data);
                l++;
            }
            if (r & 1) {
                --r;
                g(data[r].data, data[r].func_data);
            }
            l >>= 1; r >>= 1;
        }
    }

    /// @brief 位置 k を含む全ノードの func_data に g を適用する / 呼ぶ回数は O(log n)
    /// g は [&] (F &func_data) -> void の形にする
    template <class G>
    void apply_point(int k, G &&g) {
        k += size;
        while (k > 0) {
            g(data[k].func_data);
            k >>= 1;
        }
    }
};
} // namespace titan23
