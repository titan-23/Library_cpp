#include <array>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#if defined(TEST_BEAM_TURN_OPTIMIZED)
#include "titan_cpplib/ahc/beam_search/beam_search_turn_optimized.cpp"
#else
#include "titan_cpplib/ahc/beam_search/beam_search_turn.cpp"
#endif

using namespace std;

using ScoreType = int;
using HashType = uint64_t;
constexpr ScoreType INF = 1 << 28;

void require(bool condition, const string &message) {
    if (!condition) throw runtime_error(message);
}

struct LifeToken {
    static int live_count;

    LifeToken() { ++live_count; }
    LifeToken(const LifeToken &) { ++live_count; }
    LifeToken(LifeToken &&) noexcept { ++live_count; }
    LifeToken &operator=(const LifeToken &) = default;
    LifeToken &operator=(LifeToken &&) noexcept = default;
    ~LifeToken() { --live_count; }
};

int LifeToken::live_count = 0;

struct Action {
    LifeToken life;
    int edge = -1;
    int from = -1;
    int to = -1;
    int pre_turn = -1;
    int target_turn = -1;
    uint64_t payload_tag = 0;
    array<uint64_t, 24> large_payload{};
    vector<uint32_t> dynamic_payload;
    string reuse_marker;

    string to_string() const {
        return std::to_string(edge) + ":" + std::to_string(from) + ":" + std::to_string(to) + ":" +
               std::to_string(pre_turn) + ":" + std::to_string(target_turn) + ":" +
               std::to_string(payload_tag) + ":" + reuse_marker;
    }
};

struct Edge {
    int from;
    int to;
    int target_turn;
    ScoreType score;
    HashType hash;
    bool finished;
    bool valid;
    uint64_t payload_tag;
};

struct Scenario {
    string name;
    int max_turn;
    int beam_width;
    bool clear_hash_every_turn = true;
    int seen_hash_capacity_hint = 0;
    vector<vector<int>> outgoing = vector<vector<int>>(1);
    vector<int> node_turn = vector<int>(1, 0);
    vector<Edge> edges;

    int expected_status = -1;
    ScoreType expected_score = INF;
    int expected_turns = -1;
    bool check_expected_path = false;
    vector<int> expected_path;
};

class ScenarioBuilder {
private:
    HashType next_hash = 1000003;

public:
    Scenario scenario;

    ScenarioBuilder(string name, int max_turn, int beam_width) {
        scenario.name = move(name);
        scenario.max_turn = max_turn;
        scenario.beam_width = beam_width;
    }

    int add(int from, int target_turn, ScoreType score,
            HashType hash = numeric_limits<HashType>::max(), bool finished = false, bool valid = true) {
        require(0 <= from && from < (int)scenario.outgoing.size(), scenario.name + ": invalid parent");
        int to = (int)scenario.outgoing.size();
        scenario.outgoing.emplace_back();
        scenario.node_turn.push_back(target_turn);
        int edge = (int)scenario.edges.size();
        if (hash == numeric_limits<HashType>::max()) hash = next_hash++;
        uint64_t payload = 0x9e3779b97f4a7c15ULL * (uint64_t)(edge + 1) ^ (uint64_t)(target_turn + 17);
        scenario.edges.push_back({from, to, target_turn, score, hash, finished, valid, payload});
        scenario.outgoing[from].push_back(edge);
        return to;
    }

    void expect(flying_squirrel::BeamStatus status, ScoreType score, int turns, vector<int> path) {
        scenario.expected_status = (int)status;
        scenario.expected_score = score;
        scenario.expected_turns = turns;
        scenario.check_expected_path = true;
        scenario.expected_path = move(path);
    }

    Scenario finish() { return move(scenario); }
};

struct Event {
    char kind;
    int node;
    int turn;
    int edge;
    int aux;
    int64_t value;
};

struct Runtime {
    const Scenario *scenario = nullptr;
    vector<Event> events;
};

Runtime *active_runtime = nullptr;

uint64_t payload_word(const Edge &edge, int index) {
    uint64_t x = edge.payload_tag + 0x9e3779b97f4a7c15ULL * (uint64_t)(index + 1);
    x ^= x >> 30;
    x *= 0xbf58476d1ce4e5b9ULL;
    x ^= x >> 27;
    return x ^ (x >> 31);
}

void verify_action_payload(const Action &action, const Edge &edge, const string &where) {
    require(action.edge >= 0, where + ": missing edge");
    require(action.from == edge.from && action.to == edge.to, where + ": endpoint payload was corrupted");
    require(action.target_turn == edge.target_turn, where + ": target_turn was corrupted");
    require(action.payload_tag == edge.payload_tag, where + ": scalar payload was corrupted");
    require(action.reuse_marker == "enumerator-reuse", where + ": string payload was moved or corrupted");
    require(action.dynamic_payload.size() == 9, where + ": vector payload size was corrupted");
    for (int i = 0; i < 9; ++i) {
        uint32_t expected = (uint32_t)(payload_word(edge, i + 24) >> 11);
        require(action.dynamic_payload[i] == expected, where + ": vector payload was corrupted");
    }
    for (int i = 0; i < (int)action.large_payload.size(); ++i) {
        require(action.large_payload[i] == payload_word(edge, i), where + ": array payload was corrupted");
    }
}

