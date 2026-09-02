#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#if defined(BEAM_BENCH_BASELINE)
#include "test/ahc/beam_search_baseline.cpp"
#elif defined(BEAM_BENCH_STANDARD)
#include "titan_cpplib/ahc/beam_search/beam_search.cpp"
#elif defined(BEAM_BENCH_PARENT)
#include "titan_cpplib/ahc/beam_search/beam_search_parent.cpp"
#elif defined(BEAM_BENCH_PARENT_COMPACT)
#include "titan_cpplib/ahc/beam_search/beam_search_parent_compact.cpp"
#else
#error Define one BEAM_BENCH backend macro
#endif

using namespace std;

using ScoreType = int64_t;
using HashType = uint64_t;
constexpr ScoreType INF = numeric_limits<ScoreType>::max() / 4;

enum class Topology {
    Serial,
    Sibling,
    Shallow,
    Deep,
    Replacement,
};

struct Scenario {
    string name;
    Topology topology;
    bool callback;
    int width;
    int depth;
    int branch;
    int split_start;
    int state_work;
    int action_words;
};

struct ActionCounters {
    uint64_t default_ctor = 0;
    uint64_t copy_ctor = 0;
    uint64_t move_ctor = 0;
    uint64_t copy_assign = 0;
    uint64_t move_assign = 0;
    uint64_t destruct = 0;
};

struct RunCounters {
    uint64_t expanded = 0;
    uint64_t apply_ops = 0;
    uint64_t rollback_ops = 0;
    uint64_t stream_digest = 0x243f6a8885a308d3ULL;
    ActionCounters action;
};

struct RunContext {
    const Scenario *scenario = nullptr;
    RunCounters counters;
};

RunContext *active_context = nullptr;
volatile uint64_t benchmark_sink = 0;

uint64_t mix64(uint64_t value) {
    value += 0x9e3779b97f4a7c15ULL;
    value = (value ^ (value >> 30)) * 0xbf58476d1ce4e5b9ULL;
    value = (value ^ (value >> 27)) * 0x94d049bb133111ebULL;
    return value ^ (value >> 31);
}

void digest_add(uint64_t &digest, uint64_t value) {
    digest = mix64(digest ^ mix64(value));
}

template<size_t PaddingWords>
struct ActionData {
    uint64_t parent_key = 0;
    uint64_t child_key = 0;
    ScoreType score = 0;
    uint32_t parent_lineage = 0;
    uint32_t child_lineage = 0;
    uint32_t child = 0;
    array<uint64_t, PaddingWords> padding{};
};

template<size_t PaddingWords, bool Instrument>
struct BenchAction;

template<size_t PaddingWords>
struct BenchAction<PaddingWords, false> : ActionData<PaddingWords> {
    string to_string() const { return std::to_string(this->child_key); }
};

template<size_t PaddingWords>
struct BenchAction<PaddingWords, true> : ActionData<PaddingWords> {
    using Base = ActionData<PaddingWords>;

    BenchAction() {
        if (active_context) ++active_context->counters.action.default_ctor;
    }

    BenchAction(const BenchAction &other) : Base(other) {
        if (active_context) ++active_context->counters.action.copy_ctor;
    }

    BenchAction(BenchAction &&other) noexcept : Base(move(other)) {
        if (active_context) ++active_context->counters.action.move_ctor;
    }

    BenchAction &operator=(const BenchAction &other) {
        Base::operator=(other);
        if (active_context) ++active_context->counters.action.copy_assign;
        return *this;
    }

    BenchAction &operator=(BenchAction &&other) noexcept {
        Base::operator=(move(other));
        if (active_context) ++active_context->counters.action.move_assign;
        return *this;
    }

    ~BenchAction() {
        if (active_context) ++active_context->counters.action.destruct;
    }

    string to_string() const { return std::to_string(this->child_key); }
};

template<size_t PaddingWords, bool Instrument, bool Callback>
class SyntheticState {
private:
    using Action = BenchAction<PaddingWords, Instrument>;

    int depth = 0;
    uint64_t node_key = 0x6a09e667f3bcc909ULL;
    uint32_t lineage = 0;
    uint64_t work_digest = 0;

