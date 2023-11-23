#include <iostream>
#include <vector>
#include <cassert>
using namespace std;

namespace titan23 {

  template <class T,
          T (*op)(T, T),
          T (*e)()>
  struct SparseTable {
    vector<T> a;
    int size;
    vector<vector<T>> data;

    SparseTable() {}

    SparseTable(vector<T> a) {
      this->a = a;
      this->size = (int)a.size();
      build();
    }

    void build() {
      int log = 32 - __builtin_clz(size) - 1;
      data.resize(log+1);
      data[0].resize(size);
      for (int i = 0; i < size; ++i) {
        data[0][i] = a[i];
      }
      data[0] = a;
      for (int i = 0; i < log; ++i) {
        int l = 1 << i;
        data[i+1].resize(data[i].size()-l);
        for (int j = 0; j < data[i].size()-l; ++j) {
          data[i+1][j] = op(data[i][j], data[i][j+l]);
        }
      }
    }

    T prod(int l, int r) const {
      assert(0 <= l && l <= r && r < size);
      if (l == r) {
        return e();
      }
      int u = 32 - __builtin_clz(r-l) - 1;
      return op(data[u][l], data[u][r-(1<<u)]);
    }

    T get(int k) const {
      assert(0 <= k && k < size);
      return data[0][k];
    }

    int len() const {
      return size;
    }

    void print() const {
      cout << '[';
      for (int i = 0; i < size-1; ++i) {
        cout << data[0][i] << ", ";
      }
      if (size > 0) {
        cout << data[0][size-1];
      }
      cout << ']' << endl;
    }
  };
} // namespace titan23

