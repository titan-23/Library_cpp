#include <iostream>
#include <vector>
#include <cassert>
using namespace std;

// FenwickTree2D
namespace titan23 {

template<typename T>
struct FenwickTree2D {
    long long _h, _w;
    vector<T> _bit;

    FenwickTree2D() {}
    FenwickTree2D(int h, int w) : _h(h+1), _w(w+1), _bit(_h*_w, 0) {
        assert(_h * _w < 1e9);
    }

    void add(int h, int w, T x) {
        assert(0 <= h && h < _h);
        assert(0 <= w && w < _w);
        ++h; ++w;
        while (h < _h) {
            int j = w;
            while (j < _w) {
                _bit[h*_w+j] += x;
                j += (j & (-j));
            }
            h += (h & (-h));
        }
    }

    void set(int h, int w, T x) {
        assert(0 <= h && h < _h);
        assert(0 <= w && w < _w);
        add(h, w, x - get(h, w));
    }

    T sum(int h, int w) const {
        assert(0 <= h && h < _h);
        assert(0 <= w && w < _w);
        T res = 0;
        while (h > 0) {
            int j = w;
            while (j > 0) {
                res += _bit[h*_w + j];
                j -= (j & (-j));
            }
            h -= (h & (-h));
        }
        return res;
    }

    T sum(int h1, int w1, int h2, int w2) const {
        assert(0 <= h1 && h1 <= h2 && h2 <= _h);
        assert(0 <= w1 && w1 <= w2 && w2 <= _w);
        return sum(h2, w2) - sum(h2, w1) - sum(h1, w2) + sum(h1, w1);
    }

    T get(int h, int w) const {
        assert(0 <= h && h < _h);
        assert(0 <= w && w < _w);
        return sum(h, w, h+1, w+1);
    }

    void print() const {
        cout << "[" << endl;
        for (int i = 0; i < _h-1; ++i) {
            cout << "  ";
            for (int j = 0; j < _w-1; ++j) {
                cout << get(i, j) << " ";
            }
            cout << endl;
        }
        cout << "]" << endl;
    }
};
}  // namespace titan23
