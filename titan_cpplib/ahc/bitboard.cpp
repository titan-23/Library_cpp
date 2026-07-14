#pragma once

#include <vector>
#include <string>
#include <cstdint>
#include <cassert>
#include <utility>
#include <ostream>
#include <algorithm>
using namespace std;

namespace titan23 {

enum class Neighborhood {
    Four,
    Eight,
};

/// @brief グリッドの 1 行を 1 つの Word で持つ BitBoard W <= sizeof(Word)*8 が前提
///        壁/道の盤面に対し flood, BFS, 連結成分などをビット並列で行う
///        Word は uint64_t (W<=64) か __uint128_t (W<=128) を使う
///
/// マスク(Set)は長さ H の Word 配列で 1 ビットが 1 セルに対応する
/// 結果は new_set() で用意した out バッファに書き込み メソッドは中で再確保しない
/// 探索系は内部バッファを共有するため同じインスタンスの並列実行は不可
/// 近傍はテンプレート引数 NB で 4 近傍・8 近傍を選ぶ
///
/// 主なメソッド
///   from_grid(grid, '#')        grid から壁/道を読む
///   flood(sources, out)         多始点到達集合
///   bfs_dist(sources, dist)     多始点最短距離 dist[r*W+c] 未到達 -1
///   bfs_nearest(srcs, dist, id) 最短距離と最近傍始点番号
///   connected(a, b)             2 点が連結か
///   distance(a, b)              2 点間最短距離
///   nearest_in_set(p, s, hit)   集合 s の最寄りセル
///   components() / label(lab)   連結成分の数 / 番号付け
///   largest_component(out)      最大連結成分
///   component(r, c, out)        指定セルの連結成分
///   expand / border / shift     近傍展開 / 外周 / 平行移動
///   dilate / erode              膨張 / 収縮
///   band/bor/bxor/bdiff ほか    マスクの集合演算
template<class Word, Neighborhood NB = Neighborhood::Four>
class Bitboard {
public:
    using Set = vector<Word>;
    static_assert(sizeof(Word) == 8 || sizeof(Word) == 16, "Word must be uint64_t or __uint128_t");
    static_assert(Word(-1) > Word(0), "Word must be unsigned");

private:
    int H, W;
    Word FULL;
    Set road;

    struct Run {
        int l, r;
        int parent;
        int size;
        int id;
    };

    mutable Set frontier, next, seen, comp;
    mutable vector<Run> runs;
    mutable vector<int> row_start;

    static constexpr int word_bits() { return (int)sizeof(Word) * 8; }

    Word lowmask(int w) const {
        if (w >= word_bits()) return ~Word(0);
        return (Word(1) << w) - 1;
    }

    Word bit(int c) const { return Word(1) << c; }

    static uint64_t mix64(uint64_t x) {
        x += 0x9e3779b97f4a7c15ULL;
        x = (x ^ (x >> 30)) * 0xbf58476d1ce4e5b9ULL;
        x = (x ^ (x >> 27)) * 0x94d049bb133111ebULL;
        return x ^ (x >> 31);
    }

    uint64_t hash_word(Word x) const {
        uint64_t h = mix64((uint64_t)x);
        if constexpr (sizeof(Word) > 8) {
            h ^= mix64((uint64_t)(x >> 64) + 0x517cc1b727220a95ULL);
        }
        return h;
    }

    static constexpr int DIRS = NB == Neighborhood::Four ? 4 : 8;
    static constexpr int DR[8] = {0, 0, 1, -1, 1, 1, -1, -1};
    static constexpr int DC[8] = {1, -1, 0, 0, 1, -1, 1, -1};

    bool in_bounds(int r, int c) const {
        return 0 <= r && r < H && 0 <= c && c < W;
    }

    void assert_set(const Set &s) const {
        assert((int)s.size() == H);
    }

    Word vertical_bits(Word x) const {
        if constexpr (NB == Neighborhood::Four) {
            return x;
        } else {
            return ((x << 1) | x | (x >> 1)) & FULL;
        }
    }

    Word eroded_row(const Set &s, int r) const {
        Word x = s[r] & (s[r] << 1) & (s[r] >> 1);
        if (r == 0 || r + 1 == H) return Word(0);
        if constexpr (NB == Neighborhood::Four) {
            x &= s[r - 1] & s[r + 1];
        } else {
            Word u = s[r - 1];
            Word d = s[r + 1];
            x &= u & (u << 1) & (u >> 1);
            x &= d & (d << 1) & (d >> 1);
        }
        return x & FULL;
    }

