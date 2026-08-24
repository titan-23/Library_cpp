/// https://github.com/titan-23/Library_cpp/blob/main/titan_cpplib/ahc/grid/edge_wall_bitboard.cpp
#pragma once

#include <algorithm>
#include <array>
#include <cassert>
#include <cstdint>
#include <limits>
#include <ostream>
#include <string>
#include <utility>
#include <vector>
#include "titan_cpplib/others/bit.cpp"
using namespace std;

#ifdef TITAN_DEBUG
#define TITAN_EDGE_WALL_BITBOARD_ASSERT(expr) assert(expr)
#else
#define TITAN_EDGE_WALL_BITBOARD_ASSERT(expr) ((void)sizeof(expr))
#endif

namespace titan23 {

enum class GridDirection : uint8_t {
    Up,
    Right,
    Down,
    Left,
};

/// @brief セル間の壁を持つ4近傍グリッド用Bitboard
///        1行を1つのWordで持ち、幅はWordのビット数以下とする
///        Setを出力するメソッドでは入力と出力に別のバッファを渡す
///        探索系は内部バッファを共有するため、同じインスタンスで同時に呼ばない
template<class Word>
class EdgeWallBitboard {
public:
    using Set = vector<Word>;
    static_assert(sizeof(Word) == 8 || sizeof(Word) == 16, "Word must be uint64_t or __uint128_t");
    static_assert(Word(-1) > Word(0), "Word must be unsigned");

private:
    static constexpr int DIRECTION_COUNT = 4;
    static constexpr int UP = 0;
    static constexpr int RIGHT = 1;
    static constexpr int DOWN = 2;
    static constexpr int LEFT = 3;

    int H = 0;
    int W = 0;
    int N = 0;
    Word FULL = 0;
    array<Set, DIRECTION_COUNT> movable;
    array<vector<int>, DIRECTION_COUNT> destination;
    mutable Set frontier;
    mutable Set next;
    mutable Set seen;
    mutable vector<int> path_dist;

    static constexpr int word_bits() { return (int)sizeof(Word) * 8; }

    static constexpr int direction_index(GridDirection direction) {
        return (int)direction;
    }

    Word lowmask(int width) const {
        if (width >= word_bits()) return ~Word(0);
        return (Word(1) << width) - 1;
    }

    Word bit(int column) const { return Word(1) << column; }

    bool in_bounds(int row, int column) const {
        return 0 <= row && row < H && 0 <= column && column < W;
    }

    void assert_set(const Set& set) const {
#ifdef TITAN_DEBUG
        TITAN_EDGE_WALL_BITBOARD_ASSERT((int)set.size() == H);
#else
        (void)set;
#endif
    }

    int checked_direction_index(GridDirection direction) const {
        int d = direction_index(direction);
        TITAN_EDGE_WALL_BITBOARD_ASSERT(0 <= d && d < DIRECTION_COUNT);
        return d;
    }

    int ctz(Word value) const {
        return titan23::countr_zero(value);
    }

    int popcount(Word value) const {
        return titan23::popcount(value);
    }

    void rebuild_destinations() {
        for (int d = 0; d < DIRECTION_COUNT; ++d) destination[d].resize(N);
        for (int row = 0; row < H; ++row) {
            for (int column = 0; column < W; ++column) {
                int index = row * W + column;
                destination[UP][index] = movable[UP][row] & bit(column) ? index - W : index;
                destination[RIGHT][index] = movable[RIGHT][row] & bit(column) ? index + 1 : index;
                destination[DOWN][index] = movable[DOWN][row] & bit(column) ? index + W : index;
                destination[LEFT][index] = movable[LEFT][row] & bit(column) ? index - 1 : index;
            }
        }
    }

    void expand_into(const Set& source, Set& out, int low_row, int high_row) const {
        if (low_row > high_row) return;
        int output_low = max(0, low_row - 1);
        int output_high = min(H - 1, high_row + 1);
        const Set& up = movable[UP];
        const Set& right = movable[RIGHT];
        const Set& down = movable[DOWN];
        const Set& left = movable[LEFT];
        for (int row = output_low; row <= output_high; ++row) {
            Word value = 0;
            if (low_row <= row && row <= high_row) {
                Word current = source[row];
                value = ((current & right[row]) << 1)
                      | ((current & left[row]) >> 1);
            }
            if (row + 1 <= high_row) value |= source[row + 1] & up[row + 1];
            if (low_row <= row - 1) value |= source[row - 1] & down[row - 1];
            out[row] = value & FULL;
        }
    }

public:
    EdgeWallBitboard() = default;
    EdgeWallBitboard(int height, int width) { resize(height, width); }

