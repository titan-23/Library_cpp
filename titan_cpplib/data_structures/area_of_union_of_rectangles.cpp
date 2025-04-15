#include <vector>
#include "titan_cpplib/algorithm/zaatsu.cpp"
#include "titan_cpplib/data_structures/lazy_segment_tree.cpp"
using namespace std;

namespace titan23 {

namespace AOUFR {
using S = long long;
using F = int;
const int bit = 31;
const int msk = (1ll<<bit)-1;
S op(S s, S t) {
    int s_val = s>>bit, s_cnt = s&msk;
    int t_val = t>>bit, t_cnt = t&msk;
    if (s_val < t_val) return s;
    if (t_val < s_val) return t;
    return (long long)s_val<<bit | (s_cnt + t_cnt);
}
S mapping(F f, S s) {
    int s_val = s>>bit, s_cnt = s&msk;
    return ((long long)s_val+f)<<bit | s_cnt;
}
F composition(F f, F g) {
    return f + g;
}
S e() {
    return 1ll<<61;
}
F id() {
    return 0;
}
} // namespace AOUFR

template<typename T>
class AreaOfUnionOfRectangles {
  private:
    int n;
    vector<tuple<T, T, T, T>> ldru;
    vector<int> X, Y;

  public:
    AreaOfUnionOfRectangles() : n(0) {}
    AreaOfUnionOfRectangles(int n) : n(n) {
        ldru.reserve(n);
        X.reserve(2*n);
        Y.reserve(2*n);
    }

    void add_rectangle(T l, T d, T r, T u) {
        ldru.emplace_back(l, d, r, u);
        X.emplace_back(l);
        X.emplace_back(r);
        Y.emplace_back(d);
        Y.emplace_back(u);
    }

    T get_sum() {
        titan23::Zaatsu<int> ZX(X), ZY(Y);
        vector<vector<pair<int, int>>> left(ZY.len()), right(ZY.len());
        for (auto [l, d, r, u] : ldru) {
            l = ZX.to_zaatsu(l);
            r = ZX.to_zaatsu(r);
            d = ZY.to_zaatsu(d);
            u = ZY.to_zaatsu(u);
            left[d].emplace_back(l, r);
            right[u].emplace_back(l, r);
        }

        vector<int> nX(ZX.len()), nY(ZY.len());
        for (int i = 0; i < ZX.len(); ++i) {
            nX[i] = ZX.to_origin(i);
        }
        for (int i = 0; i < ZY.len(); ++i) {
            nY[i] = ZY.to_origin(i);
        }
        vector<AOUFR::S> init(ZX.len()-1);
        for (int i = 0; i < ZX.len()-1; ++i) {
            init[i] = nX[i+1] - nX[i];
        }
        titan23::LazySegmentTree<AOUFR::S, AOUFR::op, AOUFR::e, AOUFR::F, AOUFR::mapping, AOUFR::composition, AOUFR::id> seg(init);

        T ans = 0;
        T s = nX.back() - nX[0];
        for (int i = 0; i < ZY.len()-1; ++i) {
            for (auto [l, r] : left[i]) {
                seg.apply(l, r, 1);
            }

            T p_all = seg.all_prod();
            int p_val = p_all>>AOUFR::bit, p_cnt = p_all&AOUFR::msk;
            T p = p_val == 0 ? s-p_cnt : s;
            ans += p * (nY[i+1]-nY[i]);

            for (auto [l, r] : right[i+1]) {
                seg.apply(l, r, -1);
            }
        }

        return ans;
    }
};
}
