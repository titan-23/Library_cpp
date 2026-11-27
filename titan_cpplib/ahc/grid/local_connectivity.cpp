/// https://github.com/titan-23/Library_cpp/blob/main/titan_cpplib/ahc/grid/local_connectivity.cpp
#pragma once

#include <array>
#include <bit>
#include <vector>
#include "titan_cpplib/ahc/grid/grid_common.cpp"
using namespace std;

namespace titan23 {

/// @brief 上をビット 0 とする時計回りの同色マスクに対し、局所判定の十分条件を満たすかを保持する
template<Neighborhood N>
inline constexpr auto removal_table = [] {
    constexpr bool four = N == Neighborhood::Four;
    constexpr auto offsets = grid_directions8;
    array<unsigned, 8> adjacent{};
    for (int i = 0; i < 8; ++i) {
        for (int j = 0; j < 8; ++j) {
            int dr = offsets[i].row_delta - offsets[j].row_delta;
            int dc = offsets[i].column_delta - offsets[j].column_delta;
            int d2 = dr * dr + dc * dc;
            if (d2 == 1 || (!four && d2 == 2)) adjacent[i] |= 1u << j;
        }
    }

    array<bool, 256> table{};
    for (unsigned mask = 0; mask < table.size(); ++mask) {
        unsigned targets = mask & (four ? 0x55u : 0xffu);
        unsigned reached = targets & (0u - targets); // 隣接マスのうち 1 マスを選択 (空集合なら 0)
        unsigned pending = reached;
        while (pending) {
            int i = countr_zero(pending);
            pending &= pending - 1;
            unsigned added = adjacent[i] & mask & ~reached;
            reached |= added;
            pending |= added;
        }
        table[mask] = (targets & ~reached) == 0;
    }
    return table;
}();

/// @brief 周囲 8 マスの配置から、マス (r, c) の削除後も同色領域が連結であると判定できる場合に true を返す / O(1)
/// @note 長方形の盤面と連結な同色領域を前提とし、最後の 1 マスの削除を許す
template<Neighborhood N, class Cell>
bool locally_removable(const vector<vector<Cell>>& grid, int r, int c) {
    int h = static_cast<int>(grid.size());
    int w = static_cast<int>(grid[r].size());
    unsigned mask = 0;
    for (int i = 0; i < 8; ++i) {
        auto [dr, dc] = grid_directions8[i];
        int y = r + dr, x = c + dc;
        if (0 <= y && y < h && 0 <= x && x < w && grid[y][x] == grid[r][c]) {
            mask |= 1u << i;
        }
    }
    return removal_table<N>[mask];
}

}  // namespace titan23
