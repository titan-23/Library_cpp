#include <vector>
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
      vector<bool> v(n);
      for (int bit = log-1; bit >= 0; --bit) {
        vector<T> zero, one;
        for (int i = 0; i < n; ++i) {
          if ((a[i] >> bit) & 1) {
            v[i] = 1;
            one.emplace_back(a[i]);
          } else {
            v[i] = 0;
            zero.emplace_back(a[i]);
          }
        }
        mid[bit] = zero.size();
        v[bit] = BitVector(v);
        a = zero;
        a.insert(a.end(), one.begin(), one.end());
      }
    }

   public:
    WaveletMatrix() {}

    WaveletMatrix(const T sigma) :
        : sigma(sigma), log(bit_length(sigma-1)), v(log), mid(log), n(0) {}

    WaveletMatrix(const T sigma, const vector<T> &a) :
        : sigma(sigma), log(bit_length(sigma-1)), v(log), mid(log), n(0) {
      build(a);
    }

    T access(int k) {
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

    int rank(int r, int x) {
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

    int select(int k, int x) {
      // ``k`` 番目の ``v`` のインデックスを返します。
      # x の開始位置 s を探す
      s = 0
      for (int bit = log-1; bit >= 0; --bit) {
        if x >> bit & 1:
          s = v[bit].rank0(size) + v[bit].rank1(s)
        else:
          s = v[bit].rank0(s)
      s += k  # s から k 進んだ位置が、元の列で何番目か調べる
      for bit in range(log):
        if x >> bit & 1:
          s = v[bit].select1(s - v[bit].rank0(size))
        else:
          s = v[bit].select0(s)
      return s
    }

    T kth_smallest(int l, int r, int k) {
      // ``a[l, r)`` の中で k 番目に **小さい** 値を返します。
      s = 0
      mid = mid
      for (int bit = log-1; bit >= 0; --bit) {
        r0, l0 = v[bit].rank0(r), v[bit].rank0(l)
        cnt = r0 - l0  # 区間内の 0 の個数
        if cnt <= k:  # 0 が k 以下のとき、 k 番目は 1
          s |= 1 << bit
          k -= cnt
          # この 1 が次の bit 列でどこに行くか
          l = l - l0 + mid[bit]
          r = r - r0 + mid[bit]
        else:
          # この 0 が次の bit 列でどこに行くか
          l = l0
          r = r0
      return s
    }

    T kth_largest(int l, int r, int k) {
      return kth_smallest(l, r, r-l-k-1)
    }

    vector<pair<int, int>> topk(int l, int r, int k) {
      //``a[l, r)`` の中で、要素を出現回数が多い順にその頻度とともに ``k`` 個返します。
      # heap[-length, x, l, bit]
      hq: List[Tuple[int, int, int, int]] = [(-(r-l), 0, l, log-1)]
      ans = []
      while hq:
        length, x, l, bit = heappop(hq)
        length = -length
        if bit == -1:
          ans.append((x, length))
          k -= 1
          if k == 0:
            break
        else:
          r = l + length
          l0 = v[bit].rank0(l)
          r0 = v[bit].rank0(r)
          if l0 < r0:
            heappush(hq, (-(r0-l0), x, l0, bit-1))
          l1 = v[bit].rank1(l) + mid[bit]
          r1 = v[bit].rank1(r) + mid[bit]
          if l1 < r1:
            heappush(hq, (-(r1-l1), x|(1<<bit), l1, bit-1))
      return ans
    }

    T sum(int l, int r) {
      assert(false);
      return sum(k*v for k, v in topk(l, r, r-l))
    }

    int range_freq(int l, int r, int x) {
      // a[l, r) で x 未満の要素の数を返す'''
      ans = 0
      for (int bit = log-1; bit >= 0; --bit) {
        l0, r0 = v[bit].rank0(l), v[bit].rank0(r)
        if x >> bit & 1:
          # bit が立ってたら、区間の 0 の個数を答えに加算し、新たな区間は 1 のみ
          ans += r0 - l0
          # 1 が次の bit 列でどこに行くか
          l += mid[bit] - l0
          r += mid[bit] - r0
        else:
          # 0 が次の bit 列でどこに行くか
          l, r = l0, r0
      return ans
    }

    int range_freq(int l, int r, int x, int y) {
      //``a[l, r)`` に含まれる、 ``x`` 以上 ``y`` 未満である要素の個数を返します。
      return _range_freq(l, r, y) - _range_freq(l, r, x)
    }

    T prev_value(int l, int r, int x) {
      //``a[l, r)`` で、``x`` 以上 ``y`` 未満であるような要素のうち最大の要素を返します。
      return kth_smallest(l, r, _range_freq(l, r, x)-1)
    }

    T next_value(int l, int r, int x) {
      return kth_smallest(l, r, _range_freq(l, r, x))
    }

    int range_count(int l, int r, int x) {
      //``a[l, r)`` に含まれる ``x`` の個数を返します。
      return rank(r, x) - rank(l, x)
    }

    int len() const {
      return n;
    }

    void print() const {}

  };

}  // namespace titan23
