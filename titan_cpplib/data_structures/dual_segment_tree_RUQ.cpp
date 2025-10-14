#include <iostream>
#include <vector>
#include <cassert>
using namespace std;

// DualSegmentTreeRUQ
namespace titan23 {

template <class T>
class DualSegmentTreeRUQ {
private:
    int bit_length(const int x) const {
        return x == 0 ? 0 : 32 - __builtin_clz(x);
    }

    int n, time, log, size;
    vector<pair<int, T>> data;

public:
    DualSegmentTreeRUQ() : time(0) {}
    DualSegmentTreeRUQ(const int n, const T init) : n(n), time(0), log(bit_length(n)), size(1<<log) {
        this->data.resize(2*size, {-1, init});
        for (int i = 0; i < n; ++i) {
            data[i+size].first = time;
        }
    }

    DualSegmentTreeRUQ(const vector<T> a) : n(a.size()), time(0), log(bit_length(n)), size(1<<log) {
        this->data.resize(2*size, {-1, T{}});
        for (int i = 0; i < n; ++i) {
            data[i+size] = {time, a[i]};
        }
    }

    void apply_point(int k, T f) {
        time++;
        data[k+size].first = time;
        data[k+size].second = f;
    }

    void apply(int l, int r, const T f) {
        assert(0 <= l && l <= r && r <= n);
        l += size;
        r += size;
        time++;
        while (l < r) {
            if (l & 1) {
                data[l].first = time;
                data[l].second = f;
                l++;
            }
            if (r & 1) {
                r--;
                data[r].first = time;
                data[r].second = f;
            }
            l >>= 1;
            r >>= 1;
        }
    }

    void all_apply(T f) {
        time++;
        data[1].first = time;
        data[1].second = f;
    }

    vector<T> tovector() {
        for (int k = 1; k < size; ++k) {
            int t = data[k].first;
            if (data[k<<1].first < t) {
                data[k<<1] = data[k];
            }
            if (data[k<<1|1].first < t) {
                data[k<<1|1] = data[k];
            }
        }
        vector<T> res(n);
        for (int i = 0; i < n; ++i) {
            res[i] = data[i+size].second;
        }
        return res;
    }

    T get(int k) const {
        k += size;
        auto [t, ans] = data[k];
        for (int i = 1; i <= log; ++i) {
            if (t < data[k>>i].first) {
                t = data[k>>i].first;
                ans = data[k>>i].second;
            }
        }
        return ans;
    }
};
}  // namespace titan23
