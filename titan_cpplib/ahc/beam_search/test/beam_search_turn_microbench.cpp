// Lightweight State benchmark for beam_search_turn.cpp.
// Select the backend with TURN_BEAM_BENCH_BASELINE or TURN_BEAM_BENCH_OPTIMIZED.
// Runtime overrides include --scenario, --width, --max-turn, --branch,
// --target-gap, --gap-mode (unit/fixed/spread), and --action-words (0/8/32).
#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <limits>
#include <optional>
#include <stdexcept>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#if defined(TURN_BEAM_BENCH_BASELINE) && defined(TURN_BEAM_BENCH_OPTIMIZED)
#error Define exactly one turn-beam backend macro
#elif defined(TURN_BEAM_BENCH_BASELINE)
#include "titan_cpplib/ahc/beam_search/beam_search_turn.cpp"
#elif defined(TURN_BEAM_BENCH_OPTIMIZED)
#ifdef TURN_BEAM_BENCH_OPTIMIZED_HEADER
#include TURN_BEAM_BENCH_OPTIMIZED_HEADER
#else
#include "titan_cpplib/ahc/beam_search/beam_search_turn_optimized.cpp"
#endif
#else
#error Define TURN_BEAM_BENCH_BASELINE or TURN_BEAM_BENCH_OPTIMIZED
#endif

using namespace std;

using ScoreType = int64_t;
using HashType = uint64_t;
constexpr ScoreType INF = numeric_limits<ScoreType>::max() / 4;

enum class GapMode {
    Unit,
    Fixed,
    Spread,
};

struct Scenario {
    string name;
    int width;
    int max_turn;
    int branch;
    int target_gap;
    GapMode gap_mode;
    int action_words;
    bool clear_hash_every_turn;
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
    uint64_t try_ops = 0;
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
    ScoreType parent_score = 0;
    ScoreType score = 0;
    uint32_t parent_quality = 0;
    uint32_t child_quality = 0;
    uint32_t child = 0;
    int parent_turn = 0;
    int target_turn = -1;
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

template<size_t PaddingWords, bool Instrument>
class SyntheticTurnState {
private:
    using Action = BenchAction<PaddingWords, Instrument>;

    int turn = 0;
    uint64_t node_key = 0x6a09e667f3bcc909ULL;
    ScoreType score = 0;
    uint32_t quality = 0;

    int step_for_child(int child) const {
        const Scenario &scenario = *active_context->scenario;
        switch (scenario.gap_mode) {
            case GapMode::Unit:
                return 1;
            case GapMode::Fixed:
                return scenario.target_gap;
            case GapMode::Spread:
                return 1 + child % scenario.target_gap;
        }
        abort();
    }

    void fill_action(Action &action, int child) const {
        action.parent_key = node_key;
        action.parent_score = score;
        action.parent_quality = quality;
        action.parent_turn = turn;
        action.child = (uint32_t)child;
        action.target_turn = turn + step_for_child(child);

        // Later submissions are better. This makes a bounded candidate pool replace
        // entries repeatedly instead of filling once and rejecting the remainder.
        const uint32_t improvement = (uint32_t)(active_context->scenario->branch - 1 - child);
        action.child_quality = quality + improvement;
        action.child_key = mix64(node_key ^ ((uint64_t)(uint32_t)action.target_turn << 32) ^
                                 (uint64_t)(uint32_t)(child + 1));
        action.score = (ScoreType)action.child_quality * (ScoreType)(1ULL << 32) +
                       (ScoreType)(uint32_t)action.child_key;
        if constexpr (PaddingWords > 0) {
            action.padding.front() = mix64(action.child_key);
            action.padding.back() = mix64(action.child_key ^ 0xd1b54a32d192ed03ULL);
        }
    }

public:
    void init() {
        turn = 0;
        node_key = 0x6a09e667f3bcc909ULL;
        score = 0;
        quality = 0;
    }

    template<class Submit>
    void enumerate_actions(const Action &, Submit &&submit) const {
        const int branch = active_context->scenario->branch;
        // The callback API permits reusing one Action. Keeping construction outside
        // the loop prevents synthetic Action initialization from dominating the
        // candidate-container and arena costs that this benchmark targets.
        Action action;
        for (int child = 0; child < branch; ++child) {
            fill_action(action, child);
            submit(action);
        }
    }

