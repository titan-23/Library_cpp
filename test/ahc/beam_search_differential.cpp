#include <algorithm>
#include <cstdlib>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#if defined(TEST_BEAM_BASELINE)
#include "test/ahc/beam_search_baseline.cpp"
#elif defined(TEST_BEAM_STANDARD)
#include "titan_cpplib/ahc/beam_search/beam_search.cpp"
#elif defined(TEST_BEAM_PARENT_COMPACT)
#include "titan_cpplib/ahc/beam_search/beam_search_parent_compact.cpp"
#elif defined(TEST_BEAM_PARENT)
#include "titan_cpplib/ahc/beam_search/beam_search_parent.cpp"
#else
#error Define one TEST_BEAM backend macro
#endif

using namespace std;

using ScoreType = int;
using HashType = uint64_t;
constexpr ScoreType INF = 1 << 28;

struct Action {
    static int live_count;

    int edge = -1;
    int from = -1;
    int to = -1;
    int payload = -1;
    string reuse_marker;

    Action() { ++live_count; }

    Action(const Action &other)
        : edge(other.edge), from(other.from), to(other.to), payload(other.payload),
          reuse_marker(other.reuse_marker) {
        ++live_count;
    }

    Action(Action &&other) noexcept
        : edge(other.edge), from(other.from), to(other.to), payload(other.payload),
          reuse_marker(move(other.reuse_marker)) {
        ++live_count;
    }

    Action &operator=(const Action &other) {
        edge = other.edge;
        from = other.from;
        to = other.to;
        payload = other.payload;
        reuse_marker = other.reuse_marker;
        return *this;
    }

    Action &operator=(Action &&other) noexcept {
        edge = other.edge;
        from = other.from;
        to = other.to;
        payload = other.payload;
        reuse_marker = move(other.reuse_marker);
        return *this;
    }

    ~Action() { --live_count; }

    string to_string() const {
        return std::to_string(edge) + ":" + std::to_string(from) + ":" + std::to_string(to) + ":" +
               std::to_string(payload) + ":" + reuse_marker;
    }
};

int Action::live_count = 0;

struct Edge {
    int from;
    int to;
    ScoreType score;
    HashType hash;
    bool finished;
    bool valid;
    int payload;
};

struct Scenario {
    string name;
    int max_turn;
    int beam_width;
    bool is_adjusting = false;
    bool clear_hash_every_turn = true;
    int hash_window_turns = 0;
    vector<vector<int>> outgoing = vector<vector<int>>(1);
    vector<Edge> edges;
};

class ScenarioBuilder {
private:
    HashType next_hash = 1;

public:
    Scenario scenario;

    ScenarioBuilder(string name, int max_turn, int beam_width) {
        scenario.name = move(name);
        scenario.max_turn = max_turn;
        scenario.beam_width = beam_width;
    }