    // 4 近傍
    void expand_into(const Set &fr, Set &out) const {
        for (int r = 0; r < H; ++r) {
            Word f = fr[r];
            Word x = ((f << 1) | (f >> 1)) & FULL;
            if (r > 0)     x |= vertical_bits(fr[r - 1]);
            if (r + 1 < H) x |= vertical_bits(fr[r + 1]);
            out[r] = x & road[r];
        }
    }

    Word row_fill(Word s, Word mask) const {
        Word g = s & mask;
        Word p = mask;
        for (int sh = 1; sh < W; sh <<= 1) {
            g |= p & (g << sh);
            p &= p << sh;
        }

        Word h = s & mask;
        p = mask;
        for (int sh = 1; sh < W; sh <<= 1) {
            h |= p & (h >> sh);
            p &= p >> sh;
        }
        return g | h;
    }

    void flood_closure_inplace(Set &out) const {
        bool active = false;
        for (Word x : out) active |= x != 0;
        if (!active) return;

        bool changed;
        do {
            changed = false;
            for (int r = 0; r < H; ++r) {
                Word s = out[r];
                if (r > 0) s |= vertical_bits(out[r - 1]);
                Word x = row_fill(s, road[r]);
                changed |= x != out[r];
                out[r] = x;
            }
            for (int r = H - 1; r >= 0; --r) {
                Word s = out[r];
                if (r + 1 < H) s |= vertical_bits(out[r + 1]);
                Word x = row_fill(s, road[r]);
                changed |= x != out[r];
                out[r] = x;
            }
        } while (changed);
    }

    int flood_point_into(int r0, int c0, Set &out) const {
        clear(out);
        if (!is_road(r0, c0)) return 0;
        out[r0] = bit(c0);
        flood_closure_inplace(out);
        return count(out);
    }

    int distance_points(int r0, int c0, int r1, int c1) const {
        if (!is_road(r0, c0) || !is_road(r1, c1)) return -1;
        if (r0 == r1 && c0 == c1) return 0;
        int dr = r0 < r1 ? r1 - r0 : r0 - r1;
        int dc = c0 < c1 ? c1 - c0 : c0 - c1;
        if constexpr (NB == Neighborhood::Four) {
            if (dr + dc == 1) return 1;
        } else {
            if (max(dr, dc) == 1) return 1;
        }

        clear(seen);
        clear(frontier);
        seen[r0] = bit(c0);
        frontier[r0] = bit(c0);
        const Word target = bit(c1);
        int d = 0;
        while (true) {
            expand_into(frontier, next);
            bool changed = false;
            ++d;
            for (int r = 0; r < H; ++r) {
                Word add = next[r] & ~seen[r];
                next[r] = add;
                seen[r] |= add;
                changed |= add != 0;
            }
            if (next[r1] & target) return d;
            if (!changed) return -1;
            frontier.swap(next);
        }
    }

    bool connected_points(int r0, int c0, int r1, int c1) const {
        if (!is_road(r0, c0) || !is_road(r1, c1)) return false;
        if (r0 == r1 && c0 == c1) return true;

        clear(seen);
        clear(frontier);
        seen[r0] = bit(c0);
        frontier[r0] = bit(c0);
        Word target = bit(c1);

        constexpr int LAYER_LIMIT = 4;
        for (int layer = 0; layer < LAYER_LIMIT; ++layer) {
            expand_into(frontier, next);
            bool changed = false;
            for (int r = 0; r < H; ++r) {
                Word add = next[r] & ~seen[r];
                next[r] = add;
                seen[r] |= add;
                changed |= add != 0;
            }
            if (next[r1] & target) return true;
            if (!changed) return false;
            frontier.swap(next);
        }

        flood_closure_inplace(seen);
        return (seen[r1] & target) != 0;
    }

    int find_run(int u) const {
        int root = u;
        while (runs[root].parent != root) root = runs[root].parent;
        while (runs[u].parent != u) {
            int p = runs[u].parent;
            runs[u].parent = root;
            u = p;
        }
        return root;
    }

