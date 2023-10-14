#include <iostream>
#include <vector>
#include <cassert>
using namespace std;

namespace titan23 {
  template<typename T>
  struct FenwickTree {

    int _n, _s;
    vector<T> _tree;

    FenwickTree(const int n) {
      _n = n;
      _tree.resize(n+1, 0);
      _s = 1 << (32 - __builtin_clz(_n-1));
    }

    FenwickTree(const vector<T> &a) {
      _n = a.size();
      _tree.resize(_n+1, 0);
      for (int i = 1; i <= _n; ++i) _tree[i] = a[i-1];
      for (int i = 1; i < _n; ++i) {
        if (i + (i & (-i)) <= _n) {
          _tree[i + (i & (-i))] += _tree[i];
        }
      }
      _s = 1 << (32 - __builtin_clz(_n-1));
    }

    T pref(int r) const {
      T res = 0;
      while (r > 0) {
        res += _tree[r];
        r -= r & (-r);
      }
      return res;
    }

    T suff(int l) const {
      return pref(_n) - pref(l);
    }

    T sum(const int l, const int r) const {
      return pref(r) - pref(l);
    }

    void add(int k, const T x) {
      ++k;
      while (k <= _n) {
        _tree[k] += x;
        k += k & (-k);
      }
    }

    T get(const int k) const {
      return pref(k+1) - pref(k);
    }

    void set(const int k, const T x) {
      T pre = get(k);
      add(k, x-pre);
    }

    int bisect_left(T w) const {
      int i = 0, s = _s;
      while (s) {
        if (i+s <= _n && _tree[i+s] < w) {
          w -= _tree[i+s];
          i += s;
        }
        s >>= 1;
      }
      return (w? i : -1);
    }

    int bisect_right(T w) const {
      int i = 0, s = _s;
      while (s) {
        if (i+s <= _n && _tree[i+s] <= w) {
          w -= _tree[i+s];
          i += s;
        }
        s >>= 1;
      }
      return i;
    }

    vector<T> tovector() const {
      vector<T> sub(_n+1), res(_n);
      for (int i = 0; i <= _n; ++i) sub[i] = pref(i);
      for (int i = 0; i < _n; ++i) res[i] = sub[i+1] - sub[i];
      return res;
    }

    void print() const {
      vector<T> fw = tovector();
      cout << "[";
      for (int i = 0; i < _n-1; ++i) cout << fw[i] << ", ";
      cout << fw[_n-1] << "]\n";
    }
  };
} // namespace titan23

