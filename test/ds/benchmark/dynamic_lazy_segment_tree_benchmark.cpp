/// https://github.com/titan-23/Library_cpp/blob/main/test/ds/benchmark/dynamic_lazy_segment_tree_benchmark.cpp
// g++ -std=c++17 -O3 -DNDEBUG -I. this_file.cpp -o /tmp/dlst_bench
// /tmp/dlst_bench --distribution wide --verify
// Each invocation measures a fresh tree. Run separate processes for repeats.
// Override DYNAMIC_LAZY_SEGMENT_TREE_HEADER and define DLST_BENCH_BASELINE
// to compare a corrected pointer-based snapshot exposing node_count().
// Define DLST_BENCH_WIDE_SUM for 128-bit sums; use the same build for both N values.
#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <limits>
#include <random>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>
#if defined(__linux__) || defined(__APPLE__)
#include <sys/resource.h>
#endif

#ifndef DYNAMIC_LAZY_SEGMENT_TREE_HEADER
#define DYNAMIC_LAZY_SEGMENT_TREE_HEADER "titan_cpplib/ds/dynamic_lazy_segment_tree.cpp"
#endif
#include DYNAMIC_LAZY_SEGMENT_TREE_HEADER

namespace {
using Index = long long;
using Clock = std::chrono::steady_clock;
#ifdef DLST_BENCH_WIDE_SUM
using Sum = __int128_t;
using UnsignedSum = __uint128_t;
constexpr int sum_bits = 128;
#else
using Sum = long long;
using UnsignedSum = std::uint64_t;
constexpr int sum_bits = 64;
#endif
struct S { Sum sum; Index len; };
S op(S a, S b) { return {a.sum + b.sum, a.len + b.len}; }
S e() { return {0, 0}; }
S mapping(long long f, S s) { return {s.sum + static_cast<Sum>(f) * s.len, s.len}; }
long long composition(long long f, long long g) { return f + g; }
long long id() { return 0; }
S repeat(S s, Index n) { return {s.sum * n, s.len * n}; }
using Tree = titan23::DynamicLazySegmentTree<Index, S, op, e, long long,
                                           mapping, composition, id, repeat>;

struct Options {
    Index n = 1000000000LL;
    std::size_t q = 500000;
    std::uint64_t seed = 23;
    std::string distribution = "wide", mode = "mixed";
    std::size_t reserve = 0;
    long long initial = 0;
    bool verify = false;
};

Options parse_options(int argc, char** argv) {
    Options opt;
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--help") {
            std::cout << "--n N --q Q --seed SEED --distribution wide|short|local\n"
                         "--mode mixed|search --reserve NODES --init VALUE --verify\n"
                         "Q must be positive and even. Mixed: Q/2 apply + Q/2 prod.\n"
                         "Search: Q/2 preparation updates, then Q/2 max_right + Q/2 min_left.\n"
                         "Generation, construction, reservation and verification are not timed.\n";
            std::exit(0);
        }
        if (arg == "--verify") { opt.verify = true; continue; }
        if (i + 1 == argc) throw std::invalid_argument("missing argument for " + arg);
        const std::string value = argv[++i];
        if (arg == "--n") opt.n = std::stoll(value);
        else if (arg == "--q") opt.q = std::stoull(value);
        else if (arg == "--seed") opt.seed = std::stoull(value);
        else if (arg == "--distribution") opt.distribution = value;
        else if (arg == "--mode") opt.mode = value;
        else if (arg == "--reserve") opt.reserve = std::stoull(value);
        else if (arg == "--init") opt.initial = std::stoll(value);
        else throw std::invalid_argument("unknown option " + arg);
    }
    if (opt.n <= 0 || opt.q == 0 || opt.q % 2 || opt.initial < 0)
        throw std::invalid_argument("require n > 0, positive even q and init >= 0");
    if (opt.distribution != "wide" && opt.distribution != "short" && opt.distribution != "local")
        throw std::invalid_argument("distribution must be wide, short or local");
    if (opt.mode != "mixed" && opt.mode != "search")
        throw std::invalid_argument("mode must be mixed or search");
    // Every addition is at most 9; also keep composed long-long actions in range.
    if (opt.q / 2 > static_cast<std::uint64_t>(std::numeric_limits<long long>::max()) / 9)
        throw std::invalid_argument("q can overflow a signed 64-bit lazy action");
    const UnsignedSum max_sum = (~UnsignedSum(0)) >> 1;
    const UnsignedSum max_value = max_sum / static_cast<UnsignedSum>(opt.n);
    if (static_cast<UnsignedSum>(opt.initial) > max_value ||
        static_cast<UnsignedSum>(opt.q / 2) > (max_value - static_cast<UnsignedSum>(opt.initial)) / 9) {
#ifdef DLST_BENCH_WIDE_SUM
        throw std::invalid_argument("n, q and init can overflow a signed 128-bit sum");
#else
        throw std::invalid_argument("n, q and init can overflow a signed 64-bit sum; rebuild with -DDLST_BENCH_WIDE_SUM for 128-bit sums");
#endif
    }
