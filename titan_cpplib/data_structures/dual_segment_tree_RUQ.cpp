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
        if (x == 0) return 0;
        return 32 - __builtin_clz(x);
    }

    int n, size, log;
    int time;
    vector<pair<int, T>> data;

public:
    DualSegmentTreeRUQ() : time(0) {}
    DualSegmentTreeRUQ(const int n, const T init) : n(n), time(0) {
        this->log = bit_length(n);
        this->size = 1 << log;
        this->data.resize(2*size, {-1, T{}});
        for (int i = 0; i < n; ++i) {
            data[i+size] = {time, init};
        }
    }

    DualSegmentTreeRUQ(const vector<T> a) : time(0) {
        int n = (int)a.size();
        this->n = n;
        this->log = bit_length(n);
        this->size = 1 << log;
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
        data[1] = {time, f};
    }

    vector<T> tovector() {
        auto all_apply = [&] (int k, pair<int, T> f) -> void {
            if (data[k].first < f.first) {
                data[k] = f;
            }
        };

        for (int k = 1; k < size; ++k) {
            all_apply(k<<1, data[k]);
            all_apply(k<<1|1, data[k]);
            data[k].first = -1;
        }
        vector<T> res(n);
        for (int i = 0; i < n; ++i) {
            res[i] = data[i+size].second;
        }
        return res;
    }

    T get(int k) {
        k += size;
        auto [t, ans] = data[k];
        for (int i = log; i > 0; --i) {
            if (t < data[k>>1].first) {
                ans = data[k>>1].second;
            }
        }
        return ans;
    }
};
}  // namespace titan23
