#include <iostream>
#include <vector>
#include "titan_cpplib/ds/partial_persistent_array.cpp"
using namespace std;

namespace titan23 {

class PartialPersistentUnionFind {
private:
    int n, last_time;
    titan23::PartialPersistentArray<int> par;

public:
    PartialPersistentUnionFind() {}
    PartialPersistentUnionFind(int n) {
        vector<int> p(n, -1);
        par = titan23::PartialPersistentArray<int>(p);
        last_time = 0;
    }

    int root(int x, int t = -1) {
        assert(t == -1 || t <= last_time);
        while (1) {
            int p = par.get(x, t);
            if (p < 0) return x;
            x = p;
        }
    }

    bool unite(int x, int y, int t) {
        assert(t == -1 || t >= last_time);
        last_time = t;
        x = root(x, t); y = root(y, t);
        if (x == y) return false;
        if (par.get(x, t) > par.get(y, t)) swap(x, y);
        par.set(x, par.get(x, t) + par.get(y, t), t);
        par.set(y, x, t);
        return true;
    }

    int size(int x, int t = -1) {
        assert(t == -1 || t <= last_time);
        return -par.get(root(x, t), t);
    }

    bool same(int x, int y, int t = -1) {
        assert(t == -1 || t <= last_time);
        return root(x, t) == root(y, t);
    }
};
} // namespace titan23