    int split_levels() const {
        int leaves = 1;
        int levels = 0;
        while (leaves < active_context->scenario->width) {
            leaves *= active_context->scenario->branch;
            ++levels;
        }
        return levels;
    }

    int degree() const {
        const Scenario &scenario = *active_context->scenario;
        switch (scenario.topology) {
            case Topology::Serial:
                return scenario.branch;
            case Topology::Sibling:
                if (depth == 0 || lineage == 0) return scenario.width;
                return 1;
            case Topology::Shallow:
            case Topology::Replacement:
                return depth == 0 ? scenario.width : scenario.branch;
            case Topology::Deep:
                if (depth < scenario.split_start) return 1;
                return scenario.branch;
        }
        abort();
    }

    pair<ScoreType, uint32_t> score_and_lineage(int child) const {
        const Scenario &scenario = *active_context->scenario;
        switch (scenario.topology) {
            case Topology::Serial:
                return {child, (uint32_t)child};
            case Topology::Sibling:
                if (depth == 0 || lineage == 0) return {child, (uint32_t)child};
                return {(ScoreType)scenario.width + lineage, lineage};
            case Topology::Shallow:
                if (depth == 0) return {child, (uint32_t)child};
                return {(ScoreType)child * scenario.width + lineage, lineage};
            case Topology::Deep: {
                int split_end = scenario.split_start + split_levels();
                if (depth < scenario.split_start) return {0, 0};
                if (depth < split_end) {
                    uint32_t next_lineage = lineage * (uint32_t)scenario.branch + (uint32_t)child;
                    return {next_lineage, next_lineage};
                }
                return {(ScoreType)child * scenario.width + lineage, lineage};
            }
            case Topology::Replacement: {
                if (depth == 0) return {child, (uint32_t)child};
                ScoreType score = (ScoreType)lineage * scenario.branch + scenario.branch - 1 - child;
                return {score, (uint32_t)score};
            }
        }
        abort();
    }

    Action make_action(int child) const {
        Action action;
        auto [score, next_lineage] = score_and_lineage(child);
        action.parent_key = node_key;
        action.child_key = mix64(node_key ^ ((uint64_t)(depth + 1) << 32) ^ (uint32_t)(child + 1));
        action.score = score;
        action.parent_lineage = lineage;
        action.child_lineage = next_lineage;
        action.child = (uint32_t)child;
        for (size_t i = 0; i < PaddingWords; ++i) {
            action.padding[i] = mix64(action.child_key + i * 0x9e3779b97f4a7c15ULL);
        }
        return action;
    }

    uint64_t burn(uint64_t value) const {
        for (int i = 0; i < active_context->scenario->state_work; ++i) {
            value ^= value >> 12;
            value ^= value << 25;
            value ^= value >> 27;
            value *= 0x2545f4914f6cdd1dULL;
        }
        return value;
    }

public:
    void init() {
        depth = 0;
        node_key = 0x6a09e667f3bcc909ULL;
        lineage = 0;
        work_digest = 0;
    }

    template<class Submit>
    void enumerate_actions(int turn, const Action &, Submit &&submit) const requires(Callback) {
        if (turn != depth) abort();
        int count = degree();
        for (int child = 0; child < count; ++child) {
            Action action = make_action(child);
            submit(action);
        }
    }

    void enumerate_actions(vector<Action> &actions, int turn, const Action &, ScoreType) const requires(!Callback) {
        if (turn != depth) abort();
        int count = degree();
        actions.reserve(count);
        for (int child = 0; child < count; ++child) actions.push_back(make_action(child));
    }

    tuple<ScoreType, HashType, bool> try_op(Action &action, ScoreType threshold) const {
        if (action.parent_key != node_key) abort();
        if constexpr (Instrument) {
            ++active_context->counters.expanded;
            digest_add(active_context->counters.stream_digest, action.parent_key);
            digest_add(active_context->counters.stream_digest, action.child_key);
            digest_add(active_context->counters.stream_digest, (uint64_t)action.score);
            digest_add(active_context->counters.stream_digest, (uint64_t)threshold);
        }
        return {action.score, action.child_key, false};
    }

