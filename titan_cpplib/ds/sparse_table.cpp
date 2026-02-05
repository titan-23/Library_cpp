#include <iostream>
#include <vector>
#include <cassert>
using namespace std;

// SparseTable
namespace titan23 {

template <class T, T (*op)(T, T), T (*e)()>
struct SparseTable {
private:
    int n;
    vector<T> data;
    vector<int> offset;

public:
    SparseTable() {}
    SparseTable(vector<T> &a) : n((int)a.size()) {
        int log = 32 - __builtin_clz(n) - 1;
        offset.resize(log+1);
        int sm = 0;
        for (int i = 0; i <= log; ++i) {
            offset[i] = sm;
            sm += n - (1<<i) + 1;
        }
        data.resize(sm);
        memcpy(data.data(), a.data(), n*sizeof(T));
        for (int i = 0; i < log; ++i) {
            int l = 1 << i;
            int s = n - l + 1;
            const int x = s - l;
            const auto pre1 = &data[offset[i]];
            const auto pre2 = &data[offset[i]+l];
            auto nxt = &data[offset[i+1]];
            for (int j = 0; j < x; ++j) {
                nxt[j] = op(pre1[j], pre2[j]);
            }
        }
    }

    T prod(const int l, const int r) const {
        assert(0 <= l && l <= r && r <= n);
        if (l == r) return e();
        int u = 32 - __builtin_clz(r-l) - 1;
        return op(data[offset[u]+l], data[offset[u]+r-(1<<u)]);
    }

    T get(const int k) const {
        assert(0 <= k && k < n);
        return data[k];
    }

    int len() const {
        return n;
    }

    void print() const {
        cout << '[';
        for (int i = 0; i < n-1; ++i) {
            cout << data[i] << ", ";
        }
        if (n > 0) {
            cout << data[n-1];
        }
        cout << ']' << endl;
    }
};
}  // namespace titan23
