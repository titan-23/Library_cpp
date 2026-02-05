#include <iostream>
#include <vector>
#include <unordered_map>
#include <map>
#include <cassert>
using namespace std;

#include <ext/pb_ds/assoc_container.hpp>
#include <ext/pb_ds/hash_policy.hpp>

// DynamicFenwickTree2D
namespace titan23 {

  template<typename T, typename W>
  struct DynamicFenwickTree2D {
    int _h, _w;
    unordered_map<T, unordered_map<T, W>> _bit;

    DynamicFenwickTree2D() {}
    DynamicFenwickTree2D(T h, T w) : _h(h+1), _w(w+1) {}

    void add(T h, T w, W x) {
      assert(0 <= h && h < _h);
      assert(0 <= w && w < _w);
      ++h; ++w;
      while (h < _h) {
        T j = w;
        auto it = _bit.find(h);
        if (it == _bit.end()) {
          _bit[h];
          it = _bit.find(h);
        }
        while (j < _w) {
          it->second[j] += x;
          j += j & (-j);
        }
        h += h & (-h);
      }
    }

    void set(int h, int w, T x) {
      assert(0 <= h && h < _h);
      assert(0 <= w && w < _w);
      add(h, w, x - get(h, w));
    }

    W sum(T h, T w) const {
      assert(0 <= h && h < _h);
      assert(0 <= w && w < _w);
      W res = 0;
      while (h > 0) {
        T j = w;
        auto it_h = _bit.find(h);
        if (it_h != _bit.end()) {
          while (j > 0) {
            auto it_j = it_h->second.find(j);
            if (it_j != it_h->second.end()) {
              res += it_j->second;
            }
            j -= j & (-j);
          }
        }
        h -= h & (-h);
      }
      return res;
    }

    W sum(T h1, T w1, T h2, T w2) const {
      assert(0 <= h1 && h1 <= h2 && h2 <= _h);
      assert(0 <= w1 && w1 <= w2 && w2 <= _w);
      return sum(h2, w2) - sum(h2, w1) - sum(h1, w2) + sum(h1, w1);
    }

    W get(T h, T w) {
      assert(0 <= h && h < _h);
      assert(0 <= w && w < _w);
      return sum(h, w, h+1, w+1);
    }

    void print() {
    }
  };
}  // namespace titan23
