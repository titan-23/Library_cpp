/// https://github.com/titan-23/Library_cpp/blob/main/titan_cpplib/ahc/beam_search/beam_result.cpp
#pragma once

#include <memory>
#include <utility>
#include <vector>
using namespace std;

namespace flying_squirrel {

/// @brief ビームサーチの終了理由
enum class BeamStatus {
    Finished,
    MaxTurnReached,
    NoCandidates,
    InvalidParameter,
};

/// @brief ビームサーチの実行結果
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

/// @brief 必要な場合に未適用の操作を実行し、最終状態を生成する
/// @param applied_prefix state に適用済みの先頭操作数
/// @note materialize_final_state が true の場合、state は返却値へムーブされる
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
