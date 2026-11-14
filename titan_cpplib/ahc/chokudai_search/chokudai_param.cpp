/// https://github.com/titan-23/Library_cpp/blob/main/titan_cpplib/ahc/chokudai_search/chokudai_param.cpp
#pragma once
#include "titan_cpplib/ahc/beam_search/beam_param.cpp"

namespace flying_squirrel {

/// @brief ChokudaiSearch の設定 / BeamParam のままでも探索できる
/// @note beam_width は各巡回で各世代から展開する数、time_limit は打ち切り時間 [ms] で負なら無制限
/// @note 動的幅とハッシュ破棄の設定は使わない
struct ChokudaiParam : BeamParam {
    using BeamParam::BeamParam;
    /// @brief 世代を昇順に走査する回数 / -1 は無制限、0 は展開しない
    long long max_sweeps = -1;
};

} // namespace flying_squirrel
