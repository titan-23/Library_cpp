/// https://github.com/titan-23/Library_cpp/blob/main/titan_cpplib/ahc/grid/flat_bitboard.cpp
#pragma once

#include <algorithm>
#include <array>
#include <cassert>
#include <cstdint>
#include <ostream>
#include <string>
#include <utility>
#include <vector>
#include "titan_cpplib/ahc/grid/grid_common.cpp"
#include "titan_cpplib/others/bit.cpp"
using namespace std;

namespace titan23 {

/// @brief グリッド全体を 1 つの Word で保持する Bitboard / H*W <= sizeof(Word)*8 が前提
/// セル (r,c) はビット r*W+c に対応する
/// 探索の処理は内部の作業領域を共有するため、同じインスタンスで並列に実行できない
template<class Word, Neighborhood NB = Neighborhood::Four>
class FlatBitboard {
public:
    using Set = Word;
    static_assert(sizeof(Word) == 8 || sizeof(Word) == 16, "Word must be uint64_t or __uint128_t");
    static_assert(Word(-1) > Word(0), "Word must be unsigned");

private:
    int H, W, N;
    Word FULL, LEFT_COL, RIGHT_COL;
    Word road;
    mutable Word frontier, next, seen, comp;
    mutable array<int, sizeof(Word) * 8> path_dist;

    static constexpr int DIRS = NB == Neighborhood::Four ? 4 : 8;

    static constexpr int word_bits() { return (int)sizeof(Word) * 8; }

    Word lowmask(int n) const {
        if (n >= word_bits()) return ~Word(0);
        return (Word(1) << n) - 1;
    }

    int index(int r, int c) const { return r * W + c; }
    Word bit_index(int i) const { return Word(1) << i; }
    Word bit(int r, int c) const { return bit_index(index(r, c)); }

    bool in_bounds(int r, int c) const {
        return 0 <= r && r < H && 0 <= c && c < W;
    }

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

    Word move_left(Word s) const {
        return (s & ~LEFT_COL) >> 1;
    }

    Word move_right(Word s) const {
        return ((s & ~RIGHT_COL) << 1) & FULL;
    }

    Word move_up(Word s) const {
        if (W == 0 || W >= word_bits()) return Word(0);
        return s >> W;
    }

    Word move_down(Word s) const {
        if (W == 0 || W >= word_bits()) return Word(0);
        return (s << W) & FULL;
    }

    Word expand_raw(Word s) const {
        Word l = move_left(s);
        Word r = move_right(s);
        Word x = l | r | move_up(s) | move_down(s);
        if constexpr (NB == Neighborhood::Eight) {
            Word horizontal = l | r;
            x |= move_up(horizontal) | move_down(horizontal);
        }
        return x & FULL;
    }

    Word eroded(Word s) const {
        s &= FULL;
        Word x = s & move_left(s) & move_right(s) & move_up(s) & move_down(s);
        if constexpr (NB == Neighborhood::Eight) {
            Word l = move_left(s);
            Word r = move_right(s);
            x &= move_up(l) & move_up(r) & move_down(l) & move_down(r);
        }
        return x & FULL;
    }

    Word flood_closure(Word sources) const {
        Word out = sources & road;
        Word fr = out;
        while (fr) {
            Word add = expand_raw(fr) & road & ~out;
            out |= add;
            fr = add;
        }
        return out;
    }

    int distance_points(int r0, int c0, int r1, int c1) const {
        if (!is_road(r0, c0) || !is_road(r1, c1)) return -1;
        int i0 = index(r0, c0), i1 = index(r1, c1);
        if (i0 == i1) return 0;
        Word target = bit_index(i1);
        seen = frontier = bit_index(i0);
        int d = 0;
        while (frontier) {
            next = expand_raw(frontier) & road & ~seen;
            ++d;
            if (next & target) return d;
            seen |= next;
            frontier = next;
        }
        return -1;
    }

