/// https://github.com/titan-23/Library_cpp/blob/main/titan_cpplib/ahc/chokudai_search/chokudai_search.cpp
#pragma once
#include "titan_cpplib/ahc/chokudai_search/chokudai_search_base.cpp"

namespace flying_squirrel {

/// @brief 1 操作で 1 世代進む、状態差分型 ChokudaiSearch
template<typename ScoreType, typename HashType, class Action, class State, ScoreType INF, bool record_history=false>
using ChokudaiSearch = chokudai_detail::Search<ScoreType, HashType, Action, State, INF, false, record_history>;

template<typename ScoreType, typename HashType, class Action, class State, ScoreType INF, bool record_history=false>
using ChokudaiSearchWithTree = ChokudaiSearch<ScoreType, HashType, Action, State, INF, record_history>;

} // namespace flying_squirrel
