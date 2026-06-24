#pragma once

#include <vector>
#include <string>
#include <cstdint>
#include <cassert>
#include <utility>
#include <ostream>
using namespace std;

namespace titan23 {

/// @brief グリッドの 1 行を 1 つの Word で持つ BitBoard W <= sizeof(Word)*8 が前提
///        壁/道の盤面に対し flood, BFS, 連結成分などをビット並列で行う
///        Word は uint64_t (W<=64) か __uint128_t (W<=128) を使う
///
/// マスク(Set)は長さ H の Word 配列で 1 ビットが 1 セルに対応する
/// 結果は new_set() で用意した out バッファに書き込み メソッドは中で再確保しない
/// 探索系は内部バッファを共有するため同じインスタンスの並列実行は不可
///
/// 主なメソッド
///   from_grid(grid, '#')        grid から壁/道を読む
///   flood(sources, out)         多始点到達集合
///   bfs_dist(sources, dist)     多始点最短距離 dist[r*W+c] 未到達 -1
///   bfs_nearest(srcs, dist, id) 最短距離と最近傍始点番号
///   connected(a, b)             2 点が連結か
///   components() / label(lab)   連結成分の数 / 番号付け
///   largest_component(out)      最大連結成分
///   expand / border / shift     近傍展開 / 外周 / 平行移動
///   band/bor/bxor/bdiff ほか    マスクの集合演算
template<class Word>
class Bitboard {
public:
    using Set = vector<Word>;

private:
    int H, W;
    Word FULL;
    Set road;
    mutable Set frontier, next, seen, comp;

    int word_bits() const { return (int)sizeof(Word) * 8; }

    Word lowmask(int w) const {
        if (w >= word_bits()) return ~Word(0);
        return (Word(1) << w) - 1;
    }

    Word bit(int c) const { return Word(1) << c; }

    static constexpr int DR[4] = {0, 0, 1, -1};
    static constexpr int DC[4] = {1, -1, 0, 0};
    // static constexpr int DR[8] = {0, 0, 1, -1, 1, 1, -1, -1};
    // static constexpr int DC[8] = {1, -1, 0, 0, 1, -1, 1, -1};

    // 4 近傍
    void expand_into(const Set &fr, Set &out) const {
        for (int r = 0; r < H; ++r) {
            Word f = fr[r];

            // 4 近傍
            Word x = ((f << 1) | (f >> 1)) & FULL;
            if (r > 0)     x |= fr[r - 1];
            if (r + 1 < H) x |= fr[r + 1];

            // 8 近傍
            // Word x = ((f << 1) | (f >> 1)) & FULL;
            // if (r > 0)     { Word u = fr[r - 1]; x |= ((u << 1) | u | (u >> 1)) & FULL; }
            // if (r + 1 < H) { Word d = fr[r + 1]; x |= ((d << 1) | d | (d >> 1)) & FULL; }

            out[r] = x & road[r];
        }
    }

    void flood_point_into(int r0, int c0, Set &out) const {
        clear(out);
        for (int r = 0; r < H; ++r) frontier[r] = Word(0);
        if (!is_road(r0, c0)) return;
        out[r0] |= bit(c0);
        frontier[r0] |= bit(c0);
        while (true) {
            expand_into(frontier, next);
            bool changed = false;
            for (int r = 0; r < H; ++r) {
                Word add = next[r] & ~out[r];
                if (add) { out[r] |= add; frontier[r] = add; changed = true; }
                else frontier[r] = Word(0);
            }
            if (!changed) break;
        }
    }

public:
    /// @brief 立っているビット数
    int popcount(Word x) const {
        if constexpr (sizeof(Word) <= 8) {
            return __builtin_popcountll((uint64_t)x);
        } else {
            return __builtin_popcountll((uint64_t)x) + __builtin_popcountll((uint64_t)(x >> 64));
        }
    }

    /// @brief 最下位の立っているビットの位置 x != 0 が前提
    int ctz(Word x) const {
        if constexpr (sizeof(Word) <= 8) {
            return __builtin_ctzll((uint64_t)x);
        } else {
            uint64_t lo = (uint64_t)x;
            if (lo) return __builtin_ctzll(lo);
            return 64 + __builtin_ctzll((uint64_t)(x >> 64));
        }
    }

    Bitboard() : H(0), W(0), FULL(0) {}
    Bitboard(int h, int w) { resize(h, w); }

