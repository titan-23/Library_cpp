#include <unordered_map>
#include <vector>
#include <stack>
#include <algorithm>
#include <cassert>
using namespace std;

// OfflineDynamicConnectivity
namespace titan23 {

  struct OfflineDynamicConnectivity {

    struct UndoableUnionFind {
      int _n, _group_count;
      vector<int> _parents;
      vector<pair<int, int>> _history;

      UndoableUnionFind() {}

      UndoableUnionFind(int n) : _n(n),
                                 _group_count(n),
                                 _parents(n, -1) {
      }

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
    
    int _n, _query_count, _size, _q;
    long long _bit, _msk;
    unordered_map<long long, pair<int, int>> start;
    vector<vector<long long>> data;
    vector<tuple<int, int, long long>> edge_data;
    UndoableUnionFind uf;

    OfflineDynamicConnectivity (int n, int q) : 
        _n(n),
        _query_count(0),
        _size(1 << (bit_length(q-1))),
        _q(q),
        _bit(bit_length(n) + 1),
        _msk((1ll << (bit_length(n) + 1)) - 1),
        data(_size<<1),
        uf(n) {
      start.reserve(_q);
      edge_data.reserve(_q);
    }

    int bit_length(const int n) const {
      if (n == 0) return 0;
      return 32 - __builtin_clz(n);
    }

    void add_edge(const int u, const int v) {
      auto [s, t] = minmax(u, v);
      long long edge = (long long)s<<_bit|t;
      if (start[edge].first == 0) {
        start[edge].second = _query_count;
      }
      ++start[edge].first;
    }

    void delete_edge(const int u, const int v) {
      auto [s, t] = minmax(u, v);
      long long edge = (long long)s<<_bit|t;
      auto it = start.find(edge);
      if (it->second.first == 1) {
        edge_data.emplace_back(it->second.second, _query_count, edge);
      }
      --it->second.first;
    }

    void next_query() {
      ++_query_count;
    }

    void _internal_add(int l, int r, const long long edge) {
      l += _size;
      r += _size;
      while (l < r) {
        if (l & 1) {
          data[l++].emplace_back(edge);
        }
        if (r & 1) {
          data[--r].emplace_back(edge);
        }
        l >>= 1;
        r >>= 1;
      }
    }

    template<typename F> // out(k: int) -> None
    void run(F &&out) {
      assert(_query_count <= _q);
      for (const auto &[edge, p]: start) {
        if (p.first != 0) {
          _internal_add(p.second, _query_count, edge);
        }
      }
      for (const auto &[l, r, edge]: edge_data) {
        _internal_add(l, r, edge);
      }
      int size2 = _size<<1;
      int ptr = 0;
      int todo[bit_length(_q)<<2];
      todo[++ptr] = 1;
      while (ptr) {
        int v = todo[ptr--];
        if (v >= 0) {
          for (const long long &uv: data[v]) {
            uf.unite(uv>>_bit, uv&_msk);
          }
          todo[++ptr] = ~v;
          if ((v<<1|1) < size2) {
            todo[++ptr] = v<<1|1;
            todo[++ptr] = v<<1;
          } else if (v - _size < _query_count) {
            out(v-_size);
          }
        } else {
          for (const long long &_: data[~v]) {
            uf.undo();
          }
        }
      }
    }
  };
}  // namespace titan23
