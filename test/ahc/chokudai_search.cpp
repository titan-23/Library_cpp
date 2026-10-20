#include <algorithm>
#include <cstdint>
#include <iostream>
#include <limits>
#include <map>
#include <random>
#include <stdexcept>
#include <string>
#include <tuple>
#include <type_traits>
#include <vector>

#include "titan_cpplib/ahc/chokudai_search/chokudai_search.cpp"
#include "titan_cpplib/ahc/chokudai_search/chokudai_search_turn.cpp"

using namespace std;
using flying_squirrel::BeamStatus;
constexpr int INF = 1 << 28;

void require(bool condition, const string &message) {
    if (!condition) throw runtime_error(message);
}

struct Action {
    int edge = -1;
    int from = -1;
    int to = -1;
    int pre_turn = -1;
    int target_turn = -1;
    string payload;
    vector<int> checksum;

    string to_string() const { return std::to_string(edge); }
};

struct Edge {
    int from, to, target_turn, score;
    uint64_t hash;
    bool finished;
};

struct Scenario {
    string name;
    int max_turn = 5;
    int width = 1;
    long long sweeps = -1;
    double time_limit = -1;
    vector<vector<int>> outgoing{{}};
    vector<int> turns{0};
    vector<Edge> edges;

    explicit Scenario(string name) : name(move(name)) {}

    int add(int from, int turn, int score, bool finished = false,
            uint64_t hash = numeric_limits<uint64_t>::max()) {
        int to = (int)outgoing.size();
        int id = (int)edges.size();
        if (hash == numeric_limits<uint64_t>::max()) hash = (uint64_t)id + 100;
        edges.push_back({from, to, turn, score, hash, finished});
        outgoing[from].push_back(id);
        outgoing.emplace_back();
        turns.push_back(turn);
        return to;
    }
};

struct Runtime {
    const Scenario *scenario;
    int initializations = 0;
    int applies = 0;
    int rollbacks = 0;
    vector<int> expanded;
};
Runtime *runtime = nullptr;

struct StateBase {
    int node = 0;
    int turn = 0;
    vector<int> path;

    void init() {
        ++runtime->initializations;
        node = turn = 0;
        path.clear();
    }

    void before_enumerate(const Action &previous) const {
        require(runtime->scenario->turns[node] == turn, "State turn mismatch");
        require(previous.edge == (path.empty() ? -1 : path.back()), "previous Action mismatch");
        runtime->expanded.push_back(node);
    }

    tuple<int, uint64_t, bool> evaluate(Action &action) const {
        const Edge &edge = runtime->scenario->edges.at(action.edge);
        require(edge.from == node, "try_op source mismatch");
        require(action.payload == "reusable", "callback-owned Action was moved from");
        action.from = node;
        action.to = edge.to;
        action.pre_turn = turn;
        action.target_turn = edge.target_turn;
        action.checksum = {action.edge, edge.from, edge.to, edge.target_turn};
        return {edge.score, edge.hash, edge.finished};
    }

    void check_action(const Action &action) const {
        const Edge &edge = runtime->scenario->edges.at(action.edge);
        require(action.payload == "reusable", "stored Action string payload corrupted");
        require(action.from == edge.from && action.to == edge.to, "stored Action endpoints corrupted");
        require(action.checksum == vector<int>({action.edge, edge.from, edge.to, edge.target_turn}),
                "stored Action vector payload corrupted");
    }

    void apply_op(const Action &action) {
        check_action(action);
        require(node == action.from && turn == action.pre_turn, "apply_op source mismatch");
        node = action.to;
        turn = action.target_turn;
        path.push_back(action.edge);
        ++runtime->applies;
    }

    void rollback(const Action &action) {
        check_action(action);
        require(node == action.to && turn == action.target_turn, "rollback destination mismatch");
        require(!path.empty() && path.back() == action.edge, "rollback path mismatch");
        node = action.from;
        turn = action.pre_turn;
        path.pop_back();
        ++runtime->rollbacks;
    }

    string get_state_info() const { return "{\"node\":" + std::to_string(node) + "}"; }
};

// Mode 0: fixed-depth callbacks, mode 1: the legacy vector API, mode 2: jumping turns.
template<int mode>
struct State : StateBase {
    template<class Submit>
    void enumerate_actions(int depth, const Action &previous, Submit &&submit) const requires(mode == 0) {
        require(depth == turn, "fixed-depth callback turn mismatch");
        before_enumerate(previous);
        Action action;
        action.payload = "reusable";
        for (int edge : runtime->scenario->outgoing[node]) {
            require(submit.threshold() == INF, "Chokudai callback must not prune the persistent frontier");
            action.edge = edge;
            submit(action);
            require(action.payload == "reusable", "submit moved from a reusable Action");
        }
    }

