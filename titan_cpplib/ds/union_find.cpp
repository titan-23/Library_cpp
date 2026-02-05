#include <iostream>
#include <vector>
#include <map>
#include <cassert>
using namespace std;

// UnionFind
namespace titan23 {

class UnionFind {
private:
    int n, group_numbers;
    vector<int> par;

public:

    UnionFind() {}
    UnionFind(int n) : n(n), group_numbers(n), par(n, -1) {}

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
        --group_numbers;
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

    vector<int> all_roots() const {
        vector<int> res;
        for (int i = 0; i < n; ++i) {
            if (par[i] < 0) res.emplace_back(i);
        }
        return res;
    }

    map<int, vector<int>> all_group_members() {
        map<int, vector<int>> res;
        for (int i = 0; i < n; ++i) {
            res[root(i)].emplace_back(i);
        }
        return res;
    }

    int group_count() const {
        return group_numbers;
    }

    void clear() {
        group_numbers = n;
        std::fill(par.begin(), par.end(), -1);
    }

    friend ostream& operator<<(ostream& os, titan23::UnionFind &uf) {
        map<int, vector<int>> group_members = uf.all_group_members();
        os << "<UnionFind>\n";
        for (auto &[key, val]: group_members) {
            os << "  " << key << " : ";
            for (const int &v : val) {
                os << v << ", ";
            }
        }
        return os;
    }
};
}  // namespace titan23