    bool connected_points(int r0, int c0, int r1, int c1) const {
        if (!is_road(r0, c0) || !is_road(r1, c1)) return false;
        int i0 = index(r0, c0), i1 = index(r1, c1);
        if (i0 == i1) return true;
        Word target = bit_index(i1);
        seen = frontier = bit_index(i0);
        while (frontier) {
            next = expand_raw(frontier) & road & ~seen;
            if (next & target) return true;
            seen |= next;
            frontier = next;
        }
        return false;
    }

public:
    /// @brief 1 であるビットの個数を返す
    int popcount(Word x) const {
        return titan23::popcount(x);
    }

    /// @brief 1 であるビットのうち最下位の位置を返す / x != 0 が前提
    int ctz(Word x) const {
        return titan23::countr_zero(x);
    }

    /// @brief 1 であるビットのうち最上位の位置を返す / x != 0 が前提
    int msb(Word x) const {
        return titan23::bit_length(x) - 1;
    }

    FlatBitboard()
        : H(0), W(0), N(0), FULL(0), LEFT_COL(0), RIGHT_COL(0), road(0),
          frontier(0), next(0), seen(0), comp(0) {}

    /// @brief h*w の盤面を作成し、全セルを道にする
    FlatBitboard(int h, int w) { resize(h, w); }

    /// @brief 盤面を h*w の大きさで再作成し、全セルを道にする / O(H)
    void resize(int h, int w) {
        assert(0 <= h && 0 <= w);
        assert((long long)h * w <= word_bits());
        H = h;
        W = w;
        N = h * w;
        FULL = lowmask(N);
        LEFT_COL = RIGHT_COL = Word(0);
        if (W > 0) {
            for (int r = 0; r < H; ++r) {
                LEFT_COL |= bit(r, 0);
                RIGHT_COL |= bit(r, W - 1);
            }
        }
        road = FULL;
        frontier = next = seen = comp = Word(0);
    }

    int height() const { return H; }
    int width() const { return W; }
    int cell_count() const { return N; }
    bool inside(int r, int c) const { return in_bounds(r, c); }

    /// @brief 道のセルの集合を返す / O(1)
    const Set &road_set() const { return road; }
    /// @brief 道のセルの個数を返す / O(1)
    int road_count() const { return count(road); }
    /// @brief 壁のセルの個数を返す / O(1)
    int wall_count() const { return N - road_count(); }
    /// @brief 壁のセルの集合を out に格納する / O(1)
    void wall_set(Set &out) const { out = FULL & ~road; }
    /// @brief 出力先として使用する空集合を作成する / O(1)
    Set new_set() const { return Word(0); }

    /// @brief 全セルを道にする / O(1)
    void open_all() { road = FULL; }

    /// @brief 全セルを壁にする / O(1)
    void block_all() { road = Word(0); }

    /// @brief grid の文字 wall_ch を壁、それ以外の文字を道として読み込む / O(HW)
    void from_grid(const vector<string> &grid, char wall_ch = '#') {
        assert((int)grid.size() == H);
        road = Word(0);
        for (int r = 0; r < H; ++r) {
            assert((int)grid[r].size() == W);
            for (int c = 0; c < W; ++c) {
                if (grid[r][c] != wall_ch) road |= bit(r, c);
            }
        }
    }

    /// @brief (r,c) を道にする / O(1)
    void set_road(int r, int c) {
        TITAN_GRID_ASSERT(in_bounds(r, c));
        road |= bit(r, c);
    }

    /// @brief (r,c) を壁にする / O(1)
    void set_wall(int r, int c) {
        TITAN_GRID_ASSERT(in_bounds(r, c));
        road &= ~bit(r, c);
    }

    /// @brief (r,c) が道であるかを返す / O(1)
    bool is_road(int r, int c) const {
        TITAN_GRID_ASSERT(in_bounds(r, c));
        return (road & bit(r, c)) != 0;
    }

