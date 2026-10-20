/// https://github.com/titan-23/Library_cpp/blob/main/titan_cpplib/ahc/chokudai_search/chokudai_search_turn.cpp
#pragma once
#include "titan_cpplib/ahc/chokudai_search/chokudai_search_base.cpp"

namespace flying_squirrel {

/// @brief Action::target_turn で複数世代先へ進む、状態差分型 ChokudaiSearch
template<typename ScoreType, typename HashType, class Action, class State, ScoreType INF, bool record_history=false>
using ChokudaiSearchTurn = chokudai_detail::Search<ScoreType, HashType, Action, State, INF, true, record_history>;

template<typename ScoreType, typename HashType, class Action, class State, ScoreType INF, bool record_history=false>
using ChokudaiSearchWithTreeTurn = ChokudaiSearchTurn<ScoreType, HashType, Action, State, INF, record_history>;

} // namespace flying_squirrel