#ifdef DLST_BENCH_BASELINE
    if (opt.reserve || opt.mode == "search")
        throw std::invalid_argument("baseline supports mixed mode without reservation only");
#endif
    return opt;
}

struct Query {
    Index l, r;
    long long value;
    bool update;
};

std::pair<Index, Index> interval(const Options& opt, std::mt19937_64& rng) {
    if (opt.distribution == "short") {
        const Index l = std::uniform_int_distribution<Index>(0, opt.n - 1)(rng);
        const Index len = std::uniform_int_distribution<Index>(1, std::min<Index>(1024, opt.n))(rng);
        return {l, l + std::min(len, opt.n - l)};
    }
    const Index width = opt.distribution == "local" ? std::min<Index>(1000000, opt.n) : opt.n;
    const Index offset = (opt.n - width) / 2;
    Index l = std::uniform_int_distribution<Index>(0, width - 1)(rng);
    Index r = std::uniform_int_distribution<Index>(0, width - 1)(rng);
    if (l > r) std::swap(l, r);
    return {l + offset, r + offset + 1};
}

std::vector<Query> generate(const Options& opt, std::mt19937_64& rng, bool updates_only) {
    const std::size_t count = updates_only ? opt.q / 2 : opt.q;
    std::vector<Query> queries(count);
    for (std::size_t i = 0; i < count; ++i) {
        const auto [l, r] = interval(opt, rng);
        queries[i] = {l, r, std::uniform_int_distribution<long long>(1, 9)(rng),
                      updates_only || i < count / 2};
    }
    std::shuffle(queries.begin(), queries.end(), rng);
    return queries;
}

void checksum_add(std::uint64_t& checksum, Sum value) {
    checksum ^= static_cast<std::uint64_t>(value) + UINT64_C(0x9e3779b97f4a7c15) +
                (checksum << 6) + (checksum >> 2);
#ifdef DLST_BENCH_WIDE_SUM
    checksum ^= static_cast<std::uint64_t>(static_cast<UnsignedSum>(value) >> 64) +
                UINT64_C(0x9e3779b97f4a7c15) + (checksum << 6) + (checksum >> 2);
#endif
}

#if defined(DLST_BENCH_WIDE_SUM) && !defined(DLST_BENCH_BASELINE)
Sum random_sum_limit(Sum maximum, std::mt19937_64& rng) {
    const UnsignedSum bound = static_cast<UnsignedSum>(maximum) + 1;
    const UnsignedSum threshold = (UnsignedSum(0) - bound) % bound;
    while (true) {
        const UnsignedSum high = rng();
        const UnsignedSum low = rng();
        const UnsignedSum value = (high << 64) | low;
        if (value >= threshold) return static_cast<Sum>(value % bound);
    }
}
#endif

long long peak_rss_bytes() {
#if defined(__linux__) || defined(__APPLE__)
    rusage usage{};
    if (getrusage(RUSAGE_SELF, &usage) != 0) return -1;
#if defined(__APPLE__)
    return usage.ru_maxrss;
#else
    return static_cast<long long>(usage.ru_maxrss) * 1024;
#endif
#else
    return -1;
#endif
}

// Independent reference: integrate the piecewise-constant difference sequence.
// Two Fenwick trees store delta and coordinate * delta. P(x) = x*A(x)-B(x).
class IntegralReference {
    std::vector<Index> xs;
    std::vector<Sum> delta, weighted;
    Sum initial;