    int add(int from, ScoreType score, HashType hash = numeric_limits<HashType>::max(), bool finished = false,
            bool valid = true) {
        int to = (int)scenario.outgoing.size();
        scenario.outgoing.emplace_back();
        int edge = (int)scenario.edges.size();
        if (hash == numeric_limits<HashType>::max()) hash = next_hash++;
        scenario.edges.push_back({from, to, score, hash, finished, valid, edge * 1009 + 17});
        scenario.outgoing[from].push_back(edge);
        return to;
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

void require(bool condition, const string &message) {
    if (!condition) throw runtime_error(message);
}

template<bool use_callback>
class ScriptState {
public:
    int node = 0;
    vector<int> path;

    void init() {
        node = 0;
        path.clear();
        active_runtime->events.push_back({'I', node, 0, -1, 0, 0});
    }

    template<class Submit>
    void enumerate_actions(int turn, const Action &previous, Submit &&submit) const requires(use_callback) {
        require(turn == (int)path.size(), "callback turn does not match the state depth");
        int previous_edge = path.empty() ? -1 : path.back();
        require(previous.edge == previous_edge, "callback previous Action does not match the state path");
        active_runtime->events.push_back({'E', node, turn, previous.edge, 1, 0});
        Action action;
        action.reuse_marker = "callback-reuse";
        for (int edge : active_runtime->scenario->outgoing[node]) {
            active_runtime->events.push_back({'Q', node, turn, edge, 0, submit.threshold()});
            action.edge = edge;
            submit(action);
        }
    }

    void enumerate_actions(vector<Action> &actions, int turn, const Action &previous,
                           ScoreType threshold) const requires(!use_callback) {
        require(turn == (int)path.size(), "vector turn does not match the state depth");
        int previous_edge = path.empty() ? -1 : path.back();
        require(previous.edge == previous_edge, "vector previous Action does not match the state path");
        active_runtime->events.push_back({'E', node, turn, previous.edge, 0, threshold});
        for (int edge : active_runtime->scenario->outgoing[node]) {
            Action action;
            action.edge = edge;
            action.reuse_marker = "vector";
            actions.push_back(move(action));
        }
    }

    tuple<ScoreType, HashType, bool> try_op(Action &action, ScoreType threshold) const {
        require(action.edge >= 0 && action.edge < (int)active_runtime->scenario->edges.size(),
                "try_op received an invalid edge");
        const Edge &edge = active_runtime->scenario->edges[action.edge];
        require(edge.from == node, "try_op received an edge for another parent");
        if constexpr (use_callback) {
            require(action.reuse_marker == "callback-reuse", "callback Action was moved before its next reuse");
        } else {
            require(action.reuse_marker == "vector", "vector Action marker was corrupted");
        }
        action.from = edge.from;
        action.to = edge.to;
        action.payload = edge.payload;
        ScoreType score = edge.valid ? edge.score : INF;
        active_runtime->events.push_back({'T', node, (int)path.size(), action.edge, threshold, score});
        return {score, edge.hash, edge.finished};
    }

    void apply_op(const Action &action) {
        require(action.from == node, "apply_op source does not match the current state");
        require(action.edge >= 0 && action.edge < (int)active_runtime->scenario->edges.size(),
                "apply_op received an invalid edge");
        const Edge &edge = active_runtime->scenario->edges[action.edge];
        require(edge.from == action.from && edge.to == action.to && edge.payload == action.payload,
                "apply_op Action payload was corrupted");
        active_runtime->events.push_back({'A', node, (int)path.size(), action.edge, action.to, action.payload});
        node = action.to;
        path.push_back(action.edge);
    }

    void rollback(const Action &action) {
        require(action.to == node, "rollback destination does not match the current state");
        require(!path.empty() && path.back() == action.edge, "rollback Action does not match the state path");
        active_runtime->events.push_back({'R', node, (int)path.size(), action.edge, action.from, action.payload});
        path.pop_back();
        node = action.from;
    }

    string get_state_info() const { return "{\"node\":" + std::to_string(node) + "}"; }
};

struct LogicalCandidate {
    int parent_leaf;
    ScoreType score;
    HashType hash;
    vector<int> path;
};

class ReferenceSelector {
private:
    int beam_width = 0;
    unordered_map<HashType, int> position;

    int worst_index() const {
        int result = 0;
        for (int i = 1; i < (int)entries.size(); ++i) {
            if (entries[i].score >= entries[result].score) result = i;
        }
        return result;
    }

public:
    vector<LogicalCandidate> entries;

    void reset(int turn, int width, bool clear_hash, int hash_window_turns) {
        beam_width = width;
        if (clear_hash || (hash_window_turns > 0 && turn % hash_window_turns == 0)) position.clear();
        if (!clear_hash) {
            for (const LogicalCandidate &candidate : entries) position[candidate.hash] = -2;
        }
        entries.clear();
    }

    void push(int parent_leaf, ScoreType score, HashType hash, vector<int> path) {
        if ((int)entries.size() == beam_width && score >= entries[worst_index()].score) return;
        auto found = position.find(hash);
        int index = found == position.end() ? -1 : found->second;
        if (index == -2) return;
        if (index >= 0) {
            if (score < entries[index].score) entries[index] = {parent_leaf, score, hash, move(path)};
            return;
        }
        if ((int)entries.size() < beam_width) {
            int slot = (int)entries.size();
            position[hash] = slot;
            entries.push_back({parent_leaf, score, hash, move(path)});
            return;
        }
        int slot = worst_index();
        position[entries[slot].hash] = -1;
        position[hash] = slot;
        entries[slot] = {parent_leaf, score, hash, move(path)};
    }
};

struct ReferenceResult {
    flying_squirrel::BeamStatus status;
    ScoreType score;
    int turns_done;
    vector<int> path;
    vector<int> width_hist;
};

class ReferenceWidth {
private:
    const Scenario &scenario;
    int beam_width_sum = 0;
    int turn_sum = 0;
    int previous_width = -1;

public:
    explicit ReferenceWidth(const Scenario &scenario) : scenario(scenario) {}

    int get() {
        if (!scenario.is_adjusting || turn_sum <= 10) return scenario.beam_width;
        if (turn_sum % 10 != 0 && previous_width != -1) return previous_width;
        int average_width = (double)beam_width_sum / turn_sum;
        int width = (2 + average_width) / 3;
        previous_width = max(1, width);
        return previous_width;
    }

    void timestamp(int width) {
        beam_width_sum += width;
        ++turn_sum;
    }
};

ReferenceResult run_reference(const Scenario &scenario) {
    if (scenario.max_turn <= 0 || scenario.beam_width <= 0) {
        return {flying_squirrel::BeamStatus::InvalidParameter, INF, 0, {}, {}};
    }
    ReferenceSelector selector;
    ReferenceWidth width(scenario);
    selector.reset(0, width.get(), scenario.clear_hash_every_turn, scenario.hash_window_turns);
    bool found_finished = false;
    ScoreType finished_score = INF;
    vector<int> finished_path;
    vector<int> width_hist;
    for (int edge_id : scenario.outgoing[0]) {
        const Edge &edge = scenario.edges[edge_id];
        ScoreType score = edge.valid ? edge.score : INF;
        if (score >= INF) continue;
        vector<int> path = {edge_id};
        if (edge.finished) {
            if (!found_finished || score < finished_score) {
                found_finished = true;
                finished_score = score;
                finished_path = move(path);
            }
        } else {
            selector.push(0, score, edge.hash, move(path));
        }
    }
    if (found_finished) {
        return {flying_squirrel::BeamStatus::Finished, finished_score, 1, move(finished_path), {}};
    }
    if (selector.entries.empty()) return {flying_squirrel::BeamStatus::NoCandidates, INF, 0, {}, {}};
    vector<LogicalCandidate> current = selector.entries;
    int turns_done = 1;
    for (int turn = 1; turn < scenario.max_turn; ++turn) {
        selector.reset(turn, width.get(), scenario.clear_hash_every_turn, scenario.hash_window_turns);
        int parent_leaf = 0;
        for (int i = (int)current.size() - 1; i >= 0; --i, ++parent_leaf) {
            int node = scenario.edges[current[i].path.back()].to;
            for (int edge_id : scenario.outgoing[node]) {
                const Edge &edge = scenario.edges[edge_id];
                ScoreType score = edge.valid ? edge.score : INF;
                if (score >= INF) continue;
                vector<int> path = current[i].path;
                path.push_back(edge_id);
                if (edge.finished) {
                    if (!found_finished || score < finished_score) {
                        found_finished = true;
                        finished_score = score;
                        finished_path = move(path);
                    }
                } else {
                    selector.push(parent_leaf, score, edge.hash, move(path));
                }
            }
        }
        if (found_finished) {
            return {flying_squirrel::BeamStatus::Finished, finished_score, turn + 1, move(finished_path),
                    move(width_hist)};
        }
        if (selector.entries.empty()) {
            return {flying_squirrel::BeamStatus::NoCandidates, INF, turn, {}, move(width_hist)};
        }
        current = selector.entries;
        sort(current.begin(), current.end(), [](const LogicalCandidate &left, const LogicalCandidate &right) {
            if (left.parent_leaf != right.parent_leaf) return left.parent_leaf < right.parent_leaf;
            return left.score < right.score;
        });
        width.timestamp(selector.entries.size());
        width_hist.push_back(selector.entries.size());
        turns_done = turn + 1;
    }
    int best = 0;
    for (int i = 1; i < (int)current.size(); ++i) {
        if (current[i].score < current[best].score) best = i;
    }
    return {flying_squirrel::BeamStatus::MaxTurnReached, current[best].score, turns_done, current[best].path,
            move(width_hist)};
}

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
        for (int i = 0; i < 8; ++i) add_byte((bits >> (i * 8)) & 255);
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
    vector<int> final_path;
    vector<Event> events;
    vector<int> width_hist;
    int pool_size_sum;
    int beam_width_sum;
    int turn_sum;
    string history;
};

uint64_t digest_result(const Outcome &outcome) {
    Digest digest;
    digest.add_int(outcome.status);
    digest.add_int(outcome.score);
    digest.add_int(outcome.turns_done);
    digest.add_int((int)outcome.actions.size());
    for (const Action &action : outcome.actions) {
        digest.add_int(action.edge);
        digest.add_int(action.from);
        digest.add_int(action.to);
        digest.add_int(action.payload);
        digest.add_string(action.reuse_marker);
    }
    digest.add_int(outcome.has_final_state);
    digest.add_int(outcome.final_node);
    digest.add_int((int)outcome.final_path.size());
    for (int edge : outcome.final_path) digest.add_int(edge);
    digest.add_int((int)outcome.width_hist.size());
    for (int width : outcome.width_hist) digest.add_int(width);
    digest.add_int(outcome.pool_size_sum);
    digest.add_int(outcome.beam_width_sum);
    digest.add_int(outcome.turn_sum);
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
    if (!stream) return "<missing>";
    return string(istreambuf_iterator<char>(stream), istreambuf_iterator<char>());
}

void validate_outcome(const Scenario &scenario, const ReferenceResult &reference, const Outcome &outcome,
                      bool use_callback, bool materialize) {
    require(outcome.status == (int)reference.status, scenario.name + ": status mismatch");
    require(outcome.score == reference.score, scenario.name + ": score mismatch");
    require(outcome.turns_done == reference.turns_done, scenario.name + ": turns_done mismatch");
    require(outcome.actions.size() == reference.path.size(), scenario.name + ": Action count mismatch");
    int node = 0;
    for (int i = 0; i < (int)reference.path.size(); ++i) {
        int edge_id = reference.path[i];
        const Edge &edge = scenario.edges[edge_id];
        const Action &action = outcome.actions[i];
        require(action.edge == edge_id, scenario.name + ": result path mismatch");
        require(action.from == node && action.from == edge.from, scenario.name + ": result Action source mismatch");
        require(action.to == edge.to, scenario.name + ": result Action destination mismatch");
        require(action.payload == edge.payload, scenario.name + ": result Action payload mismatch");
        string marker = use_callback ? "callback-reuse" : "vector";
        require(action.reuse_marker == marker, scenario.name + ": result Action marker mismatch");
        node = edge.to;
    }
    bool should_have_state = materialize && !reference.path.empty();
    require(outcome.has_final_state == should_have_state, scenario.name + ": final State presence mismatch");
    if (should_have_state) {
        require(outcome.final_node == node, scenario.name + ": final State node mismatch");
        require(outcome.final_path == reference.path, scenario.name + ": final State path mismatch");
    }
    require(outcome.width_hist == reference.width_hist, scenario.name + ": width history mismatch");
    int width_sum = 0;
    for (int width : outcome.width_hist) width_sum += width;
    require(outcome.beam_width_sum == width_sum, scenario.name + ": beam width sum mismatch");
    require(outcome.turn_sum == (int)outcome.width_hist.size(), scenario.name + ": telemetry turn mismatch");
}

template<bool use_callback, bool record_history>
using TestedBeam = flying_squirrel::BeamSearchWithTree<ScoreType, HashType, Action, ScriptState<use_callback>, INF,
                                                       record_history>;

template<bool use_callback, bool record_history, bool materialize, class Beam>
Outcome execute_case_with_beam(Beam &beam, const Scenario &scenario,
                               const filesystem::path &history_directory, int run_index) {
    Runtime runtime;
    runtime.scenario = &scenario;
    active_runtime = &runtime;
    flying_squirrel::BeamParam param(scenario.max_turn, scenario.beam_width, -1, scenario.is_adjusting,
                                     scenario.clear_hash_every_turn, scenario.hash_window_turns);
    filesystem::path history_path = history_directory / ("history_" + to_string(run_index) + ".json");
    filesystem::remove(history_path);
    auto result = beam.template search<materialize>(param, false, history_path.string());
    require(result.elapsed_ms >= 0.0, scenario.name + ": elapsed time is negative");
    Outcome outcome;
    outcome.status = (int)result.status;
    outcome.score = result.score;
    outcome.turns_done = result.turns_done;
    outcome.actions = move(result.actions);
    outcome.has_final_state = (bool)result.final_state;
    outcome.final_node = result.final_state ? result.final_state->node : -1;
    if (result.final_state) outcome.final_path = result.final_state->path;
    outcome.events = move(runtime.events);
    outcome.width_hist = move(param.width_hist);
    outcome.pool_size_sum = param.pool_size_sum;
    outcome.beam_width_sum = param.beam_width_sum;
    outcome.turn_sum = param.turn_sum;
    if constexpr (record_history) {
        outcome.history = read_file(history_path);
    } else {
        require(!filesystem::exists(history_path), scenario.name + ": history was written while disabled");
    }
    filesystem::remove(history_path);
    active_runtime = nullptr;
    return outcome;
}

template<bool use_callback, bool record_history, bool materialize>
Outcome execute_case(const Scenario &scenario, const filesystem::path &history_directory, int run_index) {
    TestedBeam<use_callback, record_history> beam;
    return execute_case_with_beam<use_callback, record_history, materialize>(beam, scenario, history_directory,
                                                                             run_index);
}

template<bool record_history>
void print_outcome(const Scenario &scenario, const Outcome &outcome, const string &mode) {
    cout << scenario.name << ' ' << mode << ' ' << hex << setw(16) << setfill('0') << digest_result(outcome) << ' '
         << setw(16) << digest_events(outcome.events) << ' ' << dec << outcome.events.size();
    if constexpr (record_history) {
        cout << ' ' << hex << setw(16) << digest_text(outcome.history) << dec << ' ' << outcome.history.size();
    }
    cout << '\n';
}

template<bool use_callback, bool record_history, bool materialize>
void run_mode(const Scenario &scenario, const ReferenceResult &reference,
              const filesystem::path &history_directory, int &run_index, const string &mode) {
    Outcome outcome = execute_case<use_callback, record_history, materialize>(scenario, history_directory,
                                                                              run_index++);
    validate_outcome(scenario, reference, outcome, use_callback, materialize);
    print_outcome<record_history>(scenario, outcome, mode);
}

template<bool use_callback, bool materialize>
void run_reused_mode(TestedBeam<use_callback, false> &beam, const Scenario &scenario,
                     const filesystem::path &history_directory, int &run_index, const string &mode) {
    ReferenceResult reference = run_reference(scenario);
    Outcome outcome = execute_case_with_beam<use_callback, false, materialize>(beam, scenario, history_directory,
                                                                               run_index++);
    validate_outcome(scenario, reference, outcome, use_callback, materialize);
    print_outcome<false>(scenario, outcome, mode);
}

#if defined(TEST_BEAM_STANDARD) || defined(TEST_BEAM_PARENT) || defined(TEST_BEAM_PARENT_COMPACT)
template<bool use_callback>
void verify_search_reuse_clears_hash(const filesystem::path &history_directory, int &run_index) {
    ScenarioBuilder builder("reused_clear_hash_false", 1, 3);
    builder.add(0, 3, 101);
    builder.add(0, 2, 102);
    builder.add(0, 1, 103);
    builder.scenario.clear_hash_every_turn = false;
    Scenario scenario = builder.finish();
    ReferenceResult reference = run_reference(scenario);

    TestedBeam<use_callback, false> reused_beam;
    Outcome first = execute_case_with_beam<use_callback, false, true>(reused_beam, scenario, history_directory,
                                                                      run_index++);
    Outcome second = execute_case_with_beam<use_callback, false, true>(reused_beam, scenario, history_directory,
                                                                       run_index++);
    TestedBeam<use_callback, false> fresh_beam;
    Outcome fresh = execute_case_with_beam<use_callback, false, true>(fresh_beam, scenario, history_directory,
                                                                      run_index++);

    validate_outcome(scenario, reference, first, use_callback, true);
    validate_outcome(scenario, reference, second, use_callback, true);
    validate_outcome(scenario, reference, fresh, use_callback, true);
    require(digest_result(second) == digest_result(fresh),
            scenario.name + ": reused Beam result differs from a fresh Beam");
    require(digest_events(second.events) == digest_events(fresh.events),
            scenario.name + ": reused Beam State events differ from a fresh Beam");
}
#endif

vector<Scenario> make_hand_cases() {
    vector<Scenario> cases;
    {
        ScenarioBuilder builder("w1_chain", 5, 1);
        builder.add(0, 9);
        int node = builder.add(0, 1);
        for (int depth = 1; depth < 5; ++depth) {
            builder.add(node, 20 + depth);
            node = builder.add(node, depth);
        }
        cases.push_back(builder.finish());
    }
    {
        ScenarioBuilder builder("siblings_parent_gap_replacement_endpoint_gone", 5, 4);
        int p0 = builder.add(0, 4);
        int p1 = builder.add(0, 3);
        builder.add(0, 2);
        int p3 = builder.add(0, 1);
        builder.add(p3, 90);
        int endpoint = builder.add(p3, 10);
        builder.add(p1, 30);
        int middle = builder.add(p1, 8);
        builder.add(p1, 50);
        int right_a = builder.add(p0, 7);
        int right_b = builder.add(p0, 6);
        int a = builder.add(right_a, 25);
        int b = builder.add(right_b, 4);
        builder.add(right_b, 20);
        int c = builder.add(middle, 5);
        builder.add(endpoint, INF, numeric_limits<HashType>::max(), false, false);
        int only = builder.add(b, 3);
        builder.add(a, INF, numeric_limits<HashType>::max(), false, false);
        builder.add(c, INF, numeric_limits<HashType>::max(), false, false);
        builder.add(only, 2);
        cases.push_back(builder.finish());
    }
    {
        ScenarioBuilder builder("root_finished_multiple", 4, 4);
        builder.add(0, 6);
        builder.add(0, 9, numeric_limits<HashType>::max(), true);
        builder.add(0, 3, numeric_limits<HashType>::max(), true);
        cases.push_back(builder.finish());
    }
    {
        ScenarioBuilder builder("middle_finished_multiple", 5, 3);
        int p0 = builder.add(0, 3);
        int p1 = builder.add(0, 2);
        int p2 = builder.add(0, 1);
        builder.add(p2, 20, numeric_limits<HashType>::max(), true);
        builder.add(p2, 2);
        builder.add(p1, 5, numeric_limits<HashType>::max(), true);
        builder.add(p0, 12, numeric_limits<HashType>::max(), true);
        cases.push_back(builder.finish());
    }
    {
        ScenarioBuilder builder("no_candidates_root", 4, 3);
        builder.add(0, INF, numeric_limits<HashType>::max(), false, false);
        cases.push_back(builder.finish());
    }
    {
        ScenarioBuilder builder("no_candidates_after_first", 4, 3);
        builder.add(0, 1);
        cases.push_back(builder.finish());
    }
    {
        ScenarioBuilder builder("max_turn_one", 1, 3);
        builder.add(0, 5);
        builder.add(0, 1);
        builder.add(0, 3);
        cases.push_back(builder.finish());
    }
    {
        ScenarioBuilder builder("best_not_endpoint", 2, 3);
        int p0 = builder.add(0, 3);
        int p1 = builder.add(0, 2);
        int p2 = builder.add(0, 1);
        builder.add(p2, 1);
        builder.add(p1, 4);
        builder.add(p0, 5);
        cases.push_back(builder.finish());
    }
    {
        ScenarioBuilder builder("hash_duplicate_and_closed", 4, 4);
        int p0 = builder.add(0, 8, 41);
        int p1 = builder.add(0, 3, 41);
        builder.add(0, 5, 41);
        int p2 = builder.add(0, 4, 42);
        builder.add(p2, 7, 41);
        int child = builder.add(p2, 2, 43);
        builder.add(p1, 1, 42);
        builder.add(p0, 9, 44);
        builder.add(child, 1, 45);
        builder.scenario.clear_hash_every_turn = false;
        cases.push_back(builder.finish());
    }
    {
        ScenarioBuilder builder("score_ties", 3, 3);
        int p0 = builder.add(0, 5);
        int p1 = builder.add(0, 5);
        int p2 = builder.add(0, 5);
        builder.add(0, 5);
        builder.add(p2, 7);
        builder.add(p2, 7);
        builder.add(p1, 7);
        builder.add(p0, 7);
        cases.push_back(builder.finish());
    }
    {
        ScenarioBuilder builder("population_grow_shrink_grow", 4, 8);
        int root_child = builder.add(0, 1);
        vector<int> wide;
        for (int i = 0; i < 6; ++i) wide.push_back(builder.add(root_child, 10 - i));
        int left = builder.add(wide[1], 3);
        int right = builder.add(wide[4], 2);
        for (int i = 0; i < 5; ++i) builder.add(left, 20 - i);
        for (int i = 0; i < 5; ++i) builder.add(right, 10 - i);
        cases.push_back(builder.finish());
    }
    {
        ScenarioBuilder builder("dynamic_width_deep_reuse", 26, 8);
        vector<int> layer;
        for (int lane = 0; lane < 8; ++lane) layer.push_back(builder.add(0, lane));
        for (int depth = 1; depth < 26; ++depth) {
            vector<int> next_layer;
            for (int lane = 0; lane < 8; ++lane) {
                next_layer.push_back(builder.add(layer[lane], lane));
                builder.add(layer[lane], 100 + lane);
            }
            layer = move(next_layer);
        }
        builder.scenario.is_adjusting = true;
        cases.push_back(builder.finish());
    }
    return cases;
}

class XorShift64 {
private:
    uint64_t state;

public:
    explicit XorShift64(uint64_t seed) : state(seed) {}