    /// @brief 盤面を作り直し、外周以外のすべての辺を開く
    void resize(int height, int width) {
        assert(0 <= height);
        assert(0 <= width && width <= word_bits());
        assert((long long)height * width <= numeric_limits<int>::max());
        H = height;
        W = width;
        N = height * width;
        FULL = lowmask(width);
        for (Set& set : movable) set.assign(H, Word(0));

        Word horizontal = width == 0 ? Word(0) : lowmask(width - 1);
        for (int row = 0; row < H; ++row) {
            movable[RIGHT][row] = horizontal;
            movable[LEFT][row] = (horizontal << 1) & FULL;
            if (row + 1 < H) movable[DOWN][row] = FULL;
            if (row > 0) movable[UP][row] = FULL;
        }

        frontier.assign(H, Word(0));
        next.assign(H, Word(0));
        seen.assign(H, Word(0));
        path_dist.resize(N);
        rebuild_destinations();
    }

    int height() const { return H; }
    int width() const { return W; }
    int cell_count() const { return N; }
    bool inside(int row, int column) const { return in_bounds(row, column); }

    /// @brief 右壁と下壁を読み取る '0' は通行可能、'1' は壁
    void from_walls(
        const vector<string>& right_walls,
        const vector<string>& down_walls
    ) {
        assert((int)right_walls.size() == H);
        assert((int)down_walls.size() == max(0, H - 1));
        for (Set& set : movable) fill(set.begin(), set.end(), Word(0));

        int right_width = max(0, W - 1);
        for (int row = 0; row < H; ++row) {
            assert((int)right_walls[row].size() == right_width);
            Word open = 0;
            for (int column = 0; column < right_width; ++column) {
                assert(right_walls[row][column] == '0' || right_walls[row][column] == '1');
                if (right_walls[row][column] == '0') open |= bit(column);
            }
            movable[RIGHT][row] = open;
            movable[LEFT][row] = (open << 1) & FULL;
        }

        for (int row = 0; row + 1 < H; ++row) {
            assert((int)down_walls[row].size() == W);
            Word open = 0;
            for (int column = 0; column < W; ++column) {
                assert(down_walls[row][column] == '0' || down_walls[row][column] == '1');
                if (down_walls[row][column] == '0') open |= bit(column);
            }
            movable[DOWN][row] = open;
            movable[UP][row + 1] = open;
        }
        rebuild_destinations();
    }

    bool can_move(int row, int column, GridDirection direction) const {
        TITAN_EDGE_WALL_BITBOARD_ASSERT(in_bounds(row, column));
        int d = checked_direction_index(direction);
        return (movable[d][row] & bit(column)) != 0;
    }

    /// @brief 壁なら同じindex、移動できるなら移動先indexを返す
    int destination_index(int index, GridDirection direction) const {
        TITAN_EDGE_WALL_BITBOARD_ASSERT(0 <= index && index < N);
        int d = checked_direction_index(direction);
        return destination[d][index];
    }

    Set new_set() const { return Set(H, Word(0)); }

    void clear(Set& set) const {
        assert_set(set);
        fill(set.begin(), set.end(), Word(0));
    }

    bool test(const Set& set, int row, int column) const {
        assert_set(set);
        TITAN_EDGE_WALL_BITBOARD_ASSERT(in_bounds(row, column));
        return (set[row] & bit(column)) != 0;
    }

    void set(Set& set, int row, int column) const {
        assert_set(set);
        TITAN_EDGE_WALL_BITBOARD_ASSERT(in_bounds(row, column));
        set[row] |= bit(column);
    }

    void reset(Set& set, int row, int column) const {
        assert_set(set);
        TITAN_EDGE_WALL_BITBOARD_ASSERT(in_bounds(row, column));
        set[row] &= ~bit(column);
    }

    void flip(Set& set, int row, int column) const {
        assert_set(set);
        TITAN_EDGE_WALL_BITBOARD_ASSERT(in_bounds(row, column));
        set[row] ^= bit(column);
    }

    int count(const Set& set) const {
        assert_set(set);
        int result = 0;
        for (Word row : set) result += popcount(row & FULL);
        return result;
    }