    void add(std::vector<Sum>& bit, std::size_t i, Sum value) {
        for (++i; i < bit.size(); i += i & -i) bit[i] += value;
    }
    Sum sum(const std::vector<Sum>& bit, std::size_t i) const {
        Sum result = 0;
        for (; i; i -= i & -i) result += bit[i];
        return result;
    }
    void add_difference(Index x, long long value) {
        const auto i = std::lower_bound(xs.begin(), xs.end(), x) - xs.begin();
        add(delta, i, value);
        add(weighted, i, static_cast<Sum>(value) * x);
    }

public:
    IntegralReference(const std::vector<Query>& queries, long long init) : initial(init) {
        xs.reserve(queries.size() * 2);
        for (const auto& q : queries) { xs.push_back(q.l); xs.push_back(q.r); }
        std::sort(xs.begin(), xs.end());
        xs.erase(std::unique(xs.begin(), xs.end()), xs.end());
        delta.resize(xs.size() + 1);
        weighted.resize(xs.size() + 1);
    }
    void apply(Index l, Index r, long long value) {
        add_difference(l, value);
        add_difference(r, -value);
    }
    Sum prefix(Index x) const {
        const auto i = std::upper_bound(xs.begin(), xs.end(), x) - xs.begin();
        return static_cast<Sum>(x) * (initial + sum(delta, i)) - sum(weighted, i);
    }
    Sum prod(Index l, Index r) const { return prefix(r) - prefix(l); }
};

std::uint64_t verify_mixed(const std::vector<Query>& queries, long long initial) {
    IntegralReference reference(queries, initial);
    std::uint64_t checksum = 0;
    for (const auto& q : queries) {
        if (q.update) reference.apply(q.l, q.r, q.value);
        else checksum_add(checksum, reference.prod(q.l, q.r));
    }
    return checksum;
}

#ifndef DLST_BENCH_BASELINE
// Static compressed prefix sums answer boundary searches independently of the tree.
class SearchReference {
    std::vector<Index> xs;
    std::vector<Sum> values, prefixes;

public:
    SearchReference(Index n, long long initial, const std::vector<Query>& updates) : xs{0, n} {
        for (const auto& q : updates) { xs.push_back(q.l); xs.push_back(q.r); }
        std::sort(xs.begin(), xs.end());
        xs.erase(std::unique(xs.begin(), xs.end()), xs.end());
        values.assign(xs.size(), 0);
        for (const auto& q : updates) {
            values[std::lower_bound(xs.begin(), xs.end(), q.l) - xs.begin()] += q.value;
            values[std::lower_bound(xs.begin(), xs.end(), q.r) - xs.begin()] -= q.value;
        }
        prefixes.resize(xs.size());
        Sum current = initial;
        for (std::size_t i = 0; i + 1 < xs.size(); ++i) {
            current += values[i];
            values[i] = current;
            prefixes[i + 1] = prefixes[i] + current * (xs[i + 1] - xs[i]);
        }
    }
    Sum prefix(Index x) const {
        if (x == xs.back()) return prefixes.back();
        const auto i = std::upper_bound(xs.begin(), xs.end(), x) - xs.begin() - 1;
        return prefixes[i] + values[i] * (x - xs[i]);
    }
    Index max_right(Index l, Sum limit) const {
        const Sum start = prefix(l);
        if (limit >= prefixes.back() - start) return xs.back();
        const Sum target = start + limit;
        const auto i = std::upper_bound(prefixes.begin(), prefixes.end(), target) - prefixes.begin() - 1;
        return xs[i] + static_cast<Index>((target - prefixes[i]) / values[i]);
    }
    Index min_left(Index r, Sum limit) const {
        const Sum finish = prefix(r);
        if (limit >= finish) return 0;
        const Sum target = finish - limit;
        const auto j = std::lower_bound(prefixes.begin(), prefixes.end(), target) - prefixes.begin();
        if (prefixes[j] == target) return xs[j];
        const Sum remaining = target - prefixes[j - 1];
        return xs[j - 1] + static_cast<Index>(remaining / values[j - 1] + (remaining % values[j - 1] != 0));
    }
};
#endif

} // namespace