    void apply_op(const Action &action) {
        if (action.parent_key != node_key || action.parent_lineage != lineage) abort();
        if constexpr (Instrument) ++active_context->counters.apply_ops;
        work_digest ^= burn(action.child_key ^ (uint64_t)depth);
        node_key = action.child_key;
        lineage = action.child_lineage;
        ++depth;
    }

    void rollback(const Action &action) {
        if (action.child_key != node_key || action.child_lineage != lineage || depth == 0) abort();
        --depth;
        work_digest ^= burn(action.child_key ^ (uint64_t)depth);
        node_key = action.parent_key;
        lineage = action.parent_lineage;
        if constexpr (Instrument) ++active_context->counters.rollback_ops;
    }

    string get_state_info() const { return "{}"; }

    uint64_t digest() const {
        uint64_t result = mix64(node_key ^ work_digest);
        digest_add(result, lineage);
        digest_add(result, (uint64_t)depth);
        return result;
    }
};

struct Observation {
    uint64_t result_digest = 0;
    uint64_t stream_digest = 0;
    uint64_t expanded = 0;
    uint64_t apply_ops = 0;
    uint64_t rollback_ops = 0;
    ActionCounters action;
    ScoreType score = INF;
    int turns_done = 0;
    int status = 0;
    size_t path_length = 0;
    size_t action_bytes = 0;
};

int choose_batch(const Scenario &scenario, const Observation &observation, uint64_t target_expanded) {
    uint64_t expanded_units = observation.expanded * (uint64_t)(scenario.action_words + 1);
    uint64_t state_units = (observation.apply_ops + observation.rollback_ops) *
                           (uint64_t)(scenario.state_work + 1);
    uint64_t denominator = max<uint64_t>(1, expanded_units + state_units);
    return (int)clamp<uint64_t>((target_expanded + denominator - 1) / denominator, 1, 128);
}

template<class Result>
uint64_t result_digest(const Result &result) {
    uint64_t digest = 0x13198a2e03707344ULL;
    digest_add(digest, (uint64_t)result.score);
    digest_add(digest, (uint64_t)result.turns_done);
    digest_add(digest, (uint64_t)result.status);
    for (const auto &action : result.actions) {
        digest_add(digest, action.parent_key);
        digest_add(digest, action.child_key);
        digest_add(digest, (uint64_t)action.score);
        digest_add(digest, action.child_lineage);
        for (uint64_t value : action.padding) digest_add(digest, value);
    }
    if (result.final_state) digest_add(digest, result.final_state->digest());
    return digest;
}

template<size_t PaddingWords, bool Instrument, bool Callback>
Observation execute_search(const Scenario &scenario) {
    using Action = BenchAction<PaddingWords, Instrument>;
    using State = SyntheticState<PaddingWords, Instrument, Callback>;
    using Search = flying_squirrel::BeamSearchWithTree<ScoreType, HashType, Action, State, INF>;

    RunContext context;
    context.scenario = &scenario;
    active_context = &context;

    flying_squirrel::BeamParam param(scenario.depth, scenario.width, -1.0, false, true);
    auto result = [&] {
        Search search;
        return search.search(param, false);
    }();

    Observation observation;
    observation.result_digest = result_digest(result);
    observation.stream_digest = context.counters.stream_digest;
    observation.expanded = context.counters.expanded;
    observation.apply_ops = context.counters.apply_ops;
    observation.rollback_ops = context.counters.rollback_ops;
    observation.action = context.counters.action;
    observation.score = result.score;
    observation.turns_done = result.turns_done;
    observation.status = (int)result.status;
    observation.path_length = result.actions.size();
    observation.action_bytes = sizeof(Action);

    active_context = nullptr;
    return observation;
}

template<size_t PaddingWords, bool Callback>
void run_timed_batch(const Scenario &scenario, int batch) {
    using Action = BenchAction<PaddingWords, false>;
    using State = SyntheticState<PaddingWords, false, Callback>;
    using Search = flying_squirrel::BeamSearchWithTree<ScoreType, HashType, Action, State, INF>;

    RunContext context;
    context.scenario = &scenario;
    active_context = &context;
    for (int iteration = 0; iteration < batch; ++iteration) {
        flying_squirrel::BeamParam param(scenario.depth, scenario.width, -1.0, false, true);
        auto result = [&] {
            Search search;
            return search.search(param, false);
        }();
        uint64_t value = (uint64_t)result.score ^ (uint64_t)result.actions.size();
        if (!result.actions.empty()) value ^= result.actions.back().child_key;
        benchmark_sink = benchmark_sink ^ value;
    }
    active_context = nullptr;
}

