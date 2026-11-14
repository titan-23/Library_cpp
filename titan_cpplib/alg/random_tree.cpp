/// https://github.com/titan-23/Library_cpp/blob/main/titan_cpplib/alg/random_tree.cpp
#pragma once

#include <vector>
#include <set>
#include <utility>
#include <cassert>
#include "titan_cpplib/alg/random_mt.cpp"
using namespace std;

namespace titan23 {

/// @brief 無作為な木を生成する
class RandomTree {
private:
    titan23::RandomMT random;
    int n;

    static int _idx(vector<int> &a, int x) {
        for (int i = 0; i < (int)a.size(); ++i) {
            if (a[i] == x) return i;
        }
        return -1;
    }

public:
    /// @brief 乱数の種を random_device から取って構築する
    RandomTree() {}
    /// @brief 乱数の種 seed で構築する
    RandomTree(int seed) : random(seed) {}

    /// @brief n 頂点のラベル付き木を一様に選び、その辺集合を返す / O(n log n)
    /// 参考: https://speakerdeck.com/tsutaj/beerbash-lt-230711?slide=27
    vector<pair<int, int>> gen_random(int n) {
        assert(n >= 0);
        vector<pair<int, int>> edges;
        if (n <= 1) {
            return edges;
        }
        edges.reserve(n-1);
        vector<int> D(n, 1);
        vector<int> A(n-2, 0);
        for (int i = 0; i < n-2; ++i) {
            int v = random.randrange(0, n);
            D[v]++;
            A[i] = v;
        }
        set<pair<int, int>> st;
        for (int i = 0; i < n; ++i) {
            st.insert({D[i], i});
        }
        for (const int &a : A) {
            auto it = st.begin();
            int d = it->first, v = it->second;
            st.erase(it);
            assert(d == 1);
            edges.emplace_back(v, a);
            D[v]--;
            st.erase({D[a], a});
            D[a]--;
            if (D[a] >= 1) {
                st.insert({D[a], a});
            }
        }
        int u = _idx(D, 1);
        D[u]--;
        int v = _idx(D, 1);
        edges.emplace_back(u, v);
        assert(edges.size() == n-1);
        random.shuffle(edges);
        return edges;
    }

    /// @brief n 頂点の無作為なパスグラフの辺集合を返す / O(n)
    vector<pair<int, int>> gen_path(int n) {
        // 一様分布になるかは未確認
        assert(n >= 0);
        vector<pair<int, int>> edges;
        if (n <= 1) {
            return edges;
        }
        vector<int> p(n);
        for (int i = 0; i < n; ++i) {
            p[i] = i;
        }
        random.shuffle(p);
        edges.resize(n-1);
        for (int i = 0; i < n-1; ++i) {
            if (random.randint(1)) {
                edges[i] = {p[i], p[i+1]};
            } else {
                edges[i] = {p[i+1], p[i]};
            }
        }
        random.shuffle(edges);
        return edges;
    }
};
}  // namespace titan23