    int count_and(const Set& first, const Set& second) const {
        assert_set(first);
        assert_set(second);
        int result = 0;
        for (int row = 0; row < H; ++row) result += popcount(first[row] & second[row] & FULL);
        return result;
    }

    bool any(const Set& set) const {
        assert_set(set);
        for (Word row : set) {
            if (row & FULL) return true;
        }
        return false;
    }

    bool none(const Set& set) const { return !any(set); }

    bool equals(const Set& first, const Set& second) const {
        assert_set(first);
        assert_set(second);
        for (int row = 0; row < H; ++row) {
            if (((first[row] ^ second[row]) & FULL) != 0) return false;
        }
        return true;
    }

    bool intersects(const Set& first, const Set& second) const {
        assert_set(first);
        assert_set(second);
        for (int row = 0; row < H; ++row) {
            if (first[row] & second[row] & FULL) return true;
        }
        return false;
    }

    bool disjoint(const Set& first, const Set& second) const {
        return !intersects(first, second);
    }

    bool is_subset(const Set& first, const Set& second) const {
        assert_set(first);
        assert_set(second);
        for (int row = 0; row < H; ++row) {
            if (first[row] & ~second[row] & FULL) return false;
        }
        return true;
    }

    void band(const Set& first, const Set& second, Set& out) const {
        assert_set(first);
        assert_set(second);
        assert_set(out);
        for (int row = 0; row < H; ++row) out[row] = first[row] & second[row] & FULL;
    }

    void bor(const Set& first, const Set& second, Set& out) const {
        assert_set(first);
        assert_set(second);
        assert_set(out);
        for (int row = 0; row < H; ++row) out[row] = (first[row] | second[row]) & FULL;
    }

    void bxor(const Set& first, const Set& second, Set& out) const {
        assert_set(first);
        assert_set(second);
        assert_set(out);
        for (int row = 0; row < H; ++row) out[row] = (first[row] ^ second[row]) & FULL;
    }

    void bdiff(const Set& first, const Set& second, Set& out) const {
        assert_set(first);
        assert_set(second);
        assert_set(out);
        for (int row = 0; row < H; ++row) out[row] = first[row] & ~second[row] & FULL;
    }

    void iand(Set& first, const Set& second) const {
        assert_set(first);
        assert_set(second);
        for (int row = 0; row < H; ++row) first[row] &= second[row];
    }

    void ior(Set& first, const Set& second) const {
        assert_set(first);
        assert_set(second);
        for (int row = 0; row < H; ++row) first[row] = (first[row] | second[row]) & FULL;
    }

    void ixor(Set& first, const Set& second) const {
        assert_set(first);
        assert_set(second);
        for (int row = 0; row < H; ++row) first[row] = (first[row] ^ second[row]) & FULL;
    }

    void idiff(Set& first, const Set& second) const {
        assert_set(first);
        assert_set(second);
        for (int row = 0; row < H; ++row) first[row] &= ~second[row];
    }

    void complement_into(const Set& source, Set& out) const {
        assert_set(source);
        assert_set(out);
        for (int row = 0; row < H; ++row) out[row] = FULL & ~source[row];
    }

    void make_set(const vector<pair<int, int>>& cells, Set& out) const {
        assert_set(out);
        clear(out);
        for (auto [row, column] : cells) {
            TITAN_EDGE_WALL_BITBOARD_ASSERT(in_bounds(row, column));
            out[row] |= bit(column);
        }
    }

    template<class Function>
    void for_each(const Set& set, Function&& function) const {
        assert_set(set);
        for (int row = 0; row < H; ++row) {
            for (Word bits = set[row] & FULL; bits; bits &= bits - 1) {
                function(row, ctz(bits));
            }
        }
    }

    void cells(const Set& set, vector<pair<int, int>>& out) const {
        out.clear();
        out.reserve(count(set));
        for_each(set, [&](int row, int column) { out.emplace_back(row, column); });
    }

    /// @brief 移動に成功したセルの移動先だけをoutに書く
    void move_success(const Set& source, GridDirection direction, Set& out) const {
        assert_set(source);
        assert_set(out);
        TITAN_EDGE_WALL_BITBOARD_ASSERT(&source != &out);
        int d = checked_direction_index(direction);
        const Set& allowed = movable[d];
        if (d == UP) {
            for (int row = 0; row + 1 < H; ++row) out[row] = source[row + 1] & allowed[row + 1];
            if (H > 0) out[H - 1] = 0;
        } else if (d == RIGHT) {
            for (int row = 0; row < H; ++row) out[row] = ((source[row] & allowed[row]) << 1) & FULL;
        } else if (d == DOWN) {
            if (H > 0) out[0] = 0;
            for (int row = 1; row < H; ++row) out[row] = source[row - 1] & allowed[row - 1];
        } else {
            for (int row = 0; row < H; ++row) out[row] = (source[row] & allowed[row]) >> 1;
        }
    }

