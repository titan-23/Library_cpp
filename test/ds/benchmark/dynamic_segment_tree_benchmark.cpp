// Fixed ordinary-tree benchmark. Build: g++ -std=c++17 -O3 -DNDEBUG -I. this_file.cpp
// Each invocation is a fresh process; construction/generation/verification are untimed.
// The companion prepare_dynamic_segment_tree_baseline.py recreates the pointer baseline.
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

#include "titan_cpplib/ds/dynamic_segment_tree.cpp"
#include "titan_cpplib/ds/dynamic_segment_tree_init.cpp"
#ifndef DST_BENCH_IMPLEMENTATION
#define DST_BENCH_IMPLEMENTATION "patricia"
#endif

namespace {
using Index = long long;
using Clock = std::chrono::steady_clock;
long long op(long long a, long long b) { return a + b; }
long long e() { return 0; }
long long repeat(long long value, Index length) { return value * length; }
using Plain = titan23::DynamicSegmentTree<Index, long long, op, e>;
using Initial = titan23::DynamicSegmentTreeInit<Index, long long, op, e, repeat>;

struct Options {
    Index n = 1000000000LL;
    std::size_t q = 500000;
    std::uint64_t seed = 23;
    std::string distribution = "wide", kind = "plain";
    bool verify = false;
};

Options parse_options(int argc, char** argv) {
    Options opt;
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--help") {
            std::cout << "--n N --q Q --seed SEED --distribution wide|short|local\n"
                         "--kind plain|init --verify\n"
                         "Q/2 set and Q/2 prod, shuffled; init tree starts at 3.\n";
            std::exit(0);
        }
        if (arg == "--verify") { opt.verify = true; continue; }
        if (++i == argc) throw std::invalid_argument("missing value for " + arg);
        const std::string value = argv[i];
        if (arg == "--n") opt.n = std::stoll(value);
        else if (arg == "--q") opt.q = std::stoull(value);
        else if (arg == "--seed") opt.seed = std::stoull(value);
        else if (arg == "--distribution") opt.distribution = value;
        else if (arg == "--kind") opt.kind = value;
        else throw std::invalid_argument("unknown option " + arg);
    }
    if (opt.n <= 0 || opt.n > std::numeric_limits<Index>::max() / 9 ||
        opt.q == 0 || opt.q % 2) throw std::invalid_argument("require positive n, positive even q, and 9*n fitting int64");
    if (opt.distribution != "wide" && opt.distribution != "short" && opt.distribution != "local")
        throw std::invalid_argument("invalid distribution");
    if (opt.kind != "plain" && opt.kind != "init") throw std::invalid_argument("invalid kind");
    return opt;
}

struct Query { Index l, r; long long value; bool update; };

std::pair<Index, Index> interval(const Options& opt, std::mt19937_64& rng) {
    if (opt.distribution == "short") {
        const Index l = std::uniform_int_distribution<Index>(0, opt.n - 1)(rng);
        const Index length = std::uniform_int_distribution<Index>(1, std::min<Index>(1024, opt.n))(rng);
        return {l, l + std::min(length, opt.n - l)};
    }
    const Index width = opt.distribution == "local" ? std::min<Index>(1000000, opt.n) : opt.n;
    const Index offset = (opt.n - width) / 2;
    Index l = std::uniform_int_distribution<Index>(0, width - 1)(rng);
    Index r = std::uniform_int_distribution<Index>(0, width - 1)(rng);
    if (l > r) std::swap(l, r);
    return {l + offset, r + offset + 1};
}

std::vector<Query> generate(const Options& opt) {
    std::mt19937_64 rng(opt.seed);
    std::vector<Query> queries;
    queries.reserve(opt.q);
    const Index width = opt.distribution == "local" ? std::min<Index>(1000000, opt.n) : opt.n;
    const Index offset = (opt.n - width) / 2;
    for (std::size_t i = 0; i < opt.q; ++i) {
        Index l, r;
        if (i < opt.q / 2) {
            l = offset + std::uniform_int_distribution<Index>(0, width - 1)(rng);
            r = l + 1;
        } else {
            const auto range = interval(opt, rng);
            l = range.first;
            r = range.second;
        }
        queries.push_back({l, r, std::uniform_int_distribution<long long>(1, 9)(rng), i < opt.q / 2});
    }
    std::shuffle(queries.begin(), queries.end(), rng);
    return queries;
}

void checksum_add(std::uint64_t& checksum, long long value) {
    checksum ^= static_cast<std::uint64_t>(value) + UINT64_C(0x9e3779b97f4a7c15) +
                (checksum << 6) + (checksum >> 2);
}

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

// Independent reference: coordinate-compressed point values and a Fenwick tree.
std::uint64_t reference_checksum(const std::vector<Query>& queries, long long initial) {
    std::vector<Index> xs;
    for (const auto& q : queries) if (q.update) xs.push_back(q.l);
    std::sort(xs.begin(), xs.end());
    xs.erase(std::unique(xs.begin(), xs.end()), xs.end());
    std::vector<long long> values(xs.size(), initial), bit(xs.size() + 1, 0);
    const auto prefix = [&](Index x) {
        auto i = static_cast<std::size_t>(std::lower_bound(xs.begin(), xs.end(), x) - xs.begin());
        long long result = 0;
        for (; i; i -= i & -i) result += bit[i];
        return result;
    };
    std::uint64_t checksum = 0;
    for (const auto& q : queries) {
        if (q.update) {
            auto index = static_cast<std::size_t>(std::lower_bound(xs.begin(), xs.end(), q.l) - xs.begin());
            const long long delta = q.value - values[index];
            values[index] = q.value;
            for (++index; index < bit.size(); index += index & -index) bit[index] += delta;
        } else checksum_add(checksum, initial * (q.r - q.l) + prefix(q.r) - prefix(q.l));
    }
    return checksum;
}

template<class Tree>
void run(Tree& tree, const Options& opt, const std::vector<Query>& queries, long long initial) {
    std::uint64_t checksum = 0;
    const auto start = Clock::now();
    for (const auto& q : queries) {
        if (q.update) tree.set(q.l, q.value);
        else checksum_add(checksum, tree.prod(q.l, q.r));
    }
    const auto stop = Clock::now();
    const auto rss = peak_rss_bytes();
    const auto count = tree.node_count();
    if (opt.verify && reference_checksum(queries, initial) != checksum)
        throw std::runtime_error("independent Fenwick reference disagrees");
    std::cout << std::fixed << std::setprecision(3)
              << "{\"implementation\":\"" << DST_BENCH_IMPLEMENTATION
              << "\",\"kind\":\"" << opt.kind << "\",\"distribution\":\"" << opt.distribution
              << "\",\"n\":" << opt.n << ",\"q\":" << opt.q << ",\"seed\":" << opt.seed
              << ",\"initial\":" << initial << ",\"elapsed_ms\":"
              << std::chrono::duration<double, std::milli>(stop - start).count()
              << ",\"node_count\":" << count << ",\"peak_rss_bytes\":" << rss
              << ",\"checksum\":" << checksum << ",\"verified\":" << (opt.verify ? "true" : "false") << "}\n";
}
}  // namespace

int main(int argc, char** argv) {
    try {
        const auto opt = parse_options(argc, argv);
        const auto queries = generate(opt);
        if (opt.kind == "plain") {
            Plain tree(opt.n);
            run(tree, opt, queries, 0);
        } else {
            Initial tree(opt.n, 3);
            run(tree, opt, queries, 3);
        }
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
