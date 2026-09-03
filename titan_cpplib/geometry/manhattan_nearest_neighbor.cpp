/// https://github.com/titan-23/Library_cpp/blob/main/titan_cpplib/geometry/manhattan_nearest_neighbor.cpp
#pragma once

#include <algorithm>
#include <array>
#include <bit>
#include <limits>
#include <stdexcept>
#include <utility>
#include <vector>

using namespace std;

namespace titan23 {

/**
 * @brief 点を有効化・無効化できる2次元Manhattan最近傍探索
 *
 * コンストラクタには、将来追加する可能性がある点をすべて渡す
 * 点IDはコンストラクタへ渡したvectorのindexとする
 * `add(id)` で点を1個追加し、`remove(id)` で1個削除できる
 * 同じIDを複数個保持でき、個数が正なら検索対象になる
 * `add_all()` で個数0の全点を1個にできる
 * `nearest` で全体の最近傍を、
 * `nearest_four` で閉じた4象限それぞれの最近傍を取得できる
 * 同距離ではIDが小さい点を返し、該当する点がなければ `-1` を返す
 *
 * 方向の順番は NE, NW, SW, SE である
 * 象限は境界を含むため、軸上の点やクエリと同じ座標の点は複数方向の答えになり得る
 *
 * 構築・一括追加 O(n log(n))
 * 追加・削除・検索 O(log^2(n))、メモリ O(n log(n))
 */
template<typename T>
class ManhattanNearest {
public:
    struct Point { T x, y; };

    enum Direction : int { NE = 0, NW = 1, SW = 2, SE = 3 };

    static constexpr int DIRECTION_COUNT = 4;

private:
    class PrefixMin2D {
    private:
        static constexpr int INF = numeric_limits<int>::max();
        int n = 0;
        vector<int> key_off, seg_off, leaf_off, keys, leaf_pos;
        vector<array<int, 2>> seg, full;
        bool full_ready = false;

        int prod(int off, int m, int l, int r, int k) const {
            int ans = INF;
            l += m;
            r += m;
            while (l < r) {
                if (l & 1) ans = min(ans, seg[off + l++][k]);
                if (r & 1) ans = min(ans, seg[off + --r][k]);
                l >>= 1;
                r >>= 1;
            }
            return ans;
        }

        array<int, 2> split_prod(int off, int m, int pos) const {
            array<int, 2> ans = {INF, INF};
            int node = 1;
            int l = 0;
            int r = m;
            while (true) {
                if (pos == l) {
                    ans[0] = min(ans[0], seg[off + node][0]);
                    break;
                }
                if (pos == r) {
                    ans[1] = min(ans[1], seg[off + node][1]);
                    break;
                }
                int mid = (l + r) >> 1;
                if (pos < mid) {
                    ans[0] = min(ans[0], seg[off + node * 2 + 1][0]);
                    node *= 2;
                    r = mid;
                } else {
                    ans[1] = min(ans[1], seg[off + node * 2][1]);
                    node = node * 2 + 1;
                    l = mid;
                }
            }
            return ans;
        }

    public:
        PrefixMin2D() = default;

        template<class GetX>
        void build(int size, const vector<array<int, 2>>& r, const vector<int>& yo, GetX get_x) {
            n = size;
            leaf_off.assign(r.size() + 1, 0);
            int psize = r.size();
            vector<int> len(n + 1, 0);
            for (int idx = 0; idx < psize; ++idx) {
                int x = get_x(idx);
                int cnt = 0;
                for (int i = x; i <= n; i += i & -i) {
                    ++len[i];
                    ++cnt;
                }
                leaf_off[idx + 1] = cnt;
            }
            for (int i = 1; i <= psize; ++i) leaf_off[i] += leaf_off[i - 1];

            key_off.assign(n + 2, 0);
            seg_off.assign(n + 2, 0);
            for (int i = 1; i <= n; ++i) {
                int m = len[i];
                int cap = bit_ceil(static_cast<unsigned>(m));
                if (cap > m + m / 8) cap = m;
                key_off[i + 1] = key_off[i] + m;
                seg_off[i + 1] = seg_off[i] + cap * 2;
            }

            keys.resize(key_off[n + 1]);
            leaf_pos.resize(leaf_off.back());
            vector<int> next(n + 1, 0), leaf = leaf_off;
            for (int idx : yo) {
                int x = get_x(idx);
                for (int i = x; i <= n; i += i & -i) {
                    int pos = next[i]++;
                    keys[key_off[i] + pos] = r[idx][1];
                    leaf_pos[leaf[idx]++] = pos;
                }
            }
            seg.assign(seg_off[n + 1], {INF, INF});
        }

