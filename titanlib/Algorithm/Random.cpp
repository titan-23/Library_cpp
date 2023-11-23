#include <cassert>
#include <vector>
using namespace std;

namespace titan23 {

  struct Random {
    int _x, _y, _z, _w;

    Random() {
      _x = 123456789;
      _y = 362436069;
      _z = 521288629;
      _w = 88675123;
    }

    int _xor128() {
      int t = _x ^ (_x << 11);
      _x = _y;
      _y = _z;
      _z = _w;
      _w = (_w ^ (_w >> 19)) ^ (t ^ (t >> 8));
      return _w;
    }

    double random() {
      return (double)(_xor128()) / 0xFFFFFFFF;
    }

    int randint(int begin, int end) {
      assert(begin <= end);
      return begin + _xor128() % (end - begin + 1);
    }

    int randrange(int begin, int end) {
      assert(begin < end);
      return begin + _xor128() % (end - begin);
    }

    template <typename T>
    void shuffle(vector<T> &a) {
      int n = (int)a.size();
      for (int i = 0; i < n-1; ++i) {
        int j = randrange(i, n);
        swap(a[i], a[j]);
      }
    }
  };
} // namespace titan23

