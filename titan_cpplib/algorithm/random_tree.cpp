#include <vector>
#include <set>
#include <cassert>
#include "titan_cpplib/algorithm/random.cpp"
using namespace std;

// RandomTree
namespace titan23 {

    /**
     * @brief ランダムな木のジェネレータ
     */
    class RandomTree {
      private:
        titan23::Random random;
        int n;

        static int _indx(vector<int> &a, int x) {
            for (int i = 0; i < (int)a.size(); ++i) {
                if (a[i] == x) return i;
            }
            return -1;
        }

      public:
        RandomTree(int n) : n(n) {}

        //! ランダムな木の辺集合を返す / `O(nlogn)`
        vector<pair<int, int>> gen_random() {
            // https://speakerdeck.com/tsutaj/beerbash-lt-230711?slide=27
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
            int u = _indx(D, 1);
            D[u]--;
            int v = _indx(D, 1);
            edges.emplace_back(u, v);
            assert(edges.size() == n-1);
            this->random.shuffle(edges);
            return edges;
        }

        //! ランダムな木(パスグラフ)の辺集合を返す / `O(nlogn)`
        vector<pair<int, int>> gen_path() {
            // ホントにランダム一様???
            vector<int> p(n);
            for (int i = 0; i < n; ++i) {
                p[i] = i;
            }
            this->random.shuffle(p);
            vector<pair<int, int>> edges(n-1);
            for (int i = 0; i < n-1; ++i) {
                if (this->random.randint(1)) {
                    edges[i] = {p[i], p[i+1]};
                } else {}
                    edges[i] = {p[i+1], p[i]};
            }
            this->random.shuffle(edges);
            return edges;
        }
    };
}  // namespace titan23