    /// @brief 盤面を out に保存する / O(1)
    void snapshot(Set &out) const { out = road; }
    /// @brief 保存した盤面を復元する / O(1)
    void restore(const Set &snap) { road = snap & FULL; }

    /// @brief H, W, 壁の配置がすべて一致するかを返す
    bool operator==(const FlatBitboard &o) const {
        return H == o.H && W == o.W && road == o.road;
    }

    /// @brief 盤面が異なるかを返す
    bool operator!=(const FlatBitboard &o) const { return !(*this == o); }

    /// @brief 盤面を os に出力する / 壁は '#', 道は '.'
    friend ostream &operator<<(ostream &os, const FlatBitboard &b) {
        for (int r = 0; r < b.H; ++r) {
            for (int c = 0; c < b.W; ++c) {
                os << (b.is_road(r, c) ? '.' : '#');
            }
            os << '\n';
        }
        return os;
    }

    /// @brief 集合を os に出力する / 0 は '#', 1 は '.'
    void print(const Set &s, ostream &os) const {
        for (int r = 0; r < H; ++r) {
            for (int c = 0; c < W; ++c) {
                os << (test(s, r, c) ? '.' : '#');
            }
            os << '\n';
        }
    }

    /// @brief 集合を文字 one と文字 zero からなる文字列の配列に変換する / O(HW)
    void to_grid(const Set &s, vector<string> &grid, char one = '.', char zero = '#') const {
        grid.assign(H, string(W, zero));
        for (Word x = s & FULL; x; x &= x - 1) {
            int i = ctz(x);
            grid[i / W][i % W] = one;
        }
    }

    /// @brief 集合を空にする / O(1)
    void clear(Set &s) const { s = Word(0); }

    /// @brief (r,c) が集合に含まれるかを返す / O(1)
    bool test(const Set &s, int r, int c) const {
        TITAN_GRID_ASSERT(in_bounds(r, c));
        return (s & bit(r, c)) != 0;
    }

    /// @brief (r,c) を集合に追加する / O(1)
    void set(Set &s, int r, int c) const {
        TITAN_GRID_ASSERT(in_bounds(r, c));
        s |= bit(r, c);
    }

    /// @brief (r,c) を集合から削除する / O(1)
    void reset(Set &s, int r, int c) const {
        TITAN_GRID_ASSERT(in_bounds(r, c));
        s &= ~bit(r, c);
    }

    /// @brief (r,c) が集合に含まれるかどうかを反転する / O(1)
    void flip(Set &s, int r, int c) const {
        TITAN_GRID_ASSERT(in_bounds(r, c));
        s ^= bit(r, c);
    }

    /// @brief 集合に含まれるセルの個数を返す / O(1)
    int count(const Set &s) const { return popcount(s & FULL); }
    /// @brief a と b の両方に含まれるセルの個数を返す / O(1)
    int count_and(const Set &a, const Set &b) const { return popcount(a & b & FULL); }
    /// @brief 集合が空でないかを返す / O(1)
    bool any(const Set &s) const { return (s & FULL) != 0; }
    /// @brief 集合が空であるかを返す / O(1)
    bool none(const Set &s) const { return !any(s); }
    /// @brief a と b が一致するかを返す / O(1)
    bool equals(const Set &a, const Set &b) const { return ((a ^ b) & FULL) == 0; }
    /// @brief a と b に共通のセルがあるかを返す / O(1)
    bool intersects(const Set &a, const Set &b) const { return (a & b & FULL) != 0; }
    /// @brief a と b に共通のセルがないかを返す / O(1)
    bool disjoint(const Set &a, const Set &b) const { return !intersects(a, b); }
    /// @brief a が b の部分集合であるかを返す / O(1)
    bool is_subset(const Set &a, const Set &b) const { return (a & ~b & FULL) == 0; }

