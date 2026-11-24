/// https://github.com/titan-23/Library_cpp/blob/main/titan_cpplib/ahc/grid/bitboard.cpp
#pragma once

#include <array>
#include <vector>
#include <string>
#include <cstdint>
#include <cassert>
#include <utility>
#include <ostream>
#include <algorithm>
#include "titan_cpplib/ahc/grid/row_bitboard_base.cpp"
#include "titan_cpplib/others/bit.cpp"
using namespace std;

namespace titan23 {

/// @brief グリッドの 1 行を 1 つの Word で保持する Bitboard / W <= sizeof(Word)*8 が前提
/// 結果は new_set() で作成した out に格納する
/// 探索の処理は内部の作業領域を共有するため、同じインスタンスで並列に実行できない
/// 盤面の形だけで結果が定まる集合演算 (count, band, for_each, shift など) は RowBitboardBase が提供する
template<class Word, Neighborhood NB = Neighborhood::Four>
class Bitboard : public RowBitboardBase<Word> {
    using Base = RowBitboardBase<Word>;

public:
    using Set = vector<Word>;

    // テンプレート引数に依存する基底クラスのメンバは修飾なしの名前で参照できないため、using 宣言で導入する
    using Base::popcount;
    using Base::ctz;
    using Base::msb;
    using Base::height;
    using Base::width;
    using Base::cell_count;
    using Base::inside;
    using Base::new_set;
    using Base::print;
    using Base::to_grid;
    using Base::clear;
    using Base::test;
    using Base::set;
    using Base::reset;
    using Base::flip;
    using Base::count;
    using Base::count_and;
    using Base::any;
    using Base::none;
    using Base::equals;
    using Base::intersects;
    using Base::disjoint;
    using Base::is_subset;
    using Base::hash64;
    using Base::band;
    using Base::bor;
    using Base::bxor;
    using Base::bdiff;
    using Base::iand;
    using Base::ior;
    using Base::ixor;
    using Base::idiff;
    using Base::for_each;
    using Base::cells;
    using Base::first;
    using Base::kth_cell;
    using Base::bounding_box;
    using Base::rect;
    using Base::shift;
    using Base::shift_left;
    using Base::shift_right;
    using Base::shift_up;
    using Base::shift_down;

private:
    using Base::H;
    using Base::W;
    using Base::FULL;
    using Base::word_bits;
    using Base::lowmask;
    using Base::bit;
    using Base::in_bounds;
    using Base::assert_set;
    using Base::init_shape;

    Set road;

    struct Run {
        int row;
        int l, r;
        int parent;
        int size;
        int id;
    };

    mutable Set frontier, next, seen, comp, point_work;
    mutable int point_lo = 0, point_hi = -1;
    mutable vector<Run> runs;
    mutable vector<int> row_start;
    mutable vector<int> component_head, component_next, component_roots, touched_rows;
    mutable vector<int> path_dist;
    mutable vector<int> dfs_disc, dfs_low, dfs_parent, dfs_next, dfs_children, dfs_stack;

    static constexpr int DIRS = NB == Neighborhood::Four ? 4 : 8;

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

    Word spread3(Word x) const {
        return ((x << 1) | x | (x >> 1)) & FULL;
    }

    // fr の 0 でない行が [lo,hi] の範囲にあるとして、fr に隣接する道のセルを out の [lo-1,hi+1] の行に格納する
    void expand_into(const Set &fr, Set &out, int lo, int hi) const {
        if (lo > hi) return;
        int out_lo = max(0, lo - 1);
        int out_hi = min(H - 1, hi + 1);
        if constexpr (NB == Neighborhood::Four) {
            for (int r = out_lo; r <= out_hi; ++r) {
                Word f = lo <= r && r <= hi ? fr[r] : Word(0);
                Word x = ((f << 1) | (f >> 1)) & FULL;
                if (lo <= r - 1 && r - 1 <= hi) x |= fr[r - 1];
                if (lo <= r + 1 && r + 1 <= hi) x |= fr[r + 1];
                out[r] = x & road[r];
            }
        } else {
            auto spread_at = [&](int r) {
                return lo <= r && r <= hi ? spread3(fr[r]) : Word(0);
            };
            Word prev = spread_at(out_lo - 1);
            Word cur = spread_at(out_lo);
            Word following = spread_at(out_lo + 1);
            for (int r = out_lo; r <= out_hi; ++r) {
                Word f = lo <= r && r <= hi ? fr[r] : Word(0);
                Word x = ((f << 1) | (f >> 1)) | prev | following;
                out[r] = x & road[r] & FULL;
                prev = cur;
                cur = following;
                following = spread_at(r + 2);
            }
        }
    }

