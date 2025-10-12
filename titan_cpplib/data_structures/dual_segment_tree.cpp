#include <iostream>
#include <vector>
#include <cassert>
using namespace std;

// DualSegmentTree
namespace titan23 {

template <class T,
        class F,
        T (*mapping)(F, T),
        F (*composition)(F, F),
        F (*id)()>
struct DualSegmentTree {

    int _n, _size, _log;
    vector<T> _data;
    vector<F> _lazy;

    DualSegmentTree() {}

    DualSegmentTree(const int n, const T init) {
        this->_n = n;
        this->_log = 32 - __builtin_clz(n);
        this->_size = 1 << _log;
        this->_data.resize(_size);
        this->_lazy.resize(_size, id());
    }

    DualSegmentTree(const vector<T> a) {
        int n = (int)a.size();
        this->_n = n;
        this->_log = 32 - __builtin_clz(n);
        this->_size = 1 << _log;
        this->_data.resize(_size);
        this->_lazy.resize(_size, id());
        for (int i = 0; i < _n; ++i) {
            _data[i] = a[i];
        }
    }

    void _all_apply(int k, F f) {
        if (k < _size) {
            _lazy[k] = composition(f, _lazy[k]);
            return;
        }
        k -= _size;
        _data[k] = mapping(f, _data[k]);
    }

    void _propagate(const int k) {
        if (_lazy[k] == id()) return;
        _all_apply(k<<1, _lazy[k]);
        _all_apply(k<<1|1, _lazy[k]);
        _lazy[k] = id();
    }

    void apply_point(int k, F f) {
        k += _size;
        for (int i = _log; i > 0; --i) {
            _propagate(k >> i);
        }
        _data[k-_size] = mapping(f, _data[k-_size]);
    }

    void apply(int l, int r, const F f) {
        assert(0 <= l && l <= r && r <= _n);
        if (l == r || f == id()) return;
        l += _size;
        r += _size;
        for (int i = _log; i > 0; --i) {
            if (((l >> i << i) != l)) {
                _propagate(l>>i);
            }
            if (((r >> i << i) != r)) {
                _propagate((r-1)>>i);
            }
        }
        if ((l-_size) & 1) {
            _data[l-_size] = mapping(f, _data[l-_size]);
            ++l;
        }
        if ((r-_size) & 1) {
            _data[(r-_size)^1] = mapping(f, _data[(r-_size)^1]);
            r ^= 1;
        }
        l >>= 1;
        r >>= 1;
        while (l < r) {
            if (l & 1) {
                _lazy[l] = composition(f, _lazy[l]);
                ++l;
            }
            if (r & 1) {
                --r;
                _lazy[r] = composition(f, _lazy[r]);
            }
            l >>= 1;
            r >>= 1;
        }
    }

    void all_apply(F f) {
        _lazy[1] = composition(f, _lazy[1]);
    }

    void all_propagate() {
        for (int i = 0;  i < _size; ++i) {
            _propagate(i);
        }
    }

    vector<T> tovector() {
        all_propagate();
        vector<T> res = _data;
        return res;
    }

    T get(int k) {
        if (k < 0) k += _n;
        k += _size;
        for (int i = _log; i > 0; --i) {
            _propagate(k>>i);
        }
        return _data[k-_size];
    }

    void set(int k, T v) {
        if (k < 0) k += _n;
        k += _size;
        for (int i = _log; i > 0; --i) {
            _propagate(k>>i);
        }
        _data[k-_size] = v;
    }

    void print() {
        cout << '[';
        for (int i = 0; i < _n-1; ++i) {
            cout << get(i) << ", ";
        }
        cout << get(_n-1);
        cout << ']' << endl;
    }
};
}  // namespace titan23