    void unite_runs(int u, int v) const {
        u = find_run(u);
        v = find_run(v);
        if (u == v) return;
        if (runs[u].size < runs[v].size) swap(u, v);
        runs[v].parent = u;
        runs[u].size += runs[v].size;
    }

    void build_runs() const {
        runs.clear();
        if ((int)row_start.size() != H + 1) row_start.resize(H + 1);

        for (int r = 0; r < H; ++r) {
            row_start[r] = runs.size();
            Word x = road[r];
            while (x) {
                int l = ctz(x);
                Word tail = x >> l;
                Word zeros = ~tail;
                int len = zeros ? ctz(zeros) : word_bits();
                int rr = min(W, l + len);
                int idx = runs.size();
                runs.push_back({l, rr, idx, rr - l, -1});
                x &= ~(lowmask(rr) ^ lowmask(l));
            }
        }
        row_start[H] = runs.size();

        constexpr int DIAG = NB == Neighborhood::Eight ? 1 : 0;
        for (int r = 1; r < H; ++r) {
            int i = row_start[r - 1];
            int i_end = row_start[r];
            int j = row_start[r];
            int j_end = row_start[r + 1];
            while (i < i_end && j < j_end) {
                const Run &a = runs[i];
                const Run &b = runs[j];
                if (a.r + DIAG <= b.l) {
                    ++i;
                } else if (b.r + DIAG <= a.l) {
                    ++j;
                } else {
                    unite_runs(i, j);
                    if (a.r < b.r) ++i;
                    else if (b.r < a.r) ++j;
                    else { ++i; ++j; }
                }
            }
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
        assert(0 <= h);
        assert(0 <= w && w <= word_bits());
        H = h; W = w; FULL = lowmask(w);
        road.assign(h, FULL);
        frontier.assign(h, Word(0));
        next.assign(h, Word(0));
        seen.assign(h, Word(0));
        comp.assign(h, Word(0));
        runs.clear();
        runs.reserve((size_t)h * ((w + 1) / 2));
        row_start.assign(h + 1, 0);
    }

    int height() const { return H; }
    int width() const { return W; }
    bool inside(int r, int c) const { return in_bounds(r, c); }

    /// @brief 道セルのマスク
    const Set &road_set() const { return road; }

    /// @brief 道セル数
    int road_count() const { return count(road); }

    /// @brief 壁セル数
    int wall_count() const { return H * W - road_count(); }

    /// @brief 壁セルのマスクを out に書く
    void wall_set(Set &out) const {
        assert_set(out);
        for (int r = 0; r < H; ++r) out[r] = FULL & ~road[r];
    }

    /// @brief 全ビット 0 のマスクを作る バッファ準備用
    Set new_set() const { return Set(H, Word(0)); }

    /// @brief 全セルを道にする
    void clear() {
        for (int r = 0; r < H; ++r) road[r] = FULL;
    }

    /// @brief 全セルを道にする
    void open_all() { clear(); }

    /// @brief 全セルを壁にする
    void fill() {
        for (int r = 0; r < H; ++r) road[r] = Word(0);
    }

    /// @brief 全セルを壁にする
    void block_all() { fill(); }

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
    void set_road(int r, int c) {
        assert(in_bounds(r, c));
        road[r] |= bit(c);
    }
    /// @brief (r,c) を壁にする
    void set_wall(int r, int c) {
        assert(in_bounds(r, c));
        road[r] &= ~bit(c);
    }
    /// @brief (r,c) が道か
    bool is_road(int r, int c) const {
        assert(in_bounds(r, c));
        return (road[r] >> c) & 1;
    }

    /// @brief 盤面を out に退避する
    void snapshot(Set &out) const { out = road; }

    /// @brief 退避した盤面を戻す
    void restore(const Set &snap) {
        assert_set(snap);
        road = snap;
    }

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

    /// @brief 任意のマスクを os に出力する 0=# 1=.
    void print(const Set &s, ostream &os) const {
        assert_set(s);
        for (int r = 0; r < H; ++r) {
            for (int c = 0; c < W; ++c) {
                os << (test(s, r, c) ? '.' : '#');
            }
            os << '\n';
        }
    }

    /// @brief マスクを空にする
    void clear(Set &s) const {
        assert_set(s);
        for (Word &x : s) x = Word(0);
    }
    /// @brief (r,c) が立っているか
    bool test(const Set &s, int r, int c) const {
        assert_set(s);
        assert(in_bounds(r, c));
        return (s[r] >> c) & 1;
    }
    /// @brief (r,c) を立てる
    void set(Set &s, int r, int c) const {
        assert_set(s);
        assert(in_bounds(r, c));
        s[r] |= bit(c);
    }
    /// @brief (r,c) を落とす
    void reset(Set &s, int r, int c) const {
        assert_set(s);
        assert(in_bounds(r, c));
        s[r] &= ~bit(c);
    }
    /// @brief (r,c) を反転する
    void flip(Set &s, int r, int c) const {
        assert_set(s);
        assert(in_bounds(r, c));
        s[r] ^= bit(c);
    }

    /// @brief 立っているセル数
    int count(const Set &s) const {
        assert_set(s);
        int res = 0;
        for (Word x : s) res += popcount(x);
        return res;
    }
    /// @brief 立っているセルが 1 つ以上あるか
    bool any(const Set &s) const {
        assert_set(s);
        for (Word x : s) if (x) return true;
        return false;
    }
    /// @brief 空か
    bool none(const Set &s) const { return !any(s); }
    /// @brief a と b が一致するか
    bool equals(const Set &a, const Set &b) const {
        assert_set(a);
        assert_set(b);
        for (int i = 0; i < H; ++i) if (a[i] != b[i]) return false;
        return true;
    }

    /// @brief a と b が共通セルを持つか
    bool intersects(const Set &a, const Set &b) const {
        assert_set(a);
        assert_set(b);
        for (int r = 0; r < H; ++r) if (a[r] & b[r]) return true;
        return false;
    }

    /// @brief a と b が共通セルを持たないか
    bool disjoint(const Set &a, const Set &b) const {
        return !intersects(a, b);
    }

    /// @brief a が b の部分集合か
    bool is_subset(const Set &a, const Set &b) const {
        assert_set(a);
        assert_set(b);
        for (int r = 0; r < H; ++r) if (a[r] & ~b[r]) return false;
        return true;
    }

    /// @brief マスクの 64bit ハッシュ 衝突の可能性はある
    uint64_t hash64(const Set &s) const {
        assert_set(s);
        uint64_t h = mix64(((uint64_t)(uint32_t)H << 32) | (uint32_t)W);
        for (int r = 0; r < H; ++r) {
            h = mix64(h ^ hash_word(s[r] & FULL) ^ mix64((uint64_t)r));
        }
        return h;
    }

    /// @brief 盤面の道セルの 64bit ハッシュ 衝突の可能性はある
    uint64_t hash64() const { return hash64(road); }

    /// @brief 積 a & b を out に書く
    void band(const Set &a, const Set &b, Set &out) const {
        assert_set(a); assert_set(b); assert_set(out);
        for (int i = 0; i < H; ++i) out[i] = a[i] & b[i];
    }
    /// @brief 和 a | b を out に書く
    void bor(const Set &a, const Set &b, Set &out) const {
        assert_set(a); assert_set(b); assert_set(out);
        for (int i = 0; i < H; ++i) out[i] = a[i] | b[i];
    }
    /// @brief 対称差 a ^ b を out に書く
    void bxor(const Set &a, const Set &b, Set &out) const {
        assert_set(a); assert_set(b); assert_set(out);
        for (int i = 0; i < H; ++i) out[i] = a[i] ^ b[i];
    }
    /// @brief 差 a \ b を out に書く
    void bdiff(const Set &a, const Set &b, Set &out) const {
        assert_set(a); assert_set(b); assert_set(out);
        for (int i = 0; i < H; ++i) out[i] = a[i] & ~b[i];
    }
    /// @brief a &= b
    void iand(Set &a, const Set &b) const {
        assert_set(a); assert_set(b);
        for (int i = 0; i < H; ++i) a[i] &= b[i];
    }
    /// @brief a |= b
    void ior(Set &a, const Set &b) const {
        assert_set(a); assert_set(b);
        for (int i = 0; i < H; ++i) a[i] |= b[i];
    }
    /// @brief a ^= b
    void ixor(Set &a, const Set &b) const {
        assert_set(a); assert_set(b);
        for (int i = 0; i < H; ++i) a[i] ^= b[i];
    }
    /// @brief a \= b
    void idiff(Set &a, const Set &b) const {
        assert_set(a); assert_set(b);
        for (int i = 0; i < H; ++i) a[i] &= ~b[i];
    }
    /// @brief 道セル内での補集合 road & ~a を out に書く
    void complement_into(const Set &a, Set &out) const {
        assert_set(a); assert_set(out);
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
        assert_set(s);
        for (int r = 0; r < H; ++r)
            for (Word x = s[r]; x; x &= x - 1) f(r, ctz(x));
    }
    /// @brief 立っているセルを out に集める
    void cells(const Set &s, vector<pair<int,int>> &out) const {
        out.clear();
        for_each(s, [&](int r, int c) { out.emplace_back(r, c); });
    }

    /// @brief 最初のセルを (r,c) に書く 空集合なら false
    bool first(const Set &s, int &r, int &c) const {
        assert_set(s);
        for (int i = 0; i < H; ++i) {
            if (s[i]) {
                r = i;
                c = ctz(s[i]);
                return true;
            }
        }
        return false;
    }

    /// @brief 行優先で k 番目 (0-indexed) のセルを返す 範囲外なら false
    bool kth_cell(const Set &s, int k, int &r, int &c) const {
        assert_set(s);
        if (k < 0) return false;
        for (int i = 0; i < H; ++i) {
            int pc = popcount(s[i]);
            if (k >= pc) {
                k -= pc;
                continue;
            }
            Word x = s[i];
            while (k--) x &= x - 1;
            r = i;
            c = ctz(x);
            return true;
        }
        return false;
    }

    /// @brief 半開矩形 [r1,r2) x [c1,c2) のマスクを out に書く
    void rect(int r1, int c1, int r2, int c2, Set &out) const {
        assert(0 <= r1 && r1 <= r2 && r2 <= H);
        assert(0 <= c1 && c1 <= c2 && c2 <= W);
        assert_set(out);
        Word cols = lowmask(c2) ^ lowmask(c1);
        for (int r = 0; r < r1; ++r) out[r] = Word(0);
        for (int r = r1; r < r2; ++r) out[r] = cols;
        for (int r = r2; r < H; ++r) out[r] = Word(0);
    }

    /// @brief s を (dr,dc) 平行移動して out に書く 壁は考慮しない
    void shift(const Set &s, int dr, int dc, Set &out) const {
        assert_set(s);
        assert_set(out);
        assert(&s != &out);
        if (dc <= -word_bits() || dc >= word_bits()) { clear(out); return; }
        int begin = max(0, min(H, dr));
        int end = max(0, min(H, H + dr));
        for (int r = 0; r < begin; ++r) out[r] = Word(0);
        for (int r = end; r < H; ++r) out[r] = Word(0);
        if (dc >= 0) {
            for (int r = begin; r < end; ++r) out[r] = (s[r - dr] << dc) & FULL;
        } else {
            int sh = -dc;
            for (int r = begin; r < end; ++r) out[r] = s[r - dr] >> sh;
        }
    }

    /// @brief 1 マス左へ移動する 壁は考慮しない
    void shift_left(const Set &s, Set &out) const {
        assert_set(s);
        assert_set(out);
        assert(&s != &out);
        for (int r = 0; r < H; ++r) out[r] = s[r] >> 1;
    }

    /// @brief 1 マス右へ移動する 壁は考慮しない
    void shift_right(const Set &s, Set &out) const {
        assert_set(s);
        assert_set(out);
        assert(&s != &out);
        for (int r = 0; r < H; ++r) out[r] = (s[r] << 1) & FULL;
    }

    /// @brief 1 マス上へ移動する 壁は考慮しない
    void shift_up(const Set &s, Set &out) const {
        assert_set(s);
        assert_set(out);
        assert(&s != &out);
        for (int r = 0; r + 1 < H; ++r) out[r] = s[r + 1];
        if (H) out[H - 1] = Word(0);
    }

    /// @brief 1 マス下へ移動する 壁は考慮しない
    void shift_down(const Set &s, Set &out) const {
        assert_set(s);
        assert_set(out);
        assert(&s != &out);
        if (H) out[0] = Word(0);
        for (int r = 1; r < H; ++r) out[r] = s[r - 1];
    }
    /// @brief shift してから道セルでマスクする (1 マス移動)
    void step(const Set &s, int dr, int dc, Set &out) const {
        assert_set(s);
        assert_set(out);
        assert(&s != &out);
        if (dc <= -word_bits() || dc >= word_bits()) { clear(out); return; }
        int begin = max(0, min(H, dr));
        int end = max(0, min(H, H + dr));
        for (int r = 0; r < begin; ++r) out[r] = Word(0);
        for (int r = end; r < H; ++r) out[r] = Word(0);
        if (dc >= 0) {
            for (int r = begin; r < end; ++r) out[r] = (s[r - dr] << dc) & FULL & road[r];
        } else {
            int sh = -dc;
            for (int r = begin; r < end; ++r) out[r] = (s[r - dr] >> sh) & road[r];
        }
    }

    /// @brief s を道セル内で設定された近傍に 1 ステップ拡張して out に書く
    void expand(const Set &s, Set &out) const {
        assert_set(s);
        assert_set(out);
        assert(&s != &out);
        expand_into(s, out);
    }

    /// @brief s に隣接する道セル (s 自身は除く) を out に書く
    void border(const Set &s, Set &out) const {
        assert_set(s);
        assert_set(out);
        assert(&s != &out);
        for (int r = 0; r < H; ++r) {
            Word f = s[r];
            Word x = ((f << 1) | (f >> 1)) & FULL;
            if (r > 0)     x |= vertical_bits(s[r - 1]);
            if (r + 1 < H) x |= vertical_bits(s[r + 1]);
            out[r] = x & road[r] & ~s[r];
        }
    }

    /// @brief s と隣接セルの和集合を out に書く road は考慮しない
    void dilate(const Set &s, Set &out) const {
        assert_set(s);
        assert_set(out);
        assert(&s != &out);
        for (int r = 0; r < H; ++r) {
            Word f = s[r];
            Word x = f | (f << 1) | (f >> 1);
            if (r > 0)     x |= vertical_bits(s[r - 1]);
            if (r + 1 < H) x |= vertical_bits(s[r + 1]);
            out[r] = x & FULL;
        }
    }

    /// @brief 周囲すべてが s に含まれるセルを out に書く 盤面外は 0 と扱う
    void erode(const Set &s, Set &out) const {
        assert_set(s);
        assert_set(out);
        assert(&s != &out);
        for (int r = 0; r < H; ++r) out[r] = eroded_row(s, r);
    }

    /// @brief s の内側境界を out に書く 盤面外は 0 と扱う
    void inner_border(const Set &s, Set &out) const {
        assert_set(s);
        assert_set(out);
        assert(&s != &out);
        for (int r = 0; r < H; ++r) out[r] = s[r] & ~eroded_row(s, r);
    }

    /// @brief sources からの到達集合を out に書く
    void flood(const vector<pair<int,int>> &sources, Set &out) const {
        make_set(sources, out);
        flood_closure_inplace(out);
    }

    /// @brief sources からの到達集合を out に書く sources と out は別バッファ
    void flood(const Set &sources, Set &out) const {
        assert_set(sources);
        assert_set(out);
        assert(&sources != &out);
        for (int r = 0; r < H; ++r) out[r] = sources[r] & road[r];
        flood_closure_inplace(out);
    }

    /// @brief (r,c) からの到達集合を out に書く
    void flood(int r, int c, Set &out) const {
        assert(in_bounds(r, c));
        assert_set(out);
        flood_point_into(r, c, out);
    }

    /// @brief max_steps 回までの拡張に限った到達集合を out に書く
    void flood_limited(const vector<pair<int,int>> &sources, int max_steps, Set &out) const {
        make_set(sources, comp);
        flood_limited(comp, max_steps, out);
    }

    /// @brief sources から max_steps 回までの拡張に限った到達集合を out に書く
    void flood_limited(const Set &sources, int max_steps, Set &out) const {
        assert(0 <= max_steps);
        assert_set(sources);
        assert_set(out);
        assert(&sources != &out);
        for (int r = 0; r < H; ++r) out[r] = sources[r] & road[r];
        for (int r = 0; r < H; ++r) frontier[r] = out[r];
        for (int step_i = 0; step_i < max_steps; ++step_i) {
            expand_into(frontier, next);
            bool changed = false;
            for (int r = 0; r < H; ++r) {
                Word add = next[r] & ~out[r];
                next[r] = add;
                out[r] |= add;
                changed |= add != 0;
            }
            if (!changed) break;
            frontier.swap(next);
        }
    }

    /// @brief a と b が連結か
    bool connected(pair<int,int> a, pair<int,int> b) const {
        assert(in_bounds(a.first, a.second));
        assert(in_bounds(b.first, b.second));
        return connected_points(a.first, a.second, b.first, b.second);
    }

    /// @brief a から b までの最短距離 到達不能なら -1
    int distance(pair<int,int> a, pair<int,int> b) const {
        assert(in_bounds(a.first, a.second));
        assert(in_bounds(b.first, b.second));
        return distance_points(a.first, a.second, b.first, b.second);
    }

    /// @brief (r,c) から targets の最寄りセルまでの距離 行優先最小のセルを hit に書く
    int nearest_in_set(int r, int c, const Set &targets, pair<int,int> &hit) const {
        assert(in_bounds(r, c));
        assert_set(targets);
        if (!is_road(r, c)) return -1;

        clear(seen);
        clear(frontier);
        seen[r] = bit(c);
        frontier[r] = bit(c);
        if (targets[r] & bit(c)) {
            hit = {r, c};
            return 0;
        }

        int d = 0;
        while (true) {
            expand_into(frontier, next);
            bool changed = false;
            ++d;
            for (int rr = 0; rr < H; ++rr) {
                Word add = next[rr] & ~seen[rr];
                next[rr] = add;
                seen[rr] |= add;
                changed |= add != 0;
            }
            for (int rr = 0; rr < H; ++rr) {
                Word found = next[rr] & targets[rr];
                if (found) {
                    hit = {rr, ctz(found)};
                    return d;
                }
            }
            if (!changed) return -1;
            frontier.swap(next);
        }
    }

    /// @brief (r,c) を含む連結成分を out に書く 壁なら空集合
    void component(int r, int c, Set &out) const {
        assert(in_bounds(r, c));
        assert_set(out);
        flood_point_into(r, c, out);
    }

    /// @brief (r,c) を含む連結成分のセル数 壁なら 0
    int component_size(int r, int c) const {
        assert(in_bounds(r, c));
        return flood_point_into(r, c, comp);
    }

    /// @brief 多始点最短距離を dist (長さ H*W, index は r*W+c) に書く 未到達は -1
    void bfs_dist(const vector<pair<int,int>> &sources, vector<int> &dist) const {
        make_set(sources, comp);
        bfs_dist(comp, dist);
    }

    /// @brief Set の多始点最短距離を dist に書く 未到達は -1
    void bfs_dist(const Set &sources, vector<int> &dist) const {
        assert_set(sources);
        dist.assign(H * W, -1);
        for (int r = 0; r < H; ++r) seen[r] = sources[r] & road[r];
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
                next[r] = add;
                if (!add) continue;
                seen[r] |= add;
                changed = true;
                for (Word x = add; x; x &= x - 1) dist[r * W + ctz(x)] = layer;
            }
            if (!changed) break;
            frontier.swap(next);
        }
    }

