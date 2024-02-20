#include <iostream>
#include <vector>
#include <cassert>
#include <map>
#include <set>
using namespace std;

// UnionFind
namespace titan23 {

  struct UnionFind {

    int n, group_numbers;
    vector<int> par;
    vector<vector<int>> G;

    UnionFind(int n) : n(n), group_numbers(n), par(n, -1), G(n) {}

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
      G[x].emplace_back(y);
      G[y].emplace_back(x);
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

    set<int> members(const int x) {
      set<int> seen;
      seen.emplace(x);
      vector<int> todo = {x};
      while (!todo.empty()) {
        int v = todo.back(); todo.pop_back();
        for (const int &x: G[v]) {
          if (seen.find(x) != seen.end()) continue;
          todo.emplace_back(x);
          seen.emplace(x);
        }
      }
      return seen;
    }

    vector<int> all_roots() const {
      vector<int> res;
      res.reserve(group_numbers);
      for (int i = 0; i < n; ++i) {
        if (par[i] < 0) res.emplace_back(i);
      }
      return res;
    }

    int group_count() const {
      return group_numbers;
    }

    void clear() {
      group_numbers = n;
      for (int i = 0; i < n; ++i) par[i] = -1;
    }

    map<int, vector<int>> all_group_members() {
      map<int, vector<int>> res;
      for (int i = 0; i < n; ++i) {
        res[root(i)].emplace_back(i);
      }
      return res;
    }

    void print() {
      map<int, vector<int>> group_members = all_group_members();
      printf("<UnionFind>\n");
      for (auto &[key, val]: group_members) {
        printf(" %d:", key);
        for (const int &v: val) {
          printf(" %d", v);
        }
        cout << endl;
      }
    }
  };
}  // namespace titan23