    void enumerate_actions(vector<Action> &actions, int depth, const Action &previous,
                           int threshold) const requires(mode == 1) {
        require(depth == turn && threshold == INF, "legacy enumeration arguments mismatch");
        before_enumerate(previous);
        for (int edge : runtime->scenario->outgoing[node]) {
            Action action;
            action.payload = "reusable";
            action.edge = edge;
            actions.push_back(move(action));
        }
    }

    template<class Submit>
    void enumerate_actions(const Action &previous, Submit &&submit) const requires(mode == 2) {
        before_enumerate(previous);
        Action action;
        action.payload = "reusable";
        for (int edge : runtime->scenario->outgoing[node]) {
            require(submit.threshold(runtime->scenario->edges[edge].target_turn) == INF,
                    "jumping-turn callback must not prune the persistent frontier");
            action.edge = edge;
            submit(action);
            require(action.payload == "reusable", "submit moved from a reusable Action");
        }
    }

    tuple<int, uint64_t, bool> try_op(Action &action, int threshold) const requires(mode != 2) {
        require(threshold == INF, "fixed-depth try_op threshold must be INF");
        return evaluate(action);
    }

    tuple<int, uint64_t, bool> try_op(Action &action,
                                    const vector<int> &thresholds) const requires(mode == 2) {
        require(thresholds.size() == (size_t)runtime->scenario->max_turn + 1,
                "jumping-turn thresholds must include max_turn");
        require(all_of(thresholds.begin(), thresholds.end(), [](int x) { return x == INF; }),
                "jumping-turn try_op thresholds must be INF");
        return evaluate(action);
    }
};

template<int mode>
struct ImmovableState : State<mode> {
    ImmovableState() = default;
    ImmovableState(const ImmovableState &) = delete;
    ImmovableState(ImmovableState &&) = delete;
    ImmovableState &operator=(const ImmovableState &) = delete;
    ImmovableState &operator=(ImmovableState &&) = delete;
};

struct Expected {
    BeamStatus status = BeamStatus::NoCandidates;
    int score = INF;
    int turns_done = 0;
    vector<int> path;
    vector<int> expanded;
};

// Deliberately use full paths and a linear scan over a flat list. The oracle has no
// search tree, state rollback, per-generation heap, or implementation helpers.
Expected oracle(const Scenario &scenario) {
    struct Candidate {
        int node, turn, score;
        uint64_t hash;
        vector<int> path;
        bool finished;
        bool pending;
    };
    Expected result;
    if (scenario.max_turn <= 0 || scenario.width <= 0 || scenario.sweeps < -1 ||
        !isfinite(scenario.time_limit)) {
        result.status = BeamStatus::InvalidParameter;
        return result;
    }
    if (scenario.time_limit == 0) {
        result.status = BeamStatus::TimeLimitReached;
        return result;
    }
    vector<Candidate> all{{0, 0, INF, 0, {}, false, true}};
    map<pair<int, uint64_t>, int> best_score;
    BeamStatus stop = BeamStatus::NoCandidates;
    long long sweeps = 0;
    while (true) {
        bool any_pending = false;
        for (const Candidate &candidate : all) any_pending |= candidate.pending;
        if (!any_pending) break;
        if (scenario.sweeps >= 0 && sweeps == scenario.sweeps) {
            stop = BeamStatus::SweepLimitReached;
            break;
        }
        ++sweeps;
        for (int turn = 0; turn < scenario.max_turn; ++turn) {
            for (int count = 0; count < scenario.width; ++count) {
                int selected = -1;
                for (int i = 0; i < (int)all.size(); ++i) {
                    if (!all[i].pending || all[i].turn != turn) continue;
                    if (selected == -1 || all[i].score < all[selected].score) selected = i;
                }
                if (selected == -1) break;
                all[selected].pending = false;
                Candidate parent = all[selected];
                result.expanded.push_back(parent.node);
                for (int edge_id : scenario.outgoing[parent.node]) {
                    const Edge &edge = scenario.edges[edge_id];
                    if (edge.score >= INF || edge.target_turn <= turn || edge.target_turn > scenario.max_turn)
                        continue;
                    if (edge.finished || edge.target_turn == scenario.max_turn) {
                        vector<int> path = parent.path;
                        path.push_back(edge_id);
                        all.push_back({edge.to, edge.target_turn, edge.score, edge.hash, move(path),
                                       edge.finished, false});
                        result.turns_done = max(result.turns_done, edge.target_turn);
                        continue;
                    }
                    auto key = make_pair(edge.target_turn, edge.hash);
                    auto previous = best_score.find(key);
                    if (previous != best_score.end() && previous->second <= edge.score) continue;
                    best_score[key] = edge.score;
                    for (Candidate &old : all) {
                        if (old.turn == edge.target_turn && old.hash == edge.hash) old.pending = false;
                    }
                    vector<int> path = parent.path;
                    path.push_back(edge_id);
                    all.push_back({edge.to, edge.target_turn, edge.score, edge.hash, move(path), edge.finished,
                                   !edge.finished && edge.target_turn < scenario.max_turn});
                    result.turns_done = max(result.turns_done, edge.target_turn);
                }
            }
        }
    }
    int selected = -1;
    auto rank = [&](const Candidate &candidate) {
        int category = candidate.finished ? 2 : (candidate.turn == scenario.max_turn ? 1 : 0);
        return make_tuple(category, category == 0 ? candidate.turn : 0, -candidate.score);
    };
    for (int i = 1; i < (int)all.size(); ++i) {
        if (selected == -1 || rank(all[i]) > rank(all[selected])) selected = i;
    }
    result.status = stop;
    if (selected >= 0) {
        result.path = all[selected].path;
        result.score = all[selected].score;
        if (all[selected].finished) result.status = BeamStatus::Finished;
        else if (all[selected].turn == scenario.max_turn) result.status = BeamStatus::MaxTurnReached;
    }
    return result;
}