    /// @brief 指示方向へ移動し、壁に当たるセルは元の位置へ残す
    void advance(const Set& source, GridDirection direction, Set& out) const {
        assert_set(source);
        assert_set(out);
        TITAN_EDGE_WALL_BITBOARD_ASSERT(&source != &out);
        int d = checked_direction_index(direction);
        const Set& allowed = movable[d];
        if (d == UP) {
            for (int row = 0; row < H; ++row) {
                Word moved = row + 1 < H ? source[row + 1] & allowed[row + 1] : Word(0);
                out[row] = (moved | (source[row] & ~allowed[row])) & FULL;
            }
        } else if (d == RIGHT) {
            for (int row = 0; row < H; ++row) {
                out[row] = (((source[row] & allowed[row]) << 1)
                         | (source[row] & ~allowed[row])) & FULL;
            }
        } else if (d == DOWN) {
            for (int row = H - 1; row >= 0; --row) {
                Word moved = row > 0 ? source[row - 1] & allowed[row - 1] : Word(0);
                out[row] = (moved | (source[row] & ~allowed[row])) & FULL;
            }
        } else {
            for (int row = 0; row < H; ++row) {
                out[row] = (((source[row] & allowed[row]) >> 1)
                         | (source[row] & ~allowed[row])) & FULL;
            }
        }
    }

    /// @brief 指示を実行する場合と実行しない場合の到達候補をoutに書く
    void advance_uncertain(const Set& source, GridDirection direction, Set& out) const {
        assert_set(source);
        assert_set(out);
        TITAN_EDGE_WALL_BITBOARD_ASSERT(&source != &out);
        int d = checked_direction_index(direction);
        const Set& allowed = movable[d];
        if (d == UP) {
            for (int row = 0; row < H; ++row) {
                Word moved = row + 1 < H ? source[row + 1] & allowed[row + 1] : Word(0);
                out[row] = (source[row] | moved) & FULL;
            }
        } else if (d == RIGHT) {
            for (int row = 0; row < H; ++row) {
                out[row] = (source[row] | ((source[row] & allowed[row]) << 1)) & FULL;
            }
        } else if (d == DOWN) {
            for (int row = H - 1; row >= 0; --row) {
                Word moved = row > 0 ? source[row - 1] & allowed[row - 1] : Word(0);
                out[row] = (source[row] | moved) & FULL;
            }
        } else {
            for (int row = 0; row < H; ++row) {
                out[row] = (source[row] | ((source[row] & allowed[row]) >> 1)) & FULL;
            }
        }
    }

    /// @brief 1手で移動できる隣接セルをoutに書く
    void expand(const Set& source, Set& out) const {
        assert_set(source);
        assert_set(out);
        TITAN_EDGE_WALL_BITBOARD_ASSERT(&source != &out);
        expand_into(source, out, 0, H - 1);
    }

    /// @brief sourcesから到達できるセルをoutに書く
    void flood(const Set& sources, Set& out) const {
        assert_set(sources);
        assert_set(out);
        TITAN_EDGE_WALL_BITBOARD_ASSERT(&sources != &out);
        int low_row = H;
        int high_row = -1;
        for (int row = 0; row < H; ++row) {
            out[row] = sources[row] & FULL;
            frontier[row] = out[row];
            if (out[row]) {
                low_row = min(low_row, row);
                high_row = row;
            }
        }

        while (high_row >= 0) {
            expand_into(frontier, next, low_row, high_row);
            int output_low = max(0, low_row - 1);
            int output_high = min(H - 1, high_row + 1);
            int next_low = H;
            int next_high = -1;
            for (int row = output_low; row <= output_high; ++row) {
                Word added = next[row] & ~out[row];
                next[row] = added;
                out[row] |= added;
                if (added) {
                    next_low = min(next_low, row);
                    next_high = row;
                }
            }
            if (next_high < 0) break;
            frontier.swap(next);
            low_row = next_low;
            high_row = next_high;
        }
    }