    /// @brief 集合の 64 ビットのハッシュ値を返す / 異なる集合で値が衝突する場合がある / O(1)
    uint64_t hash64(const Set &s) const {
        uint64_t shape = ((uint64_t)(uint32_t)H << 32) | (uint32_t)W;
        return mix64(hash_word(s & FULL) ^ mix64(shape));
    }

    /// @brief 道のセルの集合の 64 ビットのハッシュ値を返す / 異なる盤面で値が衝突する場合がある / O(1)
    uint64_t hash64() const { return hash64(road); }

    /// @brief 積集合 a & b を out に格納する / O(1)
    void band(const Set &a, const Set &b, Set &out) const { out = a & b & FULL; }
    /// @brief 和集合 a | b を out に格納する / O(1)
    void bor(const Set &a, const Set &b, Set &out) const { out = (a | b) & FULL; }
    /// @brief 対称差 a ^ b を out に格納する / O(1)
    void bxor(const Set &a, const Set &b, Set &out) const { out = (a ^ b) & FULL; }
    /// @brief 差集合 a \ b を out に格納する / O(1)
    void bdiff(const Set &a, const Set &b, Set &out) const { out = a & ~b & FULL; }
    /// @brief a &= b を計算する / O(1)
    void iand(Set &a, const Set &b) const { a &= b; }
    /// @brief a |= b を計算する / O(1)
    void ior(Set &a, const Set &b) const { a = (a | b) & FULL; }
    /// @brief a ^= b を計算する / O(1)
    void ixor(Set &a, const Set &b) const { a = (a ^ b) & FULL; }
    /// @brief a \= b を計算する / O(1)
    void idiff(Set &a, const Set &b) const { a &= ~b; }
    /// @brief 道のセルを全体集合とする補集合 road & ~a を out に格納する / O(1)
    void complement_into(const Set &a, Set &out) const { out = road & ~a; }

    /// @brief cells のセルからなる集合を out に格納する / 壁のセルは除外する
    void make_set(const vector<pair<int,int>> &cells, Set &out) const {
        out = Word(0);
        for (auto [r, c] : cells) {
            TITAN_GRID_ASSERT(in_bounds(r, c));
            out |= bit(r, c) & road;
        }
    }

    /// @brief 集合に含まれる各セル (r,c) について f(r,c) を呼び出す
    template<class F>
    void for_each(const Set &s, F &&f) const {
        for (Word x = s & FULL; x; x &= x - 1) {
            int i = ctz(x);
            f(i / W, i % W);
        }
    }

    /// @brief 集合に含まれるセルの座標を out に格納する
    void cells(const Set &s, vector<pair<int,int>> &out) const {
        out.clear();
        for_each(s, [&](int r, int c) { out.emplace_back(r, c); });
    }

    /// @brief 行優先の順で先頭のセルを (r,c) に格納する / 空集合なら false / O(1)
    bool first(const Set &s, int &r, int &c) const {
        Word x = s & FULL;
        if (!x) return false;
        int i = ctz(x);
        r = i / W;
        c = i % W;
        return true;
    }

    /// @brief 行優先の順で k 番目のセルを (r,c) に格納する / k は 0 始まりで、範囲外なら false
    bool kth_cell(const Set &s, int k, int &r, int &c) const {
        Word x = s & FULL;
        if (k < 0 || k >= popcount(x)) return false;
        while (k--) x &= x - 1;
        int i = ctz(x);
        r = i / W;
        c = i % W;
        return true;
    }

    /// @brief s を含む最小の半開区間の長方形 [r1,r2) x [c1,c2) を格納する / 空集合なら false
    bool bounding_box(const Set &s, int &r1, int &c1, int &r2, int &c2) const {
        Word x = s & FULL;
        if (!x) return false;
        r1 = H;
        c1 = W;
        r2 = c2 = 0;
        for (; x; x &= x - 1) {
            int i = ctz(x);
            int r = i / W, c = i % W;
            r1 = min(r1, r);
            c1 = min(c1, c);
            r2 = max(r2, r + 1);
            c2 = max(c2, c + 1);
        }
        return true;
    }

