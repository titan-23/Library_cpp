/// https://github.com/titan-23/Library_cpp/blob/main/titan_cpplib/ds/multiset_topk.cpp
#pragma once

#include <vector>
#include <set>

using namespace std;

namespace titan23 {

/// @brief 小さい方から K 個の要素の総積を求められる多重集合 / op, e, inv が可換群をなすことが前提
template<class T, T(*op)(T, T), T(*e)(), T(*inv)(T)>
class MultisetTopK {
private:
    int K;
    multiset<T> small, large;
    T product;

    void rebuild() {
        while ((int)small.size() < K && !large.empty()) {
            auto it = large.begin();
            product = op(product, *it);
            small.insert(*it);
            large.erase(it);
        }
        while (!small.empty() && !large.empty()) {
            auto s = --small.end();
            auto l = large.begin();
            T vs = *s, vl = *l;
            if (vs <= vl) break;
            product = op(product, inv(vs));
            product = op(product, vl);
            small.erase(s); small.insert(vl);
            large.erase(l); large.insert(vs);
        }
    }

public:
    MultisetTopK() : K(-1), product(e()) {}
    /// @brief 空の集合で構築し、小さい方から K 個を対象にする
    MultisetTopK(int K) : K(K), product(e()) {}
    /// @brief 列 a の要素で構築し、小さい方から K 個を対象にする
    MultisetTopK(int K, vector<T> a) : K(K), large(a.begin(), a.end()), product(e()) {}

    /// @brief v を追加する / O(log n)
    void insert(T v) {
        large.insert(v);
    }

    /// @brief v を 1 個削除する / なければ何もしない / O(log n)
    void erase(T v) {
        if (small.find(v) != small.end()) {
            small.erase(small.find(v));
            product = op(product, inv(v));
        } else if (large.find(v) != large.end()) {
            large.erase(large.find(v));
        }
    }

    /// @brief 小さい方から K 個の要素の総積を返す / 要素数が K 未満なら全要素の総積
    T sum() { 
        rebuild();
        return product;
    }

    /// @brief 要素数を返す / O(1)
    int len() const { return small.size() + large.size(); }
};
} // namespace titan23