int main(int argc, char** argv) {
    try {
        const Options opt = parse_options(argc, argv);
        std::mt19937_64 rng(opt.seed);
        auto queries = generate(opt, rng, opt.mode == "search");
        Tree tree(opt.n, S{opt.initial, 1});
#ifndef DLST_BENCH_BASELINE
        if (opt.reserve) tree.reserve_nodes(opt.reserve);
#endif
        std::uint64_t checksum = 0;
        double seconds = 0, prepare_seconds = 0;
        bool verified = false;
        long long rss = -1;
        std::size_t nodes = 0;
        if (opt.mode == "mixed") {
            const auto start = Clock::now();
            for (const auto& q : queries) {
                if (q.update) tree.apply(q.l, q.r, q.value);
                else checksum_add(checksum, tree.prod(q.l, q.r).sum);
            }
            seconds = std::chrono::duration<double>(Clock::now() - start).count();
            rss = peak_rss_bytes();
            nodes = tree.node_count();
            if (opt.verify) {
                if (verify_mixed(queries, opt.initial) != checksum)
                    throw std::runtime_error("mixed checksum differs from Fenwick integral reference");
                verified = true;
            }
        }
#ifndef DLST_BENCH_BASELINE
        else {
            const auto prepare_start = Clock::now();
            for (const auto& q : queries) tree.apply(q.l, q.r, q.value);
            prepare_seconds = std::chrono::duration<double>(Clock::now() - prepare_start).count();
            const Sum total = tree.all_prod().sum;
            auto probes = generate(opt, rng, false);
#ifdef DLST_BENCH_WIDE_SUM
            std::vector<Sum> limits(probes.size());
            for (auto& limit : limits) limit = random_sum_limit(total, rng);
#else
            for (auto& q : probes)
                q.value = std::uniform_int_distribution<long long>(0, total)(rng);
#endif
            const auto nodes_before = tree.node_count();
            const auto start = Clock::now();
#ifdef DLST_BENCH_WIDE_SUM
            for (std::size_t i = 0; i < probes.size(); ++i) {
                const auto& q = probes[i];
                const Sum limit = limits[i];
                const auto pred = [limit](S value) { return value.sum <= limit; };
                checksum_add(checksum, q.update ? tree.max_right(q.l, pred) : tree.min_left(q.r, pred));
            }
#else
            for (const auto& q : probes) {
                const auto pred = [&q](S value) { return value.sum <= q.value; };
                checksum_add(checksum, q.update ? tree.max_right(q.l, pred) : tree.min_left(q.r, pred));
            }
#endif
            seconds = std::chrono::duration<double>(Clock::now() - start).count();
            rss = peak_rss_bytes();
            nodes = tree.node_count();
            if (nodes != nodes_before) throw std::runtime_error("boundary searches allocated nodes");
            if (opt.verify) {
                SearchReference reference(opt.n, opt.initial, queries);
                std::uint64_t expected = 0;
#ifdef DLST_BENCH_WIDE_SUM
                for (std::size_t i = 0; i < probes.size(); ++i) {
                    const auto& q = probes[i];
                    checksum_add(expected, q.update ? reference.max_right(q.l, limits[i]) : reference.min_left(q.r, limits[i]));
                }
#else
                for (const auto& q : probes)
                    checksum_add(expected, q.update ? reference.max_right(q.l, q.value) : reference.min_left(q.r, q.value));
#endif
                if (expected != checksum)
                    throw std::runtime_error("search checksum differs from compressed prefix reference");
                verified = true;
            }
        }
#endif
        std::cout << std::fixed << std::setprecision(6)
#ifdef DLST_BENCH_BASELINE
                  << "implementation=baseline"
#else
                  << "implementation=current"
#endif
                  << " n=" << opt.n << " q=" << opt.q << " seed=" << opt.seed
                  << " sum_bits=" << sum_bits
                  << " distribution=" << opt.distribution << " mode=" << opt.mode
                  << " init=" << opt.initial << " reserve=" << opt.reserve
                  << " seconds=" << seconds << " prepare_seconds=" << prepare_seconds
                  << " checksum=" << checksum << " nodes=" << nodes
                  << " peak_rss_bytes=" << rss << " verified=" << (verified ? "yes" : "no") << '\n';
    } catch (const std::exception& error) {
        std::cerr << "benchmark: " << error.what() << '\n';
        return 1;
    }
}
