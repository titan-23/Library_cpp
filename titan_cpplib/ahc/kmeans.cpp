#pragma once

#include <vector>
#include <set>
#include <cassert>
#include "titan_cpplib/algorithm/random.cpp"
#include <atcoder/mincostflow>
using namespace std;

// Kmeans
namespace titan23 {

template <class DistType, class ElmType, DistType (*dist)(const ElmType&, const ElmType&), ElmType (*mean)(const vector<ElmType>&)>
class Kmeans {
private:
    int k, max_iter;
    titan23::Random krnd;

public:
    Kmeans() : k(0), max_iter(0) {}
    Kmeans(const int k, const int max_iter) : k(k), max_iter(max_iter) {}

    pair<vector<int>, vector<ElmType>> fit(const vector<ElmType> &X, const vector<ElmType> &init_centers) {
        int n = (int)X.size();
        assert(k <= n);
        assert((int)init_centers.size() == k);
        vector<ElmType> cluster_centers = init_centers;
        vector<int> labels(n, -1);
        for (int _ = 0; _ < max_iter; ++_) {
            bool changed = false;
            for (int i = 0; i < n; ++i) {
                DistType min_d = dist(X[i], cluster_centers[0]);
                int best_k = 0;
                for (int j = 1; j < k; ++j) {
                    DistType tmp = dist(X[i], cluster_centers[j]);
                    if (tmp < min_d) {
                        min_d = tmp;
                        best_k = j;
                    }
                }
                if (labels[i] != best_k) {
                    labels[i] = best_k;
                    changed = true;
                }
            }
            if (!changed && _ > 0) break;
            vector<vector<ElmType>> syuukei(k);
            for (int i = 0; i < n; ++i) {
                syuukei[labels[i]].emplace_back(X[i]);
            }

            cluster_centers.clear();
            for (int j = 0; j < k; ++j) {
                cluster_centers.emplace_back(mean(syuukei[j]));
            }
        }

        return {labels, cluster_centers};
    }

    pair<vector<int>, vector<ElmType>> fit(const vector<ElmType> &X) {
        int n = (int)X.size();
        assert(k <= n);
        vector<ElmType> first_cluster = {krnd.choice(X)};
        set<ElmType> cluster_set;
        cluster_set.insert(first_cluster[0]);
        while ((int)first_cluster.size() < k) {
            vector<int> p_f(n, 0);
            bool flag = false;
            for (int i = 0; i < n; ++i) {
                if (cluster_set.find(X[i]) == cluster_set.end()) {
                    DistType min_d = dist(X[i], first_cluster[0]);
                    for (int j = 1; j < (int)first_cluster.size(); ++j) {
                        min_d = min(min_d, dist(X[i], first_cluster[j]));
                    }
                    flag = true;
                    p_f[i] = min_d + 1;
                }
            }
            assert(flag);
            ElmType tmpk = krnd.choice(X, p_f, false);
            assert(cluster_set.find(tmpk) == cluster_set.end());
            first_cluster.emplace_back(tmpk);
            cluster_set.insert(tmpk);
        }
        return fit(X, first_cluster);
    }

    // cost_scale=1e6など
    pair<vector<int>, vector<ElmType>> fit_flow(
        const vector<ElmType> &X,
        const vector<ElmType> &init_centers,
        const vector<int> &target_sizes,
        double cost_scale = 1.0)
    {
        int n = (int)X.size();
        assert(k <= n);
        assert((int)init_centers.size() == k);
        assert((int)target_sizes.size() == k);
        long long sum_sizes = 0;
        for(int s : target_sizes) sum_sizes += s;
        assert(sum_sizes == n);
        vector<ElmType> cluster_centers = init_centers;
        vector<int> labels(n, -1);
        for (int _ = 0; _ < max_iter; ++_) {
            atcoder::mcf_graph<int, long long> graph(n + k + 2);
            int S = n + k;
            int T = n + k + 1;
            for (int i = 0; i < n; ++i) {
                graph.add_edge(S, i, 1, 0);
            }
            for (int j = 0; j < k; ++j) {
                graph.add_edge(n + j, T, target_sizes[j], 0);
            }
            for (int i = 0; i < n; ++i) {
                for (int j = 0; j < k; ++j) {
                    DistType d = dist(X[i], cluster_centers[j]);
                    long long cost = std::round(static_cast<double>(d) * cost_scale);
                    graph.add_edge(i, n + j, 1, cost);
                }
            }
            auto result = graph.flow(S, T, n);
            assert(result.first == n);
            bool changed = false;
            for (const auto& edge : graph.edges()) {
                if (edge.from < n && edge.to >= n && edge.to < n + k && edge.flow == 1) {
                    int c_id = edge.to - n;
                    if (labels[edge.from] != c_id) {
                        labels[edge.from] = c_id;
                        changed = true;
                    }
                }
            }
            if (!changed && _ > 0) break;
            vector<vector<ElmType>> syuukei(k);
            for (int i = 0; i < n; ++i) {
                syuukei[labels[i]].emplace_back(X[i]);
            }
            cluster_centers.clear();
            for (int j = 0; j < k; ++j) {
                if (!syuukei[j].empty()) {
                    cluster_centers.emplace_back(mean(syuukei[j]));
                } else {
                    cluster_centers.emplace_back(init_centers[j]);
                }
            }
        }
        return {labels, cluster_centers};
    }

    // cost_scale=1e6など
    pair<vector<int>, vector<ElmType>> fit_flow(
        const vector<ElmType> &X,
        const vector<int> &target_sizes,
        double cost_scale = 1.0)
    {
        int n = (int)X.size();
        assert(k <= n);
        vector<ElmType> first_cluster = {krnd.choice(X)};
        set<ElmType> cluster_set;
        cluster_set.insert(first_cluster[0]);
        while ((int)first_cluster.size() < k) {
            vector<int> p_f(n, 0);
            bool flag = false;
            for (int i = 0; i < n; ++i) {
                if (cluster_set.find(X[i]) == cluster_set.end()) {
                    DistType min_d = dist(X[i], first_cluster[0]);
                    for (int j = 1; j < (int)first_cluster.size(); ++j) {
                        min_d = min(min_d, dist(X[i], first_cluster[j]));
                    }
                    flag = true;
                    p_f[i] = min_d + 1;
                }
            }
            assert(flag);
            ElmType tmpk = krnd.choice(X, p_f, false);
            assert(cluster_set.find(tmpk) == cluster_set.end());
            first_cluster.emplace_back(tmpk);
            cluster_set.insert(tmpk);
        }
        return fit_flow(X, first_cluster, target_sizes, cost_scale);
    }
};
}