    /// @brief a から b への最短経路を path に書く dist は再利用バッファ 到達不能なら false
    bool shortest_path(
        pair<int,int> a,
        pair<int,int> b,
        vector<pair<int,int>> &path,
        vector<int> &dist
    ) const {
        assert(in_bounds(a.first, a.second));
        assert(in_bounds(b.first, b.second));
        path.clear();
        dist.assign(H * W, -1);
        if (!is_road(a.first, a.second) || !is_road(b.first, b.second)) return false;

        clear(seen);
        clear(frontier);
        seen[b.first] = bit(b.second);
        frontier[b.first] = bit(b.second);
        dist[b.first * W + b.second] = 0;

        bool found = a == b;
        int layer = 0;
        while (!found) {
            expand_into(frontier, next);
            bool changed = false;
            ++layer;
            for (int r = 0; r < H; ++r) {
                Word add = next[r] & ~seen[r];
                next[r] = add;
                seen[r] |= add;
                changed |= add != 0;
                for (Word x = add; x; x &= x - 1) {
                    int c = ctz(x);
                    dist[r * W + c] = layer;
                }
            }
            found = (next[a.first] & bit(a.second)) != 0;
            if (!changed) return false;
            if (!found) frontier.swap(next);
        }

        pair<int,int> cur = a;
        path.push_back(cur);
        while (cur != b) {
            int d = dist[cur.first * W + cur.second];
            bool moved = false;
            for (int k = 0; k < DIRS; ++k) {
                int nr = cur.first + DR[k], nc = cur.second + DC[k];
                if (!in_bounds(nr, nc) || dist[nr * W + nc] != d - 1) continue;
                cur = {nr, nc};
                path.push_back(cur);
                moved = true;
                break;
            }
            assert(moved);
        }
        return true;
    }