    /// @brief 半開区間の長方形 [r1,r2) x [c1,c2) の集合を out に格納する / O(H)
    void rect(int r1, int c1, int r2, int c2, Set &out) const {
        assert(0 <= r1 && r1 <= r2 && r2 <= H);
        assert(0 <= c1 && c1 <= c2 && c2 <= W);
        out = Word(0);
        Word cols = lowmask(c2) ^ lowmask(c1);
        if (!cols) return;
        for (int r = r1; r < r2; ++r) out |= cols << (r * W);
    }

    /// @brief s を (dr,dc) だけ平行移動した集合を out に格納する / 壁は考慮しない / O(H)
    void shift(const Set &s, int dr, int dc, Set &out) const {
        TITAN_GRID_ASSERT(&s != &out);
        out = Word(0);
        if (W == 0 || dc <= -W || dc >= W || dr <= -H || dr >= H) return;
        Word row_mask = lowmask(W);
        for (int r = 0; r < H; ++r) {
            int nr = r + dr;
            if (nr < 0 || nr >= H) continue;
            Word row = (s >> (r * W)) & row_mask;
            if (dc >= 0) row = (row << dc) & row_mask;
            else row >>= -dc;
            out |= row << (nr * W);
        }
    }

    /// @brief s を左へ 1 セル移動した集合を out に格納する / 壁は考慮しない / O(1)
    void shift_left(const Set &s, Set &out) const {
        TITAN_GRID_ASSERT(&s != &out);
        out = move_left(s);
    }

    /// @brief s を右へ 1 セル移動した集合を out に格納する / 壁は考慮しない / O(1)
    void shift_right(const Set &s, Set &out) const {
        TITAN_GRID_ASSERT(&s != &out);
        out = move_right(s);
    }

    /// @brief s を上へ 1 セル移動した集合を out に格納する / 壁は考慮しない / O(1)
    void shift_up(const Set &s, Set &out) const {
        TITAN_GRID_ASSERT(&s != &out);
        out = move_up(s);
    }

    /// @brief s を下へ 1 セル移動した集合を out に格納する / 壁は考慮しない / O(1)
    void shift_down(const Set &s, Set &out) const {
        TITAN_GRID_ASSERT(&s != &out);
        out = move_down(s);
    }

    /// @brief s を (dr,dc) だけ平行移動し、道のセルだけを out に格納する / O(H)
    void step(const Set &s, int dr, int dc, Set &out) const {
        TITAN_GRID_ASSERT(&s != &out);
        shift(s, dr, dc, out);
        out &= road;
    }

    /// @brief s の各セルから近傍へ 1 セル移動した先の道のセルを out に格納する / O(1)
    void expand(const Set &s, Set &out) const {
        TITAN_GRID_ASSERT(&s != &out);
        out = expand_raw(s) & road;
    }

    /// @brief s に隣接し、s に含まれない道のセルを out に格納する / O(1)
    void border(const Set &s, Set &out) const {
        TITAN_GRID_ASSERT(&s != &out);
        out = expand_raw(s) & road & ~s;
    }

    /// @brief s と、s に隣接するセルの和集合を out に格納する / road は考慮しない / O(1)
    void dilate(const Set &s, Set &out) const {
        TITAN_GRID_ASSERT(&s != &out);
        out = (s | expand_raw(s)) & FULL;
    }

    /// @brief s を盤面内で k 回膨張させた集合を out に格納する / road は考慮しない / O(k)
    void dilate(const Set &s, int k, Set &out) const {
        assert(0 <= k);
        TITAN_GRID_ASSERT(&s != &out);
        out = s & FULL;
        for (int step_i = 0; step_i < k; ++step_i) out |= expand_raw(out);
    }

