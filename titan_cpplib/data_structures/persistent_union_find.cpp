#include <vector>
#include "titan_cpplib/data_structures/persistent_array.cpp"
using namespace std;

namespace titan23 {
class PersistentUnionFind {
private:
    int n;
    using PType = titan23::PersistentArray<int>;
    PType par;

    PersistentUnionFind (PType new_par) : n(new_par.len()), par(new_par) {}

public:
    PersistentUnionFind() : n(0) {}
    PersistentUnionFind(int n) : n(n) {
        vector<int> p(n, -1);
        par = PType(p);
    }

    PersistentUnionFind copy() {
        return PersistentUnionFind(par.copy());
    }

    int root(int x) {
        while (1) {
            int p = par.get(x);
            if (p < 0) return x;
            x = p;
        }
        vector<int> a;
        while (1) {
            int p = par.get(x);
            if (p < 0) break;
            a.emplace_back(x);
            x = p;
        }
        par = par.multiset_uf(a, x);
        return x;
    }

    PersistentUnionFind unite(int x, int y) {
        x = root(x);
        y = root(y);
        PType new_par = par.copy();
        if (x == y) {
            return PersistentUnionFind(new_par);
        }
        int px = new_par.get(x);
        int py = new_par.get(y);
        if (px > py) swap(x, y);
        new_par = new_par.set(x, px + py);
        new_par = new_par.set(y, x);
        return PersistentUnionFind(new_par);
    }

    int size(int x) { return par.get(root(x)); }

    bool same(int x, int y) { return root(x) == root(y); }
};
} // namespace titan23
