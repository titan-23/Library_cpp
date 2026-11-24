/// https://github.com/titan-23/Library_cpp/blob/main/titan_cpplib/ahc/chokudai_search/chokudai_param.cpp
#pragma once
#include "titan_cpplib/ahc/beam_search/beam_param.cpp"

namespace flying_squirrel {

/// @brief ChokudaiSearch の設定 / BeamParam を渡しても探索できる
/// @note beam_width は各巡回で各世代から展開する候補の数 / time_limit は打ち切り時間 [ms] で、負の値は無制限を表す
/// @note ビーム幅の動的な調整とハッシュの破棄に関する設定は使用しない
struct ChokudaiParam : BeamParam {
    using BeamParam::BeamParam;
    /// @brief 世代を昇順に走査する回数 / -1 なら回数を制限せず、0 なら展開しない
    long long max_sweeps = -1;
};

} // namespace flying_squirrel