    /// @brief a から b への最短経路を path に書く 到達不能なら false
    bool shortest_path(pair<int,int> a, pair<int,int> b, vector<pair<int,int>> &path) const {
        vector<int> dist;
        return shortest_path(a, b, path, dist);
    }

    /// @brief 多始点最短距離 dist と最近傍始点番号 src を書く 同距離は番号最小を選ぶ 未到達は -1
    void bfs_nearest(const vector<pair<int,int>> &sources, vector<int> &dist, vector<int> &src) const {
        dist.assign(H * W, -1);
        src.assign(H * W, -1);
        clear(seen);
        int ns = sources.size();
        for (int i = 0; i < ns; ++i) {
            auto [r, c] = sources[i];
            assert(in_bounds(r, c));
            if (!is_road(r, c)) continue;
            int idx = r * W + c;
            if (dist[idx] == -1) { dist[idx] = 0; src[idx] = i; seen[r] |= bit(c); }
        }
        for (int r = 0; r < H; ++r) frontier[r] = seen[r];
        int layer = 0;
        while (true) {
            expand_into(frontier, next);
            bool changed = false;
            ++layer;
            for (int r = 0; r < H; ++r) {
                Word add = next[r] & ~seen[r];
                next[r] = add;
                if (!add) continue;
                seen[r] |= add;
                changed = true;
                for (Word x = add; x; x &= x - 1) {
                    int c = ctz(x);
                    int idx = r * W + c;
                    dist[idx] = layer;
                    int best = -1;
                    for (int k = 0; k < DIRS; ++k) {
                        int nr = r + DR[k], nc = c + DC[k];
                        if (nr < 0 || nc < 0 || nr >= H || nc >= W) continue;
                        int nidx = nr * W + nc;
                        if (!(frontier[nr] & bit(nc))) continue;
                        if (best == -1 || src[nidx] < best) best = src[nidx];
                    }
                    src[idx] = best;
                }
            }
            if (!changed) break;
            frontier.swap(next);
        }
    }

