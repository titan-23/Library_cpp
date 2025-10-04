#include <iostream>
#include <vector>
using namespace std;

namespace titan23 {

/// 定数倍の軽い range add range sum
template<typename T>
class FenwickTreeRAQ {
private:
    int n;
    vector<T> bit0, bit1;
    int bit_size;

    void _add(vector<T> &bit, int k, T x) {
        k += 1;
        while (k <= bit_size) {
            bit[k] += x;
            k += k & -k;
        }
    }

    T _pref(const vector<T> &bit, int r) const {
        T ret = 0;
        while (r > 0) {
            ret += bit[r];
            r -= r & -r;
        }
        return ret;
    }

public:
    FenwickTreeRAQ() : n(0), bit_size(0) {}
    FenwickTreeRAQ(int n) : n(n), bit0(n+2, 0), bit1(n+2, 0), bit_size(n+1) {}

    FenwickTreeRAQ(vector<T> a) : n(a.size()), bit0(n+2, 0), bit1(n+2, 0), bit_size(n+1) {
        for (int i = 0; i < n; ++i) {
            add(i, a[i]);
        }
    }

    /// all 0
    void clear() {
        fill(bit0.begin(), bit0.end(), (T)0);
        fill(bit1.begin(), bit1.end(), (T)0);
    }

    /// add x to a[l, r)
    void add_range(int l, int r, T x) {
        _add(bit0, l, -x * l);
        _add(bit0, r, x * r);
        _add(bit1, l, x);
        _add(bit1, r, -x);
    }

    /// a[k] += x;
    void add(int k, T x) {
        add_range(k, k+1, x);
    }

    /// sum(a[l, r))
    T sum(int l, int r) const {
        return _pref(bit0, r) + (T)r*_pref(bit1, r) - _pref(bit0, l) - (T)l*_pref(bit1, l);
    }

    /// a[k] <- x
    void set(int k, T x) {
        T pre = get(k);
        add(k, x-pre);
    }

    /// a[k]
    T get(int k) const {
        return sum(k, k+1);
    }

    vector<T> tovector() const {
        vector<T> res(n);
        for (int i = 0; i < n; ++i) {
            res[i] = get(i);
        }
    }

    friend ostream& operator<<(ostream& os, const titan23::FenwickTreeRAQ<T> &fw) {
        os << fw.tovector();
        return os;
    }
};
} // namespace titan23
