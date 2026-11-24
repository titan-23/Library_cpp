/// https://github.com/titan-23/Library_cpp/blob/main/titan_cpplib/ahc/grid/row_bitboard_base.cpp
#pragma once

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <limits>
#include <ostream>
#include <string>
#include <utility>
#include <vector>
#include "titan_cpplib/ahc/grid/grid_common.cpp"
#include "titan_cpplib/others/bit.cpp"
using namespace std;

namespace titan23 {

/// @brief 1 行を 1 つの Word で保持する Bitboard に共通の基底クラス / 盤面の形だけで結果が定まる集合演算を提供する
/// Set の各行で下位 W ビット以外のビットは常に 0 とする
/// どの演算も、この条件を満たす入力からこの条件を満たす結果を作成する
/// 作業領域を保持しないため、どの演算も同じインスタンスに対して同時に呼び出すことができる
template<class Word>
class RowBitboardBase {
public:
    using Set = vector<Word>;
    static_assert(sizeof(Word) == 8 || sizeof(Word) == 16, "Word must be uint64_t or __uint128_t");
    static_assert(Word(-1) > Word(0), "Word must be unsigned");

protected:
    int H = 0;
    int W = 0;
    Word FULL = 0;  // 下位 W ビットが 1

    static constexpr int word_bits() { return (int)sizeof(Word) * 8; }

    Word lowmask(int width) const {
        if (width >= word_bits()) return ~Word(0);
        return (Word(1) << width) - 1;
    }

    Word bit(int column) const { return Word(1) << column; }

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

    bool in_bounds(int row, int column) const {
        return 0 <= row && row < H && 0 <= column && column < W;
    }

    // TITAN_DEBUG の定義時に、行数が H であることを確認する
    void assert_set(const Set &s) const {
        TITAN_GRID_ASSERT((int)s.size() == H);
    }

    // TITAN_DEBUG の定義時に、行数が H であることと下位 W ビット以外のビットが 0 であることを確認する
    // 下位 W ビット以外のビットをマスクせずに参照する演算の入力に使用する
    void assert_clean(const Set &s) const {
#ifdef TITAN_DEBUG
        assert((int)s.size() == H);
        for (Word x : s) assert((x & ~FULL) == 0);
#else
        (void)s;
#endif
    }