        void set(int x, int idx, int val0, int val1) {
            int k = leaf_off[idx];
            array<int, 2> val = {val0 < 0 ? INF : val0, val1 < 0 ? INF : val1};
            for (int i = x; i <= n; i += i & -i) {
                int m = (seg_off[i + 1] - seg_off[i]) / 2;
                int pos = leaf_pos[k++];
                int off = seg_off[i];
                int node = m + pos;
                seg[off + node] = val;
                for (node >>= 1; node > 0; node >>= 1) {
                    int v0 = min(seg[off + node * 2][0], seg[off + node * 2 + 1][0]);
                    int v1 = min(seg[off + node * 2][1], seg[off + node * 2 + 1][1]);
                    if (m >= 1024 && seg[off + node][0] == v0 && seg[off + node][1] == v1) break;
                    seg[off + node] = {v0, v1};
                }
            }
        }

        template<class GetX>
        void set_all(int size, const vector<array<int, DIRECTION_COUNT>>& prio, int dir0, int dir1, GetX get_x) {
            for (int idx = 0; idx < size; ++idx) {
                int k = leaf_off[idx];
                for (int i = get_x(idx); i <= n; i += i & -i) {
                    int m = (seg_off[i + 1] - seg_off[i]) / 2;
                    seg[seg_off[i] + m + leaf_pos[k++]] = {prio[idx][dir0], prio[idx][dir1]};
                }
            }
            for (int i = 1; i <= n; ++i) {
                int m = (seg_off[i + 1] - seg_off[i]) / 2;
                int off = seg_off[i];
                for (int node = m - 1; node > 0; --node) {
                    seg[off + node][0] = min(seg[off + node * 2][0], seg[off + node * 2 + 1][0]);
                    seg[off + node][1] = min(seg[off + node * 2][1], seg[off + node * 2 + 1][1]);
                }
            }
            if (full_ready) return;
            full.resize(keys.size());
            for (int i = 1; i <= n; ++i) {
                int koff = key_off[i];
                int m = key_off[i + 1] - koff;
                int cap = (seg_off[i + 1] - seg_off[i]) / 2;
                int off = seg_off[i] + cap;
                int best = INF;
                for (int pos = 0; pos < m; ++pos) {
                    best = min(best, seg[off + pos][1]);
                    full[koff + pos][1] = best;
                }
                best = INF;
                for (int pos = m - 1; pos >= 0; --pos) {
                    best = min(best, seg[off + pos][0]);
                    full[koff + pos][0] = best;
                }
            }
            full_ready = true;
        }

        array<int, 2> prod(int x, int ylt, int yle) const {
            array<int, 2> ans = {INF, INF};
            for (int i = x; i > 0; i -= i & -i) {
                int next = i - (i & -i);
                if (next > 0) {
                    int first = key_off[next];
                    int last = key_off[next + 1];
                    __builtin_prefetch(keys.data() + ((first + last) >> 1));
                }
                int koff = key_off[i];
                int m = key_off[i + 1] - koff;
                auto first = keys.begin() + koff;
                auto last = first + m;
                int lo = upper_bound(first, last, ylt) - first;
                int hi = ylt == yle ? lo : upper_bound(first, last, yle) - first;
                int off = seg_off[i];
                int cap = (seg_off[i + 1] - off) / 2;
                if (has_single_bit(static_cast<unsigned>(cap))) {
                    auto val = split_prod(off, cap, lo);
                    ans[0] = min(ans[0], val[0]);
                    ans[1] = min(ans[1], val[1]);
                    if (lo < hi) ans[1] = min(ans[1], prod(off, cap, lo, hi, 1));
                } else {
                    ans[0] = min(ans[0], prod(off, cap, lo, cap, 0));
                    ans[1] = min(ans[1], prod(off, cap, 0, hi, 1));
                }
            }
            if (ans[0] == INF) ans[0] = -1;
            if (ans[1] == INF) ans[1] = -1;
            return ans;
        }

