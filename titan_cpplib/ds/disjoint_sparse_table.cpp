#include <iostream>
#include <vector>
#include <cassert>
using namespace std;

namespace titan23 {

template <class T, T (*op)(T, T), T (*e)()>
struct DisjointSparseTable {
private:
    int n;
    vector<T> data;

public:
    DisjointSparseTable() {}
    DisjointSparseTable(const vector<T> &a) : n((int)a.size()) {
        if (n == 0) {
            return;
        }
        if (n == 1) {
            data = {a[0]};
            return;
        }
        int log = 32 - __builtin_clz(n - 1);
        data.assign(log * n, e());
        for (int i = 0; i < n; ++i) {
            data[i] = a[i];
        }
        for (int i = 1; i < log; ++i) {
            int offset = i * n;
            int half = 1 << i;
            for (int j = half; j < n; j += half * 2) {
                data[offset + j - 1] = data[j - 1];
                for (int k = 2; k <= half; ++k) {
                    data[offset + j - k] = op(data[j - k], data[offset + j - k + 1]);
                }
                data[offset + j] = data[j];
                for (int k = 1; k < half; ++k) {
                    if (j + k >= n) break;
                    data[offset + j + k] = op(data[offset + j + k - 1], data[j + k]);
                }
            }
        }
    }

    // [l, r)
    T prod(const int l, const int r) const {
        assert(0 <= l && l <= r && r <= n);
        if (l == r) return e();
        if (l+1 == r) return data[l];
        int u = 31 - __builtin_clz(l ^ (r-1));
        return op(data[u*n+l], data[u*n+(r-1)]);
    }

    T all_prod() const {
        return prod(0, n);
    }

    T get(const int k) const {
        assert(0 <= k && k < n);
        return data[k];
    }

    int size() const {
        return n;
    }

    void print() const {
        cout << '[';
        for (int i = 0; i < n - 1; ++i) {
            cout << data[i] << ", ";
        }
        if (n > 0) {
            cout << data[n - 1];
        }
        cout << ']' << endl;
    }
};
}  // namespace titan23
