/// https://github.com/titan-23/Library_cpp/blob/main/titan_cpplib/ahc/grid/euler_contribution.cpp
#pragma once

#include <bit>
#include "titan_cpplib/ahc/grid/grid_common.cpp"
using namespace std;

namespace titan23 {

/// @brief 2 × 2 の配置を表す 4 ビットのマスクから、オイラー数の 4 倍に対する寄与を返す / O(1)
/// @note 左上、右上、左下、右下を順にビット 0 ～ 3 とし、対象色のマスのビットを 1 とする
constexpr int euler_contribution(unsigned mask, Neighborhood foreground) {
    if (mask == 0b1001 || mask == 0b0110)
        return foreground == Neighborhood::Four ? 2 : -2;
    int count = popcount(mask);
    return (count == 1) - (count == 3);
}

}  // namespace titan23