template<size_t PaddingWords, bool Callback>
pair<Observation, uint64_t> measure(const Scenario &scenario, int warmup, int repetitions,
                                    uint64_t target_expanded) {
    static_assert(sizeof(BenchAction<PaddingWords, false>) == sizeof(BenchAction<PaddingWords, true>));
    Observation tracked = execute_search<PaddingWords, true, Callback>(scenario);
    Observation plain = execute_search<PaddingWords, false, Callback>(scenario);
    if (tracked.result_digest != plain.result_digest) throw runtime_error("instrumentation changed result digest");

    int batch = choose_batch(scenario, tracked, target_expanded);
    for (int i = 0; i < warmup; ++i) run_timed_batch<PaddingWords, Callback>(scenario, batch);

    vector<uint64_t> samples;
    samples.reserve(repetitions);
    for (int i = 0; i < repetitions; ++i) {
        auto start = chrono::steady_clock::now();
        run_timed_batch<PaddingWords, Callback>(scenario, batch);
        auto stop = chrono::steady_clock::now();
        uint64_t elapsed = chrono::duration_cast<chrono::nanoseconds>(stop - start).count();
        samples.push_back(elapsed / (uint64_t)batch);
    }
    sort(samples.begin(), samples.end());
    return {tracked, samples[samples.size() / 2]};
}

string topology_name(Topology topology) {
    switch (topology) {
        case Topology::Serial: return "serial_w1";
        case Topology::Sibling: return "sibling_p1";
        case Topology::Shallow: return "shallow_cross_parent";
        case Topology::Deep: return "deep_shared_prefix";
        case Topology::Replacement: return "replacement_heavy";
    }
    abort();
}

string backend_name() {
#if defined(BEAM_BENCH_BASELINE)
    return "baseline";
#elif defined(BEAM_BENCH_STANDARD)
    return "standard";
#elif defined(BEAM_BENCH_PARENT)
    return "parent_oracle";
#else
    return "parent_compact";
#endif
}

vector<Scenario> scenarios() {
    return {
        {"serial_w1", Topology::Serial, true, 1, 256, 4, 0, 0, 0},
        {"sibling_p1", Topology::Sibling, true, 256, 96, 2, 0, 0, 0},
        {"core_w64_d32", Topology::Shallow, true, 64, 32, 2, 0, 0, 0},
        {"core_w64_d128", Topology::Shallow, true, 64, 128, 2, 0, 0, 0},
        {"core_w512_d32", Topology::Shallow, true, 512, 32, 2, 0, 0, 0},
        {"core_w512_d128", Topology::Shallow, true, 512, 128, 2, 0, 0, 0},
        {"shallow_reference", Topology::Shallow, true, 256, 96, 2, 0, 0, 0},
        {"deep_branch", Topology::Deep, true, 256, 96, 4, 48, 0, 0},
        {"parent_replacement", Topology::Replacement, true, 256, 96, 4, 0, 0, 0},
        {"state_work_32", Topology::Shallow, true, 256, 96, 2, 0, 32, 0},
        {"state_work_128", Topology::Shallow, true, 256, 96, 2, 0, 128, 0},
        {"action_words_8", Topology::Shallow, true, 256, 96, 2, 0, 0, 8},
        {"action_words_32", Topology::Shallow, true, 256, 96, 2, 0, 0, 32},
        {"branch_8", Topology::Shallow, true, 256, 96, 8, 0, 0, 0},
        {"vector_enumeration", Topology::Shallow, false, 256, 96, 2, 0, 0, 0},
    };
}