class ScriptState {
public:
    int node = 0;
    int turn = 0;
    vector<int> path;

    void init() {
        node = 0;
        turn = 0;
        path.clear();
        active_runtime->events.push_back({'I', node, turn, -1, 0, 0});
    }

    template<class Submit>
    void enumerate_actions(const Action &last_action, Submit &&submit) const {
        const Scenario &scenario = *active_runtime->scenario;
        require(scenario.node_turn[node] == turn, scenario.name + ": node turn disagrees with State");
        if (path.empty()) {
            require(last_action.target_turn == -1, scenario.name + ": root did not receive the dummy Action");
        } else {
            require(last_action.edge == path.back(), scenario.name + ": previous Action edge mismatch");
            verify_action_payload(last_action, scenario.edges[path.back()], scenario.name + ": previous Action");
        }
        active_runtime->events.push_back({'E', node, turn, last_action.edge, 0,
                                          (int64_t)scenario.outgoing[node].size()});
        Action action;
        action.reuse_marker = "enumerator-reuse";
        for (int edge_id : scenario.outgoing[node]) {
            const Edge &edge = scenario.edges[edge_id];
            ScoreType threshold = submit.threshold(edge.target_turn);
            active_runtime->events.push_back({'Q', node, turn, edge_id, edge.target_turn, threshold});
            action.edge = edge_id;
            submit(action);
            // submit(Action&) is not allowed to consume a callback-owned reusable Action.
            require(action.reuse_marker == "enumerator-reuse",
                    scenario.name + ": submit moved from the reusable Action");
        }
    }

    tuple<ScoreType, HashType, bool> try_op(Action &action, const vector<ScoreType> &thresholds) const {
        const Scenario &scenario = *active_runtime->scenario;
        require(0 <= action.edge && action.edge < (int)scenario.edges.size(),
                scenario.name + ": try_op received an invalid edge");
        const Edge &edge = scenario.edges[action.edge];
        require(edge.from == node, scenario.name + ": try_op edge belongs to another node");
        require(action.reuse_marker == "enumerator-reuse",
                scenario.name + ": reusable Action was consumed before try_op");

        action.from = edge.from;
        action.to = edge.to;
        action.pre_turn = turn;
        action.target_turn = edge.target_turn;
        action.payload_tag = edge.payload_tag;
        for (int i = 0; i < (int)action.large_payload.size(); ++i) {
            action.large_payload[i] = payload_word(edge, i);
        }
        action.dynamic_payload.resize(9);
        for (int i = 0; i < 9; ++i) {
            action.dynamic_payload[i] = (uint32_t)(payload_word(edge, i + 24) >> 11);
        }

        ScoreType threshold = INF;
        if (0 <= edge.target_turn && edge.target_turn < (int)thresholds.size()) {
            threshold = thresholds[edge.target_turn];
        }
        ScoreType score = edge.valid ? edge.score : INF;
        active_runtime->events.push_back({'T', node, turn, action.edge, threshold, score});
        return {score, edge.hash, edge.finished};
    }

    void apply_op(const Action &action) {
        const Scenario &scenario = *active_runtime->scenario;
        require(0 <= action.edge && action.edge < (int)scenario.edges.size(),
                scenario.name + ": apply_op received an invalid edge");
        const Edge &edge = scenario.edges[action.edge];
        verify_action_payload(action, edge, scenario.name + ": apply_op");
        require(action.from == node && action.pre_turn == turn,
                scenario.name + ": apply_op source does not match State");
        active_runtime->events.push_back({'A', node, turn, action.edge, action.target_turn,
                                          (int64_t)action.payload_tag});
        node = action.to;
        turn = action.target_turn;
        path.push_back(action.edge);
    }

    void rollback(const Action &action) {
        const Scenario &scenario = *active_runtime->scenario;
        require(0 <= action.edge && action.edge < (int)scenario.edges.size(),
                scenario.name + ": rollback received an invalid edge");
        const Edge &edge = scenario.edges[action.edge];
        verify_action_payload(action, edge, scenario.name + ": rollback");
        require(node == action.to && turn == action.target_turn,
                scenario.name + ": rollback destination does not match State");
        require(!path.empty() && path.back() == action.edge,
                scenario.name + ": rollback path does not match Action");
        active_runtime->events.push_back({'R', node, turn, action.edge, action.pre_turn,
                                          (int64_t)action.payload_tag});
        path.pop_back();
        node = action.from;
        turn = action.pre_turn;
    }

    string get_state_info() const {
        return "{\"node\":" + std::to_string(node) + ",\"turn\":" + std::to_string(turn) + "}";
    }
};

// HashDict currently has uint64_t keys even when BeamSearchWithTree::HashType is wider.  Keep an explicit
// compatibility test: hashes that differ only above bit 63 must therefore be treated as duplicates.
#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
#endif
using WideHashType = unsigned __int128;
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

struct WideAction {
    int choice = -1;
    int from = -1;
    int to = -1;
    int pre_turn = -1;
    int target_turn = -1;

