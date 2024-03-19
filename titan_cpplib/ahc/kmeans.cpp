#include <vector>
#include <set>
#include "titan_cpplib/algorithm/random.cpp"
using namespace std;

// Kmeans
namespace titan23 {

  template <class T,
            int (*dist)(const T&, const T&),
            T (*mean)(const vector<T>&)>
  class Kmeans {
   private:
    int k, max_iter;
    titan23::Random my_random;

   public:
    Kmeans() : k(0), max_iter() {}
    Kmeans(int k, int max_iter) : k(k), max_iter(max_iter) {}

    pair<vector<int>, vector<T>> fit(const vector<T> &X) {
      int n = (int)X.size();
      assert(k <= n);
      vector<T> first_cluster = {my_random.choice(X)};
      set<T> cluster_set;
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
        T tmpk = my_random.choice(X, p_f, false);
        assert(cluster_set.find(tmpk) == cluster_set.end());
        first_cluster.emplace_back(tmpk);
        cluster_set.insert(tmpk); // iranai
      }

      vector<T> cluster_centers = first_cluster;
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
        vector<vector<T>> syuukei(k);
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


/*
using T = pair<int, int>;
int dist(const T &s, const T &t) {
  return abs(s.first-t.first) + abs(s.second-t.second);
}
T mean(const vector<T> &L) {
  return {0, 0};
}
*/