void print_header() {
    cout << "backend\tscenario\ttopology\tenumeration\twidth\tdepth\tbranch\tsplit_start\tstate_work"
            "\taction_words\taction_bytes\tbatch\twarmup\trepetitions\tmedian_ns\tns_per_expanded\texpanded"
            "\tapply_ops\trollback_ops\tdefault_ctor\tcopy_ctor\tmove_ctor\tcopy_assign\tmove_assign\tdestruct"
            "\tresult_score\tturns_done\tstatus\tpath_length\tresult_digest\tstream_digest\n";
}

template<size_t PaddingWords, bool Callback>
void print_measurement(const Scenario &scenario, int warmup, int repetitions, uint64_t target_expanded) {
    auto [observation, median_ns] = measure<PaddingWords, Callback>(scenario, warmup, repetitions,
                                                                   target_expanded);
    int batch = choose_batch(scenario, observation, target_expanded);
    double ns_per_expanded = observation.expanded == 0 ? 0.0 : (double)median_ns / observation.expanded;

    cout << backend_name() << '\t' << scenario.name << '\t' << topology_name(scenario.topology) << '\t'
         << (Callback ? "callback" : "vector") << '\t' << scenario.width << '\t' << scenario.depth << '\t'
         << scenario.branch << '\t' << scenario.split_start << '\t' << scenario.state_work << '\t'
         << scenario.action_words << '\t' << observation.action_bytes << '\t' << batch << '\t' << warmup << '\t'
         << repetitions << '\t' << median_ns << '\t' << fixed << setprecision(3) << ns_per_expanded << '\t'
         << observation.expanded << '\t' << observation.apply_ops << '\t' << observation.rollback_ops << '\t'
         << observation.action.default_ctor << '\t' << observation.action.copy_ctor << '\t'
         << observation.action.move_ctor << '\t' << observation.action.copy_assign << '\t'
         << observation.action.move_assign << '\t' << observation.action.destruct << '\t' << observation.score
         << '\t' << observation.turns_done << '\t' << observation.status << '\t' << observation.path_length << '\t'
         << hex << setw(16) << setfill('0') << observation.result_digest << '\t' << setw(16)
         << observation.stream_digest << dec << setfill(' ') << '\n';
}

void dispatch(const Scenario &scenario, int warmup, int repetitions, uint64_t target_expanded) {
    if (scenario.callback) {
        if (scenario.action_words == 0) print_measurement<0, true>(scenario, warmup, repetitions, target_expanded);
        else if (scenario.action_words == 8) {
            print_measurement<8, true>(scenario, warmup, repetitions, target_expanded);
        } else if (scenario.action_words == 32) {
            print_measurement<32, true>(scenario, warmup, repetitions, target_expanded);
        } else {
            throw runtime_error("unsupported action_words");
        }
    } else {
        if (scenario.action_words == 0) print_measurement<0, false>(scenario, warmup, repetitions, target_expanded);
        else if (scenario.action_words == 8) {
            print_measurement<8, false>(scenario, warmup, repetitions, target_expanded);
        } else if (scenario.action_words == 32) {
            print_measurement<32, false>(scenario, warmup, repetitions, target_expanded);
        } else {
            throw runtime_error("unsupported action_words");
        }
    }
}

int main(int argc, char **argv) {
    int warmup = 2;
    int repetitions = 7;
    uint64_t target_expanded = 250000;
    string selected;

    for (int i = 1; i < argc; ++i) {
        string argument = argv[i];
        if (argument == "--warmup" && i + 1 < argc) warmup = stoi(argv[++i]);
        else if (argument == "--repetitions" && i + 1 < argc) repetitions = stoi(argv[++i]);
        else if (argument == "--target-expanded" && i + 1 < argc) target_expanded = stoull(argv[++i]);
        else if (argument == "--scenario" && i + 1 < argc) selected = argv[++i];
        else throw runtime_error("invalid argument: " + argument);
    }
    if (warmup < 0 || repetitions <= 0 || target_expanded == 0) throw runtime_error("invalid benchmark setting");

    print_header();
    bool found = selected.empty();
    for (const Scenario &scenario : scenarios()) {
        if (!selected.empty() && scenario.name != selected) continue;
        found = true;
        dispatch(scenario, warmup, repetitions, target_expanded);
    }
    if (!found) throw runtime_error("unknown scenario: " + selected);
    return 0;
}