    uint64_t next() {
        state ^= state << 7;
        state ^= state >> 9;
        return state;
    }
};

Scenario make_random_case(int index) {
    XorShift64 random(0x9e3779b97f4a7c15ULL ^ (uint64_t)index * 0xbf58476d1ce4e5b9ULL);
    int max_turn = 1 + random.next() % 6;
    int width = 1 + random.next() % 8;
    ScenarioBuilder builder("random_" + to_string(index), max_turn, width);
    vector<int> layer = {0};
    HashType unique_hash = 1000000 + index * 10000;
    for (int depth = 0; depth < max_turn; ++depth) {
        vector<int> next_layer;
        for (int node : layer) {
            int branch = random.next() % 5;
            for (int j = 0; j < branch; ++j) {
                ScoreType score = random.next() % 50;
                bool valid = random.next() % 29 != 0;
                bool finished = depth > 0 && random.next() % 41 == 0;
                HashType hash = random.next() % 7 == 0 ? 700000 + depth * 31 + random.next() % 5 : unique_hash++;
                int child = builder.add(node, score, hash, finished, valid);
                if (!finished && valid) next_layer.push_back(child);
            }
        }
        layer = move(next_layer);
        if (layer.empty() || layer.size() > 400) break;
    }
    return builder.finish();
}

int main(int argc, char **argv) {
    try {
        filesystem::path history_directory = argc >= 2 ? argv[1] : filesystem::temp_directory_path();
        int random_cases = 120;
        if (const char *value = getenv("BEAM_RANDOM_CASES")) random_cases = stoi(value);
        if (argc >= 3) random_cases = stoi(argv[2]);
        require(random_cases >= 0, "random case count must be nonnegative");
        vector<Scenario> hand_cases = make_hand_cases();
        int run_index = 0;
        for (int i = 0; i < (int)hand_cases.size(); ++i) {
            const Scenario &scenario = hand_cases[i];
            ReferenceResult reference = run_reference(scenario);
            run_mode<true, false, true>(scenario, reference, history_directory, run_index, "callback_state");
            run_mode<false, false, true>(scenario, reference, history_directory, run_index, "vector_state");
            run_mode<true, false, false>(scenario, reference, history_directory, run_index, "callback_no_state");
            if (i == 1 || i == 2 || i == 4 || i == 8) {
                run_mode<true, true, true>(scenario, reference, history_directory, run_index, "callback_history");
            }
            if (i == 1) {
                run_mode<false, true, false>(scenario, reference, history_directory, run_index,
                                             "vector_history_no_state");
            }
        }
        for (int i = 0; i < random_cases; ++i) {
            Scenario scenario = make_random_case(i);
            ReferenceResult reference = run_reference(scenario);
            if (i % 2 == 0) {
                run_mode<true, false, true>(scenario, reference, history_directory, run_index,
                                            "random_callback_state");
            } else {
                run_mode<true, false, false>(scenario, reference, history_directory, run_index,
                                             "random_callback_no_state");
            }
            if (i % 3 == 0) {
                run_mode<false, false, true>(scenario, reference, history_directory, run_index,
                                             "random_vector_state");
            }
        }
        {
            TestedBeam<true, false> beam;
            run_reused_mode<true, true>(beam, hand_cases[1], history_directory, run_index,
                                        "reused_callback_state_1");
            run_reused_mode<true, false>(beam, hand_cases[4], history_directory, run_index,
                                         "reused_callback_no_state_2");
            run_reused_mode<true, true>(beam, hand_cases[0], history_directory, run_index,
                                        "reused_callback_state_3");
            run_reused_mode<true, false>(beam, hand_cases[2], history_directory, run_index,
                                         "reused_callback_no_state_4");
            run_reused_mode<true, true>(beam, hand_cases.back(), history_directory, run_index,
                                        "reused_callback_state_5");
        }
        {
            TestedBeam<false, false> beam;
            run_reused_mode<false, true>(beam, hand_cases[7], history_directory, run_index,
                                         "reused_vector_state_1");
            run_reused_mode<false, false>(beam, hand_cases[5], history_directory, run_index,
                                          "reused_vector_no_state_2");
            run_reused_mode<false, true>(beam, hand_cases.back(), history_directory, run_index,
                                         "reused_vector_state_3");
        }
#if defined(TEST_BEAM_STANDARD) || defined(TEST_BEAM_PARENT) || defined(TEST_BEAM_PARENT_COMPACT)
        verify_search_reuse_clears_hash<true>(history_directory, run_index);
        verify_search_reuse_clears_hash<false>(history_directory, run_index);
#endif
        require(Action::live_count == 0, "Action lifetime count did not return to zero");
    } catch (const exception &error) {
        cerr << "beam_search_differential: " << error.what() << '\n';
        return 1;
    }
    return 0;
}
