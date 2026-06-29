#pragma once

#include <vector>
#include "titan_cpplib/ds/persistent_array.cpp"
using namespace std;

namespace titan23 {
class PersistentUnionFind {
private:
    int n;
    using PType = titan23::PersistentArray<int>;
    PType par;

    PersistentUnionFind(PType p) : n(p.len()), par(p) {}

public:
    PersistentUnionFind() : n(0) {}
    PersistentUnionFind(int n) : n(n) {
        vector<int> p(n, -1);
        par = PType(p);
    }

    PersistentUnionFind copy() const {
        return PersistentUnionFind(par.copy());
    }

    int root(int x) const {
        while (1) {
            int p = par.get(x);
            if (p < 0) return x;
            x = p;
        }
        // vector<int> a;
        // while (1) {
        //     int p = par.get(x);
        //     if (p < 0) break;
        //     a.emplace_back(x);
        //     x = p;
        // }
        // par = par.multiset_uf(a, x);
        // return x;
    }

    PersistentUnionFind unite(int x, int y) const {
        x = root(x);
        y = root(y);
        PType p = par.copy();
        if (x == y) {
            return PersistentUnionFind(p);
        }
        int px = p.get(x);
        int py = p.get(y);
        if (px > py) swap(x, y);
        p = p.set(x, px + py);
        p = p.set(y, x);
        return PersistentUnionFind(p);
    }

    int size(int x) const { return -par.get(root(x)); }

    bool same(int x, int y) const { return root(x) == root(y); }
};
} // namespace titan23
