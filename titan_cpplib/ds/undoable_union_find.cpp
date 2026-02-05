#include <vector>
#include <stack>
using namespace std;

// UndoableUnionFind
namespace titan23 {

class UndoableUnionFind {
private:
    int n, cnt;
    vector<int> par;
    stack<pair<int, int>> history;

public:
    UndoableUnionFind() {}
    UndoableUnionFind(int n) : n(n), cnt(n), par(n, -1) {}

    void undo() {
        auto [y, py] = history.top(); history.pop();
        if (y == -1) return;
        auto [x, px] = history.top(); history.pop();
        ++cnt;
        par[y] = py;
        par[x] = px;
    }

    int root(int x) const {
        while (par[x] >= 0) {
            x = par[x];
        }
        return x;
    }

    bool unite(int x, int y) {
        x = root(x);
        y = root(y);
        if (x == y) {
            history.emplace(-1, -1);
            return false;
        }
        if (par[x] > par[y]) swap(x, y);
        cnt--;
        history.emplace(x, par[x]);
        history.emplace(y, par[y]);
        par[x] += par[y];
        par[y] = x;
        return true;
    }

    int size(const int x) const {
        return -par[root(x)];
    }

    bool same(const int x, const int y) const {
        return root(x) == root(y);
    }

    int group_count() const {
        return cnt;
    }
};
} // namespace titan23
