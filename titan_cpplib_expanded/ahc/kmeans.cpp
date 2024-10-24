// #include "titan_cpplib/ahc/kmeans.cpp"
#include <vector>
#include <set>
// #include "titan_cpplib/algorithm/random.cpp"
#include <cassert>
#include <unordered_set>
#include <vector>
using namespace std;

// Random
namespace titan23 {

    /**
     * @brief (疑似)乱数生成クラス(XOR shift)
     */
    class Random {
      private:
        unsigned int _x, _y, _z, _w;

        unsigned int _xor128() {
            const unsigned int t = _x ^ (_x << 11);
            _x = _y;
            _y = _z;
            _z = _w;
            _w = (_w ^ (_w >> 19)) ^ (t ^ (t >> 8));
            return _w;
        }

      public:
        Random() : _x(123456789),
                   _y(362436069),
                   _z(521288629),
                   _w(88675123) {}

        //! `[0, 1]` の乱数を返す(実数)
        double random() { return (double)(_xor128()) / 0xFFFFFFFF; }

        //! `[0, end]` の乱数を返す(整数)
        int randint(const int end) {
            assert(0 <= end);
            return (((unsigned long long)_xor128() * (end+1)) >> 32);
        }

        //! `[begin, end]` の乱数を返す(整数)
        int randint(const int begin, const int end) {
            assert(begin <= end);
            return begin + (((unsigned long long)_xor128() * (end-begin+1)) >> 32);
        }

        //! `[0, end)` の乱数を返す(整数)
        int randrange(const int end) {
            assert(0 < end);
            return (((unsigned long long)_xor128() * end) >> 32);
        }

        //! `[begin, end)` の乱数を返す(整数)
        int randrange(const int begin, const int end) {
            assert(begin < end);
            return begin + (((unsigned long long)_xor128() * (end-begin)) >> 32);
        }

        //! `[0, u64_MAX)` の乱数を返す / zobrist hash等の使用を想定
        unsigned long long rand_u64() {
            return ((unsigned long long)_xor128() << 32) | _xor128();
        }

        //! `[0, end)` の異なる乱数を2つ返す
        pair<int, int> rand_pair(const int end) {
            assert(end >= 2);
            int u = randrange(end);
            int v = u + randrange(1, end);
            if (v >= end) v -= end;
            if (u > v) swap(u, v);
            return {u, v};
        }

        //! `[begin, end)` の異なる乱数を2つ返す
        pair<int, int> rand_pair(const int begin, const int end) {
            assert(end - begin >= 2);
            int u = randrange(begin, end);
            int v = (u + randrange(1, end-begin));
            if (v >= end) v -= (end-begin);
            if (u > v) swap(u, v);
            return {u, v};
        }

        //! Note `a`は非const
        vector<int> rand_vec(const int cnt, vector<int> &a) {
            int n = a.size();
            for (int i = 0; i < cnt; ++i) {
                int j = randrange(i, n);
                swap(a[i], a[j]);
            }
            vector<int> res(a.begin(), a.begin()+cnt);
            return res;
        }

        //! `[begin, end)` の乱数を返す(実数)
        double randdouble(const double begin, const double end) {
            assert(begin < end);
            return begin + random() * (end-begin);
        }

        //! `vector` をインプレースにシャッフルする / `O(n)`
        template <typename T>
        void shuffle(vector<T> &a) {
            int n = a.size();
            for (int i = 0; i < n-1; ++i) {
                int j = randrange(i, n);
                swap(a[i], a[j]);
            }
        }

        template <typename T>
        vector<T> choices(const vector<T> &a, const int k) {
            assert(a.size() >= k);
            vector<T> r(k);
            unordered_set<int> seen;
            for (int i = 0; i < k; ++i) {
                int x = randrange(a.size());
                while (seen.find(x) != seen.end()) x = randrange(a.size());
                seen.insert(x);
                r[i] = a[x];
            }
            return r;
        }

        template <typename T>
        T choice(const vector<T> &a) {
            return a[randrange(a.size())];
        }

        template <typename T>
        T choice(const vector<T> &a, const vector<int> &w, bool normal) {
            assert(normal == false);
            assert(a.size() == w.size());
            double sum = 0.0;
            for (const int &x: w) sum += x;
            assert(sum > 0);
            vector<double> x(w.size());
            for (int i = 0; i < x.size(); ++i) {
                x[i] = (double)w[i] / sum;
                if (i-1 >= 0) x[i] += x[i-1];
            }
            return choice(a, x);
        }

        template <typename T>
        T choice(const vector<T> &a, const vector<double> &w) {
            double i = random();
            int l = -1, r = a.size()-1;
            while (r - l > 1) {
                int mid = (l + r) / 2;
                if (w[mid] <= i) l = mid;
                else r = mid;
            }
            return a[r];
        }
    };
} // namespace titan23

using namespace std;

// Kmeans
namespace titan23 {

    template <class DistType,
              class ElmType,
              DistType (*dist)(const ElmType&, const ElmType&),
              ElmType (*mean)(const vector<ElmType>&)>
    class Kmeans {
      private:
        int k, max_iter;
        titan23::Random my_random;

      public:
        Kmeans() : k(0), max_iter(0) {}
        Kmeans(const int k, const int max_iter) : k(k), max_iter(max_iter) {}

        pair<vector<int>, vector<ElmType>> fit(const vector<ElmType> &X) {
            int n = (int)X.size();
            assert(k <= n);
            vector<ElmType> first_cluster = {my_random.choice(X)};
            set<ElmType> cluster_set;
            cluster_set.insert(first_cluster[0]);

            while (first_cluster.size() < k) {
                vector<int> p_f(n, 0);
                bool flag = false;
                for (int i = 0; i < n; ++i) {
                    if (cluster_set.find(X[i]) == cluster_set.end()) {
                        DistType min_d = dist(X[i], first_cluster[0]);
                        for (int i = 1; i < first_cluster.size(); ++i) {
                            min_d = min(min_d, dist(X[i], first_cluster[i]));
                        }
                        flag = true;
                        p_f[i] = min_d+1;
                    }
                }
                assert(flag);
                ElmType tmpk = my_random.choice(X, p_f, false);
                assert(cluster_set.find(tmpk) == cluster_set.end());
                first_cluster.emplace_back(tmpk);
                cluster_set.insert(tmpk); // iranai
            }

            vector<ElmType> cluster_centers = first_cluster;
            vector<int> labels(n, -1);
            for (int i = 0; i < n; ++i) {
                DistType d = dist(X[i], first_cluster[0]);
                for (int j = 0; j < k; ++j) {
                    DistType tmp = dist(X[i], first_cluster[j]);
                    if (tmp <= d) {
                        d = tmp;
                        labels[i] = j;
                    }
                }
            }

            for (int _ = 0; _ < max_iter; ++_) {
                vector<vector<ElmType>> syuukei(k);
                for (int i = 0; i < n; ++i) {
                    syuukei[labels[i]].emplace_back(X[i]);
                }
                cluster_centers.clear();
                for (int i = 0; i < k; ++i) {
                    cluster_centers.emplace_back(mean(syuukei[i]));
                }
                for (int i = 0; i < n; ++i) {
                    DistType d = dist(X[i], first_cluster[0]);
                    for (int j = 0; j < k; ++j) {
                        DistType tmp = dist(X[i], first_cluster[j]);
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
using DistType = int;
using ElmType = pair<int, int>;
DistType dist(const ElmType &s, const ElmType &t) {
    return abs(s.first-t.first) + abs(s.second-t.second);
}
ElmType mean(const vector<ElmType> &L) {
    return {0, 0};
}
*/

