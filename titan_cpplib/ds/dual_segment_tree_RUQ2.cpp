#include <iostream>
#include <vector>
#include <cassert>
using namespace std;

// DualSegmentTreeRUQ
namespace titan23 {

// Q <= Nのとき強いかも
template <class T>
class DualSegmentTreeRUQ {
private:
    int bit_length(const int x) const {
        return x == 0 ? 0 : 32 - __builtin_clz(x);
    }

    int n, time, log, size;
    vector<T> stamp;
    vector<int> data;

public:
    DualSegmentTreeRUQ() : time(0) {}
    DualSegmentTreeRUQ(const int n, const T init) : n(n), time(0), log(bit_length(n)), size(1<<log) {
        data.resize(2*size, 0);
        stamp.emplace_back(init);
    }

    DualSegmentTreeRUQ(const vector<T> a) : n(a.size()), time(0), log(bit_length(n)), size(1<<log) {
        data.resize(2*size);
        for (int i = 0; i < n; ++i) {
            data[i+size] = time;
            time++;
        }
        stamp = a;
    }

    void reserve(int q) {
        stamp.reserve(q+1);
    }

    void apply_point(int k, T f) {
        time++;
        data[k+size] = time;
        stamp.emplace_back(f);
    }

    void apply(int l, int r, const T f) {
        assert(0 <= l && l <= r && r <= n);
        l += size;
        r += size;
        time++;
        stamp.emplace_back(f);
        while (l < r) {
            if (l & 1) {
                data[l] = time;
                l++;
            }
            if (r & 1) {
                r--;
                data[r] = time;
            }
            l >>= 1;
            r >>= 1;
        }
    }

    void all_apply(T f) {
        time++;
        data[1] = time;
        stamp.emplace_back(f);
    }

    vector<T> tovector() {
        for (int k = 1; k < size; ++k) {
            int t = data[k];
            if (data[k<<1] < t) data[k<<1] = t;
            if (data[k<<1|1] < t) data[k<<1|1] = t;
        }
        vector<T> res(n);
        for (int i = 0; i < n; ++i) {
            res[i] = stamp[data[i+size]];
        }
        return res;
    }

    T get(int k) const {
        k += size;
        int t = data[k];
        for (int i = 1; i <= log; ++i) {
            if (t < data[k>>i]) {
                t = data[k>>i];
            }
        }
        return stamp[t];
    }
};
}  // namespace titan23