    string to_string() const { return std::to_string(choice); }
};

struct WideHashState {
    static vector<int> tried_choices;

    int node = 0;
    int turn = 0;
    vector<int> path;

    void init() {
        node = 0;
        turn = 0;
        path.clear();
        tried_choices.clear();
    }

    template<class Submit>
    void enumerate_actions(const WideAction &last_action, Submit &&submit) const {
        if (path.empty()) require(last_action.target_turn == -1, "wide hash: missing dummy Action");
        else require(last_action.choice == path.back(), "wide hash: previous Action mismatch");
        WideAction action;
        if (node == 0) {
            action.choice = 0;
            submit(action);
            action.choice = 1;
            submit(action);
        } else if (node == 1) {
            action.choice = 2;
            submit(action);
        } else if (node == 2) {
            action.choice = 3;
            submit(action);
        }
    }

    tuple<ScoreType, WideHashType, bool> try_op(WideAction &action,
                                                const vector<ScoreType> &) const {
        tried_choices.push_back(action.choice);
        action.from = node;
        action.pre_turn = turn;
        if (action.choice == 0) {
            require(node == 0, "wide hash: choice 0 parent mismatch");
            action.to = 1;
            action.target_turn = 1;
            return {0, (WideHashType(1) << 64) | 0x55, false};
        }
        if (action.choice == 1) {
            require(node == 0, "wide hash: choice 1 parent mismatch");
            action.to = 2;
            action.target_turn = 1;
            // Full 128-bit comparison would keep this path, which later reaches score -100.  The current
            // uint64_t HashDict key truncates it to the same key as choice 0 and rejects it as the worse one.
            return {1, (WideHashType(2) << 64) | 0x55, false};
        }
        if (action.choice == 2) {
            require(node == 1, "wide hash: choice 2 parent mismatch");
            action.to = 3;
            action.target_turn = 2;
            return {100, (WideHashType(3) << 64) | 0x66, false};
        }
        require(action.choice == 3 && node == 2, "wide hash: choice 3 parent mismatch");
        action.to = 4;
        action.target_turn = 2;
        return {-100, (WideHashType(4) << 64) | 0x77, false};
    }

    void apply_op(const WideAction &action) {
        require(action.from == node && action.pre_turn == turn, "wide hash: apply source mismatch");
        node = action.to;
        turn = action.target_turn;
        path.push_back(action.choice);
    }

    void rollback(const WideAction &action) {
        require(node == action.to && turn == action.target_turn, "wide hash: rollback target mismatch");
        require(!path.empty() && path.back() == action.choice, "wide hash: rollback path mismatch");
        path.pop_back();
        node = action.from;
        turn = action.pre_turn;
    }

    string get_state_info() const { return "{}"; }
};

vector<int> WideHashState::tried_choices;

struct CopyOnlyAction {
    int choice = -1;
    int from = -1;
    int to = -1;
    int pre_turn = -1;
    int target_turn = -1;

    CopyOnlyAction() = default;
    CopyOnlyAction(const CopyOnlyAction &) = default;
    CopyOnlyAction &operator=(const CopyOnlyAction &) = default;
    CopyOnlyAction(CopyOnlyAction &&) = delete;
    CopyOnlyAction &operator=(CopyOnlyAction &&) = delete;

    string to_string() const { return std::to_string(choice); }
};

struct CopyOnlyState {
    int node = 0;
    int turn = 0;
    vector<int> path;

    void init() {
        node = 0;
        turn = 0;
        path.clear();
    }

    template<class Submit>
    void enumerate_actions(const CopyOnlyAction &last_action, Submit &&submit) const {
        if (path.empty()) require(last_action.target_turn == -1, "copy-only: missing dummy Action");
        else require(last_action.choice == path.back(), "copy-only: previous Action mismatch");
        if (node < 3) {
            CopyOnlyAction action;
            action.choice = node;
            submit(action);
        }
    }

    tuple<ScoreType, HashType, bool> try_op(CopyOnlyAction &action,
                                            const vector<ScoreType> &) const {
        require(action.choice == node && 0 <= node && node < 3, "copy-only: invalid choice");
        action.from = node;
        action.to = node + 1;
        action.pre_turn = turn;
        action.target_turn = turn + 1;
        return {10 - node, (HashType)(7001 + node), false};
    }

    void apply_op(const CopyOnlyAction &action) {
        require(action.from == node && action.pre_turn == turn, "copy-only: apply source mismatch");
        node = action.to;
        turn = action.target_turn;
        path.push_back(action.choice);
    }

    void rollback(const CopyOnlyAction &action) {
        require(node == action.to && turn == action.target_turn, "copy-only: rollback target mismatch");
        require(!path.empty() && path.back() == action.choice, "copy-only: rollback path mismatch");
        path.pop_back();
        node = action.from;
        turn = action.pre_turn;
    }

    string get_state_info() const { return "{}"; }
};

class Digest {
private:
    uint64_t value = 1469598103934665603ULL;

public:
    void add_byte(uint8_t byte) {
        value ^= byte;
        value *= 1099511628211ULL;
    }