        array<int, 2> prod_full(int x, int ylt, int yle) const {
            array<int, 2> ans = {INF, INF};
            for (int i = x; i > 0; i -= i & -i) {
                int next = i - (i & -i);
                if (next > 0) {
                    int first = key_off[next];
                    int last = key_off[next + 1];
                    __builtin_prefetch(keys.data() + ((first + last) >> 1));
                }
                int koff = key_off[i];
                int m = key_off[i + 1] - koff;
                auto first = keys.begin() + koff;
                auto last = first + m;
                int lo = upper_bound(first, last, ylt) - first;
                int hi = ylt == yle ? lo : upper_bound(first, last, yle) - first;
                if (lo < m) ans[0] = min(ans[0], full[koff + lo][0]);
                if (hi > 0) ans[1] = min(ans[1], full[koff + hi - 1][1]);
            }
            if (ans[0] == INF) ans[0] = -1;
            if (ans[1] == INF) ans[1] = -1;
            return ans;
        }

    };

    enum Side : int { EAST = 0, WEST = 1 };
    static constexpr int SIDE_COUNT = 2;

    vector<Point> points; vector<array<int, 2>> rank; vector<int> cnt; vector<T> xs, ys;
    vector<array<int, DIRECTION_COUNT>> prio; array<vector<int>, DIRECTION_COUNT> order;
    array<PrefixMin2D, SIDE_COUNT> trees;
    int used = 0;
    bool cached = false, all = false;

    int x_rank(int idx, int side) const {
        int r = rank[idx][0];
        return side == EAST ? xs.size() + 1 - r : r;
    }

    array<int, DIRECTION_COUNT> nearest_four_idx(T x, T y) const {
        array<int, DIRECTION_COUNT> ans{};
        auto xi = lower_bound(xs.begin(), xs.end(), x);
        auto yi = lower_bound(ys.begin(), ys.end(), y);
        int xlt = xi - xs.begin();
        int ylt = yi - ys.begin();
        int xle = xlt + (xi != xs.end() && *xi == x);
        int yle = ylt + (yi != ys.end() && *yi == y);
        int xge = xs.size() - xlt;
        auto east = all ? trees[EAST].prod_full(xge, ylt, yle) : trees[EAST].prod(xge, ylt, yle);
        auto west = all ? trees[WEST].prod_full(xle, ylt, yle) : trees[WEST].prod(xle, ylt, yle);
        array<int, DIRECTION_COUNT> p = {east[0], west[0], west[1], east[1]};
        for (int dir = 0; dir < DIRECTION_COUNT; ++dir) ans[dir] = p[dir] < 0 ? -1 : order[dir][p[dir]];
        return ans;
    }

    pair<int, T> nearest_pair(T x, T y) const {
        auto ids = nearest_four_idx(x, y);
        int ans = -1;
        T best = 0;
        T u = x + y;
        T v = x - y;
        for (int dir = 0; dir < DIRECTION_COUNT; ++dir) {
            int idx = ids[dir];
            if (idx == -1) continue;
            const Point& p = points[idx];
            T z = dir == NE || dir == SW ? p.x + p.y : p.x - p.y;
            T dist;
            if (dir == NE) dist = z - u;
            else if (dir == NW) dist = v - z;
            else if (dir == SW) dist = u - z;
            else dist = z - v;
            if (ans == -1 || dist < best || (dist == best && idx < ans)) {
                ans = idx;
                best = dist;
            }
        }
        return {ans, best};
    }

    void build_priority() {
        int n = points.size();
        vector<array<T, 2>> key(n);
        for (int i = 0; i < n; ++i) {
            T x = points[i].x;
            T y = points[i].y;
            key[i] = {x + y, x - y};
        }

        prio.resize(n);
        auto build_pair = [&](int asc, int desc, int k) {
            auto& a = order[asc];
            auto& b = order[desc];
            a.resize(n);
            b.resize(n);
            for (int i = 0; i < n; ++i) a[i] = i;
            sort(a.begin(), a.end(), [&](int i, int j) {
                if (key[i][k] != key[j][k]) return key[i][k] < key[j][k];
                return i < j;
            });
            int p = 0;
            for (int r = n; r > 0;) {
                int l = r - 1;
                while (l > 0 && key[a[l - 1]][k] == key[a[r - 1]][k]) --l;
                for (int i = l; i < r; ++i) b[p++] = a[i];
                r = l;
            }
            for (int i = 0; i < n; ++i) {
                prio[a[i]][asc] = i;
                prio[b[i]][desc] = i;
            }
        };
        build_pair(NE, SW, 0);
        build_pair(SE, NW, 1);
    }

