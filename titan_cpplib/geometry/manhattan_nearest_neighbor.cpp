/// https://github.com/titan-23/Library_cpp/blob/main/titan_cpplib/geometry/manhattan_nearest_neighbor.cpp
#pragma once

#include <algorithm>
#include <array>
#include <bit>
#include <cstdint>
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
 * `add(id)` で点を有効化し、`remove(id)` で無効化できる
 * `add_all()` で全点を一括して有効化できる
 * `nearest` で全体の最近傍を、
 * `nearest_four` で閉じた4象限それぞれの最近傍を取得できる
 * 同距離ではIDが小さい点を返し、該当する点がなければ `-1` を返す
 *
 * 方向の順番は NE, NW, SW, SE である
 * 象限は境界を含むため、軸上の点やクエリと同じ座標の点は複数方向の答えになり得る
 *
 * 構築 O(n log^2(n))、一括追加 O(n log(n))
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

        int prod(int off, int m, int left, int right, int k) const {
            int ans = INF;
            left += m;
            right += m;
            while (left < right) {
                if (left & 1) ans = min(ans, seg[off + left++][k]);
                if (right & 1) ans = min(ans, seg[off + --right][k]);
                left >>= 1;
                right >>= 1;
            }
            return ans;
        }

        array<int, 2> split_prod(int off, int m, int pos) const {
            array<int, 2> ans = {INF, INF};
            int node = 1;
            int left = 0;
            int right = m;
            while (true) {
                if (pos == left) {
                    ans[0] = min(ans[0], seg[off + node][0]);
                    break;
                }
                if (pos == right) {
                    ans[1] = min(ans[1], seg[off + node][1]);
                    break;
                }
                int mid = (left + right) >> 1;
                if (pos < mid) {
                    ans[0] = min(ans[0], seg[off + node * 2 + 1][0]);
                    node *= 2;
                    right = mid;
                } else {
                    ans[1] = min(ans[1], seg[off + node * 2][1]);
                    node = node * 2 + 1;
                    left = mid;
                }
            }
            return ans;
        }

    public:
        PrefixMin2D() = default;

        void build(int size, const vector<pair<int, int>>& ps) {
            n = size;
            vector<vector<pair<int, int>>> bucket(n + 1);
            leaf_off.assign(ps.size() + 1, 0);
            int psize = ps.size();
            for (int idx = 0; idx < psize; ++idx) {
                auto [x, y] = ps[idx];
                int cnt = 0;
                for (int i = x; i <= n; i += i & -i) {
                    bucket[i].push_back({y, idx});
                    ++cnt;
                }
                leaf_off[idx + 1] = leaf_off[idx] + cnt;
            }

            key_off.assign(n + 2, 0);
            seg_off.assign(n + 2, 0);
            for (int i = 1; i <= n; ++i) {
                auto& v = bucket[i];
                sort(v.begin(), v.end());
                int m = v.size();
                int cap = bit_ceil(static_cast<unsigned>(m));
                if (cap > m + m / 8) cap = m;
                key_off[i + 1] = key_off[i] + m;
                seg_off[i + 1] = seg_off[i] + cap * 2;
            }

            keys.resize(key_off[n + 1]);
            leaf_pos.resize(leaf_off.back());
            vector<int> next = leaf_off;
            for (int i = 1; i <= n; ++i) {
                int m = bucket[i].size();
                for (int pos = 0; pos < m; ++pos) {
                    auto [y, idx] = bucket[i][pos];
                    keys[key_off[i] + pos] = y;
                    leaf_pos[next[idx]++] = pos;
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
                    seg[off + node][0] = min(seg[off + node * 2][0], seg[off + node * 2 + 1][0]);
                    seg[off + node][1] = min(seg[off + node * 2][1], seg[off + node * 2 + 1][1]);
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

    vector<Point> points; vector<array<int, 2>> rank; vector<uint8_t> active; vector<T> xs, ys;
    vector<array<int, DIRECTION_COUNT>> prio; array<vector<int>, DIRECTION_COUNT> order;
    array<PrefixMin2D, SIDE_COUNT> trees;
    int active_count = 0;
    bool full_built = false, full_ready = false;

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
        auto east = full_ready ? trees[EAST].prod_full(xge, ylt, yle) : trees[EAST].prod(xge, ylt, yle);
        auto west = full_ready ? trees[WEST].prod_full(xle, ylt, yle) : trees[WEST].prod(xle, ylt, yle);
        array<int, DIRECTION_COUNT> p = {east[0], west[0], west[1], east[1]};
        for (int dir = 0; dir < DIRECTION_COUNT; ++dir) ans[dir] = p[dir] < 0 ? -1 : order[dir][p[dir]];
        return ans;
    }

    static T absolute(T value) {
        return value < 0 ? -value : value;
    }

    T distance(int idx, T x, T y) const {
        return absolute(points[idx].x - x) + absolute(points[idx].y - y);
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

        vector<pair<int, int>> rs(points.size());
        for (int side = 0; side < SIDE_COUNT; ++side) {
            for (int i = 0; i < n; ++i) {
                rs[i] = {x_rank(i, side), rank[i][1]};
            }
            trees[side].build(xs.size(), rs);
        }
        active.assign(points.size(), false);
    }

public:
    ManhattanNearest() { build(); }

    explicit ManhattanNearest(vector<Point> ps) : points(move(ps)) { build(); }

    /// 事前登録した点を有効化する
    /// 同じIDを複数回追加しても2回目以降は何もしない
    void add(int id) {
        int n = points.size();
        if (id < 0 || id >= n) {
            throw out_of_range("ManhattanNearest::add: point ID is out of range");
        }
        if (active[id]) return;
        active[id] = true;
        ++active_count;

        trees[EAST].set(x_rank(id, EAST), id, prio[id][NE], prio[id][SE]);
        trees[WEST].set(x_rank(id, WEST), id, prio[id][NW], prio[id][SW]);
        if (active_count == n && full_built) full_ready = true;
    }

    /// 事前登録した全点を有効化する
    void add_all() {
        int n = points.size();
        trees[EAST].set_all(n, prio, NE, SE, [&](int idx) { return x_rank(idx, EAST); });
        trees[WEST].set_all(n, prio, NW, SW, [&](int idx) { return x_rank(idx, WEST); });
        active.assign(n, true);
        active_count = n;
        full_built = true;
        full_ready = true;
    }

    /// 追加済みの点を無効化する
    /// 未追加のIDを指定した場合は何もしない
    void remove(int id) {
        int n = points.size();
        if (id < 0 || id >= n) {
            throw out_of_range("ManhattanNearest::remove: point ID is out of range");
        }
        if (!active[id]) return;
        active[id] = false;
        --active_count;
        full_ready = false;

        trees[EAST].set(x_rank(id, EAST), id, -1, -1);
        trees[WEST].set(x_rank(id, WEST), id, -1, -1);
    }

    /// 追加済みの全点からManhattan距離最小の点のIDを返す
    /// 点がなければ -1
    int nearest(T x, T y) const {
        auto ids = nearest_four_idx(x, y);
        int ans = -1;
        T best = 0;
        for (int idx : ids) {
            if (idx == -1) continue;
            T dist = distance(idx, x, y);
            if (ans == -1 || dist < best || (dist == best && idx < ans)) {
                ans = idx;
                best = dist;
            }
        }
        return ans;
    }

    /// NE, NW, SW, SE の各閉象限でManhattan距離最小の点のIDを返す
    array<int, DIRECTION_COUNT> nearest_four(T x, T y) const {
        return nearest_four_idx(x, y);
    }
};
}  // namespace titan23
