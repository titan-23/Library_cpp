/// https://github.com/titan-23/Library_cpp/blob/main/titan_cpplib/alg/zaatsu.cpp
#pragma once

#include <vector>
#include <algorithm>
#include <cassert>
using namespace std;

// Zaatsu
namespace titan23 {

/**
 * @brief 座標圧縮管理クラス
 */
template<typename T>
class Zaatsu {
private:
    vector<T> a;
    int n;
    bool built;

public:
    Zaatsu() : n(0), built(true) {}

    /// @brief `used_items` からなる集合を管理するインスタンスを生成
    Zaatsu(const vector<T> &used_items) : a(used_items) {
        build();
    }

    /// @brief `v` を追加する
    void add(const T &v) {
        a.push_back(v);
        built = false;
    }

    /// @brief 構築する
    void build() {
        sort(a.begin(), a.end());
        a.erase(unique(a.begin(), a.end()), a.end());
        n = a.size();
        built = true;
    }

    /// @brief 要素の種類数を返す
    int len() const {
        assert(built);
        return n;
    }

    /// @brief 要素の種類数を返す
    int size() const {
        assert(built);
        return n;
    }

    /// @brief `v` を座標圧縮する
    int to_zaatsu(const T &v) const {
        assert(built);
        return lower_bound(a.begin(), a.end(), v) - a.begin();
    }

    /// @brief 座標圧縮された要素 `x` を戻す
    T to_origin(const int &x) const {
        assert(built);
        assert(0 <= x && x < n);
        return a[x];
    }
};
}  // namespace titan23
