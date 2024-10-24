// #include "titan_cpplib/data_structures/segment_tree.cpp"
#include <iostream>
#include <vector>
#include <cassert>
using namespace std;

namespace titan23 {

    template <class T,
              T (*_op)(T, T),
              T (*_e)()>
    struct SegmentTree {
      private:
        int n, _size, _log;
        vector<T> _data;

      public:
        SegmentTree() {}

        SegmentTree(const int n) {
            _build(n);
        }

        SegmentTree(const vector<T> &a) {
            int n = (int)a.size();
            _build(n);
            for (int i = 0; i < n; ++i) {
                _data[i+_size] = a[i];
            }
            for (int i = _size-1; i > 0; --i) {
                _data[i] = _op(_data[i<<1], _data[i<<1|1]);
            }
        }

        void _build(const int n) {
            this->n = n;
            this->_log = 32 - __builtin_clz(n);
            this->_size = 1 << _log;
            this->_data.resize(_size << 1, _e());
        }

        T get(int const k) const {
            return _data[(k<0? (k+n+_size): (k+_size))];
        }

        void set(int k, const T v) {
            if (k < 0) k += n;
            k += _size;
            _data[k] = v;
            for (int i = 0; i < _log; ++i) {
                k >>= 1;
                _data[k] = _op(_data[k<<1], _data[k<<1|1]);
            }
        }

        T prod(int l, int r) const {
            l += _size;
            r += _size;
            T lres = _e(), rres = _e();
            while (l < r) {
                if (l & 1) lres = _op(lres, _data[l++]);
                if (r & 1) rres = _op(_data[r^1], rres);
                l >>= 1;
                r >>= 1;
            }
            return _op(lres, rres);
        }

        T all_prod() const {
            return _data[1];
        }

        template<typename F>  // F: function<bool (T)> f
        int max_right(int l, F &&f) const {
            assert(0 <= l && l <= _size);
            // assert(f(_e()));
            if (l == n) return n;
            l += _size;
            T s = _e();
            while (1) {
                while ((l & 1) == 0) {
                l >>= 1;
                }
                if (!f(_op(s, _data[l]))) {
                while (l < _size) {
                    l <<= 1;
                    if (f(_op(s, _data[l]))) {
                    s = _op(s, _data[l]);
                    l |= 1;
                    }
                }
                return l - _size;
                }
                s = _op(s, _data[l]);
                ++l;
                if ((l & (-l)) == l) break;
            }
            return n;
        }

        template<typename F>  // F: function<bool (T)> f
        int min_left(int r, F &&f) const {
            assert(0 <= r && r <= n);
            // assert(f(_e()));
            if (r == 0) return 0;
            r += _size;
            T s = _e();
            while (r > 0) {
                --r;
                while (r > 1 && (r & 1)) {
                r >>= 1;
                }
                if (!f(_op(_data[r], s))) {
                while (r < _size) {
                    r = (r << 1) | 1;
                    if (f(_op(_data[r], s))) {
                    s = _op(_data[r], s);
                    r ^= 1;
                    }
                }
                return r + 1 - _size;
                }
                s = _op(_data[r], s);
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

