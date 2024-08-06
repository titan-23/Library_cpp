#include <vector>
using namespace std;

// UndoableUnionFind
namespace titan23 {
    class UndoableUnionFind {
     private:
      int _n, _group_count;
      vector<int> _parents;
      vector<pair<int, int>> _history;

     public:
      UndoableUnionFind() {}
      UndoableUnionFind(int n) :
          _n(n), _group_count(n), _parents(n, -1) {}

      void undo() {
        auto [y, py] = _history.back();
        _history.pop_back();
        if (y == -1) return;
        auto [x, px] = _history.back();
        _history.pop_back();
        ++_group_count;
        _parents[y] = py;
        _parents[x] = px;
      }

      int root(int x) const {
        while (_parents[x] >= 0) {
          x = _parents[x];
        }
        return x;
      }

      bool unite(int x, int y) {
        x = root(x);
        y = root(y);
        if (x == y) {
          _history.emplace_back(-1, -1);
          return false;
        }
        if (_parents[x] > _parents[y]) swap(x, y);
        _group_count -= 1;
        _history.emplace_back(x, _parents[x]);
        _history.emplace_back(y, _parents[y]);
        _parents[x] += _parents[y];
        _parents[y] = x;
        return true;
      }

      int size(const int x) const {
        return -_parents[root(x)];
      }

      bool same(const int x, const int y) const {
        return root(x) == root(y);
      }

      int group_count() const {
        return _group_count;
      }
    };
} // namespace titan23