template<int mode, class StateType = State<mode>, bool history = false>
using Search = conditional_t<mode == 2,
    flying_squirrel::ChokudaiSearchTurn<int, uint64_t, Action, StateType, INF, history>,
    flying_squirrel::ChokudaiSearch<int, uint64_t, Action, StateType, INF, history>>;

template<int mode, bool materialize = true, bool history = false, class StateType = State<mode>>
void check(const Scenario &scenario, Search<mode, StateType, history> &search,
           const string &history_file = "") {
    Runtime local{&scenario, 0, 0, 0, {}};
    runtime = &local;
    flying_squirrel::ChokudaiParam param(scenario.max_turn, scenario.width, scenario.time_limit);
    param.max_sweeps = scenario.sweeps;
    // These BeamParam knobs must not merge hashes across generations or prune queues.
    param.clear_hash_every_turn = false;
    param.hash_window_turns = 1;
    param.is_adjusting = true;
    const Expected expected = oracle(scenario);
    auto result = search.template search<materialize>(param, false, history_file);
    vector<int> actual_path;
    for (const Action &action : result.actions) actual_path.push_back(action.edge);
    string prefix = scenario.name + ", API " + std::to_string(mode) + ": ";
    require(result.status == expected.status, prefix + "status mismatch (actual " +
            std::to_string((int)result.status) + ", expected " + std::to_string((int)expected.status) + ")");
    require(result.score == expected.score, prefix + "score mismatch");
    require(result.turns_done == expected.turns_done, prefix + "turns_done mismatch");
    require(actual_path == expected.path, prefix + "returned path mismatch");
    require(local.expanded == expected.expanded, prefix + "expansion order mismatch");
    require(result.elapsed_ms >= 0, prefix + "negative elapsed time");
    require(result.finished() == (expected.status == BeamStatus::Finished), prefix + "finished() mismatch");
    require(result.has_path() == !expected.path.empty(), prefix + "has_path() mismatch");
    if (expected.status != BeamStatus::InvalidParameter)
        require(local.initializations == 1, prefix + "State must be initialized exactly once");
    if constexpr (materialize) {
        require(bool(result.final_state) == !expected.path.empty(), prefix + "final_state presence mismatch");
        if (result.final_state) {
            require(result.final_state->path == expected.path, prefix + "materialized State path mismatch");
            require(result.final_state->node == scenario.edges[expected.path.back()].to,
                    prefix + "materialized State endpoint mismatch");
        }
    } else {
        require(!result.final_state, prefix + "search<false> unexpectedly materialized State");
    }
}

