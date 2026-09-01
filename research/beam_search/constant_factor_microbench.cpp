#include <algorithm>
#include <array>
#include <bit>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <numeric>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "titan_cpplib/ahc/beam_search/candidates.cpp"
#include "titan_cpplib/ahc/timer.cpp"

using namespace std;

namespace {

struct OpCounts {
    uint64_t default_construct = 0;
    uint64_t copy_construct = 0;
    uint64_t move_construct = 0;
    uint64_t copy_assign = 0;
    uint64_t move_assign = 0;
    uint64_t destruct = 0;
};

template<size_t Bytes, bool Track>
class PayloadAction {
    static_assert(Bytes >= sizeof(uint64_t));
    static_assert(Bytes % sizeof(uint64_t) == 0);

    array<uint64_t, Bytes / sizeof(uint64_t)> payload_{};

    static inline OpCounts counts_{};

    static inline void opaque(const void *ptr) {
#if defined(__GNUC__) || defined(__clang__)
        asm volatile("" : : "r"(ptr) : "memory");
#else
        (void)ptr;
#endif
    }

    static inline void count(uint64_t OpCounts::*member) {
        if constexpr (Track) ++(counts_.*member);
    }

public:
    PayloadAction() {
        payload_.fill(0);
        count(&OpCounts::default_construct);
        opaque(payload_.data());
    }

    explicit PayloadAction(uint64_t seed) {
        for (size_t i = 0; i < payload_.size(); ++i) {
            seed += 0x9e3779b97f4a7c15ULL;
            uint64_t value = seed;
            value = (value ^ (value >> 30)) * 0xbf58476d1ce4e5b9ULL;
            value = (value ^ (value >> 27)) * 0x94d049bb133111ebULL;
            payload_[i] = value ^ (value >> 31);
        }
        opaque(payload_.data());
    }

    PayloadAction(const PayloadAction &other) : payload_(other.payload_) {
        count(&OpCounts::copy_construct);
        opaque(payload_.data());
    }

    PayloadAction(PayloadAction &&other) noexcept : payload_(other.payload_) {
        count(&OpCounts::move_construct);
        opaque(payload_.data());
    }

    PayloadAction &operator=(const PayloadAction &other) {
        payload_ = other.payload_;
        count(&OpCounts::copy_assign);
        opaque(payload_.data());
        return *this;
    }

    PayloadAction &operator=(PayloadAction &&other) noexcept {
        payload_ = other.payload_;
        count(&OpCounts::move_assign);
        opaque(payload_.data());
        return *this;
    }

    ~PayloadAction() { count(&OpCounts::destruct); }

    static void reset_counts() { counts_ = {}; }

    static OpCounts counts() { return counts_; }