    /// @brief 盤面を h*w に作り直し、全セルを道にする
    void resize(int h, int w) {
        assert(w <= word_bits());
        H = h; W = w; FULL = lowmask(w);
        road.assign(h, FULL);
        frontier.assign(h, Word(0));
        next.assign(h, Word(0));
        seen.assign(h, Word(0));
        comp.assign(h, Word(0));
    }

    int height() const { return H; }
    int width() const { return W; }

    /// @brief 道セルのマスク
    const Set &road_set() const { return road; }

    /// @brief 全ビット 0 のマスクを作る バッファ準備用
    Set new_set() const { return Set(H, Word(0)); }

    /// @brief 全セルを道にする
    void clear() {
        for (int r = 0; r < H; ++r) road[r] = FULL;
    }

    /// @brief 全セルを壁にする
    void fill() {
        for (int r = 0; r < H; ++r) road[r] = Word(0);
    }

    /// @brief grid の wall_ch を壁、それ以外を道として読み取る
    void from_grid(const vector<string> &grid, char wall_ch = '#') {
        assert(grid.size() == (size_t)H);
        for (int r = 0; r < H; ++r) {
            assert(grid[r].size() == (size_t)W);
            Word row = FULL;
            for (int c = 0; c < W; ++c) {
                if (grid[r][c] == wall_ch) row &= ~bit(c);
            }
            road[r] = row;
        }
    }

    /// @brief (r,c) を道にする
    void set_road(int r, int c) { road[r] |= bit(c); }
    /// @brief (r,c) を壁にする
    void set_wall(int r, int c) { road[r] &= ~bit(c); }
    /// @brief (r,c) が道か
    bool is_road(int r, int c) const { return (road[r] >> c) & 1; }

    /// @brief 盤面を out に退避する
    void snapshot(Set &out) const { out = road; }

    /// @brief 退避した盤面を戻す
    void restore(const Set &snap) { road = snap; }

    /// @brief 盤面が一致するか H, W, 壁配置すべて
    bool operator==(const Bitboard &o) const { return H == o.H && W == o.W && road == o.road; }
    /// @brief 盤面が異なるか
    bool operator!=(const Bitboard &o) const { return !(*this == o); }

    /// @brief デバッグ用 盤面を os に出力する 壁=# 道=.
    friend ostream &operator<<(ostream &os, const Bitboard &b) {
        for (int r = 0; r < b.H; ++r) {
            for (int c = 0; c < b.W; ++c) {
                os << (b.is_road(r, c) ? '.' : '#');
            }
            os << '\n';
        }
        return os;
    }

    /// @brief マスクを空にする
    void clear(Set &s) const {
        for (Word &x : s) x = Word(0);
    }
    /// @brief (r,c) が立っているか
    bool test(const Set &s, int r, int c) const { return (s[r] >> c) & 1; }
    /// @brief (r,c) を立てる
    void set(Set &s, int r, int c) const   { s[r] |= bit(c); }
    /// @brief (r,c) を落とす
    void reset(Set &s, int r, int c) const { s[r] &= ~bit(c); }
    /// @brief (r,c) を反転する
    void flip(Set &s, int r, int c) const  { s[r] ^= bit(c); }

    /// @brief 立っているセル数
    int count(const Set &s) const {
        int res = 0;
        for (Word x : s) res += popcount(x);
        return res;
    }
    /// @brief 立っているセルが 1 つ以上あるか
    bool any(const Set &s) const {
        for (Word x : s) if (x) return true;
        return false;
    }
    /// @brief 空か
    bool none(const Set &s) const { return !any(s); }
    /// @brief a と b が一致するか
    bool equals(const Set &a, const Set &b) const {
        for (int i = 0; i < H; ++i) if (a[i] != b[i]) return false;
        return true;
    }

