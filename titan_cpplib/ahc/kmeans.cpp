#include <vector>
#include <set>
#include "../algorithm/random.cpp"
using namespace std;

// Kmeans
namespace titan23 {

  using D = pair<int, int>;

  struct Kmeans {
    int k, max_iter;
    titan23::Random my_random;

    Kmeans(int k, int max_iter) : k(k), max_iter(max_iter) {}

    int dist(const D &s, const D &t) const {
      return abs(s.first-t.first) + abs(s.second-t.second);
    }

    pair<int, int> mean(const vector<D> &L) const {
      int sx = 0, sy = 0;
      for (const auto &[x, y]: L) {
        sx += x;
        sy += y;
      }
      return {sx/L.size(), sy/L.size()};
    }

    pair<vector<int>, vector<D>> fit(const vector<D> &X) {
      int n = (int)X.size();
      assert(k <= n);
      vector<D> first_cluster = {my_random.choice(X)};
      set<D> cluster_set;
      cluster_set.insert(first_cluster[0]);

      while (first_cluster.size() < k) {
        vector<int> p_f(n, 0);
        bool flag = false;
        for (int i = 0; i < n; ++i) {
          if (cluster_set.find(X[i]) == cluster_set.end()) {
            int min_d = dist(X[i], first_cluster[0]);
            for (int i = 1; i < first_cluster.size(); ++i) {
              min_d = min(min_d, dist(X[i], first_cluster[i]));
            }
            flag = true;
            p_f[i] = min_d+1;
          }
        }
        assert(flag);
        D tmpk = my_random.choice(X, p_f, false);
        assert(cluster_set.find(tmpk) == cluster_set.end());
        first_cluster.emplace_back(tmpk);
        cluster_set.insert(tmpk); // iranai
      }

      vector<D> cluster_centers = first_cluster;
      vector<int> labels(n, -1);
      for (int i = 0; i < n; ++i) {
        int d = dist(X[i], first_cluster[0]);
        for (int j = 0; j < k; ++j) {
          int tmp = dist(X[i], first_cluster[j]);
          if (tmp <= d) {
            d = tmp;
            labels[i] = j;
          }
        }
      }

      for (int _ = 0; _ < max_iter; ++_) {
        vector<vector<D>> syuukei(k);
        for (int i = 0; i < n; ++i) {
          syuukei[labels[i]].emplace_back(X[i]);
        }
        cluster_centers.clear();
        for (int i = 0; i < k; ++i) {
          cluster_centers.emplace_back(mean(syuukei[i]));
        }
        for (int i = 0; i < n; ++i) {
          int d = dist(X[i], first_cluster[0]);
          for (int j = 0; j < k; ++j) {
            int tmp = dist(X[i], first_cluster[j]);
            if (tmp <= d) {
              d = tmp;
              labels[i] = j;
            }
          }
        }
      }
      return {labels, cluster_centers};
    }
  };
}

