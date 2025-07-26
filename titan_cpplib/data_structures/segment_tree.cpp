#include <iostream>
#include <vector>
#include <cassert>
using namespace std;

namespace titan23 {

template <class T,
            T (*op)(T, T),
            T (*e)()>
struct SegmentTree {
private:
    int n, _size, _log;
    vector<T> data;

    int bit_length(const int x) const {
        if (x == 0) return 0;
        return 32 - __builtin_clz(x);
    }

public:
    SegmentTree() {}

    SegmentTree(const int n) {
        _build(n);
    }

    SegmentTree(const vector<T> &a) {
        int n = (int)a.size();
        _build(n);
        for (int i = 0; i < n; ++i) {
            data[i+_size] = a[i];
        }
        for (int i = _size-1; i > 0; --i) {
            data[i] = op(data[i<<1], data[i<<1|1]);
        }
    }

    void _build(const int n) {
        this->n = n;
        this->_log = bit_length(n);
        this->_size = 1 << _log;
        this->data.resize(this->_size*2, e());
    }

    T get(int const k) const {
        assert(0 <= k && k < n);
        return data[(k<0 ? (k+n+_size) : (k+_size))];
    }

    void set(int k, const T v) {
        assert(0 <= k && k < n);
        k += _size;
        data[k] = v;
        for (int i = 0; i < _log; ++i) {
            k >>= 1;
            data[k] = op(data[k<<1], data[k<<1|1]);
        }
    }

    T prod(int l, int r) const {
        assert(0 <= l && l <= r && r <= n);
        l += _size;
        r += _size;
        T lres = e(), rres = e();
        while (l < r) {
            if (l & 1) lres = op(lres, data[l++]);
            if (r & 1) rres = op(data[r^1], rres);
            l >>= 1;
            r >>= 1;
        }
        return op(lres, rres);
    }

    T all_prod() const {
        return data[1];
    }

    template<typename F>  // F: function<bool (T)> f
    int max_right(int l, F &&f) const {
        assert(0 <= l && l <= _size);
        // assert(f(e()));
        if (l == n) return n;
        l += _size;
        T s = e();
        while (1) {
            while ((l & 1) == 0) {
            l >>= 1;
            }
            if (!f(op(s, data[l]))) {
            while (l < _size) {
                l <<= 1;
                if (f(op(s, data[l]))) {
                s = op(s, data[l]);
                l |= 1;
                }
            }
            return l - _size;
            }
            s = op(s, data[l]);
            ++l;
            if ((l & (-l)) == l) break;
        }
        return n;
    }

    template<typename F>  // F: function<bool (T)> f
    int min_left(int r, F &&f) const {
        assert(0 <= r && r <= n);
        // assert(f(e()));
        if (r == 0) return 0;
        r += _size;
        T s = e();
        while (r > 0) {
            --r;
            while (r > 1 && (r & 1)) {
            r >>= 1;
            }
            if (!f(op(data[r], s))) {
            while (r < _size) {
                r = (r << 1) | 1;
                if (f(op(data[r], s))) {
                s = op(data[r], s);
                r ^= 1;
                }
            }
            return r + 1 - _size;
            }
            s = op(data[r], s);
            if ((r & (-r)) == r) break;
        }
        return 0;
    }

    vector<T> tovector() const {
        vector<T> res(n);
        for (int i = 0; i < n; ++i) {
            res[i] = get(i);
        }
        return res;
    }

    void print() const {
        cout << '[';
        for (int i = 0; i < n-1; ++i) {
            cout << get(i) << ", ";
        }
        if (n > 0) cout << get(n-1);
        cout << ']' << endl;
    }
};
}  // namespace titan23
