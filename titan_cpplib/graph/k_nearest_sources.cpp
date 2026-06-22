#pragma once

#include <vector>
#include <queue>
#include <tuple>
#include <algorithm>
#include <cassert>
using namespace std;

namespace titan23 {

/**
 * @brief 多始点から各頂点への、相異なる始点による近い方K個の距離と始点
 *        時間 O(K(N+M)log(KN)) / 空間 O(KN)
 */
template<typename T>
class KNearestSources {
private:
    bool update(int v, T d, int s) {
        int w = v*K;
        for (int i = 0; i < K; ++i) {
            int j = v*K + i;
            if (dist[j].second == s) {
                if (dist[j].first <= d) return false;
                dist[j].first = d;
                return true;
            }
            if (dist[w] < dist[j]) w = j;
        }
        pair<T, int> item = {d, s};
        if (item < dist[w]) {
            dist[w] = item;
            return true;
        }
        return false;
    }

public:
    int n, K;
    T INF;
    vector<pair<T, int>> dist;

    KNearestSources() {}
    KNearestSources(const vector<vector<pair<int, T>>> &G, const vector<int> &S, int K, T INF) :
            n(G.size()), K(K), INF(INF), dist(n*K, {INF, -1}) {
        using P = tuple<T, int, int>;
        priority_queue<P, vector<P>, greater<P>> hq;
        for (const int s : S) {
            if (update(s, 0, s)) {
                hq.emplace(0, s, s);
            }
        }
        while (!hq.empty()) {
            auto [d, s, v] = hq.top(); hq.pop();
            bool valid = false;
            for (int i = 0; i < K; ++i) {
                if (dist[v*K+i].second == s && dist[v*K+i].first == d) {
                    valid = true;
                    break;
                }
            }
            if (!valid) continue;
            for (const auto &[x, w] : G[v]) {
                T nd = d + w;
                if (update(x, nd, s)) {
                    hq.emplace(nd, s, x);
                }
            }
        }
        for (int v = 0; v < n; ++v) {
            sort(dist.begin()+v*K, dist.begin()+v*K+K);
        }
    }

    // v に届いた相異なる始点の数(0以上K以下)
    int count(int v) const {
        int c = 0;
        for (int i = 0; i < K; ++i) {
            if (dist[v*K+i].second != -1) ++c;
        }
        return c;
    }

    // v への k 番目(0-indexed)の距離 / 無ければ INF
    T get_dist(int v, int k) const {
        assert(0 <= k && k < K);
        return dist[v*K+k].first;
    }

    // v への k 番目(0-indexed)の始点番号 / 無ければ -1
    int get_source(int v, int k) const {
        assert(0 <= k && k < K);
        return dist[v*K+k].second;
    }
};
}  // namespace titan23