    /// @brief 積 a & b を out に書く
    void band(const Set &a, const Set &b, Set &out) const {
        for (int i = 0; i < H; ++i) out[i] = a[i] & b[i];
    }
    /// @brief 和 a | b を out に書く
    void bor(const Set &a, const Set &b, Set &out) const {
        for (int i = 0; i < H; ++i) out[i] = a[i] | b[i];
    }
    /// @brief 対称差 a ^ b を out に書く
    void bxor(const Set &a, const Set &b, Set &out) const {
        for (int i = 0; i < H; ++i) out[i] = a[i] ^ b[i];
    }
    /// @brief 差 a \ b を out に書く
    void bdiff(const Set &a, const Set &b, Set &out) const {
        for (int i = 0; i < H; ++i) out[i] = a[i] & ~b[i];
    }
    /// @brief a &= b
    void iand(Set &a, const Set &b) const {
        for (int i = 0; i < H; ++i) a[i] &= b[i];
    }
    /// @brief a |= b
    void ior(Set &a, const Set &b) const {
        for (int i = 0; i < H; ++i) a[i] |= b[i];
    }
    /// @brief a ^= b
    void ixor(Set &a, const Set &b) const {
        for (int i = 0; i < H; ++i) a[i] ^= b[i];
    }
    /// @brief a \= b
    void idiff(Set &a, const Set &b) const {
        for (int i = 0; i < H; ++i) a[i] &= ~b[i];
    }
    /// @brief 道セル内での補集合 road & ~a を out に書く
    void complement_into(const Set &a, Set &out) const {
        for (int r = 0; r < H; ++r) out[r] = road[r] & ~a[r];
    }

    /// @brief cells を立てたマスクを out に書く 壁上のセルは落とす
    void make_set(const vector<pair<int,int>> &cells, Set &out) const {
        clear(out);
        for (auto [r, c] : cells) {
            assert(0 <= r && r < H && 0 <= c && c < W);
            out[r] |= bit(c) & road[r];
        }
    }

    /// @brief 立っているセルを (r,c) で列挙して f(r,c) を呼ぶ
    template<class F>
    void for_each(const Set &s, F &&f) const {
        for (int r = 0; r < H; ++r)
            for (Word x = s[r]; x; x &= x - 1) f(r, ctz(x));
    }
    /// @brief 立っているセルを out に集める
    void cells(const Set &s, vector<pair<int,int>> &out) const {
        out.clear();
        for_each(s, [&](int r, int c) { out.emplace_back(r, c); });
    }

    /// @brief s を (dr,dc) 平行移動して out に書く 壁は考慮しない dr は -1,0,1
    void shift(const Set &s, int dr, int dc, Set &out) const {
        if (dc <= -word_bits() || dc >= word_bits()) { clear(out); return; }
        for (int r = 0; r < H; ++r) {
            int sr = r - dr;
            if (sr < 0 || sr >= H) { out[r] = Word(0); continue; }
            Word w = s[sr];
            if (dc >= 0) w <<= dc; else w >>= (-dc);
            out[r] = w & FULL;
        }
    }
    /// @brief shift してから道セルでマスクする (1 マス移動)
    void step(const Set &s, int dr, int dc, Set &out) const {
        shift(s, dr, dc, out);
        iand(out, road);
    }

    /// @brief s を道セル内で 4 近傍に 1 ステップ拡張して out に書く
    void expand(const Set &s, Set &out) const { expand_into(s, out); }

    /// @brief s に隣接する道セル (s 自身は除く) を out に書く
    void border(const Set &s, Set &out) const {
        expand_into(s, out);
        for (int r = 0; r < H; ++r) out[r] &= ~s[r];
    }

    /// @brief sources からの到達集合を out に書く
    void flood(const vector<pair<int,int>> &sources, Set &out) const {
        make_set(sources, out);
        for (int r = 0; r < H; ++r) frontier[r] = out[r];
        while (true) {
            expand_into(frontier, next);
            bool changed = false;
            for (int r = 0; r < H; ++r) {
                Word add = next[r] & ~out[r];
                if (add) { out[r] |= add; frontier[r] = add; changed = true; }
                else frontier[r] = Word(0);
            }
            if (!changed) break;
        }
    }

    /// @brief max_steps 回までの拡張に限った到達集合を out に書く
    void flood_limited(const vector<pair<int,int>> &sources, int max_steps, Set &out) const {
        make_set(sources, out);
        for (int r = 0; r < H; ++r) frontier[r] = out[r];
        for (int step_i = 0; step_i < max_steps; ++step_i) {
            expand_into(frontier, next);
            bool changed = false;
            for (int r = 0; r < H; ++r) {
                Word add = next[r] & ~out[r];
                if (add) { out[r] |= add; frontier[r] = add; changed = true; }
                else frontier[r] = Word(0);
            }
            if (!changed) break;
        }
    }

    /// @brief a と b が連結か
    bool connected(pair<int,int> a, pair<int,int> b) const {
        flood_point_into(a.first, a.second, comp);
        return test(comp, b.first, b.second);
    }