    void expand_into(const Set &fr, Set &out) const {
        expand_into(fr, out, 0, H - 1);
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

    template<bool STOP_AT_TARGET>
    bool flood_closure_impl(
        Set &out,
        int target_r = -1,
        Word target = Word(0),
        int *result_lo = nullptr,
        int *result_hi = nullptr
    ) const {
        int lo = H, hi = -1;
        for (int r = 0; r < H; ++r) {
            if (out[r]) {
                lo = min(lo, r);
                hi = r;
            }
        }
        if (hi < 0) {
            if (result_lo) *result_lo = H;
            if (result_hi) *result_hi = -1;
            return false;
        }
        if constexpr (STOP_AT_TARGET) {
            if (out[target_r] & target) return true;
        }

        auto sweep_down = [&](bool &changed) {
            for (int r = lo; r < H; ++r) {
                Word s = out[r];
                if (r > lo) s |= vertical_bits(out[r - 1]);
                Word x = row_fill(s, road[r]);
                changed |= x != out[r];
                out[r] = x;
                if (x) hi = max(hi, r);
                else if (r > hi) break;
                if constexpr (STOP_AT_TARGET) {
                    if (r == target_r && (x & target)) return true;
                }
            }
            return false;
        };

        auto sweep_up = [&](bool &changed) {
            for (int r = hi; r >= 0; --r) {
                Word s = out[r];
                if (r < hi) s |= vertical_bits(out[r + 1]);
                Word x = row_fill(s, road[r]);
                changed |= x != out[r];
                out[r] = x;
                if (x) lo = min(lo, r);
                else if (r < lo) break;
                if constexpr (STOP_AT_TARGET) {
                    if (r == target_r && (x & target)) return true;
                }
            }
            return false;
        };

        bool changed = false;
        if (sweep_down(changed)) return true;
        while (true) {
            changed = false;
            if (sweep_up(changed)) return true;
            if (!changed) {
                if (result_lo) *result_lo = lo;
                if (result_hi) *result_hi = hi;
                return false;
            }
            changed = false;
            if (sweep_down(changed)) return true;
            if (!changed) {
                if (result_lo) *result_lo = lo;
                if (result_hi) *result_hi = hi;
                return false;
            }
        }
    }

    void flood_closure_inplace(Set &out) const {
        flood_closure_impl<false>(out);
    }

    int flood_point_into(int r0, int c0, Set &out) const {
        clear(out);
        if (!is_road(r0, c0)) return 0;
        out[r0] = bit(c0);
        int lo, hi;
        flood_closure_impl<false>(out, -1, Word(0), &lo, &hi);
        int result = 0;
        for (int r = lo; r <= hi; ++r) result += popcount(out[r]);
        return result;
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
        int d = 0, lo = r0, hi = r0;
        while (true) {
            expand_into(frontier, next, lo, hi);
            int out_lo = max(0, lo - 1), out_hi = min(H - 1, hi + 1);
            int next_lo = H, next_hi = -1;
            ++d;
            for (int r = out_lo; r <= out_hi; ++r) {
                Word add = next[r] & ~seen[r];
                next[r] = add;
                seen[r] |= add;
                if (add) {
                    next_lo = min(next_lo, r);
                    next_hi = r;
                }
            }
            if (out_lo <= r1 && r1 <= out_hi && (next[r1] & target)) return d;
            if (next_hi < 0) return -1;
            frontier.swap(next);
            lo = next_lo;
            hi = next_hi;
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
        int lo = r0, hi = r0;
        for (int layer = 0; layer < LAYER_LIMIT; ++layer) {
            expand_into(frontier, next, lo, hi);
            int out_lo = max(0, lo - 1), out_hi = min(H - 1, hi + 1);
            int next_lo = H, next_hi = -1;
            for (int r = out_lo; r <= out_hi; ++r) {
                Word add = next[r] & ~seen[r];
                next[r] = add;
                seen[r] |= add;
                if (add) {
                    next_lo = min(next_lo, r);
                    next_hi = r;
                }
            }
            if (out_lo <= r1 && r1 <= out_hi && (next[r1] & target)) return true;
            if (next_hi < 0) return false;
            frontier.swap(next);
            lo = next_lo;
            hi = next_hi;
        }

        return flood_closure_impl<true>(seen, r1, target);
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
                runs.push_back({r, l, rr, idx, rr - l, -1});
                x &= ~lowmask(rr);
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
    Bitboard() = default;
    /// @brief h*w の盤面を作成し、全セルを道にする
    Bitboard(int h, int w) { resize(h, w); }

    /// @brief 盤面を h*w の大きさで再作成し、全セルを道にする / O(HW)
    void resize(int h, int w) {
        init_shape(h, w);
        road.assign(h, FULL);
        frontier.assign(h, Word(0));
        next.assign(h, Word(0));
        seen.assign(h, Word(0));
        comp.assign(h, Word(0));
        point_work.assign(h, Word(0));
        point_lo = 0;
        point_hi = -1;
        runs.clear();
        runs.reserve((size_t)h * ((w + 1) / 2));
        row_start.assign(h + 1, 0);
        size_t max_runs = (size_t)h * ((w + 1) / 2);
        component_head.reserve(max_runs);
        component_next.reserve(max_runs);
        component_roots.reserve(max_runs);
        touched_rows.reserve(h);
        path_dist.resize((size_t)h * w);
        size_t cells = (size_t)h * w;
        dfs_disc.resize(cells);
        dfs_low.resize(cells);
        dfs_parent.resize(cells);
        dfs_next.resize(cells);
        dfs_children.resize(cells);
        dfs_stack.clear();
        dfs_stack.reserve(cells);
    }

    /// @brief 道のセルの集合を返す / O(1)
    const Set &road_set() const { return road; }

    /// @brief 道のセルの個数を返す / O(H)
    int road_count() const { return count(road); }

    /// @brief 壁のセルの個数を返す / O(H)
    int wall_count() const { return H * W - road_count(); }

    /// @brief 壁のセルの集合を out に格納する / O(H)
    void wall_set(Set &out) const {
        assert_set(out);
        for (int r = 0; r < H; ++r) out[r] = FULL & ~road[r];
    }

    /// @brief 全セルを道にする / O(H)
    void open_all() {
        for (int r = 0; r < H; ++r) road[r] = FULL;
    }

    /// @brief 全セルを壁にする / O(H)
    void block_all() {
        for (int r = 0; r < H; ++r) road[r] = Word(0);
    }

    /// @brief grid の文字 wall_ch を壁、それ以外の文字を道として読み込む / O(HW)
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

    /// @brief (r,c) を道にする / O(1)
    void set_road(int r, int c) {
        TITAN_GRID_ASSERT(in_bounds(r, c));
        road[r] |= bit(c);
    }
    /// @brief (r,c) を壁にする / O(1)
    void set_wall(int r, int c) {
        TITAN_GRID_ASSERT(in_bounds(r, c));
        road[r] &= ~bit(c);
    }
    /// @brief (r,c) が道であるかを返す / O(1)
    bool is_road(int r, int c) const {
        TITAN_GRID_ASSERT(in_bounds(r, c));
        return (road[r] >> c) & 1;
    }

    /// @brief 盤面を out に保存する / O(H)
    void snapshot(Set &out) const { out = road; }

    /// @brief 保存した盤面を復元する / O(H)
    void restore(const Set &snap) {
        assert_set(snap);
        road = snap;
    }

    /// @brief H, W, 壁の配置がすべて一致するかを返す
    bool operator==(const Bitboard &o) const { return H == o.H && W == o.W && road == o.road; }
    /// @brief 盤面が異なるかを返す
    bool operator!=(const Bitboard &o) const { return !(*this == o); }

    /// @brief 盤面を os に出力する / 壁は '#', 道は '.' / デバッグ用
    friend ostream &operator<<(ostream &os, const Bitboard &b) {
        for (int r = 0; r < b.H; ++r) {
            for (int c = 0; c < b.W; ++c) {
                os << (b.is_road(r, c) ? '.' : '#');
            }
            os << '\n';
        }
        return os;
    }

    /// @brief 道のセルの集合の 64 ビットのハッシュ値を返す / 異なる盤面で値が衝突する場合がある / O(H)
    uint64_t hash64() const { return hash64(road); }

    /// @brief 道のセルを全体集合とする補集合 road & ~a を out に格納する / O(H)
    void complement_into(const Set &a, Set &out) const {
        assert_set(a); assert_set(out);
        for (int r = 0; r < H; ++r) out[r] = road[r] & ~a[r];
    }

    /// @brief cells のセルからなる集合を out に格納する / 壁のセルは除外する
    void make_set(const vector<pair<int,int>> &cells, Set &out) const {
        clear(out);
        for (auto [r, c] : cells) {
            TITAN_GRID_ASSERT(in_bounds(r, c));
            out[r] |= bit(c) & road[r];
        }
    }

    /// @brief s を (dr,dc) だけ平行移動し、道のセルだけを out に格納する / 1 セルの移動に使用する / O(H)
    void step(const Set &s, int dr, int dc, Set &out) const {
        assert_set(s);
        assert_set(out);
        TITAN_GRID_ASSERT(&s != &out);
        if (dc <= -word_bits() || dc >= word_bits() || dr <= -H || dr >= H) { clear(out); return; }
        int begin = max(0, dr);
        int end = min(H, H + dr);
        for (int r = 0; r < begin; ++r) out[r] = Word(0);
        for (int r = end; r < H; ++r) out[r] = Word(0);
        if (dc >= 0) {
            for (int r = begin; r < end; ++r) out[r] = (s[r - dr] << dc) & FULL & road[r];
        } else {
            int sh = -dc;
            for (int r = begin; r < end; ++r) out[r] = (s[r - dr] >> sh) & road[r];
        }
    }

    /// @brief s の各セルから近傍へ 1 セル移動した先の道のセルを out に格納する / O(H)
    void expand(const Set &s, Set &out) const {
        assert_set(s);
        assert_set(out);
        TITAN_GRID_ASSERT(&s != &out);
        expand_into(s, out);
    }

    /// @brief s に隣接し、s に含まれない道のセルを out に格納する / O(H)
    void border(const Set &s, Set &out) const {
        assert_set(s);
        assert_set(out);
        TITAN_GRID_ASSERT(&s != &out);
        for (int r = 0; r < H; ++r) {
            Word f = s[r];
            Word x = ((f << 1) | (f >> 1)) & FULL;
            if (r > 0)     x |= vertical_bits(s[r - 1]);
            if (r + 1 < H) x |= vertical_bits(s[r + 1]);
            out[r] = x & road[r] & ~s[r];
        }
    }

    /// @brief s と、s に隣接するセルの和集合を out に格納する / road は考慮しない / O(H)
    void dilate(const Set &s, Set &out) const {
        assert_set(s);
        assert_set(out);
        TITAN_GRID_ASSERT(&s != &out);
        for (int r = 0; r < H; ++r) {
            Word f = s[r];
            Word x = f | (f << 1) | (f >> 1);
            if (r > 0)     x |= vertical_bits(s[r - 1]);
            if (r + 1 < H) x |= vertical_bits(s[r + 1]);
            out[r] = x & FULL;
        }
    }

    /// @brief s を盤面内で k 回膨張させた集合を out に格納する / road は考慮しない / O(kH)
    void dilate(const Set &s, int k, Set &out) const {
        assert(0 <= k);
        assert_set(s);
        assert_set(out);
        TITAN_GRID_ASSERT(&s != &out);
        for (int r = 0; r < H; ++r) out[r] = s[r] & FULL;
        for (int step_i = 0; step_i < k; ++step_i) {
            Word above = Word(0);  // 更新前の out[r - 1]
            for (int r = 0; r < H; ++r) {
                Word f = out[r];
                Word x = f | (f << 1) | (f >> 1) | vertical_bits(above);
                if (r + 1 < H) x |= vertical_bits(out[r + 1]);
                above = f;
                out[r] = x & FULL;
            }
        }
    }

    /// @brief 自身と近傍のセルがすべて s に含まれるセルを out に格納する / 盤面外のセルは s に含まれないものとして扱う / O(H)
    void erode(const Set &s, Set &out) const {
        assert_set(s);
        assert_set(out);
        TITAN_GRID_ASSERT(&s != &out);
        for (int r = 0; r < H; ++r) out[r] = eroded_row(s, r);
    }

    /// @brief s のうち収縮で除かれるセルを out に格納する / 盤面外のセルは s に含まれないものとして扱う / O(H)
    void inner_border(const Set &s, Set &out) const {
        assert_set(s);
        assert_set(out);
        TITAN_GRID_ASSERT(&s != &out);
        for (int r = 0; r < H; ++r) out[r] = s[r] & ~eroded_row(s, r);
    }

    /// @brief 座標の列 sources から到達できるセルの集合を out に格納する
    void flood(const vector<pair<int,int>> &sources, Set &out) const {
        make_set(sources, out);
        flood_closure_inplace(out);
    }

    /// @brief 集合 sources から到達できるセルの集合を out に格納する / sources と out には別の Set を渡す
    void flood(const Set &sources, Set &out) const {
        assert_set(sources);
        assert_set(out);
        TITAN_GRID_ASSERT(&sources != &out);
        for (int r = 0; r < H; ++r) out[r] = sources[r] & road[r];
        flood_closure_inplace(out);
    }

    /// @brief (r,c) から到達できるセルの集合を out に格納する
    void flood(int r, int c, Set &out) const {
        TITAN_GRID_ASSERT(in_bounds(r, c));
        assert_set(out);
        flood_point_into(r, c, out);
    }

    /// @brief 盤面の外周にある道のセルから到達できる道のセルを out に格納する
    void reachable_from_border(Set &out) const {
        assert_set(out);
        clear(out);
        if (H == 0 || W == 0) return;
        Word sides = bit(0) | bit(W - 1);
        for (int r = 0; r < H; ++r) out[r] = road[r] & sides;
        out[0] = road[0];
        out[H - 1] = road[H - 1];
        flood_closure_inplace(out);
    }

    /// @brief 盤面の外周にある道のセルから到達できない道のセルを out に格納する
    void enclosed_road(Set &out) const {
        assert_set(out);
        reachable_from_border(comp);
        for (int r = 0; r < H; ++r) out[r] = road[r] & ~comp[r];
    }

    /// @brief 連結な道のセルの集合から (r,c) を削除しても連結性が維持される十分条件を 3x3 の範囲で判定する / O(1)
    bool locally_removable(int r, int c) const {
        TITAN_GRID_ASSERT(in_bounds(r, c));
        if (!is_road(r, c)) return false;
        static const array<unsigned char, 256> lut = [] {
            array<unsigned char, 256> res{};
            constexpr int RR[8] = {-1, -1, -1, 0, 0, 1, 1, 1};
            constexpr int CC[8] = {-1, 0, 1, -1, 1, -1, 0, 1};
            constexpr unsigned DIRECT4 = (1u << 1) | (1u << 3) | (1u << 4) | (1u << 6);
            for (unsigned mask = 0; mask < 256; ++mask) {
                unsigned direct = NB == Neighborhood::Four ? mask & DIRECT4 : mask;
                if (!direct || !(direct & (direct - 1))) {
                    res[mask] = 1;
                    continue;
                }
                unsigned reached = direct & (0u - direct);
                unsigned queue = reached;
                while (queue) {
                    int u = countr_zero(queue);
                    queue &= queue - 1;
                    for (int v = 0; v < 8; ++v) {
                        if (!(mask & (1u << v)) || (reached & (1u << v))) continue;
                        int dr = RR[u] > RR[v] ? RR[u] - RR[v] : RR[v] - RR[u];
                        int dc = CC[u] > CC[v] ? CC[u] - CC[v] : CC[v] - CC[u];
                        bool adjacent = NB == Neighborhood::Four ? dr + dc == 1 : max(dr, dc) == 1;
                        if (!adjacent) continue;
                        reached |= 1u << v;
                        queue |= 1u << v;
                    }
                }
                res[mask] = (direct & ~reached) == 0;
            }
            return res;
        }();

        constexpr int RR[8] = {-1, -1, -1, 0, 0, 1, 1, 1};
        constexpr int CC[8] = {-1, 0, 1, -1, 1, -1, 0, 1};
        unsigned pattern = 0;
        for (int k = 0; k < 8; ++k) {
            int nr = r + RR[k], nc = c + CC[k];
            if (in_bounds(nr, nc) && ((road[nr] >> nc) & 1)) pattern |= 1u << k;
        }
        return lut[pattern];
    }

    /// @brief 座標の列 sources から max_steps 回以内の移動で到達できるセルの集合を out に格納する
    void flood_limited(const vector<pair<int,int>> &sources, int max_steps, Set &out) const {
        make_set(sources, comp);
        flood_limited(comp, max_steps, out);
    }

    /// @brief 集合 sources から max_steps 回以内の移動で到達できるセルの集合を out に格納する
    void flood_limited(const Set &sources, int max_steps, Set &out) const {
        assert(0 <= max_steps);
        assert_set(sources);
        assert_set(out);
        TITAN_GRID_ASSERT(&sources != &out);
        int lo = H, hi = -1;
        for (int r = 0; r < H; ++r) {
            out[r] = sources[r] & road[r];
            frontier[r] = out[r];
            if (out[r]) {
                lo = min(lo, r);
                hi = r;
            }
        }
        for (int step_i = 0; step_i < max_steps; ++step_i) {
            if (hi < 0) break;
            expand_into(frontier, next, lo, hi);
            int out_lo = max(0, lo - 1), out_hi = min(H - 1, hi + 1);
            int next_lo = H, next_hi = -1;
            for (int r = out_lo; r <= out_hi; ++r) {
                Word add = next[r] & ~out[r];
                next[r] = add;
                out[r] |= add;
                if (add) {
                    next_lo = min(next_lo, r);
                    next_hi = r;
                }
            }
            if (next_hi < 0) break;
            frontier.swap(next);
            lo = next_lo;
            hi = next_hi;
        }
    }

    /// @brief a と b が連結であるかを返す
    bool connected(pair<int,int> a, pair<int,int> b) const {
        TITAN_GRID_ASSERT(in_bounds(a.first, a.second));
        TITAN_GRID_ASSERT(in_bounds(b.first, b.second));
        return connected_points(a.first, a.second, b.first, b.second);
    }

    /// @brief a から b までの最短距離を返す / 到達できない場合は -1
    int distance(pair<int,int> a, pair<int,int> b) const {
        TITAN_GRID_ASSERT(in_bounds(a.first, a.second));
        TITAN_GRID_ASSERT(in_bounds(b.first, b.second));
        return distance_points(a.first, a.second, b.first, b.second);
    }

    /// @brief (r,c) から targets の中で最も近いセルまでの距離を返し、そのセルを hit に格納する / 距離が同じ場合は行優先の順で先頭のセル、到達できない場合は -1
    int nearest_in_set(int r, int c, const Set &targets, pair<int,int> &hit) const {
        TITAN_GRID_ASSERT(in_bounds(r, c));
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

        int d = 0, lo = r, hi = r;
        while (true) {
            expand_into(frontier, next, lo, hi);
            int out_lo = max(0, lo - 1), out_hi = min(H - 1, hi + 1);
            int next_lo = H, next_hi = -1;
            ++d;
            for (int rr = out_lo; rr <= out_hi; ++rr) {
                Word add = next[rr] & ~seen[rr];
                next[rr] = add;
                seen[rr] |= add;
                if (add) {
                    next_lo = min(next_lo, rr);
                    next_hi = rr;
                }
                Word found = add & targets[rr];
                if (found) {
                    hit = {rr, ctz(found)};
                    return d;
                }
            }
            if (next_hi < 0) return -1;
            frontier.swap(next);
            lo = next_lo;
            hi = next_hi;
        }
    }

    /// @brief (r,c) を含む連結成分を out に格納する / (r,c) が壁なら空集合
    void component(int r, int c, Set &out) const {
        TITAN_GRID_ASSERT(in_bounds(r, c));
        assert_set(out);
        flood_point_into(r, c, out);
    }

    /// @brief (r,c) を含む連結成分のセルの個数を返す / (r,c) が壁なら 0
    int component_size(int r, int c) const {
        TITAN_GRID_ASSERT(in_bounds(r, c));
        for (int rr = point_lo; rr <= point_hi; ++rr) point_work[rr] = Word(0);
        if (!is_road(r, c)) {
            point_lo = 0;
            point_hi = -1;
            return 0;
        }
        point_work[r] = bit(c);
        flood_closure_impl<false>(point_work, -1, Word(0), &point_lo, &point_hi);
        int result = 0;
        for (int rr = point_lo; rr <= point_hi; ++rr) result += popcount(point_work[rr]);
        return result;
    }

    /// @brief 座標の列 sources からの多始点の最短距離を dist に格納する / dist は長さ H*W で添字は r*W+c、到達できないセルは -1
    void bfs_dist(const vector<pair<int,int>> &sources, vector<int> &dist) const {
        make_set(sources, comp);
        bfs_dist(comp, dist);
    }

    /// @brief 集合 sources からの多始点の最短距離を dist に格納する / 到達できないセルは -1
    void bfs_dist(const Set &sources, vector<int> &dist) const {
        assert_set(sources);
        dist.assign(H * W, -1);
        int lo = H, hi = -1;
        for (int r = 0; r < H; ++r) {
            seen[r] = sources[r] & road[r];
            if (seen[r]) {
                lo = min(lo, r);
                hi = r;
            }
        }
        for (int r = 0; r < H; ++r) {
            frontier[r] = seen[r];
            for (Word x = seen[r]; x; x &= x - 1) dist[r * W + ctz(x)] = 0;
        }
        int layer = 0;
        while (hi >= 0) {
            expand_into(frontier, next, lo, hi);
            int out_lo = max(0, lo - 1), out_hi = min(H - 1, hi + 1);
            int next_lo = H, next_hi = -1;
            ++layer;
            for (int r = out_lo; r <= out_hi; ++r) {
                Word add = next[r] & ~seen[r];
                next[r] = add;
                if (!add) continue;
                seen[r] |= add;
                next_lo = min(next_lo, r);
                next_hi = r;
                for (Word x = add; x; x &= x - 1) dist[r * W + ctz(x)] = layer;
            }
            if (next_hi < 0) break;
            frontier.swap(next);
            lo = next_lo;
            hi = next_hi;
        }
    }

    /// @brief a から b への最短路を path に格納する / dist は再利用する作業領域で、到達できない場合は false
    bool shortest_path(
        pair<int,int> a,
        pair<int,int> b,
        vector<pair<int,int>> &path,
        vector<int> &dist
    ) const {
        TITAN_GRID_ASSERT(in_bounds(a.first, a.second));
        TITAN_GRID_ASSERT(in_bounds(b.first, b.second));
        path.clear();
        dist.assign(H * W, -1);
        if (!is_road(a.first, a.second) || !is_road(b.first, b.second)) return false;

        clear(seen);
        clear(frontier);
        seen[b.first] = bit(b.second);
        frontier[b.first] = bit(b.second);
        dist[b.first * W + b.second] = 0;

        bool found = a == b;
        int layer = 0, lo = b.first, hi = b.first;
        while (!found) {
            expand_into(frontier, next, lo, hi);
            int out_lo = max(0, lo - 1), out_hi = min(H - 1, hi + 1);
            int next_lo = H, next_hi = -1;
            ++layer;
            for (int r = out_lo; r <= out_hi; ++r) {
                Word add = next[r] & ~seen[r];
                next[r] = add;
                seen[r] |= add;
                if (add) {
                    next_lo = min(next_lo, r);
                    next_hi = r;
                }
                for (Word x = add; x; x &= x - 1) {
                    int c = ctz(x);
                    dist[r * W + c] = layer;
                }
            }
            found = out_lo <= a.first && a.first <= out_hi
                 && (next[a.first] & bit(a.second)) != 0;
            if (next_hi < 0) return false;
            if (!found) {
                frontier.swap(next);
                lo = next_lo;
                hi = next_hi;
            }
        }

        pair<int,int> cur = a;
        path.push_back(cur);
        while (cur != b) {
            int d = dist[cur.first * W + cur.second];
            bool moved = false;
            for (int k = 0; k < DIRS; ++k) {
                const GridOffset step_k = grid_direction(NB, k);
                int nr = cur.first + step_k.row_delta, nc = cur.second + step_k.column_delta;
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

    /// @brief a から b への最短路を path に格納する / 到達できない場合は false
    bool shortest_path(pair<int,int> a, pair<int,int> b, vector<pair<int,int>> &path) const {
        TITAN_GRID_ASSERT(in_bounds(a.first, a.second));
        TITAN_GRID_ASSERT(in_bounds(b.first, b.second));
        path.clear();
        if (!is_road(a.first, a.second) || !is_road(b.first, b.second)) return false;

        clear(seen);
        clear(frontier);
        seen[b.first] = bit(b.second);
        frontier[b.first] = bit(b.second);
        path_dist[b.first * W + b.second] = 0;

        bool found = a == b;
        int layer = 0, lo = b.first, hi = b.first;
        while (!found) {
            expand_into(frontier, next, lo, hi);
            int out_lo = max(0, lo - 1), out_hi = min(H - 1, hi + 1);
            int next_lo = H, next_hi = -1;
            ++layer;
            for (int r = out_lo; r <= out_hi; ++r) {
                Word add = next[r] & ~seen[r];
                next[r] = add;
                seen[r] |= add;
                if (add) {
                    next_lo = min(next_lo, r);
                    next_hi = r;
                }
                for (Word x = add; x; x &= x - 1) {
                    path_dist[r * W + ctz(x)] = layer;
                }
            }
            found = out_lo <= a.first && a.first <= out_hi
                 && (next[a.first] & bit(a.second)) != 0;
            if (next_hi < 0) return false;
            if (!found) {
                frontier.swap(next);
                lo = next_lo;
                hi = next_hi;
            }
        }

        pair<int,int> cur = a;
        path.push_back(cur);
        while (cur != b) {
            int d = path_dist[cur.first * W + cur.second];
            bool moved = false;
            for (int k = 0; k < DIRS; ++k) {
                const GridOffset step_k = grid_direction(NB, k);
                int nr = cur.first + step_k.row_delta, nc = cur.second + step_k.column_delta;
                if (!in_bounds(nr, nc) || !(seen[nr] & bit(nc))) continue;
                if (path_dist[nr * W + nc] != d - 1) continue;
                cur = {nr, nc};
                path.push_back(cur);
                moved = true;
                break;
            }
            assert(moved);
        }
        return true;
    }

    /// @brief 多始点の最短距離を dist に、最も近い始点の番号を src に格納する / 距離が同じ場合は最小の番号、到達できないセルは -1
    void bfs_nearest(const vector<pair<int,int>> &sources, vector<int> &dist, vector<int> &src) const {
        dist.assign(H * W, -1);
        src.assign(H * W, -1);
        clear(seen);
        int lo = H, hi = -1;
        int ns = sources.size();
        for (int i = 0; i < ns; ++i) {
            auto [r, c] = sources[i];
            TITAN_GRID_ASSERT(in_bounds(r, c));
            if (!is_road(r, c)) continue;
            int idx = r * W + c;
            if (dist[idx] == -1) {
                dist[idx] = 0;
                src[idx] = i;
                seen[r] |= bit(c);
                lo = min(lo, r);
                hi = max(hi, r);
            }
        }
        for (int r = 0; r < H; ++r) frontier[r] = seen[r];
        int layer = 0;
        while (hi >= 0) {
            expand_into(frontier, next, lo, hi);
            int out_lo = max(0, lo - 1), out_hi = min(H - 1, hi + 1);
            int next_lo = H, next_hi = -1;
            ++layer;
            for (int r = out_lo; r <= out_hi; ++r) {
                Word add = next[r] & ~seen[r];
                next[r] = add;
                if (!add) continue;
                seen[r] |= add;
                next_lo = min(next_lo, r);
                next_hi = r;
                for (Word x = add; x; x &= x - 1) {
                    int c = ctz(x);
                    int idx = r * W + c;
                    dist[idx] = layer;
                    int best = -1;
                    for (int k = 0; k < DIRS; ++k) {
                        const GridOffset step_k = grid_direction(NB, k);
                        int nr = r + step_k.row_delta, nc = c + step_k.column_delta;
                        if (nr < lo || nr > hi || nc < 0 || nc >= W) continue;
                        int nidx = nr * W + nc;
                        if (!(frontier[nr] & bit(nc))) continue;
                        if (best == -1 || src[nidx] < best) best = src[nidx];
                    }
                    src[idx] = best;
                }
            }
            if (next_hi < 0) break;
            frontier.swap(next);
            lo = next_lo;
            hi = next_hi;
        }
    }

    /// @brief 道のセルを頂点とするグラフの関節点を out に格納する / O(HW)
    void articulation_cells(Set &out) const {
        assert_set(out);
        clear(out);
        std::fill(dfs_disc.begin(), dfs_disc.end(), -1);
        std::fill(dfs_parent.begin(), dfs_parent.end(), -1);
        std::fill(dfs_next.begin(), dfs_next.end(), 0);
        std::fill(dfs_children.begin(), dfs_children.end(), 0);
        dfs_stack.clear();
        int timer = 0;
        for (int sr = 0; sr < H; ++sr) {
            for (Word bits = road[sr]; bits; bits &= bits - 1) {
                int sc = ctz(bits);
                int root = sr * W + sc;
                if (dfs_disc[root] != -1) continue;
                dfs_disc[root] = dfs_low[root] = timer++;
                dfs_stack.push_back(root);
                while (!dfs_stack.empty()) {
                    int u = dfs_stack.back();
                    int r = u / W, c = u % W;
                    if (dfs_next[u] < DIRS) {
                        int k = dfs_next[u]++;
                        const GridOffset step_k = grid_direction(NB, k);
                        int nr = r + step_k.row_delta, nc = c + step_k.column_delta;
                        if (!in_bounds(nr, nc) || !((road[nr] >> nc) & 1)) continue;
                        int v = nr * W + nc;
                        if (dfs_disc[v] == -1) {
                            dfs_parent[v] = u;
                            ++dfs_children[u];
                            dfs_disc[v] = dfs_low[v] = timer++;
                            dfs_stack.push_back(v);
                        } else if (v != dfs_parent[u]) {
                            dfs_low[u] = min(dfs_low[u], dfs_disc[v]);
                        }
                        continue;
                    }

                    dfs_stack.pop_back();
                    int p = dfs_parent[u];
                    if (p == -1) {
                        if (dfs_children[u] >= 2) out[r] |= bit(c);
                        continue;
                    }
                    dfs_low[p] = min(dfs_low[p], dfs_low[u]);
                    if (dfs_parent[p] != -1 && dfs_low[u] >= dfs_disc[p]) {
                        out[p / W] |= bit(p % W);
                    }
                }
            }
        }
    }

    /// @brief 各連結成分について f(component_set, size) を呼び出し、連結成分の個数を返す
    /// component_set は f の実行中だけ有効である
    /// f の中から同じ Bitboard の探索と連結成分の処理を呼び出さない
    template<class F>
    int for_each_component(F &&f) const {
        build_runs();
        int nruns = runs.size();
        component_head.assign(nruns, -1);
        component_next.resize(nruns);
        component_roots.clear();
        for (int i = 0; i < nruns; ++i) {
            int root = find_run(i);
            if (component_head[root] == -1) component_roots.push_back(root);
            component_next[i] = component_head[root];
            component_head[root] = i;
        }

        clear(comp);
        touched_rows.clear();
        for (int root : component_roots) {
            for (int r : touched_rows) comp[r] = Word(0);
            touched_rows.clear();
            for (int i = component_head[root]; i != -1; i = component_next[i]) {
                int r = runs[i].row;
                if (!comp[r]) touched_rows.push_back(r);
                comp[r] |= lowmask(runs[i].r) ^ lowmask(runs[i].l);
            }
            f(static_cast<const Set &>(comp), runs[root].size);
        }
        int result = component_roots.size();
        runs.clear();
        return result;
    }

    /// @brief 道のセルの連結成分の個数を返す
    int components() const {
        build_runs();
        int cnt = 0;
        for (int i = 0; i < (int)runs.size(); ++i)
            if (runs[i].parent == i) ++cnt;
        runs.clear();
        return cnt;
    }

    /// @brief 各道のセルに連結成分の番号を割り当て、連結成分の個数を返す / lab は長さ H*W で、壁のセルは -1
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

    /// @brief 最大の連結成分を out に格納し、そのセルの個数を返す
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
