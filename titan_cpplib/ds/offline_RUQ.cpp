#include <iostream>
#include <numeric>
#include <vector>
#include <cassert>
using namespace std;

namespace titan23 {

template<typename T>
class OfflineRUQ {
private:
    int n;
    vector<T> a, nxt;
    vector<tuple<int, int, T>> Q;

    int find(int x) {
        if (nxt[x] == x) return x;
        return nxt[x] = find(nxt[x]);
    }

public:
    OfflineRUQ() : n(0) {}
    OfflineRUQ(int n, T init) : n(n), a(n, init), nxt(n) {}
    OfflineRUQ(int n, vector<T> a) : n(n), a(a), nxt(n) {}

    void reserve(int q) {
        Q.reserve(q);
    }

    // [l, r) <- v / O(1)
    void apply(int l, int r, T v) {
        assert(0 <= l && l <= r && r <= n);
        Q.emplace_back(l, r, v);
    }

    // クエリをまとめて実行する / O(n+qα(n))
    vector<T> tovector() {
        iota(nxt.begin(), nxt.end(), 0);
        for (int i = (int)Q.size()-1; i >= 0; --i) {
            auto [l, r, v] = Q[i];
            l = find(l);
            while (l < r) {
                a[l] = v;
                nxt[l] = l + 1;
                l = find(l);
            }
        }
        Q.clear();
        return a;
    }
};
} // namespace titan23