    void add_int(int64_t number) {
        uint64_t bits = (uint64_t)number;
        for (int i = 0; i < 8; ++i) add_byte((bits >> (8 * i)) & 255);
    }

    void add_string(const string &text) {
        add_int((int64_t)text.size());
        for (unsigned char byte : text) add_byte(byte);
    }

    uint64_t get() const { return value; }
};

struct Outcome {
    int status;
    ScoreType score;
    int turns_done;
    vector<Action> actions;
    bool has_final_state;
    int final_node;
    int final_turn;
    vector<int> final_path;
    vector<Event> events;
    vector<int> width_hist;
    long long target_step_sum;
    long long target_step_count;
    int meta_sample_count;
    long long count_active;
    string history;
};

uint64_t digest_outcome(const Outcome &outcome) {
    Digest digest;
    digest.add_int(outcome.status);
    digest.add_int(outcome.score);
    digest.add_int(outcome.turns_done);
    digest.add_int((int)outcome.actions.size());
    for (const Action &action : outcome.actions) {
        digest.add_int(action.edge);
        digest.add_int(action.from);
        digest.add_int(action.to);
        digest.add_int(action.pre_turn);
        digest.add_int(action.target_turn);
        digest.add_int(action.payload_tag);
        digest.add_string(action.reuse_marker);
        digest.add_int((int)action.dynamic_payload.size());
        for (uint32_t value : action.dynamic_payload) digest.add_int(value);
        for (uint64_t value : action.large_payload) digest.add_int(value);
    }
    digest.add_int(outcome.has_final_state);
    digest.add_int(outcome.final_node);
    digest.add_int(outcome.final_turn);
    digest.add_int((int)outcome.final_path.size());
    for (int edge : outcome.final_path) digest.add_int(edge);
    digest.add_int((int)outcome.width_hist.size());
    for (int width : outcome.width_hist) digest.add_int(width);
    digest.add_int(outcome.target_step_sum);
    digest.add_int(outcome.target_step_count);
    digest.add_int(outcome.meta_sample_count);
    digest.add_int(outcome.count_active);
    return digest.get();
}

uint64_t digest_events(const vector<Event> &events) {
    Digest digest;
    digest.add_int((int)events.size());
    for (const Event &event : events) {
        digest.add_byte(event.kind);
        digest.add_int(event.node);
        digest.add_int(event.turn);
        digest.add_int(event.edge);
        digest.add_int(event.aux);
        digest.add_int(event.value);
    }
    return digest.get();
}

uint64_t digest_text(const string &text) {
    Digest digest;
    digest.add_string(text);
    return digest.get();
}

string read_file(const filesystem::path &path) {
    ifstream stream(path, ios::binary);
    require((bool)stream, "could not open history file: " + path.string());
    return string(istreambuf_iterator<char>(stream), istreambuf_iterator<char>());
}

using TestedBeam = flying_squirrel::BeamSearchWithTree<ScoreType, HashType, Action, ScriptState, INF, false>;
using HistoryBeam = flying_squirrel::BeamSearchWithTree<ScoreType, HashType, Action, ScriptState, INF, true>;

template<bool materialize, class Beam>
Outcome execute_case_with_beam(Beam &beam, const Scenario &scenario,
                               const filesystem::path &history_directory, int run_index, bool record_history) {
    Runtime runtime;
    runtime.scenario = &scenario;
    active_runtime = &runtime;
    flying_squirrel::BeamParam param(scenario.max_turn, scenario.beam_width, -1, false,
                                     scenario.clear_hash_every_turn);
    param.seen_hash_capacity_hint = scenario.seen_hash_capacity_hint;
    filesystem::path history_path = history_directory / ("turn_history_" + to_string(run_index) + ".json");
    filesystem::remove(history_path);
    auto result = beam.template search<materialize>(param, false, record_history ? history_path.string() : "");
    require(result.elapsed_ms >= 0.0, scenario.name + ": elapsed time is negative");

    Outcome outcome;
    outcome.status = (int)result.status;
    outcome.score = result.score;
    outcome.turns_done = result.turns_done;
    outcome.actions = move(result.actions);
    outcome.has_final_state = (bool)result.final_state;
    outcome.final_node = result.final_state ? result.final_state->node : -1;
    outcome.final_turn = result.final_state ? result.final_state->turn : -1;
    if (result.final_state) outcome.final_path = result.final_state->path;
    outcome.events = move(runtime.events);
    outcome.width_hist = move(param.width_hist);
    outcome.target_step_sum = param.target_step_sum;
    outcome.target_step_count = param.target_step_count;
    outcome.meta_sample_count = param.meta_sample_count;
    outcome.count_active = param.count_active;
    if (record_history) {
        outcome.history = read_file(history_path);
        filesystem::remove(history_path);
    } else {
        require(!filesystem::exists(history_path), scenario.name + ": history unexpectedly written");
    }
    active_runtime = nullptr;
    return outcome;
}

