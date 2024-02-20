#include <vector>
using namespace std;

namespace titan23 {

  struct Mo {

    // https://take44444.github.io/Algorithm-Book/range/mo/main.html

    int n, q, query_count;
    long long max_n;
    vector<int> _l, _r;

    Mo(const int n, const int q) {
      this->max_n = 1 << 25;
      this->query_count = 0;
      this->_l.reserve(q);
      this->_r.reserve(q);
      this->n = n;
      this->q = q;
    }

    long long hilbertorder(int x, int y) const {
      long long rx, ry, d = 0;
      for (long long s = max_n>>1ll; s; s >>= 1ll) {
        rx = (x & s) > 0, ry = (y & s) > 0;
        d += s * s * ((rx * 3) ^ ry);
        if (ry) continue;
        if (rx) {
          x = max_n - 1 - x;
          y = max_n - 1 - y;
        }
        swap(x, y);
      }
      return d;
    }

    void add_query(const int &l, const int &r) {
      ++query_count;
      _l.emplace_back(l);
      _r.emplace_back(r);
    }

    // F1~F5: lambda関数
    template<typename F1, typename F2, typename F3, typename F4, typename F5>
    void run(F1 &&add_left, F2 &&add_right, F3 &&del_left, F4 &&del_right, F5 &&out) {
      assert(q == query_count);
      vector<int> qi(q);
      iota(qi.begin(), qi.end(), 0);
      vector<long long> eval(q);
      for (int i = 0; i < q; ++i) {
        eval[i] = hilbertorder(_l[i], _r[i]);
      }
      sort(qi.begin(), qi.end(), [&] (const int &i, const int &j) {
        return eval[i] < eval[j];
      });
      int nl = 0, nr = 0;
      for (const int &i: qi) {
        const int li = _l[i], ri = _r[i];
        while (nl > li) add_left(--nl);
        while (nl < li) del_left(nl++);
        while (nr < ri) add_right(nr++);
        while (nr > ri) del_right(--nr);
        out(i);
      }
    }
  };
}  // namespace titan23

