#include <vector>
using namespace std;

// MaximumIndependentSet
namespace titan23 {
    class MaximumIndependentSet {
      private:
        int n;
        vector<vector<bool>> G;

      public:
        MaximumIndependentSet() : n(-1) {}
        MaximumIndependentSet(int n) : n(n), G(n, vector<bool>(n, false)) {}

        void add_edge(int u, int v) {
            G[u][v] = true;
        }

        vector<int> get_maximum_independent_set() const {
            const int m = n/2;

            vector<int> dp_s1(m, 1);
            for (int i = 0; i < m; ++i) {
                for (int j = 0; j < m; ++j) {
                    if (G[i][j]) dp_s1[(1<<i)|(1<<j)] = 0;
                }
            }
            for (int s = 0; s < (1<<m); ++s) {
                if (dp_s1[s]) continue;
                for (int i = 0; i < m; ++i) {
                    if ((s >> i & 1) == 0) continue;
                    for (int j = 0; j < m; ++j) {
                        dp_s1[s|(1<<j)] = 0;
                    }
                }
            }

            

            for (int s1 = 0; s1 < (1<<m); ++s1) {

            }
        }
    };
} // namespace titan23