void validate_outcome(const Scenario &scenario, const Outcome &outcome, bool materialize) {
    require(outcome.turns_done >= 0 && outcome.turns_done <= max(0, scenario.max_turn),
            scenario.name + ": turns_done is outside the legal range");
    int node = 0;
    int turn = 0;
    vector<int> edge_path;
    for (const Action &action : outcome.actions) {
        require(0 <= action.edge && action.edge < (int)scenario.edges.size(),
                scenario.name + ": result contains an invalid edge");
        const Edge &edge = scenario.edges[action.edge];
        verify_action_payload(action, edge, scenario.name + ": result Action");
        require(edge.from == node && action.pre_turn == turn,
                scenario.name + ": result is not a contiguous root path");
        require(edge.target_turn > turn && edge.target_turn <= scenario.max_turn,
                scenario.name + ": result target turn is invalid");
        node = edge.to;
        turn = edge.target_turn;
        edge_path.push_back(action.edge);
    }

    if (outcome.status == (int)flying_squirrel::BeamStatus::InvalidParameter) {
        require(outcome.actions.empty() && outcome.score == INF && outcome.turns_done == 0,
                scenario.name + ": invalid parameter result is malformed");
    } else if (outcome.status == (int)flying_squirrel::BeamStatus::NoCandidates) {
        // A prefix already committed to State remains in the result even if every suffix later dies.
        require(outcome.score == INF, scenario.name + ": NoCandidates result has a finite score");
    } else {
        require(!outcome.actions.empty() && outcome.score < INF,
                scenario.name + ": successful result has no finite path");
        const Edge &last = scenario.edges[outcome.actions.back().edge];
        require(outcome.score == last.score, scenario.name + ": result score is not the final edge score");
        if (outcome.status == (int)flying_squirrel::BeamStatus::Finished) {
            require(last.finished, scenario.name + ": Finished result does not end in a finished edge");
            require(outcome.turns_done == scenario.node_turn[last.from] + 1,
                    scenario.name + ": Finished turns_done does not match its expansion turn");
        } else {
            require(outcome.status == (int)flying_squirrel::BeamStatus::MaxTurnReached,
                    scenario.name + ": unknown BeamStatus");
            require(!last.finished, scenario.name + ": MaxTurnReached path ends in a finished edge");
        }
    }

    bool expected_state = materialize && !outcome.actions.empty();
    require(outcome.has_final_state == expected_state, scenario.name + ": final State presence mismatch");
    if (expected_state) {
        require(outcome.final_node == node && outcome.final_turn == turn,
                scenario.name + ": materialized State endpoint mismatch");
        require(outcome.final_path == edge_path, scenario.name + ": materialized State path mismatch");
    }

    if (scenario.expected_status != -1) {
        require(outcome.status == scenario.expected_status, scenario.name + ": expected status mismatch");
        require(outcome.score == scenario.expected_score, scenario.name + ": expected score mismatch");
        require(outcome.turns_done == scenario.expected_turns, scenario.name + ": expected turns mismatch");
    }
    if (scenario.check_expected_path) {
        require(edge_path == scenario.expected_path, scenario.name + ": expected path mismatch");
    }
}

template<bool record_history>
void print_outcome(const Scenario &scenario, const Outcome &outcome, const string &mode) {
    cout << scenario.name << ' ' << mode << ' ' << hex << setw(16) << setfill('0')
         << digest_outcome(outcome) << ' ' << setw(16) << digest_events(outcome.events) << dec << ' '
         << outcome.events.size();
    if constexpr (record_history) {
        cout << ' ' << hex << setw(16) << digest_text(outcome.history) << dec << ' ' << outcome.history.size();
    }
    cout << '\n';
}

template<bool materialize>
void run_fresh(const Scenario &scenario, const filesystem::path &history_directory,
               int &run_index, const string &mode) {
    TestedBeam beam;
    Outcome outcome = execute_case_with_beam<materialize>(beam, scenario, history_directory, run_index++, false);
    validate_outcome(scenario, outcome, materialize);
    print_outcome<false>(scenario, outcome, mode);
}

template<bool materialize>
void run_history(const Scenario &scenario, const filesystem::path &history_directory,
                 int &run_index, const string &mode) {
    HistoryBeam beam;
    Outcome outcome = execute_case_with_beam<materialize>(beam, scenario, history_directory, run_index++, true);
    validate_outcome(scenario, outcome, materialize);
    print_outcome<true>(scenario, outcome, mode);
}

template<bool materialize>
void run_reused(TestedBeam &beam, const Scenario &scenario, const filesystem::path &history_directory,
                int &run_index, const string &mode) {
    Outcome outcome = execute_case_with_beam<materialize>(beam, scenario, history_directory, run_index++, false);
    validate_outcome(scenario, outcome, materialize);
    print_outcome<false>(scenario, outcome, mode);
}

