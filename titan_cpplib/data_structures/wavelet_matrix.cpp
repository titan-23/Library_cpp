#include <vector>
#include <queue>
#include "titan_cpplib/data_structures/bit_vector.cpp"
using namespace std;

// WaveletMatrix
namespace titan23 {

  template<typename T>
  class WaveletMatrix {

   private:
    T sigma;
    int log;
    vector<BitVector> v;
    vector<int> mid;
    int n;

    int bit_length(const int n) const {
      return 32 - __builtin_clz(n);
    }

    void build(vector<T> a) {
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
        assert(a.size() == n);
      }
    }

   public:
    WaveletMatrix() {}

    WaveletMatrix(const T sigma)
        : sigma(sigma), log(bit_length(sigma-1)), v(log), mid(log), n(0) {}

    WaveletMatrix(const T sigma, const vector<T> &a)
        : sigma(sigma), log(bit_length(sigma-1)), v(log), mid(log), n(a.size()) {
      build(a);
    }

    T access(int k) const {
      T s = 0;
      for (int bit = log-1; bit >= 0; --bit) {
        if (v[bit].access(k)) {
          s |= (T)1 << bit;
          k = v[bit].rank1(k) + mid[bit];
        } else {
          k = v[bit].rank0(k);
        }
      }
      return s;
    }

    int rank(int r, int x) const {
      // ``a[0, r)`` に含まれる ``x`` の個数を返します。
      int l = 0;
      for (int bit = log-1; bit >= 0; --bit) {
        if ((x >> bit) & 1) {
          l = v[bit].rank1(l) + mid[bit];
          r = v[bit].rank1(r) + mid[bit];
        } else {
          l = v[bit].rank0(l);
          r = v[bit].rank0(r);
        }
      }
      return r - l;
    }

    int select(int k, int x) const {
      // ``k`` 番目の ``v`` のインデックスを返します。
      int s = 0;
      for (int bit = log-1; bit >= 0; --bit) {
        if ((x >> bit) & 1) {
          s = v[bit].rank0(n) + v[bit].rank1(s);
        } else {
          s = v[bit].rank0(s);
        }
      }
      s += k;
      for (int bit = 0; bit < log; ++bit) {
        if ((x >> bit) & 1) {
          s = v[bit].select1(s - v[bit].rank0(n));
        } else {
          s = v[bit].select0(s);
        }
      }
      return s;
    }

    T kth_smallest(int l, int r, int k) const {
      // ``a[l, r)`` の中で k 番目に **小さい** 値を返します。
      T s = 0;
      for (int bit = log-1; bit >= 0; --bit) {
        const int r0 = v[bit].rank0(r), l0 = v[bit].rank0(l);
        const int cnt = r0 - l0;
        if (cnt <= k) {
          s |= (T)1 << bit;
          k -= cnt;
          l = l - l0 + mid[bit];
          r = r - r0 + mid[bit];
        } else {
          l = l0;
          r = r0;
        }
      }
      return s;
    }

    T kth_largest(int l, int r, int k) const {
      return kth_smallest(l, r, r-l-k-1);
    }

    vector<pair<int, int>> topk(int l, int r, int k) {
      //``a[l, r)`` の中で、要素を出現回数が多い順にその頻度とともに ``k`` 個返します。
      // heap[-length, x, l, bit]
      priority_queue<tuple<int, T, int, int>> hq;
      hq.emplace(r-l, 0, l, log-1);
      vector<pair<T, int>> ans;
      while (!hq.empty()) {
        auto [length, x, l, bit] = hq.top();
        hq.pop();
        if (bit == -1) {
          ans.emplace_back(x, length);
          k -= 1;
          if (k == 0) break;
        } else {
          r = l + length;
          int l0 = v[bit].rank0(l);
          int r0 = v[bit].rank0(r);
          if (l0 < r0) hq.emplace(r0-l0, x, l0, bit-1);
          int l1 = v[bit].rank1(l) + mid[bit];
          int r1 = v[bit].rank1(r) + mid[bit];
          if (l1 < r1) hq.emplace(r1-l1, x|((T)1<<bit), l1, bit-1);
        }
      }
      return ans;
    }

    T sum(int l, int r) const {
      assert(false);
      T s = 0;
      for (const auto &[sum, cnt]: topk(l, r, r-l)) {
        s += sum * cnt;
      }
      return s;
    }

    int range_freq(int l, int r, int x) const {
      // a[l, r) で x 未満の要素の数を返す'''
      int ans = 0;
      for (int bit = log-1; bit >= 0; --bit) {
        int l0 = v[bit].rank0(l), r0 = v[bit].rank0(r);
        if ((x >> bit) & 1) {
          ans += r0 - l0;
          l += mid[bit] - l0;
          r += mid[bit] - r0;
        } else {
          l = l0;
          r = r0;
        }
      }
      return ans;
    }

    int range_freq(int l, int r, int x, int y) const {
      //``a[l, r)`` に含まれる、 ``x`` 以上 ``y`` 未満である要素の個数を返します。
      return range_freq(l, r, y) - range_freq(l, r, x);
    }

    T prev_value(int l, int r, int x) const {
      //``a[l, r)`` で、``x`` 以上 ``y`` 未満であるような要素のうち最大の要素を返します。
      return kth_smallest(l, r, range_freq(l, r, x)-1);
    }

    T next_value(int l, int r, int x) const {
      return kth_smallest(l, r, range_freq(l, r, x));
    }

    int range_count(int l, int r, int x) const {
      //``a[l, r)`` に含まれる ``x`` の個数を返します。
      return rank(r, x) - rank(l, x);
    }

    int len() const {
      return n;
    }

    void print() const {}
  };
}  // namespace titan23
