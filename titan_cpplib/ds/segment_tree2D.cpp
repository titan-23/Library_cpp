/// https://github.com/titan-23/Library_cpp/blob/main/titan_cpplib/ds/segment_tree2D.cpp
#pragma once

#include <cassert>
#include <vector>
#include "titan_cpplib/others/bit.cpp"
using namespace std;

// SegmentTree2D
namespace titan23 {

/// @brief 2 次元セグメント木 / op は可換であること / 空間 O(hw)
template <class T, T (*op)(T, T), T (*e)()>
class SegmentTree2D {
private:
    int h, w;
    int hs, ws;
    vector<vector<T>> data;

public:
    SegmentTree2D() {}
    /// @brief h × w で構築する / 初期値 e() / O(hw)
    SegmentTree2D(int h, int w) : h(h), w(w) {
        hs = 1<<bit_length(h);
        ws = 1<<bit_length(w);
        data.resize(hs*2, vector<T>(ws*2, e()));
    }

    /// @brief 2 次元配列 a から構築する / O(hw)
    SegmentTree2D(const vector<vector<T>> a) {
        h = a.size();
        w = h ? a[0].size() : 0;
        hs = 1<<bit_length(h);
        ws = 1<<bit_length(w);
        data.resize(hs*2, vector<T>(ws*2, e()));
        for (int i = 0; i < h; ++i) for (int j = 0; j < w; ++j) {
            data[i+hs][j+ws] = a[i][j];
        }
        for (int i = hs; i < hs+h; ++i) {
            for (int j = ws-1; j > 0; --j) {
                data[i][j] = op(data[i][j<<1], data[i][j<<1|1]);
            }
        }
        for (int i = hs-1; i > 0; --i) {
            for (int j = 1; j < ws*2; ++j) {
                data[i][j] = op(data[i<<1][j], data[i<<1|1][j]);
            }
        }
    }

    /// @brief (y, x) の値を返す / O(1)
    T get(int y, int x) const {
        return data[y+hs][x+ws];
    }

    /// @brief (y, x) の値を v に変更する / O(log(h)log(w))
    void set(int y, int x, T v) {
        assert(0 <= y && y < h);
        assert(0 <= x && x < w);
        y += hs; x += ws;
        data[y][x] = v;
        for (int nx = x; nx > 1; nx >>= 1) {
            data[y][nx>>1] = op(data[y][nx], data[y][nx^1]);
        }
        for (int ny = y; ny > 1; ny >>= 1) {
            int nx = x;
            data[ny>>1][nx] = op(data[ny][nx], data[ny^1][nx]);
            for (nx >>= 1; nx >= 1; nx >>= 1) {
                data[ny>>1][nx] = op(data[ny>>1][nx<<1], data[ny>>1][nx<<1|1]);
            }
        }
    }

    /// @brief [u, d) × [l, r) の総積を返す / O(log(h)log(w))
    T prod(int u, int d, int l, int r) const {
        assert(0 <= u && u <= d && d <= h);
        assert(0 <= l && l <= r && r <= w);
        u += hs; d += hs;
        l += ws; r += ws;
        T ans = e();

        auto pw = [&] (const vector<T> &dat) -> void {
            int nl = l, nr = r;
            while (nl < nr) {
                if (nl & 1) ans = op(ans, dat[nl++]);
                if (nr & 1) ans = op(ans, dat[--nr]);
                nl >>= 1; nr >>= 1;
            }
        };

        while (u < d) {
            if (u & 1) pw(data[u++]);
            if (d & 1) pw(data[--d]);
            u >>= 1; d >>= 1;
        }
        return ans;
    }
};
} // namespace titan23