    // H, W, FULL を設定する / 幅は Word のビット数以下とする
    void init_shape(int height, int width) {
        assert(0 <= height);
        assert(0 <= width && width <= word_bits());
        assert((long long)height * width <= numeric_limits<int>::max());
        H = height;
        W = width;
        FULL = lowmask(width);
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

    int height() const { return H; }
    int width() const { return W; }
    int cell_count() const { return H * W; }
    bool inside(int row, int column) const { return in_bounds(row, column); }

    /// @brief 出力先として使用する空集合を作成する / O(H)
    Set new_set() const { return Set(H, Word(0)); }

    /// @brief 集合を os に出力する / 0 は '#', 1 は '.'
    void print(const Set &s, ostream &os) const {
        assert_set(s);
        for (int row = 0; row < H; ++row) {
            for (int column = 0; column < W; ++column) {
                os << (((s[row] >> column) & 1) ? '.' : '#');
            }
            os << '\n';
        }
    }

    /// @brief 集合を文字 one と文字 zero からなる文字列の配列に変換する / O(HW)
    void to_grid(const Set &s, vector<string> &grid, char one = '.', char zero = '#') const {
        assert_set(s);
        grid.assign(H, string(W, zero));
        for (int row = 0; row < H; ++row) {
            for (Word x = s[row] & FULL; x; x &= x - 1) grid[row][ctz(x)] = one;
        }
    }

    /// @brief 集合を空にする / O(H)
    void clear(Set &s) const {
        assert_set(s);
        for (Word &x : s) x = Word(0);
    }

    /// @brief (row,column) が集合に含まれるかを返す / O(1)
    bool test(const Set &s, int row, int column) const {
        assert_set(s);
        TITAN_GRID_ASSERT(in_bounds(row, column));
        return (s[row] >> column) & 1;
    }

    /// @brief (row,column) を集合に追加する / O(1)
    void set(Set &s, int row, int column) const {
        assert_set(s);
        TITAN_GRID_ASSERT(in_bounds(row, column));
        s[row] |= bit(column);
    }

    /// @brief (row,column) を集合から削除する / O(1)
    void reset(Set &s, int row, int column) const {
        assert_set(s);
        TITAN_GRID_ASSERT(in_bounds(row, column));
        s[row] &= ~bit(column);
    }

    /// @brief (row,column) が集合に含まれるかどうかを反転する / O(1)
    void flip(Set &s, int row, int column) const {
        assert_set(s);
        TITAN_GRID_ASSERT(in_bounds(row, column));
        s[row] ^= bit(column);
    }

    /// @brief 集合に含まれるセルの個数を返す / O(H)
    int count(const Set &s) const {
        assert_clean(s);
        int res = 0;
        for (Word x : s) res += popcount(x);
        return res;
    }

    /// @brief a と b の両方に含まれるセルの個数を返す / O(H)
    int count_and(const Set &a, const Set &b) const {
        assert_clean(a);
        assert_clean(b);
        int res = 0;
        for (int row = 0; row < H; ++row) res += popcount(a[row] & b[row]);
        return res;
    }

    /// @brief 集合が空でないかを返す / O(H)
    bool any(const Set &s) const {
        assert_clean(s);
        for (Word x : s) if (x) return true;
        return false;
    }

    /// @brief 集合が空であるかを返す / O(H)
    bool none(const Set &s) const { return !any(s); }

    /// @brief a と b が一致するかを返す / O(H)
    bool equals(const Set &a, const Set &b) const {
        assert_clean(a);
        assert_clean(b);
        for (int row = 0; row < H; ++row) if (a[row] != b[row]) return false;
        return true;
    }

    /// @brief a と b に共通のセルがあるかを返す / O(H)
    bool intersects(const Set &a, const Set &b) const {
        assert_clean(a);
        assert_clean(b);
        for (int row = 0; row < H; ++row) if (a[row] & b[row]) return true;
        return false;
    }

    /// @brief a と b に共通のセルがないかを返す / O(H)
    bool disjoint(const Set &a, const Set &b) const {
        return !intersects(a, b);
    }

    /// @brief a が b の部分集合であるかを返す / O(H)
    bool is_subset(const Set &a, const Set &b) const {
        assert_clean(a);
        assert_clean(b);
        for (int row = 0; row < H; ++row) if (a[row] & ~b[row]) return false;
        return true;
    }

    /// @brief 集合の 64 ビットのハッシュ値を返す / 異なる集合で値が衝突する場合がある / O(H)
    uint64_t hash64(const Set &s) const {
        assert_set(s);
        uint64_t h = mix64(((uint64_t)(uint32_t)H << 32) | (uint32_t)W);
        for (int row = 0; row < H; ++row) {
            h = mix64(h ^ hash_word(s[row] & FULL));
        }
        return h;
    }

    /// @brief 積集合 a & b を out に格納する / O(H)
    void band(const Set &a, const Set &b, Set &out) const {
        assert_clean(a); assert_clean(b); assert_set(out);
        for (int row = 0; row < H; ++row) out[row] = a[row] & b[row];
    }

    /// @brief 和集合 a | b を out に格納する / O(H)
    void bor(const Set &a, const Set &b, Set &out) const {
        assert_clean(a); assert_clean(b); assert_set(out);
        for (int row = 0; row < H; ++row) out[row] = a[row] | b[row];
    }

    /// @brief 対称差 a ^ b を out に格納する / O(H)
    void bxor(const Set &a, const Set &b, Set &out) const {
        assert_clean(a); assert_clean(b); assert_set(out);
        for (int row = 0; row < H; ++row) out[row] = a[row] ^ b[row];
    }

    /// @brief 差集合 a \ b を out に格納する / O(H)
    void bdiff(const Set &a, const Set &b, Set &out) const {
        assert_clean(a); assert_clean(b); assert_set(out);
        for (int row = 0; row < H; ++row) out[row] = a[row] & ~b[row];
    }

    /// @brief a &= b を計算する / O(H)
    void iand(Set &a, const Set &b) const {
        assert_clean(a); assert_clean(b);
        for (int row = 0; row < H; ++row) a[row] &= b[row];
    }

    /// @brief a |= b を計算する / O(H)
    void ior(Set &a, const Set &b) const {
        assert_clean(a); assert_clean(b);
        for (int row = 0; row < H; ++row) a[row] |= b[row];
    }

    /// @brief a ^= b を計算する / O(H)
    void ixor(Set &a, const Set &b) const {
        assert_clean(a); assert_clean(b);
        for (int row = 0; row < H; ++row) a[row] ^= b[row];
    }

    /// @brief a \= b を計算する / O(H)
    void idiff(Set &a, const Set &b) const {
        assert_clean(a); assert_clean(b);
        for (int row = 0; row < H; ++row) a[row] &= ~b[row];
    }

    /// @brief 集合に含まれる各セル (row,column) について f(row,column) を呼び出す
    template<class F>
    void for_each(const Set &s, F &&f) const {
        assert_clean(s);
        for (int row = 0; row < H; ++row)
            for (Word x = s[row]; x; x &= x - 1) f(row, ctz(x));
    }

    /// @brief 集合に含まれるセルの座標を out に格納する
    void cells(const Set &s, vector<pair<int,int>> &out) const {
        out.clear();
        for_each(s, [&](int row, int column) { out.emplace_back(row, column); });
    }

    /// @brief 行優先の順で先頭のセルを (row,column) に格納する / 空集合なら false / O(H)
    bool first(const Set &s, int &row, int &column) const {
        assert_clean(s);
        for (int i = 0; i < H; ++i) {
            if (s[i]) {
                row = i;
                column = ctz(s[i]);
                return true;
            }
        }
        return false;
    }

    /// @brief 行優先の順で k 番目のセルを (row,column) に格納する / k は 0 始まりで、範囲外なら false
    bool kth_cell(const Set &s, int k, int &row, int &column) const {
        assert_clean(s);
        if (k < 0) return false;
        for (int i = 0; i < H; ++i) {
            int pc = popcount(s[i]);
            if (k >= pc) {
                k -= pc;
                continue;
            }
            Word x = s[i];
            while (k--) x &= x - 1;
            row = i;
            column = ctz(x);
            return true;
        }
        return false;
    }

    /// @brief s を含む最小の半開区間の長方形 [r1,r2) x [c1,c2) を格納する / 空集合なら false / O(H)
    bool bounding_box(const Set &s, int &r1, int &c1, int &r2, int &c2) const {
        assert_set(s);
        r1 = H;
        c1 = W;
        r2 = c2 = 0;
        for (int row = 0; row < H; ++row) {
            Word x = s[row] & FULL;
            if (!x) continue;
            r1 = min(r1, row);
            r2 = row + 1;
            c1 = min(c1, ctz(x));
            c2 = max(c2, msb(x) + 1);
        }
        return r1 < r2;
    }

    /// @brief 半開区間の長方形 [r1,r2) x [c1,c2) の集合を out に格納する / O(H)
    void rect(int r1, int c1, int r2, int c2, Set &out) const {
        assert(0 <= r1 && r1 <= r2 && r2 <= H);
        assert(0 <= c1 && c1 <= c2 && c2 <= W);
        assert_set(out);
        Word cols = lowmask(c2) ^ lowmask(c1);
        for (int row = 0; row < r1; ++row) out[row] = Word(0);
        for (int row = r1; row < r2; ++row) out[row] = cols;
        for (int row = r2; row < H; ++row) out[row] = Word(0);
    }

    /// @brief s を (dr,dc) だけ平行移動した集合を out に格納する / 壁は考慮しない / O(H)
    void shift(const Set &s, int dr, int dc, Set &out) const {
        assert_clean(s);
        assert_set(out);
        TITAN_GRID_ASSERT(&s != &out);
        if (dc <= -word_bits() || dc >= word_bits() || dr <= -H || dr >= H) { clear(out); return; }
        int begin = max(0, dr);
        int end = min(H, H + dr);
        for (int row = 0; row < begin; ++row) out[row] = Word(0);
        for (int row = end; row < H; ++row) out[row] = Word(0);
        if (dc >= 0) {
            for (int row = begin; row < end; ++row) out[row] = (s[row - dr] << dc) & FULL;
        } else {
            int sh = -dc;
            for (int row = begin; row < end; ++row) out[row] = s[row - dr] >> sh;
        }
    }

    /// @brief s を左へ 1 セル移動した集合を out に格納する / 壁は考慮しない / O(H)
    void shift_left(const Set &s, Set &out) const {
        assert_clean(s);
        assert_set(out);
        TITAN_GRID_ASSERT(&s != &out);
        for (int row = 0; row < H; ++row) out[row] = s[row] >> 1;
    }

    /// @brief s を右へ 1 セル移動した集合を out に格納する / 壁は考慮しない / O(H)
    void shift_right(const Set &s, Set &out) const {
        assert_clean(s);
        assert_set(out);
        TITAN_GRID_ASSERT(&s != &out);
        for (int row = 0; row < H; ++row) out[row] = (s[row] << 1) & FULL;
    }

    /// @brief s を上へ 1 セル移動した集合を out に格納する / 壁は考慮しない / O(H)
    void shift_up(const Set &s, Set &out) const {
        assert_clean(s);
        assert_set(out);
        TITAN_GRID_ASSERT(&s != &out);
        for (int row = 0; row + 1 < H; ++row) out[row] = s[row + 1];
        if (H) out[H - 1] = Word(0);
    }

    /// @brief s を下へ 1 セル移動した集合を out に格納する / 壁は考慮しない / O(H)
    void shift_down(const Set &s, Set &out) const {
        assert_clean(s);
        assert_set(out);
        TITAN_GRID_ASSERT(&s != &out);
        if (H) out[0] = Word(0);
        for (int row = 1; row < H; ++row) out[row] = s[row - 1];
    }
};

}  // namespace titan23