    tuple<ScoreType, HashType, bool> try_op(Action &action,
                                            const vector<ScoreType> &thresholds) const {
        if (action.parent_key != node_key || action.parent_turn != turn ||
            action.parent_quality != quality || action.parent_score != score) {
            abort();
        }
        if constexpr (Instrument) {
            ++active_context->counters.try_ops;
            digest_add(active_context->counters.stream_digest, action.parent_key);
            digest_add(active_context->counters.stream_digest, action.child_key);
            digest_add(active_context->counters.stream_digest, (uint64_t)action.score);
            digest_add(active_context->counters.stream_digest, (uint64_t)(uint32_t)action.target_turn);
            if (0 <= action.target_turn && action.target_turn < (int)thresholds.size()) {
                digest_add(active_context->counters.stream_digest,
                           (uint64_t)thresholds[action.target_turn]);
            }
        }
        return {action.score, action.child_key, false};
    }

    void apply_op(const Action &action) {
        if (action.parent_key != node_key || action.parent_turn != turn ||
            action.parent_quality != quality || action.parent_score != score) {
            abort();
        }
        if constexpr (Instrument) ++active_context->counters.apply_ops;
        node_key = action.child_key;
        score = action.score;
        quality = action.child_quality;
        turn = action.target_turn;
    }

    void rollback(const Action &action) {
        if (action.child_key != node_key || action.target_turn != turn ||
            action.child_quality != quality || action.score != score) {
            abort();
        }
        if constexpr (Instrument) ++active_context->counters.rollback_ops;
        node_key = action.parent_key;
        score = action.parent_score;
        quality = action.parent_quality;
        turn = action.parent_turn;
    }

    string get_state_info() const { return "{}"; }