vector<Scenario> examples() {
    vector<Scenario> cases;
    Scenario persistent{"persistent frontier improves an early finished solution"};
    int a = persistent.add(0, 1, 0);
    int b = persistent.add(0, 1, 10);
    persistent.add(a, 2, 100, true);
    persistent.add(b, 2, -100, true);
    cases.push_back(persistent);
    persistent.name = "one sweep expands the root and every reachable generation";
    persistent.sweeps = 1;
    cases.push_back(persistent);
    persistent.name = "two sweeps preserve candidates below the expansion width";
    persistent.sweeps = 2;
    cases.push_back(persistent);

    Scenario dedup{"improved duplicate reopens an already expanded state"};
    a = dedup.add(0, 1, 0);
    b = dedup.add(0, 1, 1);
    int x = dedup.add(a, 2, 20, false, 7);
    int y = dedup.add(b, 2, 10, false, 7);
    dedup.add(b, 2, 10, false, 7); // Equal and worse candidates must not reopen it again.
    dedup.add(b, 2, 30, false, 7);
    dedup.add(x, 3, 100, true);
    dedup.add(y, 3, -100, true);
    cases.push_back(dedup);

    Scenario stale{"queued duplicate replacement keeps its own registration order"};
    a = stale.add(0, 1, 4, false, 7);
    b = stale.add(0, 1, 2, false, 8);
    x = stale.add(0, 1, 2, false, 7);
    stale.add(a, 2, -1000, true);
    stale.add(b, 2, 0, true);
    stale.add(x, 2, 0, true);
    cases.push_back(stale);

    Scenario per_turn{"the same hash in different generations is independent"};
    a = per_turn.add(0, 1, 1, false, 42);
    b = per_turn.add(a, 2, 2, false, 42);
    per_turn.add(b, 3, 3, true, 42);
    cases.push_back(per_turn);

    Scenario fallback{"exhausted search returns the deepest partial path"};
    a = fallback.add(0, 1, -100);
    b = fallback.add(0, 1, 5);
    fallback.add(a, 2, 100);
    fallback.add(b, 2, 20);
    cases.push_back(fallback);
    fallback.name = "sweep limit preserves the deepest partial path";
    fallback.sweeps = 1;
    cases.push_back(fallback);

    Scenario terminal{"best max_turn solution is retained across later sweeps"};
    terminal.max_turn = 2;
    a = terminal.add(0, 1, 0);
    b = terminal.add(0, 1, 1);
    terminal.add(a, 2, 30);
    terminal.add(b, 2, -10);
    cases.push_back(terminal);

    Scenario prefer_finished{"finished solution takes precedence over a deeper partial solution"};
    prefer_finished.max_turn = 2;
    prefer_finished.add(0, 1, 100, true);
    a = prefer_finished.add(0, 1, 0);
    prefer_finished.add(a, 2, -100);
    cases.push_back(prefer_finished);

    Scenario terminal_hash{"finished candidates do not collide with unfinished hashes"};
    terminal_hash.add(0, 1, -100, false, 7);
    terminal_hash.add(0, 1, 100, true, 7);
    terminal_hash.add(0, 1, -200, true, 7);
    cases.push_back(terminal_hash);

    Scenario invalid_edges{"INF candidates are rejected even when finished"};
    invalid_edges.add(0, 1, INF, true);
    invalid_edges.add(0, 1, INF + 1);
    cases.push_back(invalid_edges);
    cases.push_back(Scenario{"empty root"});
    Scenario stopped = persistent;
    stopped.name = "zero sweeps";
    stopped.sweeps = 0;
    cases.push_back(stopped);
    stopped.name = "zero milliseconds";
    stopped.sweeps = -1;
    stopped.time_limit = 0;
    cases.push_back(stopped);
    stopped.name = "invalid max_turn";
    stopped.max_turn = 0;
    cases.push_back(stopped);
    stopped.name = "invalid beam_width";
    stopped.max_turn = 5;
    stopped.width = 0;
    cases.push_back(stopped);
    stopped.name = "invalid max_sweeps";
    stopped.width = 1;
    stopped.sweeps = -2;
    cases.push_back(stopped);
    stopped.sweeps = -1;
    stopped.name = "invalid NaN time limit";
    stopped.time_limit = numeric_limits<double>::quiet_NaN();
    cases.push_back(stopped);
    stopped.name = "invalid infinite time limit";
    stopped.time_limit = numeric_limits<double>::infinity();
    cases.push_back(stopped);
    return cases;
}

