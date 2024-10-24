// #include "titan_cpplib/data_structures/union_find_light.cpp"
#include <iostream>
#include <vector>
#include <cassert>
using namespace std;

// UnionFindLight
namespace titan23 {

    struct UnionFindLight {
      public:
        int n;
        vector<int> par;

        UnionFindLight() {}
        UnionFindLight(int n) : n(n), par(n, -1) {}

        int root(int x) {
            int a = x, y;
            while (par[a] >= 0) a = par[a];
            while (par[x] >= 0) {
                y = x;
                x = par[x];
                par[y] = a;
            }
            return a;
        }

        bool unite(int x, int y) {
            x = root(x);
            y = root(y);
            if (x == y) return false;
            if (par[x] >= par[y]) swap(x, y);
            par[x] += par[y];
            par[y] = x;
            return true;
        }

        int size(const int x) {
            return -par[root(x)];
        }

        bool same(const int x, const int y) {
            return root(x) == root(y);
        }

        void clear() {
            fill(par.begin(), par.end(), -1);
        }
    };
}  // namespace titan23