    void build() {
        int n = points.size();
        xs.reserve(points.size());
        ys.reserve(points.size());

        for (int i = 0; i < n; ++i) {
            const Point& p = points[i];
            xs.push_back(p.x);
            ys.push_back(p.y);
        }

        sort(xs.begin(), xs.end());
        xs.erase(unique(xs.begin(), xs.end()), xs.end());
        sort(ys.begin(), ys.end());
        ys.erase(unique(ys.begin(), ys.end()), ys.end());

        rank.resize(points.size());
        for (int i = 0; i < n; ++i) {
            auto xi = lower_bound(xs.begin(), xs.end(), points[i].x);
            auto yi = lower_bound(ys.begin(), ys.end(), points[i].y);
            rank[i][0] = xi - xs.begin() + 1;
            rank[i][1] = yi - ys.begin() + 1;
        }
        build_priority();

        int yn = ys.size();
        vector<int> yo(n), off(yn + 1, 0);
        for (int i = 0; i < n; ++i) ++off[rank[i][1]];
        for (int i = 1; i <= yn; ++i) off[i] += off[i - 1];
        for (int i = n - 1; i >= 0; --i) yo[--off[rank[i][1]]] = i;

        for (int side = 0; side < SIDE_COUNT; ++side) {
            trees[side].build(xs.size(), rank, yo, [&](int idx) { return x_rank(idx, side); });
        }
        cnt.assign(points.size(), 0);
    }

public:
    ManhattanNearest() { build(); }

    explicit ManhattanNearest(vector<Point> ps) : points(move(ps)) { build(); }

    /// 事前登録した点を1個追加する
    void add(int id) {
        int n = points.size();
        if (id < 0 || id >= n) {
            throw out_of_range("ManhattanNearest::add: point ID is out of range");
        }
        if (cnt[id]++ > 0) return;
        ++used;

        trees[EAST].set(x_rank(id, EAST), id, prio[id][NE], prio[id][SE]);
        trees[WEST].set(x_rank(id, WEST), id, prio[id][NW], prio[id][SW]);
        if (used == n && cached) all = true;
    }

    /// 個数0の全点を1個にする
    void add_all() {
        int n = points.size();
        trees[EAST].set_all(n, prio, NE, SE, [&](int idx) { return x_rank(idx, EAST); });
        trees[WEST].set_all(n, prio, NW, SW, [&](int idx) { return x_rank(idx, WEST); });
        for (int& c : cnt) {
            if (c == 0) c = 1;
        }
        used = n;
        cached = true;
        all = true;
    }

    /// 追加済みの点を1個削除する
    /// 個数0のIDを指定した場合は何もしない
    void remove(int id) {
        int n = points.size();
        if (id < 0 || id >= n) {
            throw out_of_range("ManhattanNearest::remove: point ID is out of range");
        }
        if (cnt[id] == 0) return;
        if (--cnt[id] > 0) return;
        --used;
        all = false;

        trees[EAST].set(x_rank(id, EAST), id, -1, -1);
        trees[WEST].set(x_rank(id, WEST), id, -1, -1);
    }

    /// 追加済みの全点からManhattan距離最小の点のIDを返す
    /// 点がなければ -1
    int nearest(T x, T y) const {
        return nearest_pair(x, y).first;
    }

    /// 追加済みの全点との最小Manhattan距離を返す
    /// 点がなければ -1
    T nearest_dist(T x, T y) const {
        auto res = nearest_pair(x, y);
        return res.first < 0 ? -1 : res.second;
    }

    /// NE, NW, SW, SE の各閉象限でManhattan距離最小の点のIDを返す
    array<int, DIRECTION_COUNT> nearest_four(T x, T y) const {
        return nearest_four_idx(x, y);
    }
};
}  // namespace titan23