Scenario random_scenario(mt19937 &random, int id, bool jumps) {
    Scenario scenario{"random " + std::to_string(id) + (jumps ? " jumps" : " fixed")};
    scenario.max_turn = 2 + (int)(random() % 7);
    scenario.width = 1 + (int)(random() % 3);
    scenario.sweeps = random() % 2 ? -1 : 1 + (int)(random() % 4);
    for (int node = 0; node < (int)scenario.outgoing.size() && node < 120; ++node) {
        int turn = scenario.turns[node];
        if (turn >= scenario.max_turn) continue;
        int count = (int)(random() % 4);
        for (int child = 0; child < count; ++child) {
            int target = jumps ? turn + 1 + (int)(random() % (scenario.max_turn - turn)) : turn + 1;
            int score = (int)(random() % 41) - 20;
            if (random() % 15 == 0) score = INF;
            uint64_t hash = random() % 5 == 0 ? random() % 5 : (uint64_t)scenario.edges.size() + 100;
            bool finished = random() % 6 == 0;
            vector<int> joinable;
            for (int existing = 1; existing < (int)scenario.turns.size(); ++existing) {
                if (scenario.turns[existing] == target) joinable.push_back(existing);
            }
            if (!joinable.empty() && random() % 3 == 0) {
                // Multiple paths can converge on one actual State, including paths
                // with different operation counts in the jumping-turn variant.
                int to = joinable[random() % joinable.size()];
                for (const Edge &incoming : scenario.edges) {
                    if (incoming.to == to) {
                        hash = incoming.hash;
                        break;
                    }
                }
                int edge = (int)scenario.edges.size();
                scenario.edges.push_back({node, to, target, score, hash, finished});
                scenario.outgoing[node].push_back(edge);
            } else {
                scenario.add(node, target, score, finished, hash);
            }
        }
    }
    return scenario;
}

int main(int argc, char **argv) {
    try {
        string work_dir = argc > 1 ? argv[1] : "/tmp";
        int random_cases = argc > 2 ? stoi(argv[2]) : 160;
        Search<0> fixed;
        Search<1> legacy;
        Search<2> jumps;
        // All calls deliberately reuse the same engine objects.
        vector<Scenario> cases = examples();
        for (const Scenario &scenario : cases) {
            check<0>(scenario, fixed);
            check<1>(scenario, legacy);
            check<2>(scenario, jumps);
        }

        Scenario skip{"empty generations and invalid target_turn values"};
        skip.max_turn = 7;
        skip.add(0, 0, -1000, true);
        skip.add(0, -1, -1000, true);
        skip.add(0, 8, -1000, true);
        int a = skip.add(0, 3, 10);
        skip.add(a, 2, -1000, true);
        skip.add(a, 3, -1000, true);
        skip.add(a, 7, -10);
        check<2>(skip, jumps);

        Search<0, ImmovableState<0>> immovable_fixed;
        Search<2, ImmovableState<2>> immovable_jumps;
        check<0, false, false, ImmovableState<0>>(cases.front(), immovable_fixed);
        check<0, false, false, ImmovableState<0>>(cases.front(), immovable_fixed);
        check<2, false, false, ImmovableState<2>>(skip, immovable_jumps);
        check<2, false, false, ImmovableState<2>>(skip, immovable_jumps);

        Search<0, State<0>, true> history_fixed;
        Search<2, State<2>, true> history_jumps;
        check<0, true, true>(cases.front(), history_fixed, work_dir + "/fixed_history.json");
        check<2, true, true>(skip, history_jumps, work_dir + "/turn_history.json");

        mt19937 random(20261008);
        for (int i = 0; i < random_cases; ++i) {
            Scenario scenario = random_scenario(random, i, false);
            check<0>(scenario, fixed);
            check<1>(scenario, legacy);
            check<2>(scenario, jumps);
            scenario = random_scenario(random, i, true);
            check<2>(scenario, jumps);
        }

        static_assert(is_same_v<Search<0>, flying_squirrel::ChokudaiSearchWithTree<int, uint64_t,
                                Action, State<0>, INF>>);
        static_assert(is_same_v<Search<2>, flying_squirrel::ChokudaiSearchWithTreeTurn<int, uint64_t,
                                Action, State<2>, INF>>);
        // The base BeamParam overload remains usable without Chokudai-specific knobs.
        Runtime local{&cases.front(), 0, 0, 0, {}};
        runtime = &local;
        flying_squirrel::BeamParam base(5, 1, -1);
        auto result = fixed.search(base);
        require(result.status == BeamStatus::Finished && result.score == -100,
                "BeamParam compatibility overload mismatch");
        cout << "ChokudaiSearch tests passed (" << random_cases << " random cases per mode)\n";
    } catch (const exception &error) {
        cerr << error.what() << '\n';
        return 1;
    }
}
