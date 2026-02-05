#include <vector>
#include <algorithm>
#include <stack>
#include "titan_cpplib/ds/bit_vector.cpp"
#include "titan_cpplib/ds/static_RmQ.cpp"
using namespace std;

// WaveletMatrixMin
namespace titan23 {

/**
 * @tparam T 点の座標を表す型
 * @tparam W 重みを表す型
 */
template<typename T, typename W>
class WaveletMatrixMin {
private:
    W INF;
    int log;
    vector<tuple<T, T, W>> pos;
    vector<BitVector> v;
    vector<pair<T, T>> xy;
    vector<T> y;
    vector<int> mid;
    vector<titan23::StaticRmQ<W>> seg;

    int bit_length(T n) const {
        unsigned long long x = static_cast<unsigned long long>(n);
        return x == 0 ? 0 : 64 - __builtin_clzll(x);
    }

    void _build(vector<T> a) {
        for (int bit = log-1; bit >= 0; --bit) {
            vector<T> zero, one;
            v[bit] = BitVector(a.size());
            for (int i = 0; i < a.size(); ++i) {
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

public:
    WaveletMatrixMin() {}
    WaveletMatrixMin(const W INF) : INF(INF) {}

    void reserve(const int cap) {
        pos.reserve(cap);
    }

    void set_point(T x, T y, W w) {
        pos.emplace_back(x, y, w);
    }

    void build() {
        xy.reserve(pos.size());
        for (const auto &[x, y, _]: pos) {
            xy.emplace_back(x, y);
        }
        sort(xy.begin(), xy.end());
        xy.erase(unique(xy.begin(), xy.end()), xy.end());

        y.reserve(xy.size());
        for (const auto &[x, y_]: xy) {
            y.emplace_back(y_);
        }
        sort(y.begin(), y.end());
        y.erase(unique(y.begin(), y.end()), y.end());

        log = bit_length(y.size());
        v.resize(log);
        mid.resize(log);
        seg.resize(log);

        vector<T> a;
        a.reserve(xy.size());
        for (const auto &[x, y_]: xy) {
            a.emplace_back(lower_bound(y.begin(), y.end(), y_) - y.begin());
        }
        _build(a);

        vector<vector<W>> ws(log, vector<W>(xy.size(), INF));
        for (const auto [x, y_, w]: pos) {
            int k = lower_bound(xy.begin(), xy.end(), make_pair(x, y_)) - xy.begin();
            int i_y = lower_bound(y.begin(), y.end(), y_) - y.begin();
            for (int bit = log-1; bit >= 0; --bit) {
                if (((T)i_y >> bit) & 1) {
                    k = v[bit].rank1(k) + mid[bit];
                } else {
                    k = v[bit].rank0(k);
                }
                ws[bit][k] = min(ws[bit][k], w);
            }
        }

        for (int i = 0; i < log; ++i) {
            seg[i] = titan23::StaticRmQ<W>(ws[i], INF);
        }
    }

    // 領域 [x1, x2) x [y1, y2) の min を求める / O(logn)
    W range_min(T x1, T x2, T y1, T y2) const {
        assert(0 <= x1 && x1 <= x2);
        assert(0 <= y1 && y1 <= y2);
        int l = lower_bound(xy.begin(), xy.end(), make_pair(x1, (T)0)) - xy.begin();
        int r = lower_bound(xy.begin(), xy.end(), make_pair(x2, (T)0)) - xy.begin();
        T y1_idx = lower_bound(y.begin(), y.end(), y1) - y.begin();
        T y2_idx = lower_bound(y.begin(), y.end(), y2) - y.begin();
        W ans = INF;
        stack<tuple<int, int, int, T, T>> s;
        s.emplace(log-1, l, r, 0, (T)1<<log);
        while (!s.empty()) {
            auto [bit, nl, nr, y_l, y_r] = s.top(); s.pop();
            if (bit < 0 || nl >= nr) continue;
            if (y_r <= y1_idx || y_l >= y2_idx) continue;
            T y_mid = y_l | ((T)1 << bit);
            int l0 = v[bit].rank0(nl);
            int r0 = v[bit].rank0(nr);
            int l1 = v[bit].rank1(nl) + mid[bit];
            int r1 = v[bit].rank1(nr) + mid[bit];
            if (y1_idx <= y_l && y_r <= y2_idx) {
                if (l0 < r0) ans = min(ans, seg[bit].prod(l0, r0));
                if (l1 < r1) ans = min(ans, seg[bit].prod(l1, r1));
                continue;
            }
            if (bit - 1 >= 0) {
                if (y_l < y2_idx && y_mid > y1_idx) s.emplace(bit-1, l0, r0, y_l, y_mid);
                if (y_mid < y2_idx && y_r > y1_idx) s.emplace(bit-1, l1, r1, y_mid, y_r);
            } else {
                if (l0 < r0 && y_l < y2_idx && y_mid > y1_idx) {
                    ans = min(ans, seg[bit].prod(l0, r0));
                }
                if (l1 < r1 && y_mid < y2_idx && y_r > y1_idx) {
                    ans = min(ans, seg[bit].prod(l1, r1));
                }
            }
        }
        return ans;
    }
};
}  // namespace titan23