    /// @brief 自身と近傍のセルがすべて s に含まれるセルを out に格納する / 盤面外のセルは s に含まれないものとして扱う / O(1)
    void erode(const Set &s, Set &out) const {
        TITAN_GRID_ASSERT(&s != &out);
        out = eroded(s);
    }

    /// @brief s のうち収縮で除かれるセルを out に格納する / 盤面外のセルは s に含まれないものとして扱う / O(1)
    void inner_border(const Set &s, Set &out) const {
        TITAN_GRID_ASSERT(&s != &out);
        out = s & ~eroded(s) & FULL;
    }

    /// @brief 座標の列 sources から到達できるセルの集合を out に格納する
    void flood(const vector<pair<int,int>> &sources, Set &out) const {
        make_set(sources, out);
        out = flood_closure(out);
    }

    /// @brief 集合 sources から到達できるセルの集合を out に格納する / sources と out には別の Set を渡す
    void flood(const Set &sources, Set &out) const {
        TITAN_GRID_ASSERT(&sources != &out);
        out = flood_closure(sources);
    }

    /// @brief (r,c) から到達できるセルの集合を out に格納する
    void flood(int r, int c, Set &out) const {
        TITAN_GRID_ASSERT(in_bounds(r, c));
        out = flood_closure(bit(r, c));
    }

    /// @brief 盤面の外周にある道のセルから到達できる道のセルを out に格納する
    void reachable_from_border(Set &out) const {
        if (H == 0 || W == 0) {
            out = Word(0);
            return;
        }
        Word boundary = LEFT_COL | RIGHT_COL | lowmask(W);
        if (H > 1) boundary |= lowmask(W) << ((H - 1) * W);
        out = flood_closure(boundary & road);
    }