    /// @brief 道セルの連結成分数を返す
    int components() const {
        build_runs();
        int cnt = 0;
        for (int i = 0; i < (int)runs.size(); ++i)
            if (runs[i].parent == i) ++cnt;
        runs.clear();
        return cnt;
    }

    /// @brief 各道セルに連結成分番号を振る lab は長さ H*W、壁は -1 成分数を返す
    int label(vector<int> &lab) const {
        lab.assign(H * W, -1);
        build_runs();
        int id = 0;
        for (int r = 0; r < H; ++r) {
            for (int i = row_start[r]; i < row_start[r + 1]; ++i) {
                int root = find_run(i);
                if (runs[root].id == -1) runs[root].id = id++;
                int cur = runs[root].id;
                for (int c = runs[i].l; c < runs[i].r; ++c) {
                    lab[r * W + c] = cur;
                }
            }
        }
        runs.clear();
        return id;
    }

    /// @brief 最大の連結成分を out に書く セル数を返す
    int largest_component(Set &out) const {
        assert_set(out);
        clear(out);
        build_runs();
        int best = 0;
        int best_root = -1;
        for (int r = 0; r < H; ++r) {
            for (int i = row_start[r]; i < row_start[r + 1]; ++i) {
                int root = find_run(i);
                if (runs[root].size > best) {
                    best = runs[root].size;
                    best_root = root;
                }
            }
        }
        if (best_root == -1) {
            runs.clear();
            return 0;
        }
        for (int r = 0; r < H; ++r) {
            for (int i = row_start[r]; i < row_start[r + 1]; ++i) {
                if (find_run(i) == best_root) {
                    out[r] |= lowmask(runs[i].r) ^ lowmask(runs[i].l);
                }
            }
        }
        runs.clear();
        return best;
    }
};

using Bitboard64      = Bitboard<uint64_t>;
using Bitboard128     = Bitboard<__uint128_t>;
using Bitboard64Diag  = Bitboard<uint64_t, Neighborhood::Eight>;
using Bitboard128Diag = Bitboard<__uint128_t, Neighborhood::Eight>;

}  // namespace titan23