    uint64_t digest() const {
        uint64_t value = payload_.front() ^ rotl(payload_.back(), 17);
        if constexpr (payload_.size() > 2) value ^= rotl(payload_[payload_.size() / 2], 31);
        return value;
    }
};

static_assert(sizeof(PayloadAction<8, false>) == 8);
static_assert(sizeof(PayloadAction<32, false>) == 32);
static_assert(sizeof(PayloadAction<128, false>) == 128);
static_assert(sizeof(PayloadAction<512, false>) == 512);

struct DummyState {};

enum class CandidateMethod { value, lazy };
enum class SourceKind { lvalue, rvalue };
enum class BlockMethod { resize_assign, emplace_construct };

struct Config {
    int warmup = 2;
    int samples = 11;
    int beam_width = 2048;
    int candidate_ops = 65536;
    int timer_iterations = 1000000;
    size_t block_target_bytes = 32ULL << 20;
};

struct CandidateTiming {
    double ns_per_op = 0;
    uint64_t accepted = 0;
    uint64_t digest = 0;
    OpCounts operations;
};

struct BlockTiming {
    double ns_per_action = 0;
    uint64_t actions = 0;
    uint64_t digest = 0;
    OpCounts operations;
};

struct TimerTiming {
    double one_call_ns = 0;
    double two_calls_ns = 0;
    double extra_call_ns = 0;
    uint64_t digest = 0;
};

struct CandidatePair {
    CandidateTiming value;
    CandidateTiming lazy;
    OpCounts value_counts;
    OpCounts lazy_counts;
};

template<class F>
double median_sample(int warmup, int samples, F &&run) {
    for (int i = 0; i < warmup; ++i) (void)run();
    vector<double> values;
    values.reserve(samples);
    for (int i = 0; i < samples; ++i) values.push_back(run());
    nth_element(values.begin(), values.begin() + values.size() / 2, values.end());
    return values[values.size() / 2];
}

uint64_t mix_digest(uint64_t accumulator, uint64_t value) {
    accumulator ^= value + 0x9e3779b97f4a7c15ULL + rotl(accumulator, 17);
    return accumulator * 0xbf58476d1ce4e5b9ULL;
}

vector<uint8_t> make_acceptance_mask(int operations, int permille) {
    vector<uint8_t> mask(operations);
    int accepted = int(int64_t(operations) * permille / 1000);
    fill(mask.begin(), mask.begin() + accepted, uint8_t{1});
    uint64_t state = 0x243f6a8885a308d3ULL ^ uint64_t(permille);
    for (int i = operations - 1; i > 0; --i) {
        state ^= state << 7;
        state ^= state >> 9;
        state ^= state << 8;
        int j = int(state % uint64_t(i + 1));
        swap(mask[i], mask[j]);
    }
    return mask;
}

template<size_t Bytes, bool Track, CandidateMethod Method, SourceKind Source>
pair<double, CandidateTiming> run_candidate_once(span<const uint8_t> accept_mask, int beam_width) {
    using Action = PayloadAction<Bytes, Track>;
    using Selector = flying_squirrel::Candidates<int, uint64_t, Action, DummyState, 1000000000, false>;

    Selector selector;
    selector.reset(0, beam_width, true);
    Action shared(0x123456789abcdef0ULL);
    for (int i = 0; i < beam_width; ++i) {
        selector.push(beam_width - i, uint64_t(i + 1), i, shared);
    }

    vector<Action> input;
    if constexpr (Source == SourceKind::rvalue) {
        input.reserve(accept_mask.size());
        for (size_t i = 0; i < accept_mask.size(); ++i) {
            input.emplace_back(0x6a09e667f3bcc909ULL + i);
        }
    }
    Action::reset_counts();

    uint64_t accepted = 0;
    auto start = chrono::steady_clock::now();
    for (size_t i = 0; i < accept_mask.size(); ++i) {
        bool should_accept = accept_mask[i] != 0;
        int score = should_accept ? -1 - int(accepted) : 999999999;
        uint64_t hash = uint64_t(beam_width) + i + 1;
        bool result;
        if constexpr (Source == SourceKind::lvalue) {
            if constexpr (Method == CandidateMethod::value) {
                result = selector.push(score, hash, int(i & 2047), shared);
            } else {
                result = selector.push_lazy(score, hash, int(i & 2047), [&shared] { return shared; });
            }
        } else {
            if constexpr (Method == CandidateMethod::value) {
                result = selector.push(score, hash, int(i & 2047), std::move(input[i]));
            } else {
                result = selector.push_lazy(score, hash, int(i & 2047), [&input, i] {
                    return std::move(input[i]);
                });
            }
        }
        accepted += result;
    }
    auto finish = chrono::steady_clock::now();

    uint64_t digest = accepted;
    for (int i = 0; i < selector.size(); ++i) {
        const auto &candidate = selector.next_beam[i];
        digest ^= candidate.action.digest() + uint64_t(candidate.score) + uint64_t(candidate.parent_leaf);
        digest = rotl(digest, 11) * 0x9e3779b97f4a7c15ULL;
    }
    OpCounts operations = Action::counts();
    double elapsed_ns = chrono::duration<double, nano>(finish - start).count();
    return {elapsed_ns / double(accept_mask.size()), {0, accepted, digest, operations}};
}

template<size_t Bytes, SourceKind Source>
CandidatePair benchmark_candidate(int permille, const Config &config) {
    vector<uint8_t> mask = make_acceptance_mask(config.candidate_ops, permille);
    CandidatePair result;

    uint64_t value_digest = 0;
    result.value.ns_per_op = median_sample(config.warmup, config.samples, [&] {
        auto [ns, one] = run_candidate_once<Bytes, false, CandidateMethod::value, Source>(mask,
                                                                                          config.beam_width);
        result.value.accepted = one.accepted;
        value_digest = mix_digest(value_digest, one.digest);
        return ns;
    });
    result.value.digest = value_digest;

    uint64_t lazy_digest = 0;
    result.lazy.ns_per_op = median_sample(config.warmup, config.samples, [&] {
        auto [ns, one] = run_candidate_once<Bytes, false, CandidateMethod::lazy, Source>(mask,
                                                                                         config.beam_width);
        result.lazy.accepted = one.accepted;
        lazy_digest = mix_digest(lazy_digest, one.digest);
        return ns;
    });
    result.lazy.digest = lazy_digest;
    if (result.value.accepted != result.lazy.accepted || result.value.digest != result.lazy.digest) {
        cerr << "candidate result mismatch\n";
        exit(3);
    }

    int count_ops = min(config.candidate_ops, 10000);
    vector<uint8_t> count_mask = make_acceptance_mask(count_ops, permille);
    auto [value_ns, value_count_result] = run_candidate_once<Bytes, true, CandidateMethod::value, Source>(
        count_mask, config.beam_width);
    auto [lazy_ns, lazy_count_result] = run_candidate_once<Bytes, true, CandidateMethod::lazy, Source>(
        count_mask, config.beam_width);
    (void)value_ns;
    (void)lazy_ns;
    result.value_counts = value_count_result.operations;
    result.lazy_counts = lazy_count_result.operations;
    return result;
}

template<size_t Bytes, bool Track, BlockMethod Method>
pair<double, BlockTiming> run_block_once(size_t width, size_t rounds) {
    using Action = PayloadAction<Bytes, Track>;
    vector<Action> source;
    source.reserve(width);
    for (size_t i = 0; i < width; ++i) source.emplace_back(0xbb67ae8584caa73bULL + i);
    vector<Action> destination;
    destination.reserve(width);
    Action::reset_counts();

    auto start = chrono::steady_clock::now();
    for (size_t round = 0; round < rounds; ++round) {
        destination.clear();
        if constexpr (Method == BlockMethod::resize_assign) {
            destination.resize(width);
            for (size_t i = 0; i < width; ++i) destination[i] = std::move(source[i]);
        } else {
            destination.reserve(width);
            for (size_t i = 0; i < width; ++i) destination.emplace_back(std::move(source[i]));
        }
    }
    auto finish = chrono::steady_clock::now();

    uint64_t digest = 0;
    for (const Action &action : destination) {
        digest ^= action.digest();
        digest = rotl(digest, 7) * 0x94d049bb133111ebULL;
    }
    OpCounts operations = Action::counts();
    uint64_t actions = width * rounds;
    double elapsed_ns = chrono::duration<double, nano>(finish - start).count();
    return {elapsed_ns / double(actions), {0, actions, digest, operations}};
}

template<size_t Bytes>
pair<BlockTiming, BlockTiming> benchmark_block(const Config &config) {
    size_t width = size_t(config.beam_width);
    size_t bytes_per_round = max<size_t>(1, width * Bytes);
    size_t rounds = max<size_t>(1, config.block_target_bytes / bytes_per_round);
    BlockTiming old_result;
    BlockTiming new_result;

    uint64_t old_digest = 0;
    old_result.ns_per_action = median_sample(config.warmup, config.samples, [&] {
        auto [ns, one] = run_block_once<Bytes, false, BlockMethod::resize_assign>(width, rounds);
        old_result.actions = one.actions;
        old_digest = mix_digest(old_digest, one.digest);
        return ns;
    });
    old_result.digest = old_digest;

    uint64_t new_digest = 0;
    new_result.ns_per_action = median_sample(config.warmup, config.samples, [&] {
        auto [ns, one] = run_block_once<Bytes, false, BlockMethod::emplace_construct>(width, rounds);
        new_result.actions = one.actions;
        new_digest = mix_digest(new_digest, one.digest);
        return ns;
    });
    new_result.digest = new_digest;
    if (old_result.digest != new_result.digest) {
        cerr << "block result mismatch\n";
        exit(3);
    }
    return {old_result, new_result};
}

template<size_t Bytes>
pair<OpCounts, OpCounts> count_block_operations(const Config &config) {
    size_t width = size_t(config.beam_width);
    auto [old_ns, old_result] = run_block_once<Bytes, true, BlockMethod::resize_assign>(width, 1);
    auto [new_ns, new_result] = run_block_once<Bytes, true, BlockMethod::emplace_construct>(width, 1);
    (void)old_ns;
    (void)new_ns;
    return {old_result.operations, new_result.operations};
}

template<int CallsPerIteration>
pair<double, uint64_t> run_timer_once(int iterations) {
    titan23::Timer timer;
    double digest_value = 0;
    auto start = chrono::steady_clock::now();
    for (int i = 0; i < iterations; ++i) {
        for (int call = 0; call < CallsPerIteration; ++call) digest_value += timer.elapsed();
    }
    auto finish = chrono::steady_clock::now();
    double elapsed_ns = chrono::duration<double, nano>(finish - start).count();
    return {elapsed_ns / double(iterations), bit_cast<uint64_t>(digest_value)};
}

TimerTiming benchmark_timer(const Config &config) {
    TimerTiming result;
    uint64_t one_digest = 0;
    result.one_call_ns = median_sample(config.warmup, config.samples, [&] {
        auto [ns, digest] = run_timer_once<1>(config.timer_iterations);
        one_digest = mix_digest(one_digest, digest);
        return ns;
    });
    uint64_t two_digest = 0;
    result.two_calls_ns = median_sample(config.warmup, config.samples, [&] {
        auto [ns, digest] = run_timer_once<2>(config.timer_iterations);
        two_digest = mix_digest(two_digest, digest);
        return ns;
    });
    result.extra_call_ns = result.two_calls_ns - result.one_call_ns;
    result.digest = one_digest ^ rotl(two_digest, 23);
    return result;
}

int parse_positive(string_view value, string_view option) {
    string text(value);
    char *end = nullptr;
    long parsed = strtol(text.c_str(), &end, 10);
    if (end == text.c_str() || *end != '\0' || parsed <= 0 || parsed > 1000000000L) {
        cerr << "invalid value for " << option << ": " << value << '\n';
        exit(2);
    }
    return int(parsed);
}

Config parse_config(int argc, char **argv) {
    Config config;
    for (int i = 1; i < argc; ++i) {
        string_view option = argv[i];
        if (i + 1 >= argc) {
            cerr << "missing value for " << option << '\n';
            exit(2);
        }
        string_view value = argv[++i];
        if (option == "--warmup") config.warmup = parse_positive(value, option);
        else if (option == "--samples") config.samples = parse_positive(value, option);
        else if (option == "--beam-width") config.beam_width = parse_positive(value, option);
        else if (option == "--candidate-ops") config.candidate_ops = parse_positive(value, option);
        else if (option == "--timer-iterations") config.timer_iterations = parse_positive(value, option);
        else if (option == "--block-mib") {
            config.block_target_bytes = size_t(parse_positive(value, option)) << 20;
        } else {
            cerr << "unknown option: " << option << '\n';
            exit(2);
        }
    }
    if (config.samples % 2 == 0) {
        cerr << "--samples must be odd\n";
        exit(2);
    }
    if (config.candidate_ops < config.beam_width) {
        cerr << "--candidate-ops must be at least --beam-width\n";
        exit(2);
    }
    return config;
}

string_view source_name(SourceKind source_kind) {
    return source_kind == SourceKind::lvalue ? "lvalue" : "rvalue";
}

void print_counts(const OpCounts &counts, int operations) {
    auto per_k = [operations](uint64_t value) { return 1000.0 * double(value) / double(operations); };
    cout << fixed << setprecision(1) << per_k(counts.default_construct) << ',' << per_k(counts.copy_construct)
         << ',' << per_k(counts.move_construct) << ',' << per_k(counts.copy_assign) << ','
         << per_k(counts.move_assign) << ',' << per_k(counts.destruct);
}

template<size_t Bytes, SourceKind Source>
void print_candidate_rows(const Config &config) {
    constexpr array<int, 5> rates = {0, 10, 100, 500, 1000};
    for (int permille : rates) {
        CandidatePair result = benchmark_candidate<Bytes, Source>(permille, config);
        int count_ops = min(config.candidate_ops, 10000);
        double ratio = result.value.ns_per_op / result.lazy.ns_per_op;
        cout << Bytes << ',' << source_name(Source) << ',' << fixed << setprecision(1)
             << double(permille) / 10.0 << ',' << setprecision(3) << result.value.ns_per_op << ','
             << result.lazy.ns_per_op << ',' << ratio << ',' << result.value.accepted << ','
             << hex << (result.value.digest ^ rotl(result.lazy.digest, 13)) << dec << ',';
        print_counts(result.value_counts, count_ops);
        cout << ',';
        print_counts(result.lazy_counts, count_ops);
        cout << '\n';
    }
}

template<size_t Bytes>
void print_block_row(const Config &config) {
    auto [old_result, new_result] = benchmark_block<Bytes>(config);
    auto [old_counts, new_counts] = count_block_operations<Bytes>(config);
    cout << Bytes << ',' << fixed << setprecision(3) << old_result.ns_per_action << ','
         << new_result.ns_per_action << ',' << old_result.ns_per_action / new_result.ns_per_action << ','
         << old_result.actions << ',' << hex << (old_result.digest ^ rotl(new_result.digest, 19)) << dec << ',';
    print_counts(old_counts, config.beam_width);
    cout << ',';
    print_counts(new_counts, config.beam_width);
    cout << '\n';
}

}