    /// @brief 多始点最短距離を dist (長さ H*W, index は r*W+c) に書く 未到達は -1
    void bfs_dist(const vector<pair<int,int>> &sources, vector<int> &dist) const {
        dist.assign(H * W, -1);
        make_set(sources, seen);
        for (int r = 0; r < H; ++r) {
            frontier[r] = seen[r];
            for (Word x = seen[r]; x; x &= x - 1) dist[r * W + ctz(x)] = 0;
        }
        int layer = 0;
        while (true) {
            expand_into(frontier, next);
            bool changed = false;
            ++layer;
            for (int r = 0; r < H; ++r) {
                Word add = next[r] & ~seen[r];
                if (!add) { frontier[r] = Word(0); continue; }
                seen[r] |= add;
                frontier[r] = add;
                changed = true;
                for (Word x = add; x; x &= x - 1) dist[r * W + ctz(x)] = layer;
            }
            if (!changed) break;
        }
    }

    /// @brief 多始点最短距離 dist と最近傍始点番号 src を書く 同距離は番号最小を選ぶ 未到達は -1
    void bfs_nearest(const vector<pair<int,int>> &sources, vector<int> &dist, vector<int> &src) const {
        dist.assign(H * W, -1);
        src.assign(H * W, -1);
        clear(seen);
        int ns = sources.size();
        for (int i = 0; i < ns; ++i) {
            auto [r, c] = sources[i];
            if (!is_road(r, c)) continue;
            int idx = r * W + c;
            if (dist[idx] == -1) { dist[idx] = 0; src[idx] = i; seen[r] |= bit(c); }
        }
        for (int r = 0; r < H; ++r) frontier[r] = seen[r];
        int dirs = 4;
        int layer = 0;
        while (true) {
            expand_into(frontier, next);
            bool changed = false;
            ++layer;
            for (int r = 0; r < H; ++r) {
                Word add = next[r] & ~seen[r];
                if (!add) { frontier[r] = Word(0); continue; }
                seen[r] |= add;
                frontier[r] = add;
                changed = true;
                for (Word x = add; x; x &= x - 1) {
                    int c = ctz(x);
                    int idx = r * W + c;
                    dist[idx] = layer;
                    int best = -1;
                    for (int k = 0; k < dirs; ++k) {
                        int nr = r + DR[k], nc = c + DC[k];
                        if (nr < 0 || nc < 0 || nr >= H || nc >= W) continue;
                        int nidx = nr * W + nc;
                        if (dist[nidx] != layer - 1) continue;
                        if (best == -1 || src[nidx] < best) best = src[nidx];
                    }
                    src[idx] = best;
                }
            }
            if (!changed) break;
        }
    }

    /// @brief 道セルの連結成分数を返す
    int components() const {
        clear(seen);
        int cnt = 0;
        for (int r = 0; r < H; ++r) {
            for (Word rem = road[r] & ~seen[r]; rem; rem = road[r] & ~seen[r]) {
                flood_point_into(r, ctz(rem), comp);
                ior(seen, comp);
                ++cnt;
            }
        }
        return cnt;
    }

    /// @brief 各道セルに連結成分番号を振る lab は長さ H*W、壁は -1 成分数を返す
    int label(vector<int> &lab) const {
        lab.assign(H * W, -1);
        clear(seen);
        int id = 0;
        for (int r = 0; r < H; ++r) {
            for (Word rem = road[r] & ~seen[r]; rem; rem = road[r] & ~seen[r]) {
                flood_point_into(r, ctz(rem), comp);
                ior(seen, comp);
                int cur = id++;
                for_each(comp, [&](int rr, int cc) { lab[rr * W + cc] = cur; });
            }
        }
        return id;
    }

    /// @brief 最大の連結成分を out に書く セル数を返す
    int largest_component(Set &out) const {
        clear(seen);
        clear(out);
        int best = 0;
        for (int r = 0; r < H; ++r) {
            for (Word rem = road[r] & ~seen[r]; rem; rem = road[r] & ~seen[r]) {
                flood_point_into(r, ctz(rem), comp);
                ior(seen, comp);
                int s = count(comp);
                if (s > best) { best = s; out = comp; }
            }
        }
        return best;
    }
};

using Bitboard64  = Bitboard<uint64_t>;
using Bitboard128 = Bitboard<__uint128_t>;

}  // namespace titan23
