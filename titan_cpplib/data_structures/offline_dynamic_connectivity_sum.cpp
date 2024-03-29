#include <unordered_map>
#include <vector>
#include <stack>
#include <algorithm>
#include <cassert>
using namespace std;

// OfflineDynamicConnectivitySum
namespace titan23 {

  template<typename T>
  struct OfflineDynamicConnectivitySum {

    struct UndoableUnionFind {
      int _n, _group_count;
      T _e;
      vector<int> _parents;
      vector<T> _all_sum, _one_sum;
      vector<tuple<int, int, T>> _history;

      UndoableUnionFind() {}

      UndoableUnionFind(int n, T e) : _n(n),
                                      _group_count(n),
                                      _e(e),
                                      _parents(n, -1),
                                      _all_sum(n, e),
                                      _one_sum(n, e) {
      }

      void undo() {
        auto [y, py, all_sum_y] = _history.back();
        _history.pop_back();
        if (y == -1) return;
        auto [x, px, all_sum_x] = _history.back();
        _history.pop_back();
        ++_group_count;
        _parents[y] = py;
        _parents[x] = px;
        T s = (_all_sum[x] - all_sum_y - all_sum_x) / (-py-px) * (-py);
        _all_sum[y] += s;
        _all_sum[x] -= all_sum_y + s;
        _one_sum[x] -= _one_sum[y];
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
          _history.emplace_back(-1, -1, _e);
          return false;
        }
        if (_parents[x] > _parents[y]) swap(x, y);
        _group_count -= 1;
        _history.emplace_back(x, _parents[x], _all_sum[x]);
        _history.emplace_back(y, _parents[y], _all_sum[y]);
        _all_sum[x] += _all_sum[y];
        _one_sum[x] += _one_sum[y];
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

      void add_point(int x, const T v) {
        while (x >= 0) {
          _one_sum[x] += v;
          x = _parents[x];
        }
      }

      void add_group(int x, const T v) {
        x = root(x);
        _all_sum[x] += v * size(x);
      }

      int group_count() const {
        return _group_count;
      }

      T group_sum(int x) const {
        x = root(x);
        return _one_sum[x] + _all_sum[x];
      }
    };
    
    int _n, _query_count, _size, _q;
    long long _bit, _msk;
    unordered_map<long long, pair<int, int>> start;
    vector<vector<long long>> data;
    vector<tuple<int, int, long long>> edge_data;
    UndoableUnionFind uf;

    OfflineDynamicConnectivitySum (int n, int q, T e) : 
        _n(n),
        _query_count(0),
        _size(1 << (bit_length(q-1))),
        _q(q),
        _bit(bit_length(n) + 1),
        _msk((1ll << (bit_length(n) + 1)) - 1),
        data(_size<<1),
        uf(n, e) {
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
