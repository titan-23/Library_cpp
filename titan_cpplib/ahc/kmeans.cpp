#include <vector>
#include <set>
#include <cassert>
#include "titan_cpplib/algorithm/random.cpp"
using namespace std;

// Kmeans
namespace titan23 {

template <class DistType, class ElmType, DistType (*dist)(const ElmType&, const ElmType&), ElmType (*mean)(const vector<ElmType>&)>
class Kmeans {
    private:
    int k, max_iter;
    titan23::Random my_random;

    public:
    Kmeans() : k(0), max_iter(0) {}
    Kmeans(const int k, const int max_iter) : k(k), max_iter(max_iter) {}

    // 初期クラスターを指定して実行する fit
    pair<vector<int>, vector<ElmType>> fit(const vector<ElmType> &X, const vector<ElmType> &init_centers) {
        int n = (int)X.size();
        assert(k <= n);
        assert((int)init_centers.size() == k);

        vector<ElmType> cluster_centers = init_centers;
        vector<int> labels(n, -1);

        for (int _ = 0; _ < max_iter; ++_) {
            bool changed = false;

            // 1. 各データ点を最も近いクラスター中心に割り当てる
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

            // 中心が変化しなければ早期終了
            if (!changed && _ > 0) break;

            // 2. 各クラスターの新しい中心を計算する
            vector<vector<ElmType>> syuukei(k);
            for (int i = 0; i < n; ++i) {
                syuukei[labels[i]].emplace_back(X[i]);
            }

            cluster_centers.clear();
            for (int j = 0; j < k; ++j) {
                // ※ syuukei[j] が空になった時のエラー処理が必要な場合は mean 関数側で対応するか、ここに分岐を入れます
                cluster_centers.emplace_back(mean(syuukei[j]));
            }
        }

        return {labels, cluster_centers};
    }

    // 従来の fit (k-means++ で初期化して実行)
    pair<vector<int>, vector<ElmType>> fit(const vector<ElmType> &X) {
        int n = (int)X.size();
        assert(k <= n);
        vector<ElmType> first_cluster = {my_random.choice(X)};
        set<ElmType> cluster_set;
        cluster_set.insert(first_cluster[0]);

        // 初期クラスターの選定 (k-means++)
        while ((int)first_cluster.size() < k) {
            vector<int> p_f(n, 0);
            bool flag = false;
            for (int i = 0; i < n; ++i) {
                if (cluster_set.find(X[i]) == cluster_set.end()) {
                    DistType min_d = dist(X[i], first_cluster[0]);
                    // バグ修正: 変数 i のシャドウイングを避けるため j を使用
                    for (int j = 1; j < (int)first_cluster.size(); ++j) {
                        min_d = min(min_d, dist(X[i], first_cluster[j]));
                    }
                    flag = true;
                    p_f[i] = min_d + 1;
                }
            }
            assert(flag);
            ElmType tmpk = my_random.choice(X, p_f, false);
            assert(cluster_set.find(tmpk) == cluster_set.end());
            first_cluster.emplace_back(tmpk);
            cluster_set.insert(tmpk);
        }

        // 選んだ初期中心を渡してメインのロジックを実行
        return fit(X, first_cluster);
    }
};
}