void run_wide_hash_case() {
    using WideHashBeam =
        flying_squirrel::BeamSearchWithTree<ScoreType, WideHashType, WideAction, WideHashState, INF, false>;
    WideHashBeam beam;
    flying_squirrel::BeamParam param(2, 2, -1, false, true);
    auto result = beam.search<true>(param, false);

    require(result.status == flying_squirrel::BeamStatus::MaxTurnReached,
            "wide hash: unexpected status");
    require(result.score == 100 && result.turns_done == 2, "wide hash: unexpected score or turns");
    require(result.actions.size() == 2 && result.actions[0].choice == 0 && result.actions[1].choice == 2,
            "wide hash: upper 64 bits unexpectedly distinguished duplicate hashes");
    require(WideHashState::tried_choices == vector<int>({0, 1, 2}),
            "wide hash: low-64 collision did not prune the second root path");
    require(result.final_state && result.final_state->node == 3 && result.final_state->turn == 2 &&
                result.final_state->path == vector<int>({0, 2}),
            "wide hash: final State mismatch");

    Digest digest;
    digest.add_int((int)result.status);
    digest.add_int(result.score);
    digest.add_int(result.turns_done);
    for (const WideAction &action : result.actions) {
        digest.add_int(action.choice);
        digest.add_int(action.from);
        digest.add_int(action.to);
        digest.add_int(action.pre_turn);
        digest.add_int(action.target_turn);
    }
    for (int choice : WideHashState::tried_choices) digest.add_int(choice);
    cout << "wide_hash_low64_collision explicit " << hex << setw(16) << setfill('0') << digest.get()
         << dec << ' ' << WideHashState::tried_choices.size() << '\n';
}

void run_copy_only_action_case() {
    using CopyOnlyBeam =
        flying_squirrel::BeamSearchWithTree<ScoreType, HashType, CopyOnlyAction, CopyOnlyState, INF, false>;
    CopyOnlyBeam beam;
    flying_squirrel::BeamParam param(3, 1, -1, false, true);
    auto result = beam.search<true>(param, false);

    require(result.status == flying_squirrel::BeamStatus::MaxTurnReached && result.score == 8 &&
                result.turns_done == 3,
            "copy-only: unexpected result metadata");
    require(result.actions.size() == 3 && result.actions[0].choice == 0 &&
                result.actions[1].choice == 1 && result.actions[2].choice == 2,
            "copy-only: result path mismatch");
    require(result.final_state && result.final_state->node == 3 && result.final_state->turn == 3 &&
                result.final_state->path == vector<int>({0, 1, 2}),
            "copy-only: final State mismatch");

    Digest digest;
    digest.add_int((int)result.status);
    digest.add_int(result.score);
    digest.add_int(result.turns_done);
    for (const CopyOnlyAction &action : result.actions) {
        digest.add_int(action.choice);
        digest.add_int(action.from);
        digest.add_int(action.to);
        digest.add_int(action.pre_turn);
        digest.add_int(action.target_turn);
    }
    cout << "copy_only_action explicit " << hex << setw(16) << setfill('0') << digest.get()
         << dec << ' ' << result.actions.size() << '\n';
}

