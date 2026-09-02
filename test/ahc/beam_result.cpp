#include <cassert>
#include <cstdint>
#include <string>
#include <tuple>
#include <vector>

#if defined(TEST_TURN)
#include "titan_cpplib/ahc/beam_search/beam_search_turn.cpp"
#elif defined(TEST_BASELINE)
#include "test/ahc/beam_search_baseline.cpp"
#elif defined(TEST_PARENT)
#include "titan_cpplib/ahc/beam_search/beam_search_parent.cpp"
#elif defined(TEST_PARENT_COMPACT)
#include "titan_cpplib/ahc/beam_search/beam_search_parent_compact.cpp"
#elif defined(TEST_COMPOSE)
#include "titan_cpplib/ahc/beam_search/beam_search_compose.cpp"
#elif defined(TEST_RADIX)
#include "titan_cpplib/ahc/beam_search/beam_search_radix.cpp"
#elif defined(TEST_NAIVE)
#include "titan_cpplib/ahc/beam_search/naive_beam_search.cpp"
#else
#include "titan_cpplib/ahc/beam_search/beam_search.cpp"
#endif

using namespace std;

using ScoreType = int;
using HashType = uint64_t;
const ScoreType INF = 1 << 28;

#if defined(TEST_HISTORY)
const bool RECORD_HISTORY = true;
#else
const bool RECORD_HISTORY = false;
#endif

struct Action {
    int pre_value = 0;
    int nxt_value = 0;
    int pre_code = 0;
    int nxt_code = 0;
    int choice = 0;
    int target_turn = -1;

    bool compose(Action &child) {
        nxt_value = child.nxt_value;
        nxt_code = child.nxt_code;
        target_turn = child.target_turn;
        return true;
    }

    string to_string() const { return ""; }
};

class State {
public:
    static int finish_value;
    static int branch_count;
    static bool emit_action;
    int value = 0;
    int code = 0;

#if defined(TEST_NON_MOVABLE)
    State() = default;
    State(const State&) = delete;
    State& operator=(const State&) = delete;
    State(State&&) = delete;
    State& operator=(State&&) = delete;
#endif

    void init() {
        value = 0;
        code = 0;
    }

#if defined(TEST_TURN)
    template<class Submit>
    void enumerate_actions(const Action&, Submit &&submit) const {
        if (!emit_action) return;
        for (int choice = 0; choice < branch_count; ++choice) {
            Action action;
            action.choice = choice;
            submit(action);
        }
    }

    tuple<ScoreType, HashType, bool> try_op(Action &action, const vector<ScoreType>&) const {
        action.pre_value = value;
        action.nxt_value = value + 1;
        action.pre_code = code;
        action.nxt_code = code * 3 + action.choice + 1;
        action.target_turn = value + 1;
        HashType hash = ((HashType)action.nxt_value << 32) | action.nxt_code;
        return {action.nxt_value, hash, action.nxt_value == finish_value};
    }
#else
    template<class Submit>
    void enumerate_actions(int, const Action&, Submit &&submit) const {
        if (!emit_action) return;
        for (int choice = 0; choice < branch_count; ++choice) {
            Action action;
            action.choice = choice;
            submit(action);
        }
    }

    tuple<ScoreType, HashType, bool> try_op(Action &action, ScoreType) const {
        action.pre_value = value;
        action.nxt_value = value + 1;
        action.pre_code = code;
        action.nxt_code = code * 3 + action.choice + 1;
        HashType hash = ((HashType)action.nxt_value << 32) | action.nxt_code;
        return {action.nxt_value, hash, action.nxt_value == finish_value};
    }
#endif

    void apply_op(const Action &action) {
        value = action.nxt_value;
        code = action.nxt_code;
    }

    void rollback(const Action &action) {
        value = action.pre_value;
        code = action.pre_code;
    }
    string get_state_info() const { return "{}"; }
};

int State::finish_value = 3;
int State::branch_count = 1;
bool State::emit_action = true;

int main() {
    flying_squirrel::BeamParam param(5, 2, -1);

#if defined(TEST_RADIX)
    flying_squirrel::BeamSearchRadix<ScoreType, HashType, Action, State, INF> beam;
#elif defined(TEST_NAIVE)
    flying_squirrel::NaiveBeamSearch<ScoreType, HashType, Action, State, INF, RECORD_HISTORY> beam;
#else
    flying_squirrel::BeamSearchWithTree<ScoreType, HashType, Action, State, INF, RECORD_HISTORY> beam;
#endif

#if defined(TEST_NON_MOVABLE)
    auto result_without_state = beam.search<false>(param);
    assert(result_without_state.status == flying_squirrel::BeamStatus::Finished);
    assert(!result_without_state.final_state);
    return 0;
#else
    auto result = beam.search(param);
    assert(result.status == flying_squirrel::BeamStatus::Finished);
    assert(result.finished());
    assert(result.has_path());
    assert(result.score == 3);
    assert(result.turns_done == 3);
    assert(result.elapsed_ms >= 0.0);
    assert(result.final_state);
    assert(result.final_state->value == 3);

    auto result_without_state = beam.search<false>(param);
    assert(result_without_state.status == flying_squirrel::BeamStatus::Finished);
    assert(result_without_state.score == 3);
    assert(!result_without_state.final_state);

    State::finish_value = 10;
    State::branch_count = 2;
    flying_squirrel::BeamParam max_turn_param(3, 2, -1);
    auto max_turn = beam.search(max_turn_param);
    assert(max_turn.status == flying_squirrel::BeamStatus::MaxTurnReached);
    assert(max_turn.score == 3);
    assert(max_turn.turns_done == 3);
    assert(max_turn.final_state);
    assert(max_turn.final_state->value == 3);
    assert(max_turn.final_state->code == max_turn.actions.back().nxt_code);

    State::branch_count = 1;
    flying_squirrel::BeamParam forced_path_param(5, 2, -1);
    auto forced_path = beam.search(forced_path_param);
    assert(forced_path.status == flying_squirrel::BeamStatus::MaxTurnReached);
    assert(forced_path.score == 5);
    assert(forced_path.final_state);
    assert(forced_path.final_state->value == 5);
    assert(forced_path.final_state->code == forced_path.actions.back().nxt_code);

    State::emit_action = false;
    flying_squirrel::BeamParam no_candidates_param(3, 2, -1);
    auto no_candidates = beam.search(no_candidates_param);
    assert(no_candidates.status == flying_squirrel::BeamStatus::NoCandidates);
    assert(!no_candidates.has_path());
    assert(!no_candidates.final_state);

    flying_squirrel::BeamParam invalid_param(0, 2, -1);
    auto invalid = beam.search(invalid_param);
    assert(invalid.status == flying_squirrel::BeamStatus::InvalidParameter);
    assert(!invalid.has_path());
    assert(!invalid.final_state);
#endif
}
