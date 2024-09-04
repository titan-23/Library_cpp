#include <iostream>
#include <vector>
#include <cassert>
using namespace std;

// SparseTable
namespace titan23 {

    template <class T,
              T (*op)(T, T),
              T (*e)()>
    struct SparseTable {
      private:
        int n;
        vector<vector<T>> data;

      public:
        SparseTable() {}
        SparseTable(vector<T> &a) : n((int)a.size()) {
            int log = 32 - __builtin_clz(n) - 1;
            data.resize(log+1);
            data[0] = a;
            for (int i = 0; i < log; ++i) {
                int l = 1 << i;
                const vector<int> &pre = data[i];
                vector<int> &nxt = data[i+1];
                int s = pre.size();
                nxt.resize(s-l);
                for (int j = 0; j < s-l; ++j) {
                    nxt[j] = op(pre[j], pre[j+l]);
                }
            }
        }

        T prod(const int l, const int r) const {
            assert(0 <= l && l <= r && r < n);
            if (l == r) return e();
            int u = 32 - __builtin_clz(r-l) - 1;
            return op(data[u][l], data[u][r-(1<<u)]);
        }

        T get(const int k) const {
            assert(0 <= k && k < n);
            return data[0][k];
        }

        int len() const {
            return n;
        }

        void print() const {
            cout << '[';
            for (int i = 0; i < n-1; ++i) {
                cout << data[0][i] << ", ";
            }
            if (n > 0) {
                cout << data[0][n-1];
            }
            cout << ']' << endl;
        }
    };
}  // namespace titan23
