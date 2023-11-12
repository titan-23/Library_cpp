#include <unordered_map>
#include <vector>
#include <stack>
#include <algorithm>
#include <cassert>
using namespace std;

namespace titan23 {

  template<typename T>
  struct OfflineDynamicConnectivity {

    struct UndoableUnionFind {
      int _n, _group_count;
      vector<int> _parents;
      vector<T> _all_sum, _one_sum;
      vector<tuple<int, int, T>> _history;
      T _e;

      UndoableUnionFind() {}

      UndoableUnionFind(int n, T e) {
        _n = n;
        _e = e;
        _parents.resize(n, -1);
        _all_sum.resize(n, e);
        _one_sum.resize(n, e);
        _history.resize(0);
        _group_count = n;
      }

      void undo() {
        auto [y, py, all_sum_y] = _history.back();
        _history.pop_back();
        auto [x, px, all_sum_x] = _history.back();
        _history.pop_back();
        if (y == -1) return;
        ++_group_count;
        _parents[y] = py;
        _parents[x] = px;
        T s = (_all_sum[x] - all_sum_y - all_sum_x) / (-py-px) * (-py);
        _all_sum[y] += s;
        _all_sum[x] -= all_sum_y + s;
        _one_sum[x] -= _one_sum[y];
      }

      int root(int x) {
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

      int size(int x) {
        return -_parents[root(x)];
      }

      bool same(int x, int y) {
        return root(x) == root(y);
      }

      void add_point(int x, T v) {
        while (x >= 0) {
          _one_sum[x] += v;
          x = _parents[x];
        }
      }

      void add_group(int x, T v) {
        x = root(x);
        _all_sum[x] += v * size(x);
      }

      int group_count() const {
        return _group_count;
      }

      T group_sum(int x) {
        x = root(x);
        return _one_sum[x] + _all_sum[x];
      }
    };
    
    int _n, _bit, _msk, _query_count;
    unordered_map<long long, vector<int>> _edge;
    UndoableUnionFind uf;

    OfflineDynamicConnectivity (int n, T e) {
      _n = n;
      _bit = bit_length(n) + 1;
      _msk = (1ll << _bit) - 1;
      _query_count = 0;
      UndoableUnionFind _uf(n, e);
      uf = _uf;
    }

    int bit_length(int n) {
      if (n == 0) return 0;
      return 32 - __builtin_clz(n);
    }

    void add_edge(int u, int v) {
      if (u > v) swap(u, v);
      _edge[(long long)u<<_bit|v].emplace_back(_query_count<<1);
      ++_query_count;
    }

    void delete_edge(int u, int v) {
      if (u > v) swap(u, v);
      _edge[(long long)u<<_bit|v].emplace_back(_query_count<<1|1);
      ++_query_count;
    }

    void add_none() {
      ++_query_count;
    }

    void init_edge(vector<pair<int, int>> &E) {
      for (auto &[u, v]: E) {
        if (u > v) swap(u, v);
        _edge[(long long)u<<_bit|v].emplace_back(0);
      }
      ++_query_count;
    }
    
    template<typename F> // out(k: int) -> None
    void run(F &&out) {
      int log = bit_length(_query_count-1);
      int size = 1 << log;
      vector<vector<long long>> data(size<<1);
      int size2 = size * 2;
      for (auto &[k, v]: _edge) {
        vector<int> LR;
        int i = 0, cnt = 0;
        while (i < (int)v.size()) {
          if ((v[i] & 1) == 0) ++cnt;
          if (cnt > 0) {
            LR.emplace_back(v[i] >> 1);
            ++i;
            while (i < (int)v.size() && cnt > 0) {
              if ((v[i] & 1) == 0) ++cnt;
              else {
                assert(cnt >= 0); // Edge Error: minus edge.
                --cnt;
                if (cnt == 0) LR.emplace_back(v[i] >> 1);
              }
              ++i;
            }
            --i;
          } 
          ++i;
        }
        if (cnt > 0) LR.emplace_back(_query_count);
        reverse(LR.begin(), LR.end());
        while (!LR.empty()) {
          int l = LR.back() + size;
          LR.pop_back();
          int r = LR.back() + size;
          LR.pop_back();
          while (l < r) {
            if (l & 1) {
              data[l].emplace_back(k);
              ++l;
            }
            if (r & 1) {
              data[r^1].emplace_back(k);
            }
            l >>= 1;
            r >>= 1;
          }
        }
      }
      stack<int> todo;
      todo.push(1);
      while (!todo.empty()) {
        int v = todo.top();
        todo.pop();
        if (v >= 0) {
          for (const long long &uv: data[v]) {
            uf.unite(uv>>_bit, uv&_msk);
          }
          todo.push(~v);
          if ((v<<1|1) < size2) {
            todo.push(v<<1|1);
            todo.push(v<<1);
          } else if (v - size < _query_count) {
            out(v-size);
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