    uint64_t digest() const {
        uint64_t result = mix64(node_key);
        digest_add(result, (uint64_t)score);
        digest_add(result, quality);
        digest_add(result, (uint32_t)turn);
        return result;
    }
};

struct Observation {
    uint64_t result_digest = 0;
    uint64_t stream_digest = 0;
    uint64_t try_ops = 0;
    uint64_t apply_ops = 0;
    uint64_t rollback_ops = 0;
    ActionCounters action;
    ScoreType score = INF;
    int turns_done = 0;
    int status = 0;
    size_t path_length = 0;
    size_t action_bytes = 0;
};

template<class Result>
uint64_t result_digest(const Result &result) {
    uint64_t digest = 0x13198a2e03707344ULL;
    digest_add(digest, (uint64_t)result.score);
    digest_add(digest, (uint32_t)result.turns_done);
    digest_add(digest, (uint32_t)result.status);
    for (const auto &action : result.actions) {
        digest_add(digest, action.parent_key);
        digest_add(digest, action.child_key);
        digest_add(digest, (uint64_t)action.score);
        digest_add(digest, action.child_quality);
        digest_add(digest, (uint32_t)action.target_turn);
        if constexpr (tuple_size_v<decltype(action.padding)> > 0) {
            digest_add(digest, action.padding.front());
            digest_add(digest, action.padding.back());
        }
    }
    if (result.final_state) digest_add(digest, result.final_state->digest());
    return digest;
}

template<size_t PaddingWords, bool Instrument>
Observation execute_search(const Scenario &scenario) {
    using Action = BenchAction<PaddingWords, Instrument>;
    using State = SyntheticTurnState<PaddingWords, Instrument>;
    using Search = flying_squirrel::BeamSearchWithTree<ScoreType, HashType, Action, State, INF>;

    RunContext context;
    context.scenario = &scenario;
    active_context = &context;

    flying_squirrel::BeamParam param(scenario.max_turn, scenario.width, -1.0, false,
                                     scenario.clear_hash_every_turn);
    auto result = [&] {
        Search search;
        return search.search(param, false);
    }();

    Observation observation;
    observation.result_digest = result_digest(result);
    observation.stream_digest = context.counters.stream_digest;
    observation.try_ops = context.counters.try_ops;
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

int choose_batch(const Observation &observation, uint64_t target_try_ops) {
    const uint64_t denominator = max<uint64_t>(1, observation.try_ops);
    return (int)clamp<uint64_t>((target_try_ops + denominator - 1) / denominator, 1, 64);
}

template<size_t PaddingWords>
void run_timed_batch(const Scenario &scenario, int batch) {
    using Action = BenchAction<PaddingWords, false>;
    using State = SyntheticTurnState<PaddingWords, false>;
    using Search = flying_squirrel::BeamSearchWithTree<ScoreType, HashType, Action, State, INF>;

    RunContext context;
    context.scenario = &scenario;
    active_context = &context;
    Search search;
    for (int iteration = 0; iteration < batch; ++iteration) {
        flying_squirrel::BeamParam param(scenario.max_turn, scenario.width, -1.0, false,
                                         scenario.clear_hash_every_turn);
        auto result = search.search(param, false);
        uint64_t value = (uint64_t)result.score ^ (uint64_t)result.actions.size();
        if (!result.actions.empty()) value ^= result.actions.back().child_key;
        benchmark_sink = benchmark_sink ^ value;
    }
    active_context = nullptr;
}

template<size_t PaddingWords>
pair<Observation, uint64_t> measure(const Scenario &scenario, int warmup, int repetitions,
                                    uint64_t target_try_ops) {
    static_assert(sizeof(BenchAction<PaddingWords, false>) ==
                  sizeof(BenchAction<PaddingWords, true>));
    Observation tracked = execute_search<PaddingWords, true>(scenario);
    Observation plain = execute_search<PaddingWords, false>(scenario);
    if (tracked.result_digest != plain.result_digest) {
        throw runtime_error("instrumentation changed result digest for " + scenario.name);
    }

    const int batch = choose_batch(tracked, target_try_ops);
    for (int i = 0; i < warmup; ++i) run_timed_batch<PaddingWords>(scenario, batch);

    vector<uint64_t> samples;
    samples.reserve(repetitions);
    for (int i = 0; i < repetitions; ++i) {
        const auto start = chrono::steady_clock::now();
        run_timed_batch<PaddingWords>(scenario, batch);
        const auto stop = chrono::steady_clock::now();
        const uint64_t elapsed =
            chrono::duration_cast<chrono::nanoseconds>(stop - start).count();
        samples.push_back(elapsed / (uint64_t)batch);
    }
    sort(samples.begin(), samples.end());
    return {tracked, samples[samples.size() / 2]};
}

string gap_mode_name(GapMode mode) {
    switch (mode) {
        case GapMode::Unit:
            return "unit";
        case GapMode::Fixed:
            return "fixed";
        case GapMode::Spread:
            return "spread";
    }
    abort();
}

GapMode parse_gap_mode(const string &name) {
    if (name == "unit") return GapMode::Unit;
    if (name == "fixed") return GapMode::Fixed;
    if (name == "spread") return GapMode::Spread;
    throw runtime_error("unknown gap mode: " + name);
}

string backend_name() {
#if defined(TURN_BEAM_BENCH_BASELINE)
    return "baseline";
#else
    return "optimized";
#endif
}

vector<Scenario> default_scenarios() {
    return {
        {"single_path", 1, 200, 1, 1, GapMode::Unit, 0, true},
        {"tree_heavy_branch4", 64, 200, 4, 1, GapMode::Unit, 0, true},
        {"dense_churn_w64", 64, 40, 128, 1, GapMode::Unit, 0, true},
        {"dense_churn_w256", 256, 24, 128, 1, GapMode::Unit, 0, true},
        {"fixed_gap_8", 128, 128, 64, 8, GapMode::Fixed, 0, true},
        {"spread_gap_8", 64, 64, 32, 8, GapMode::Spread, 0, true},
        {"action_words_8", 128, 32, 64, 1, GapMode::Unit, 8, true},
        {"action_words_32", 128, 32, 64, 1, GapMode::Unit, 32, true},
        {"global_seen", 64, 32, 64, 1, GapMode::Unit, 0, false},
    };
}

void validate(const Scenario &scenario) {
    if (scenario.width <= 0 || scenario.max_turn <= 0 || scenario.branch <= 0 ||
        scenario.target_gap <= 0) {
        throw runtime_error("scenario dimensions must be positive");
    }
    if (scenario.action_words != 0 && scenario.action_words != 8 &&
        scenario.action_words != 32) {
        throw runtime_error("action-words must be one of 0, 8, 32");
    }
    if (scenario.gap_mode == GapMode::Unit && scenario.target_gap != 1) {
        throw runtime_error("unit gap mode requires target-gap=1");
    }
}

void print_header() {
    cout << "backend\tscenario\twidth\tmax_turn\tbranch\ttarget_gap\tgap_mode\taction_words"
            "\taction_bytes\tclear_hash\tbatch\twarmup\trepetitions\tmedian_ns\tns_per_try"
            "\ttry_ops\tapply_ops\trollback_ops\tdefault_ctor\tcopy_ctor\tmove_ctor\tcopy_assign"
            "\tmove_assign\tdestruct\tscore\tturns_done\tstatus\tpath_length\tresult_digest"
            "\tstream_digest\n";
}

template<size_t PaddingWords>
void print_measurement(const Scenario &scenario, int warmup, int repetitions,
                       uint64_t target_try_ops) {
    auto [observation, median_ns] = measure<PaddingWords>(scenario, warmup, repetitions,
                                                          target_try_ops);
    const int batch = choose_batch(observation, target_try_ops);
    const double ns_per_try = observation.try_ops == 0
                                  ? 0.0
                                  : (double)median_ns / (double)observation.try_ops;

    cout << backend_name() << '\t' << scenario.name << '\t' << scenario.width << '\t'
         << scenario.max_turn << '\t' << scenario.branch << '\t' << scenario.target_gap << '\t'
         << gap_mode_name(scenario.gap_mode) << '\t' << scenario.action_words << '\t'
         << observation.action_bytes << '\t' << (scenario.clear_hash_every_turn ? 1 : 0) << '\t'
         << batch << '\t' << warmup << '\t' << repetitions << '\t' << median_ns << '\t'
         << fixed << setprecision(3) << ns_per_try << '\t' << observation.try_ops << '\t'
         << observation.apply_ops << '\t' << observation.rollback_ops << '\t'
         << observation.action.default_ctor << '\t' << observation.action.copy_ctor << '\t'
         << observation.action.move_ctor << '\t' << observation.action.copy_assign << '\t'
         << observation.action.move_assign << '\t' << observation.action.destruct << '\t'
         << observation.score << '\t' << observation.turns_done << '\t' << observation.status << '\t'
         << observation.path_length << '\t' << hex << setw(16) << setfill('0')
         << observation.result_digest << '\t' << setw(16) << observation.stream_digest << dec
         << setfill(' ') << '\n';
}

void dispatch(const Scenario &scenario, int warmup, int repetitions, uint64_t target_try_ops) {
    if (scenario.action_words == 0) {
        print_measurement<0>(scenario, warmup, repetitions, target_try_ops);
    } else if (scenario.action_words == 8) {
        print_measurement<8>(scenario, warmup, repetitions, target_try_ops);
    } else if (scenario.action_words == 32) {
        print_measurement<32>(scenario, warmup, repetitions, target_try_ops);
    } else {
        throw runtime_error("unsupported action_words");
    }
}

int main(int argc, char **argv) {
    int warmup = 1;
    int repetitions = 5;
    uint64_t target_try_ops = 1'000'000;
    string selected;
    optional<int> width_override;
    optional<int> max_turn_override;
    optional<int> branch_override;
    optional<int> target_gap_override;
    optional<GapMode> gap_mode_override;
    optional<int> action_words_override;
    optional<bool> clear_hash_override;

    for (int i = 1; i < argc; ++i) {
        const string argument = argv[i];
        auto require_value = [&] {
            if (i + 1 >= argc) throw runtime_error("missing value after " + argument);
            return string(argv[++i]);
        };
        if (argument == "--warmup") warmup = stoi(require_value());
        else if (argument == "--repetitions") repetitions = stoi(require_value());
        else if (argument == "--target-try-ops") target_try_ops = stoull(require_value());
        else if (argument == "--scenario") selected = require_value();
        else if (argument == "--width") width_override = stoi(require_value());
        else if (argument == "--max-turn") max_turn_override = stoi(require_value());
        else if (argument == "--branch") branch_override = stoi(require_value());
        else if (argument == "--target-gap") target_gap_override = stoi(require_value());
        else if (argument == "--gap-mode") gap_mode_override = parse_gap_mode(require_value());
        else if (argument == "--action-words") action_words_override = stoi(require_value());
        else if (argument == "--clear-hash") clear_hash_override = stoi(require_value()) != 0;
        else throw runtime_error("invalid argument: " + argument);
    }
    if (warmup < 0 || repetitions <= 0 || target_try_ops == 0) {
        throw runtime_error("invalid benchmark setting");
    }

    vector<Scenario> scenarios = default_scenarios();
    const bool has_override = width_override || max_turn_override || branch_override ||
                              target_gap_override || gap_mode_override || action_words_override ||
                              clear_hash_override;
    vector<Scenario> selected_scenarios;
    for (Scenario scenario : scenarios) {
        if (!selected.empty() && scenario.name != selected) continue;
        if (selected.empty() && has_override && !selected_scenarios.empty()) break;
        if (width_override) scenario.width = *width_override;
        if (max_turn_override) scenario.max_turn = *max_turn_override;
        if (branch_override) scenario.branch = *branch_override;
        if (target_gap_override) scenario.target_gap = *target_gap_override;
        if (gap_mode_override) scenario.gap_mode = *gap_mode_override;
        if (action_words_override) scenario.action_words = *action_words_override;
        if (clear_hash_override) scenario.clear_hash_every_turn = *clear_hash_override;
        if (has_override) scenario.name += "_custom";
        validate(scenario);
        selected_scenarios.push_back(move(scenario));
    }
    if (selected_scenarios.empty()) throw runtime_error("unknown scenario: " + selected);

    print_header();
    for (const Scenario &scenario : selected_scenarios) {
        dispatch(scenario, warmup, repetitions, target_try_ops);
    }
    return benchmark_sink == 0xdeadbeefcafef00dULL;
}
