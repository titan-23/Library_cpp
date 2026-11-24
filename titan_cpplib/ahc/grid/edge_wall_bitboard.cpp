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
#include "titan_cpplib/ahc/grid/row_bitboard_base.cpp"
#include "titan_cpplib/others/bit.cpp"
using namespace std;

namespace titan23 {

/// @brief セル間に壁がある 4 近傍のグリッド用の Bitboard / 1 行を 1 つの Word で保持し、幅は Word のビット数以下とする
/// Set を出力するメソッドには入力と別の Set を渡す
/// 探索の処理は内部の作業領域を共有するため、同じインスタンスで同時に呼び出さない
/// 盤面の形だけで結果が定まる集合演算 (count, band, for_each, shift など) は RowBitboardBase が提供する
template<class Word>
class EdgeWallBitboard : public RowBitboardBase<Word> {
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

    static constexpr int DIRECTION_COUNT = 4;
    static constexpr int UP = 0;
    static constexpr int RIGHT = 1;
    static constexpr int DOWN = 2;
    static constexpr int LEFT = 3;

    int N = 0;
    array<Set, DIRECTION_COUNT> movable;
    array<vector<int>, DIRECTION_COUNT> destination;
    mutable Set frontier;
    mutable Set next;
    mutable Set seen;
    mutable vector<int> path_dist;

    static constexpr int direction_index(GridDirection direction) {
        return (int)direction;
    }

    int checked_direction_index(GridDirection direction) const {
        int d = direction_index(direction);
        TITAN_GRID_ASSERT(0 <= d && d < DIRECTION_COUNT);
        return d;
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
    /// @brief height*width の盤面を作成し、外周以外のすべての辺を通行可能にする
    EdgeWallBitboard(int height, int width) { resize(height, width); }

    /// @brief 盤面を再作成し、外周以外のすべての辺を通行可能にする / O(HW)
    void resize(int height, int width) {
        init_shape(height, width);
        N = height * width;
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

    /// @brief 右の壁と下の壁を読み込む / '0' は通行可能、'1' は壁 / O(HW)
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

    /// @brief (row,column) から direction の方向へ移動できるかを返す / O(1)
    bool can_move(int row, int column, GridDirection direction) const {
        TITAN_GRID_ASSERT(in_bounds(row, column));
        int d = checked_direction_index(direction);
        return (movable[d][row] & bit(column)) != 0;
    }

    /// @brief index のセルから direction の方向へ移動した先の番号を返す / 壁がある場合は index / O(1)
    int destination_index(int index, GridDirection direction) const {
        TITAN_GRID_ASSERT(0 <= index && index < N);
        int d = checked_direction_index(direction);
        return destination[d][index];
    }

    /// @brief 盤面全体を全体集合とする補集合 FULL & ~source を out に格納する / O(H)
    void complement_into(const Set& source, Set& out) const {
        assert_set(source);
        assert_set(out);
        for (int row = 0; row < H; ++row) out[row] = FULL & ~source[row];
    }

    /// @brief cells のセルからなる集合を out に格納する
    void make_set(const vector<pair<int, int>>& cells, Set& out) const {
        assert_set(out);
        clear(out);
        for (auto [row, column] : cells) {
            TITAN_GRID_ASSERT(in_bounds(row, column));
            out[row] |= bit(column);
        }
    }

    /// @brief 移動に成功したセルの移動先だけを out に格納する / O(H)
    void move_success(const Set& source, GridDirection direction, Set& out) const {
        assert_set(source);
        assert_set(out);
        TITAN_GRID_ASSERT(&source != &out);
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

    /// @brief 指示の方向へ移動した集合を out に格納する / 壁があって移動できないセルは元の位置に残る / O(H)
    void advance(const Set& source, GridDirection direction, Set& out) const {
        assert_set(source);
        assert_set(out);
        TITAN_GRID_ASSERT(&source != &out);
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

    /// @brief 指示を実行する場合と実行しない場合の到達候補の和集合を out に格納する / O(H)
    void advance_uncertain(const Set& source, GridDirection direction, Set& out) const {
        assert_set(source);
        assert_set(out);
        TITAN_GRID_ASSERT(&source != &out);
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

    /// @brief 1 回の移動で到達できる隣接セルを out に格納する / O(H)
    void expand(const Set& source, Set& out) const {
        assert_set(source);
        assert_set(out);
        TITAN_GRID_ASSERT(&source != &out);
        expand_into(source, out, 0, H - 1);
    }

    /// @brief sources から到達できるセルの集合を out に格納する
    void flood(const Set& sources, Set& out) const {
        assert_set(sources);
        assert_set(out);
        TITAN_GRID_ASSERT(&sources != &out);
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

    /// @brief 多始点の最短距離を distance に格納する / 添字は row*W+column で、到達できないセルは -1
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

    /// @brief start から goal への最短路を path に格納する / 到達できない場合は false
    bool shortest_path(
        pair<int, int> start,
        pair<int, int> goal,
        vector<pair<int, int>>& path
    ) const {
        TITAN_GRID_ASSERT(in_bounds(start.first, start.second));
        TITAN_GRID_ASSERT(in_bounds(goal.first, goal.second));
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
};

using EdgeWallBitboard64 = EdgeWallBitboard<uint64_t>;
using EdgeWallBitboard128 = EdgeWallBitboard<__uint128_t>;

}  // namespace titan23
