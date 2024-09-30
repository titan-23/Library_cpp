#include <vector>

namespace titan23 {
    template<typename T>
    class WeightedUnionFind {
      private:
        int n, group_members;
        vector<int> par;
        vector<T> weight;

      public:
        WeightedUnionFind() {}
        WeightedUnionFind(int n) : n(n), group_members(n), par(n, -1), weight(n, 0) {}

        int root(int x) {
            vector<int> path = {x};
            while (par[x] >= 0) {
                x = par[x];
                path.emplace_back(x);
            }
            int a = path.back();
            path.pop_back();
            while (!path.empty()) {
                x = path.back();
                path.pop_back();
                weight[x] += weight[par[x]];
                par[x] = a;
            }
            return a;
        }

        bool unite(int x, int y, T w) {
            int rx = this->root(x);
            int ry = this->root(y);
            if (rx == ry) {
                return this->diff(x, y) == w ? true : false;
            }
            w += weight[x] - weight[y];
            group_members--;
            if (par[rx] > par[ry]) {
                swap(rx, ry);
                w = -w;
            }
            par[rx] += par[ry];
            par[ry] = rx;
            weight[ry] = w;
            return true;
        }

        int size(int x) {
            return -par[root(x)];
        }

        bool same(int x, int y) {
            return root(x) == root(y);
        }

        int group_count() const {
            return group_members;
        }

        T diff(int x, int y) {
            assert(same(x, y));
            return weight[y] - weight[x];
        }
    };
}
