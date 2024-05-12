#include <vector>
#include <algorithm>
#include "titan_cpplib/data_structures/bit_vector.cpp"
#include "titan_cpplib/data_structures/cumulative_sum.cpp"
using namespace std;

// WaveletMatrixCumulativeSum
namespace titan23 {

  /**
   * @brief 
   * 
   * @tparam T 点の座標を表す型
   * @tparam W 重みを表す型
   */
  template<typename T, typename W>
  class WaveletMatrixCumulativeSum {

   private:
    T sigma;
    int log;
    vector<tuple<T, T, W>> pos;
    vector<BitVector> v;
    vector<pair<T, T>> xy;
    vector<T> y;
    vector<int> mid;
    vector<titan23::CumulativeSum<W>> cumsum;
    int n;

    int bit_length(T n) const {
      int b = 0;
      while (n) {
        n >>= 1;
        ++b;
      }
      return b;
    }

    void _build(vector<T> a) {
      for (int bit = log-1; bit >= 0; --bit) {
        vector<T> zero, one;
        v[bit] = BitVector(n);
        for (int i = 0; i < n; ++i) {
          if ((a[i] >> bit) & 1) {
            v[bit].set(i);
            one.emplace_back(a[i]);
          } else {
            zero.emplace_back(a[i]);
          }
        }
        v[bit].build();
        mid[bit] = zero.size();
        a = zero;
        a.insert(a.end(), one.begin(), one.end());
      }
    }

    template<typename S>
    static void sort_unique(vector<S> &a) {
      std::sort(a.begin(), a.end());
      a.erase(std::unique(a.begin(), a.end()), a.end());
    }

    W _sum(int l, int r, int x) const {
      W ans = 0;
      for (int bit = log-1; bit >= 0; --bit) {
        int l0 = v[bit].rank0(l);
        int r0 = v[bit].rank0(r);
        if ((x>>bit) & 1) {
          l += mid[bit] - l0;
          r += mid[bit] - r0;
          ans += cumsum[bit].sum(l0, r0);
        } else {
          l = l0;
          r = r0;
        }
      }
      return ans;
    }

   public:
    WaveletMatrixCumulativeSum() {}
    WaveletMatrixCumulativeSum(const T sigma)
        : sigma(sigma), log(bit_length(sigma-1)), v(log), mid(log), cumsum(log) {
    }

    void reserve(const int cap) {
      pos.reserve(cap);
    }

    void set_point(T x, T y, W w) {
      pos.emplace_back(x, y, w);
    }

    void build() {
      xy.reserve(pos.size());
      for (const auto &[x, y, w]: pos) {
        xy.emplace_back(x, y);
      }
      sort_unique(xy);

      this->n = xy.size();

      y.reserve(n);
      for (const auto &[x, y_]: xy) {
        y.emplace_back(y_);
      }
      sort_unique(y);

      vector<int> a;
      for (const auto &[x, y_]: xy) {
        a.emplace_back(lower_bound(y.begin(), y.end(), y_) - y.begin());
      }
      _build(a);

      vector<vector<W>> ws(log, vector<W>(n, 0));
      for (const auto [x, y_, w]: pos) {
        int k = lower_bound(xy.begin(), xy.end(), make_pair(x, y_)) - xy.begin();
        int i_y = lower_bound(y.begin(), y.end(), y_) - y.begin();
        for (int bit = log-1; bit >= 0; --bit) {
          if ((i_y >> bit) & 1) {
            k = v[bit].rank1(k) + mid[bit];
          } else {
            k = v[bit].rank0(k);
          }
          ws[bit][k] += w;
        }
      }

      for (int i = 0; i < log; ++i) {
        cumsum[i] = titan23::CumulativeSum<W>(ws[i], 0);
      }
    }

    W sum(int w1, int w2, int h1, int h2) const {
      int l = lower_bound(xy.begin(), xy.end(), make_pair(w1, 0)) - xy.begin();
      int r = lower_bound(xy.begin(), xy.end(), make_pair(w2, 0)) - xy.begin();
      return _sum(l, r, lower_bound(y.begin(), y.end(), h2) - y.begin())
            - _sum(l, r, lower_bound(y.begin(), y.end(), h1) - y.begin());
    }
  };
}  // namespace titan23