int main(int argc, char **argv) {
    Config config = parse_config(argc, argv);
    cout << "config,warmup=" << config.warmup << ",samples=" << config.samples
         << ",beam_width=" << config.beam_width << ",candidate_ops=" << config.candidate_ops
         << ",timer_iterations=" << config.timer_iterations << ",block_target_bytes="
         << config.block_target_bytes << '\n';

    cout << "candidate_bytes,source,accept_percent,value_ns_per_op,lazy_ns_per_op,value_over_lazy,accepted,digest,"
            "value_default_per_k,value_copy_ctor_per_k,value_move_ctor_per_k,value_copy_assign_per_k,"
            "value_move_assign_per_k,value_destruct_per_k,lazy_default_per_k,lazy_copy_ctor_per_k,"
            "lazy_move_ctor_per_k,lazy_copy_assign_per_k,lazy_move_assign_per_k,lazy_destruct_per_k\n";
    print_candidate_rows<8, SourceKind::lvalue>(config);
    print_candidate_rows<8, SourceKind::rvalue>(config);
    print_candidate_rows<32, SourceKind::lvalue>(config);
    print_candidate_rows<32, SourceKind::rvalue>(config);
    print_candidate_rows<128, SourceKind::lvalue>(config);
    print_candidate_rows<128, SourceKind::rvalue>(config);
    print_candidate_rows<512, SourceKind::lvalue>(config);
    print_candidate_rows<512, SourceKind::rvalue>(config);

    cout << "block_bytes,resize_assign_ns_per_action,emplace_ns_per_action,old_over_new,actions,digest,"
            "old_default_per_k,old_copy_ctor_per_k,old_move_ctor_per_k,old_copy_assign_per_k,"
            "old_move_assign_per_k,old_destruct_per_k,new_default_per_k,new_copy_ctor_per_k,"
            "new_move_ctor_per_k,new_copy_assign_per_k,new_move_assign_per_k,new_destruct_per_k\n";
    print_block_row<8>(config);
    print_block_row<32>(config);
    print_block_row<128>(config);
    print_block_row<512>(config);

    TimerTiming timer = benchmark_timer(config);
    cout << "timer_one_call_ns,timer_two_calls_ns,extra_call_ns,digest\n";
    cout << fixed << setprecision(3) << timer.one_call_ns << ',' << timer.two_calls_ns << ','
         << timer.extra_call_ns << ',' << hex << timer.digest << dec << '\n';
}