vector<Scenario> make_hand_cases() {
    vector<Scenario> cases;
    {
        ScenarioBuilder builder("invalid_max_turn", 0, 3);
        builder.expect(flying_squirrel::BeamStatus::InvalidParameter, INF, 0, {});
        cases.push_back(builder.finish());
    }
    {
        ScenarioBuilder builder("invalid_width", 5, 0);
        builder.expect(flying_squirrel::BeamStatus::InvalidParameter, INF, 0, {});
        cases.push_back(builder.finish());
    }
    {
        ScenarioBuilder builder("no_candidates_root", 5, 3);
        builder.add(0, 1, 10, numeric_limits<HashType>::max(), false, false);
        builder.add(0, 0, 2);  // Invalid non-increasing target turn.
        builder.add(0, 6, 1);  // Outside max_turn.
        builder.expect(flying_squirrel::BeamStatus::NoCandidates, INF, 5, {});
        cases.push_back(builder.finish());
    }
    {
        ScenarioBuilder builder("no_candidates_after_one", 4, 2);
        builder.add(0, 1, 3);
        builder.expect(flying_squirrel::BeamStatus::NoCandidates, INF, 4, {});
        cases.push_back(builder.finish());
    }
    {
        ScenarioBuilder builder("same_score_strict_cutoff", 1, 3);
        int e0 = (int)builder.scenario.edges.size();
        builder.add(0, 1, 5, 11);
        builder.add(0, 1, 5, 12);
        builder.add(0, 1, 5, 13);
        builder.add(0, 1, 5, 14);
        builder.expect(flying_squirrel::BeamStatus::MaxTurnReached, 5, 1, {e0});
        cases.push_back(builder.finish());
    }
    {
        ScenarioBuilder builder("same_hash_improvement_and_churn", 1, 3);
        builder.add(0, 1, 8, 91);
        builder.add(0, 1, 7, 92);
        builder.add(0, 1, 6, 93);
        int best = (int)builder.scenario.edges.size();
        builder.add(0, 1, 4, 91);
        builder.add(0, 1, 5, 91);
        builder.add(0, 1, 3, 94);
        builder.expect(flying_squirrel::BeamStatus::MaxTurnReached, 3, 1, {best + 2});
        cases.push_back(builder.finish());
    }
    {
        ScenarioBuilder builder("target_jump_and_prefix", 8, 3);
        int e0 = (int)builder.scenario.edges.size();
        int n3 = builder.add(0, 3, 10);
        int e1 = (int)builder.scenario.edges.size();
        int n6 = builder.add(n3, 6, 8);
        int e2 = (int)builder.scenario.edges.size();
        builder.add(n6, 8, 2);
        builder.expect(flying_squirrel::BeamStatus::MaxTurnReached, 2, 8, {e0, e1, e2});
        cases.push_back(builder.finish());
    }
    {
        ScenarioBuilder builder("future_pool_eviction", 5, 2);
        int n1 = builder.add(0, 1, 0, 101);
        int n2 = builder.add(0, 2, 0, 102);
        builder.add(0, 5, 50, 150);
        builder.add(0, 5, 60, 160);
        builder.add(n1, 5, 40, 170);
        builder.add(n1, 5, 30, 150);  // Improve an existing future hash.
        int winner = (int)builder.scenario.edges.size();
        builder.add(n2, 5, 20, 180);  // Evict a candidate inserted at turn 1.
        builder.add(n2, 4, 99, 190);
        builder.expect(flying_squirrel::BeamStatus::MaxTurnReached, 20, 5, {1, winner});
        cases.push_back(builder.finish());
    }
    {
        ScenarioBuilder builder("global_seen_earlier_turn", 4, 3);
        builder.scenario.clear_hash_every_turn = false;
        builder.scenario.seen_hash_capacity_hint = 8;
        builder.add(0, 5, 10, 777);
        int n1 = builder.add(0, 1, 1, 101);
        int n2 = builder.add(0, 2, 1, 102);
        builder.add(n1, 4, 30, 777);  // Earlier target is accepted despite a worse score.
        int improved = (int)builder.scenario.edges.size();
        builder.add(n2, 4, 20, 777);  // Same target and lower score improves it.
        builder.add(n2, 4, 25, 777);  // Same target and worse score is globally rejected.
        builder.add(n2, 3, 100, 777); // Earlier target is accepted despite a worse score.
        builder.add(n2, 4, 5, 777);   // A later target is rejected even with a better score.
        builder.expect(flying_squirrel::BeamStatus::MaxTurnReached, 20, 4, {2, improved});
        cases.push_back(builder.finish());
    }
    {
        ScenarioBuilder builder("root_finished_multiple", 6, 3);
        builder.add(0, 1, 8);
        builder.add(0, 2, 9, 301, true);
        int best = (int)builder.scenario.edges.size();
        builder.add(0, 4, 3, 302, true);
        builder.add(0, 3, 3, 303, true); // Equal score must not replace the first winner.
        builder.expect(flying_squirrel::BeamStatus::Finished, 3, 1, {best});
        cases.push_back(builder.finish());
    }
    {
        ScenarioBuilder builder("middle_finished_multiple", 7, 3);
        int root_edge = (int)builder.scenario.edges.size();
        int n2 = builder.add(0, 2, 2);
        builder.add(n2, 6, 10, 401, true);
        int best = (int)builder.scenario.edges.size();
        builder.add(n2, 5, 4, 402, true);
        builder.add(n2, 3, 5);
        builder.expect(flying_squirrel::BeamStatus::Finished, 4, 3, {root_edge, best});
        cases.push_back(builder.finish());
    }
    {
        ScenarioBuilder builder("earliest_future_pool_result", 4, 3);
        int early = (int)builder.scenario.edges.size();
        builder.add(0, 4, 100, 501);
        builder.add(0, 6, 1, 502);
        builder.expect(flying_squirrel::BeamStatus::MaxTurnReached, 100, 4, {early});
        cases.push_back(builder.finish());
    }
    {
        ScenarioBuilder builder("heavy_future_churn", 5, 4);
        vector<int> parents;
        for (int i = 0; i < 4; ++i) parents.push_back(builder.add(0, 1, i, 600 + i));
        for (int p = 0; p < 4; ++p) {
            for (int j = 0; j < 24; ++j) {
                int score = 500 - p * 100 - j * 3;
                HashType hash = (j % 7 == 0) ? (HashType)(900 + j / 7) : (HashType)(1000 + p * 100 + j);
                builder.add(parents[p], 5, score, hash);
            }
        }
        cases.push_back(builder.finish());
    }
    {
        ScenarioBuilder builder("width_one_repeated_eviction", 1, 1);
        builder.add(0, 1, 10, 2001);
        builder.add(0, 1, 9, 2002);
        int best = (int)builder.scenario.edges.size();
        builder.add(0, 1, 8, 2003);
        builder.expect(flying_squirrel::BeamStatus::MaxTurnReached, 8, 1, {best});
        cases.push_back(builder.finish());
    }
    {
        ScenarioBuilder builder("finished_ignores_pool_threshold", 3, 1);
        builder.add(0, 1, 1, 2101);
        int finished = (int)builder.scenario.edges.size();
        builder.add(0, 1, 100, 2102, true);
        builder.expect(flying_squirrel::BeamStatus::Finished, 100, 1, {finished});
        cases.push_back(builder.finish());
    }
    {
        ScenarioBuilder builder("finished_after_committed_prefix", 6, 1);
        int e0 = (int)builder.scenario.edges.size();
        int n1 = builder.add(0, 1, 10, 2201);
        int e1 = (int)builder.scenario.edges.size();
        int n2 = builder.add(n1, 2, 9, 2202);
        int e2 = (int)builder.scenario.edges.size();
        int n3 = builder.add(n2, 3, 8, 2203);
        int e3 = (int)builder.scenario.edges.size();
        builder.add(n3, 5, 7, 2204, true);
        builder.expect(flying_squirrel::BeamStatus::Finished, 7, 4, {e0, e1, e2, e3});
        cases.push_back(builder.finish());
    }
    {
        ScenarioBuilder builder("no_candidates_after_committed_prefix", 4, 1);
        int e0 = (int)builder.scenario.edges.size();
        int n1 = builder.add(0, 1, 3, 2301);
        int n2 = builder.add(n1, 2, 2, 2302);
        (void)n2;
        builder.expect(flying_squirrel::BeamStatus::NoCandidates, INF, 4, {e0});
        cases.push_back(builder.finish());
    }
    return cases;
}

