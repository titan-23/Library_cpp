/// https://github.com/titan-23/Library_cpp/blob/main/titan_cpplib/ds/union_find.cpp
#pragma once

#include <iostream>
#include <vector>
#include <map>
#include <cassert>
using namespace std;

namespace titan23 {

/// @brief 経路圧縮と要素数による併合を使う Union-Find
class UnionFind {
private:
    int n, group_numbers;
    vector<int> par;

public:
    UnionFind() {}
    /// @brief n 頂点で構築する / O(n)
    UnionFind(int n) : n(n), group_numbers(n), par(n, -1) {}

    /// @brief x の根を返す / 償却 O(α(n))
    int root(int x) {
        int a = x, y;
        while (par[a] >= 0) a = par[a];
        while (par[x] >= 0) {
            y = x;
            x = par[x];
            par[y] = a;
        }
        return a;
    }

    /// @brief x と y の属する集合を併合し、併合したかを返す / 償却 O(α(n))
    bool unite(int x, int y) {
        x = root(x);
        y = root(y);
        if (x == y) return false;
        --group_numbers;
        if (par[x] >= par[y]) swap(x, y);
        par[x] += par[y];
        par[y] = x;
        return true;
    }

    /// @brief x の属する集合の要素数を返す / 償却 O(α(n))
    int size(const int x) {
        return -par[root(x)];
    }

    /// @brief x と y が同じ集合に属するかを返す / 償却 O(α(n))
    bool same(const int x, const int y) {
        return root(x) == root(y);
    }

    /// @brief すべての根を昇順に返す / O(n)
    vector<int> all_roots() const {
        vector<int> res;
        for (int i = 0; i < n; ++i) {
            if (par[i] < 0) res.emplace_back(i);
        }
        return res;
    }

    /// @brief 根ごとに集合の要素を昇順に並べて返す / O(n log n)
    map<int, vector<int>> all_group_members() {
        map<int, vector<int>> res;
        for (int i = 0; i < n; ++i) {
            res[root(i)].emplace_back(i);
        }
        return res;
    }

    /// @brief 集合の個数を返す / O(1)
    int group_count() const {
        return group_numbers;
    }

    /// @brief すべての頂点を別々の集合に戻す / O(n)
    void clear() {
        group_numbers = n;
        fill(par.begin(), par.end(), -1);
    }

    friend ostream& operator<<(ostream& os, titan23::UnionFind &uf) {
        map<int, vector<int>> group_members = uf.all_group_members();
        os << "<UnionFind>\n";
        for (auto &[key, val]: group_members) {
            os << "  " << key << " : ";
            for (const int &v : val) {
                os << v << ", ";
            }
        }
        return os;
    }
};
}  // namespace titan23
