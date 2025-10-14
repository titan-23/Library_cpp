#include <vector>
#include <cassert>
using namespace std;

namespace titan23 {

// 2次元セグ木 / 空間 O(hw), 時間 O(log(h)log(w))
// 空間 4hw*sizeof(int)*sizeof(T)
// NOTE クエリが少ないときはタイムスタンプの型をshortとかにするのも手
template <class T>
class DualSegmentTree2DRUQ {
// #define LARGE_QUERY // クエリの数がnより十分でかいときは有効にすると強いかも

private:
    int stamp;
    int h, w;
    int hs, ws;
#ifdef LARGE_QUERY
    vector<pair<int, T>> data;
#else
    vector<T> data;
    vector<int> sd;
#endif

public:
    DualSegmentTree2DRUQ() {}
    DualSegmentTree2DRUQ(int h, int w) : stamp(1), h(h), w(w) {
        hs = 1; while (hs < h) hs <<= 1;
        ws = 1; while (ws < w) ws <<= 1;
#ifdef LARGE_QUERY
        data.resize(4*hs*ws, {0, T{}});
#else
        sd.resize(4*hs*ws, 0);
        data.emplace_back(T{});
#endif
    }

    DualSegmentTree2DRUQ(const vector<vector<T>> a) : stamp(1) {
        h = a.size();
        w = h ? a[0].size() : 0;
        hs = 1; while (hs < h) hs <<= 1;
        ws = 1; while (ws < w) ws <<= 1;
#ifdef LARGE_QUERY
        data.resize(4*hs*ws, {-1, T{}});
        for (int i = 0; i < h; ++i) for (int j = 0; j < w; ++j) {
            data[(i+hs)*2*ws+(j+ws)] = {0, a[i][j]};
        }
#else
        sd.resize(4*hs*ws, {-1, T{}});
        stamp = 0;
        for (int i = 0; i < h; ++i) for (int j = 0; j < w; ++j) {
            sd[(i+hs)*2*ws+(j+ws)] = stamp;
            stamp++;
            data.emplace_back(a[i][j]);
        }
#endif
    }

    T get(int y, int x) const {
        y += hs; x += ws;
#ifdef LARGE_QUERY
        pair<int, T> ans = {-1, T{}}; // oldest
        for (int i = y; i > 0; i >>= 1) {
            for (int j = x; j > 0; j >>= 1) {
                ans = max(ans, data[i*2*ws+j]);
            }
        }
        return ans.second;
#else
        int ans = -1; // oldest
        for (int i = y; i > 0; i >>= 1) {
            for (int j = x; j > 0; j >>= 1) {
                ans = max(ans, sd[i*2*ws+j]);
            }
        }
        return data[ans];
#endif
    }

    // [u, d) x [l, r) / O(log(h)log(w))
    void apply(int u, int d, int l, int r, T v) {
        assert(0 <= u && u <= d && d <= h);
        assert(0 <= l && l <= r && r <= w);
        u += hs; d += hs;
        l += ws; r += ws;
#ifdef LARGE_QUERY
        pair<int, T> nxt = {stamp, v};
#else
        data.emplace_back(v);
#endif
        auto pw = [&] (int idx) -> void {
            int nl = l, nr = r;
            while (nl < nr) {
#ifdef LARGE_QUERY
                if (nl & 1) data[idx*2*ws+nl] = nxt, nl++;
                if (nr & 1) nr--, data[idx*2*ws+nr] = nxt;
#else
                if (nl & 1) sd[idx*2*ws+nl] = stamp, nl++;
                if (nr & 1) nr--, sd[idx*2*ws+nr] = stamp;
#endif
                nl >>= 1; nr >>= 1;
            }
        };

        while (u < d) {
            if (u & 1) { pw(u), u++; }
            if (d & 1) { --d; pw(d); }
            u >>= 1; d >>= 1;
        }
        stamp++;
    }

    // O(hw)
    vector<vector<T>> tovector() {
        vector<vector<T>> ans(h, vector<T>(w));
#ifdef LARGE_QUERY
        for (int i = 1; i < hs; ++i) for (int j = 1; j < ws*2; ++j) {
            data[(i<<1)*2*ws+(j)] = max(data[(i<<1)*2*ws+(j)], data[(i)*2*ws+(j)]);
            data[(i<<1|1)*2*ws+(j)] = max(data[(i<<1|1)*2*ws+(j)], data[(i)*2*ws+(j)]);
        }
        for (int i = hs; i < hs*2; ++i) for (int j = 1; j < ws; ++j) {
            data[i*2*ws+(j<<1)] = max(data[i*2*ws+(j<<1)], data[i*2*ws+j]);
            data[i*2*ws+(j<<1|1)] = max(data[i*2*ws+(j<<1|1)], data[i*2*ws+j]);
        }
        for (int i = 0; i < h; ++i) for (int j = 0; j < w; ++j) {
            ans[i][j] = data[(i+hs)*2*ws+(j+ws)].second;
        }
#else
        for (int i = 1; i < hs; ++i) for (int j = 1; j < ws*2; ++j) {
            sd[(i<<1)*2*ws+(j)] = max(sd[(i<<1)*2*ws+(j)], sd[(i)*2*ws+(j)]);
            sd[(i<<1|1)*2*ws+(j)] = max(sd[(i<<1|1)*2*ws+(j)], sd[(i)*2*ws+(j)]);
        }
        for (int i = hs; i < hs*2; ++i) for (int j = 1; j < ws; ++j) {
            sd[i*2*ws+(j<<1)] = max(sd[i*2*ws+(j<<1)], sd[i*2*ws+j]);
            sd[i*2*ws+(j<<1|1)] = max(sd[i*2*ws+(j<<1|1)], sd[i*2*ws+j]);
        }
        for (int i = 0; i < h; ++i) for (int j = 0; j < w; ++j) {
            ans[i][j] = data[sd[(i+hs)*2*ws+(j+ws)]];
        }
#endif
        return ans;
    }
};
} // namespace titan23