    /// @brief 盤面の外周にある道のセルから到達できない道のセルを out に格納する
    void enclosed_road(Set &out) const {
        reachable_from_border(comp);
        out = road & ~comp & FULL;
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
            if (in_bounds(nr, nc) && (road & bit(nr, nc))) pattern |= 1u << k;
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
        TITAN_GRID_ASSERT(&sources != &out);
        out = sources & road;
        Word fr = out;
        for (int step_i = 0; step_i < max_steps && fr; ++step_i) {
            Word add = expand_raw(fr) & road & ~out;
            out |= add;
            fr = add;
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
        if (!is_road(r, c)) return -1;
        Word target = targets & road;
        seen = frontier = bit(r, c);
        int d = 0;
        while (frontier) {
            Word found = frontier & target;
            if (found) {
                int i = ctz(found);
                hit = {i / W, i % W};
                return d;
            }
            next = expand_raw(frontier) & road & ~seen;
            seen |= next;
            frontier = next;
            ++d;
        }
        return -1;
    }

    /// @brief (r,c) を含む連結成分を out に格納する / (r,c) が壁なら空集合
    void component(int r, int c, Set &out) const {
        TITAN_GRID_ASSERT(in_bounds(r, c));
        out = flood_closure(bit(r, c));
    }

    /// @brief (r,c) を含む連結成分のセルの個数を返す / (r,c) が壁なら 0
    int component_size(int r, int c) const {
        TITAN_GRID_ASSERT(in_bounds(r, c));
        return popcount(flood_closure(bit(r, c)));
    }

    /// @brief 座標の列 sources からの多始点の最短距離を dist に格納する / dist は長さ H*W で添字は r*W+c、到達できないセルは -1
    void bfs_dist(const vector<pair<int,int>> &sources, vector<int> &dist) const {
        make_set(sources, comp);
        bfs_dist(comp, dist);
    }

    /// @brief 集合 sources からの多始点の最短距離を dist に格納する / 到達できないセルは -1
    void bfs_dist(const Set &sources, vector<int> &dist) const {
        dist.assign(N, -1);
        seen = frontier = sources & road;
        for (Word x = frontier; x; x &= x - 1) dist[ctz(x)] = 0;
        int layer = 0;
        while (frontier) {
            next = expand_raw(frontier) & road & ~seen;
            if (!next) break;
            ++layer;
            for (Word x = next; x; x &= x - 1) dist[ctz(x)] = layer;
            seen |= next;
            frontier = next;
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
        dist.assign(N, -1);
        if (!is_road(a.first, a.second) || !is_road(b.first, b.second)) return false;

        int ia = index(a.first, a.second);
        int ib = index(b.first, b.second);
        Word target = bit_index(ia);
        seen = frontier = bit_index(ib);
        dist[ib] = 0;
        int layer = 0;
        while (!(seen & target)) {
            next = expand_raw(frontier) & road & ~seen;
            if (!next) return false;
            ++layer;
            for (Word x = next; x; x &= x - 1) dist[ctz(x)] = layer;
            seen |= next;
            frontier = next;
        }

        pair<int,int> cur = a;
        path.push_back(cur);
        while (cur != b) {
            int d = dist[index(cur.first, cur.second)];
            bool moved = false;
            for (int k = 0; k < DIRS; ++k) {
                const GridOffset step_k = grid_direction(NB, k);
                int nr = cur.first + step_k.row_delta, nc = cur.second + step_k.column_delta;
                if (!in_bounds(nr, nc) || dist[index(nr, nc)] != d - 1) continue;
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

        int ia = index(a.first, a.second);
        int ib = index(b.first, b.second);
        Word target = bit_index(ia);
        seen = frontier = bit_index(ib);
        path_dist[ib] = 0;
        int layer = 0;
        while (!(seen & target)) {
            next = expand_raw(frontier) & road & ~seen;
            if (!next) return false;
            ++layer;
            for (Word x = next; x; x &= x - 1) path_dist[ctz(x)] = layer;
            seen |= next;
            frontier = next;
        }

        pair<int,int> cur = a;
        path.push_back(cur);
        while (cur != b) {
            int d = path_dist[index(cur.first, cur.second)];
            bool moved = false;
            for (int k = 0; k < DIRS; ++k) {
                const GridOffset step_k = grid_direction(NB, k);
                int nr = cur.first + step_k.row_delta, nc = cur.second + step_k.column_delta;
                if (!in_bounds(nr, nc)) continue;
                int ni = index(nr, nc);
                if (!(seen & bit_index(ni)) || path_dist[ni] != d - 1) continue;
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
        dist.assign(N, -1);
        src.assign(N, -1);
        seen = Word(0);
        for (int id = 0; id < (int)sources.size(); ++id) {
            auto [r, c] = sources[id];
            TITAN_GRID_ASSERT(in_bounds(r, c));
            if (!is_road(r, c)) continue;
            int i = index(r, c);
            if (dist[i] == -1) {
                dist[i] = 0;
                src[i] = id;
                seen |= bit_index(i);
            }
        }

        frontier = seen;
        int layer = 0;
        while (frontier) {
            next = expand_raw(frontier) & road & ~seen;
            if (!next) break;
            ++layer;
            for (Word x = next; x; x &= x - 1) {
                int i = ctz(x);
                int r = i / W, c = i % W;
                int best = -1;
                for (int k = 0; k < DIRS; ++k) {
                    const GridOffset step_k = grid_direction(NB, k);
                    int nr = r + step_k.row_delta, nc = c + step_k.column_delta;
                    if (!in_bounds(nr, nc)) continue;
                    int ni = index(nr, nc);
                    if (!(frontier & bit_index(ni))) continue;
                    if (best == -1 || src[ni] < best) best = src[ni];
                }
                dist[i] = layer;
                src[i] = best;
            }
            seen |= next;
            frontier = next;
        }
    }

    /// @brief 道のセルを頂点とするグラフの関節点を out に格納する / O(HW)
    void articulation_cells(Set &out) const {
        out = Word(0);
        array<int, sizeof(Word) * 8> disc, low, parent, next_dir, children, stack;
        for (int i = 0; i < N; ++i) {
            disc[i] = -1;
            parent[i] = -1;
            next_dir[i] = children[i] = 0;
        }
        int timer = 0, stack_size = 0;
        for (Word roots = road; roots; roots &= roots - 1) {
            int root = ctz(roots);
            if (disc[root] != -1) continue;
            disc[root] = low[root] = timer++;
            stack[stack_size++] = root;
            while (stack_size) {
                int u = stack[stack_size - 1];
                int r = u / W, c = u % W;
                if (next_dir[u] < DIRS) {
                    int k = next_dir[u]++;
                    const GridOffset step_k = grid_direction(NB, k);
                    int nr = r + step_k.row_delta, nc = c + step_k.column_delta;
                    if (!in_bounds(nr, nc)) continue;
                    int v = index(nr, nc);
                    if (!(road & bit_index(v))) continue;
                    if (disc[v] == -1) {
                        parent[v] = u;
                        ++children[u];
                        disc[v] = low[v] = timer++;
                        stack[stack_size++] = v;
                    } else if (v != parent[u]) {
                        low[u] = min(low[u], disc[v]);
                    }
                    continue;
                }

                --stack_size;
                int p = parent[u];
                if (p == -1) {
                    if (children[u] >= 2) out |= bit_index(u);
                    continue;
                }
                low[p] = min(low[p], low[u]);
                if (parent[p] != -1 && low[u] >= disc[p]) out |= bit_index(p);
            }
        }
    }

    /// @brief 各連結成分について f(component_set, size) を呼び出し、連結成分の個数を返す
    /// component_set は f の実行中だけ有効である
    /// f の中から同じ FlatBitboard の探索と連結成分の処理を呼び出さない
    template<class F>
    int for_each_component(F &&f) const {
        Word rem = road;
        int result = 0;
        while (rem) {
            comp = flood_closure(bit_index(ctz(rem)));
            f(static_cast<const Set &>(comp), popcount(comp));
            rem &= ~comp;
            ++result;
        }
        return result;
    }

    /// @brief 道のセルの連結成分の個数を返す
    int components() const {
        Word isolated = road & ~expand_raw(road);
        Word rem = road & ~isolated;
        int count = popcount(isolated);
        while (rem) {
            comp = flood_closure(bit_index(ctz(rem)));
            rem &= ~comp;
            ++count;
        }
        return count;
    }

    /// @brief 各道のセルに連結成分の番号を割り当て、連結成分の個数を返す / lab は長さ H*W で、壁のセルは -1
    int label(vector<int> &lab) const {
        lab.assign(N, -1);
        Word rem = road;
        Word isolated = road & ~expand_raw(road);
        int id = 0;
        while (rem) {
            Word seed = bit_index(ctz(rem));
            comp = (seed & isolated) ? seed : flood_closure(seed);
            for (Word x = comp; x; x &= x - 1) lab[ctz(x)] = id;
            rem &= ~comp;
            ++id;
        }
        return id;
    }

    /// @brief 最大の連結成分を out に格納し、そのセルの個数を返す
    int largest_component(Set &out) const {
        Word isolated = road & ~expand_raw(road);
        Word rem = road & ~isolated;
        out = isolated ? isolated & (Word(0) - isolated) : Word(0);
        int best = isolated ? 1 : 0;
        while (rem) {
            comp = flood_closure(bit_index(ctz(rem)));
            int size = popcount(comp);
            if (size > best) {
                best = size;
                out = comp;
            }
            rem &= ~comp;
        }
        return best;
    }
};

using FlatBitboard64      = FlatBitboard<uint64_t>;
using FlatBitboard128     = FlatBitboard<__uint128_t>;
using FlatBitboard64Diag  = FlatBitboard<uint64_t, Neighborhood::Eight>;
using FlatBitboard128Diag = FlatBitboard<__uint128_t, Neighborhood::Eight>;

}  // namespace titan23