class DeterministicRng {
private:
    uint64_t x;

public:
    explicit DeterministicRng(uint64_t seed) : x(seed) {}

    uint64_t next() {
        x ^= x << 7;
        x ^= x >> 9;
        return x ^= x << 8;
    }

    int uniform(int limit) { return (int)(next() % (uint64_t)limit); }
};

vector<Scenario> make_random_cases(int count) {
    vector<Scenario> cases;
    DeterministicRng rng(0x3141592653589793ULL);
    for (int case_id = 0; case_id < count; ++case_id) {
        int max_turn = 4 + rng.uniform(7);
        int width = 1 + rng.uniform(7);
        ostringstream name;
        name << "random_" << setw(4) << setfill('0') << case_id;
        ScenarioBuilder builder(name.str(), max_turn, width);
        builder.scenario.clear_hash_every_turn = (case_id % 4 != 0);
        if (!builder.scenario.clear_hash_every_turn) builder.scenario.seen_hash_capacity_hint = 16;

        int next_node = 0;
        const int node_limit = 45 + rng.uniform(45);
        while (next_node < (int)builder.scenario.outgoing.size() &&
               (int)builder.scenario.outgoing.size() < node_limit) {
            int from = next_node++;
            int source_turn = builder.scenario.node_turn[from];
            if (source_turn >= max_turn) continue;
            int branch = rng.uniform(8);
            if (from == 0) branch = 2 + rng.uniform(7);
            for (int j = 0; j < branch && (int)builder.scenario.outgoing.size() < node_limit; ++j) {
                int target = source_turn + 1 + rng.uniform(max_turn - source_turn);
                ScoreType score = rng.uniform(401) - 200;
                HashType hash = (HashType)(1 + rng.uniform(31));
                bool valid = rng.uniform(17) != 0;
                bool finished = valid && source_turn > 0 && rng.uniform(37) == 0;
                builder.add(from, target, score, hash, finished, valid);
            }
        }
        cases.push_back(builder.finish());
    }
    return cases;
}

int main(int argc, char **argv) {
    try {
        require(argc == 3, "usage: beam_search_turn_differential WORK_DIR RANDOM_CASES");
        filesystem::path history_directory = argv[1];
        int random_case_count = stoi(argv[2]);
        require(random_case_count >= 0, "RANDOM_CASES must be nonnegative");
        filesystem::create_directories(history_directory);

        vector<Scenario> hand_cases = make_hand_cases();
        vector<Scenario> random_cases = make_random_cases(random_case_count);
        int run_index = 0;

        for (const Scenario &scenario : hand_cases) {
            run_fresh<false>(scenario, history_directory, run_index, "fresh-no-state");
            run_fresh<true>(scenario, history_directory, run_index, "fresh-state");
        }
        // History exercises accepted, rejected, evicted, prefix, and finished nodes without making every random
        // case write a file.
        for (int index : {5, 6, 7, 8, 10, 14, 15, 16}) {
            run_history<true>(hand_cases[index], history_directory, run_index, "history-state");
        }
        for (const Scenario &scenario : random_cases) {
            run_fresh<true>(scenario, history_directory, run_index, "random-state");
        }
        run_wide_hash_case();
        run_copy_only_action_case();

        // Reusing one Beam instance stresses pool shrinking, free Action slots, and global seen reset between runs.
        {
            TestedBeam reused_beam;
            vector<int> order = {12, 13, 5, 8, 8, 3, 7, 9, 6, 15, 16, 0, 11};
            for (int index : order) {
                run_reused<true>(reused_beam, hand_cases[index], history_directory, run_index, "reused-state");
            }
            if (!random_cases.empty()) {
                run_reused<false>(reused_beam, random_cases.front(), history_directory, run_index,
                                  "reused-random-no-state");
            }
        }

        require(active_runtime == nullptr, "active runtime leaked");
        require(LifeToken::live_count == 0,
                "Action lifetime imbalance: " + std::to_string(LifeToken::live_count) + " objects remain");
        return 0;
    } catch (const exception &error) {
        active_runtime = nullptr;
        cerr << "beam_search_turn differential test failure: " << error.what() << '\n';
        return 1;
    }
}
