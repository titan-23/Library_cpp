#include <iostream>
#include <vector>
using namespace std;

namespace titan23 {
  template<typename T>
  struct CumulativeSum {
    int _n;
    T _e;
    vector<T> _acc;

    CumulativeSum(vector<T> &a, T e) {
      _n = (int)a.size();
      _acc.resize(_n+1, e);
      for (int i = 0; i < _n; ++i) {
        _acc[i+1] = _acc[i] + a[i];
      }
    }

    T pref(const int r) const {
      return _acc[r];
    }

    T all_sum() const {
      return _acc.back();
    }

    T sum(const int l, const int r) const {
      return _acc[r] - _acc[l];
    }

    T prod(const int l, const int r) const {
      return sum(l, r);
    }

    T all_prod() const {
      return all_sum();
    }

    void print() const {
      cout << '[';
      for (int i = 0; i < _n; ++i) {
        cout << _acc[i] << ", ";
      }
      cout << _acc.back() << ']' << endl;
    }
  };
} // namespace titan23

