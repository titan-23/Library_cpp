#include <iostream>
#include <vector>
#include <cassert>
using namespace std;

namespace titan23 {
  template<typename T>
  struct CumulativeSum2D {
    int _h, _w;
    T _e;
    vector<T> _acc;

    CumulativeSum2D() {}

    CumulativeSum2D(int h, int w, vector<vector<T>> &a, T e) {
      _h = h;
      _w = w;
      _acc.resize((h+1)*(w+1), e);
      for (int ij = 0; ij < _h*_w; ++ij) {
        int i = ij / w, j = ij % w;
        _acc[(i+1)*(w+1)+j+1] = _acc[i*(w+1)+j+1] + _acc[(i+1)*(w+1)+j] - _acc[i*(w+1)+j] + a[i][j];
      }
    }

    T sum(const int h1, const int w1, const int h2, const int w2) const {
      assert(h1 <= h2 && w1 <= w2);
      return _acc[h2*(_w+1)+w2] - _acc[h2*(_w+1)+w1] - _acc[h1*(_w+1)+w2] + _acc[h1*(_w+1)+w1];
    }
  };
}  // namespace titan23
