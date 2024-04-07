#include <iostream>
#include <vector>
#include <cassert>
using namespace std;

namespace titan23 {
  template<typename T>
  struct CumulativeSum2D {
    int h, w;
    vector<T> acc;

    CumulativeSum2D() {}
    CumulativeSum2D(int h, int w, vector<vector<T>> &a, T e) : h(h), w(w), acc((h+1)*(w+1), e) {
      for (int ij = 0; ij < h*w; ++ij) {
        int i = ij / w, j = ij % w;
        acc[(i+1)*(w+1)+j+1] = acc[i*(w+1)+j+1] + acc[(i+1)*(w+1)+j] - acc[i*(w+1)+j] + a[i][j];
      }
    }

    T sum(const int h1, const int w1, const int h2, const int w2) const {
      assert(h1 <= h2 && w1 <= w2);
      return acc[h2*(w+1)+w2] - acc[h2*(w+1)+w1] - acc[h1*(w+1)+w2] + acc[h1*(w+1)+w1];
    }
  };
}  // namespace titan23