    /// @brief 多始点最短距離をrow*W+columnの配列へ書く。未到達は-1
    void bfs_dist(const Set& sources, vector<int>& distance) const {
        assert_set(sources);
        distance.assign(N, -1);
        int low_row = H;
        int high_row = -1;
        for (int row = 0; row < H; ++row) {
            seen[row] = sources[row] & FULL;
            frontier[row] = seen[row];
            if (seen[row]) {
                low_row = min(low_row, row);
                high_row = row;
            }
            for (Word bits = seen[row]; bits; bits &= bits - 1) {
                distance[row * W + ctz(bits)] = 0;
            }
        }

        int layer = 0;
        while (high_row >= 0) {
            expand_into(frontier, next, low_row, high_row);
            int output_low = max(0, low_row - 1);
            int output_high = min(H - 1, high_row + 1);
            int next_low = H;
            int next_high = -1;
            ++layer;
            for (int row = output_low; row <= output_high; ++row) {
                Word added = next[row] & ~seen[row];
                next[row] = added;
                seen[row] |= added;
                if (!added) continue;
                next_low = min(next_low, row);
                next_high = row;
                for (Word bits = added; bits; bits &= bits - 1) {
                    distance[row * W + ctz(bits)] = layer;
                }
            }
            if (next_high < 0) break;
            frontier.swap(next);
            low_row = next_low;
            high_row = next_high;
        }
    }

    /// @brief startからgoalへの最短路を返す。到達不能ならfalse
    bool shortest_path(
        pair<int, int> start,
        pair<int, int> goal,
        vector<pair<int, int>>& path
    ) const {
        TITAN_EDGE_WALL_BITBOARD_ASSERT(in_bounds(start.first, start.second));
        TITAN_EDGE_WALL_BITBOARD_ASSERT(in_bounds(goal.first, goal.second));
        path.clear();
        int start_index = start.first * W + start.second;
        int goal_index = goal.first * W + goal.second;

        for (int row = 0; row < H; ++row) {
            seen[row] = 0;
            frontier[row] = 0;
        }
        seen[goal.first] = bit(goal.second);
        frontier[goal.first] = bit(goal.second);
        path_dist[goal_index] = 0;

        bool found = start_index == goal_index;
        int layer = 0;
        int low_row = goal.first;
        int high_row = goal.first;
        while (!found) {
            expand_into(frontier, next, low_row, high_row);
            int output_low = max(0, low_row - 1);
            int output_high = min(H - 1, high_row + 1);
            int next_low = H;
            int next_high = -1;
            ++layer;
            for (int row = output_low; row <= output_high; ++row) {
                Word added = next[row] & ~seen[row];
                next[row] = added;
                seen[row] |= added;
                if (!added) continue;
                next_low = min(next_low, row);
                next_high = row;
                for (Word bits = added; bits; bits &= bits - 1) {
                    path_dist[row * W + ctz(bits)] = layer;
                }
            }
            found = (seen[start.first] & bit(start.second)) != 0;
            if (next_high < 0) return false;
            if (!found) {
                frontier.swap(next);
                low_row = next_low;
                high_row = next_high;
            }
        }

        int current = start_index;
        path.push_back(start);
        while (current != goal_index) {
            int current_distance = path_dist[current];
            bool moved = false;
            for (int d = 0; d < DIRECTION_COUNT; ++d) {
                int following = destination[d][current];
                if (following == current || path_dist[following] != current_distance - 1) continue;
                int row = following / W;
                int column = following - row * W;
                if (!(seen[row] & bit(column))) continue;
                current = following;
                path.emplace_back(row, column);
                moved = true;
                break;
            }
            if (!moved) return false;
        }
        return true;
    }

    /// @brief 集合をone/zeroの文字盤へ変換する
    void to_grid(const Set& set, vector<string>& grid, char one = '.', char zero = '#') const {
        assert_set(set);
        grid.assign(H, string(W, zero));
        for_each(set, [&](int row, int column) { grid[row][column] = one; });
    }

    void print(const Set& set, ostream& output) const {
        vector<string> grid;
        to_grid(set, grid);
        for (const string& row : grid) output << row << '\n';
    }
};

using EdgeWallBitboard64 = EdgeWallBitboard<uint64_t>;
using EdgeWallBitboard128 = EdgeWallBitboard<__uint128_t>;

}  // namespace titan23

#undef TITAN_EDGE_WALL_BITBOARD_ASSERT
