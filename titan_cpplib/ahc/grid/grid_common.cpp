/// https://github.com/titan-23/Library_cpp/blob/main/titan_cpplib/ahc/grid/grid_common.cpp
#pragma once

#include <array>
#include <cassert>
#include <cstdint>
using namespace std;

// TITAN_DEBUG の定義時だけ assert を行う
// グリッドの各ファイルで使用するため #undef しない
#ifdef TITAN_DEBUG
#define TITAN_GRID_ASSERT(expr) assert(expr)
#else
#define TITAN_GRID_ASSERT(expr) ((void)sizeof(expr))
#endif

namespace titan23 {

/// @brief 盤面の近傍の種類 / Four は 4 近傍、Eight は 8 近傍
enum class Neighborhood {
    Four,
    Eight,
};

/// @brief 盤面上の方向またはセルの辺 / 順序は上、右、下、左
enum class GridDirection : uint8_t {
    Up,
    Right,
    Down,
    Left,
};

/// @brief 反対の方向を返す / 上と下、右と左を入れ替える
constexpr GridDirection opposite(GridDirection direction) {
    return (GridDirection)(((int)direction + 2) & 3);
}

/// @brief 1 回の移動での行と列の変化量
struct GridOffset {
    int row_delta;
    int column_delta;
};

/// @brief 4 近傍の移動 / 順序は GridDirection と同じ上、右、下、左
inline constexpr array<GridOffset, 4> grid_directions4 = {{
    {-1, 0},
    {0, 1},
    {1, 0},
    {0, -1},
}};

/// @brief 8 近傍の移動 / 順序は上から時計回りに上、右上、右、右下、下、左下、左、左上
inline constexpr array<GridOffset, 8> grid_directions8 = {{
    {-1, 0},
    {-1, 1},
    {0, 1},
    {1, 1},
    {1, 0},
    {1, -1},
    {0, -1},
    {-1, -1},
}};

/// @brief 4 近傍の移動を表す文字 / 順序は grid_directions4 と同じ
inline constexpr array<char, 4> grid_direction_chars4 = {'U', 'R', 'D', 'L'};

/// @brief 近傍の種類に応じた k 番目の移動を返す / Four では k < 4、Eight では k < 8 とする
constexpr GridOffset grid_direction(Neighborhood neighborhood, int k) {
    return neighborhood == Neighborhood::Four ? grid_directions4[k] : grid_directions8[k];
}

}  // namespace titan23
