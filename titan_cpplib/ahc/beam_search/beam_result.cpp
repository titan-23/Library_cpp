/// https://github.com/titan-23/Library_cpp/blob/main/titan_cpplib/ahc/beam_search/beam_result.cpp
#pragma once

#include <memory>
#include <utility>
#include <vector>
using namespace std;

namespace flying_squirrel {

enum class BeamStatus {
    Finished,
    MaxTurnReached,
    NoCandidates,
    InvalidParameter,
};

template<class ScoreType, class Action, class State>
struct BeamResult {
    vector<Action> actions;
    ScoreType score;
    int turns_done;
    double elapsed_ms;
    BeamStatus status;
    unique_ptr<State> final_state;

    bool has_path() const { return !actions.empty(); }

    bool finished() const { return status == BeamStatus::Finished; }
};

template<bool materialize_final_state, class State, class Action>
unique_ptr<State> make_final_state(State &state, const vector<Action> &actions, int applied_prefix = 0) {
    if constexpr (materialize_final_state) {
        for (int i = applied_prefix; i < (int)actions.size(); ++i) {
            state.apply_op(actions[i]);
        }
        return make_unique<State>(move(state));
    } else {
        (void)state;
        (void)actions;
        (void)applied_prefix;
        return nullptr;
    }
}

} // namespace flying_squirrel
