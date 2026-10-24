// Ordinary-tree boundary-search benchmark
// Prepare250000 point updates from the fixed mixed trace, then time250000 searches per direction
// Query generation, preparation, and independent verification are outside the timed search loop
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

using namespace std;

namespace {
using Index = long long;
using Clock = chrono::steady_clock;
long long op(long long a, long long b) { return a + b; }
long long e() { return 0; }
long long repeat(long long value, Index length) { return value * length; }
using Plain = titan23::DynamicSegmentTree<Index, long long, op, e>;
using Initial = titan23::DynamicSegmentTreeInit<Index, long long, op, e, repeat>;

struct Options {
    Index n = 1000000000LL;
    size_t q = 500000;
    uint64_t seed = 23;
    string distribution = "wide", kind = "plain";
    bool verify = false;
};

Options parse_options(int argc, char** argv) {
    Options opt;
    for (int i = 1; i < argc; ++i) {
        const string arg = argv[i];
        if (arg == "--help") {
            cout << "--n N --q Q --seed SEED --distribution wide|short|local\n"
                         "--kind plain|init --verify\n"
                         "Q/2 max_right and Q/2 min_left, shuffled; prepare Q/2 set; Init starts at 3\n";
            exit(0);
        }
        if (arg == "--verify") { opt.verify = true; continue; }
        if (++i == argc) throw invalid_argument("missing value for " + arg);
        const string value = argv[i];
        if (arg == "--n") opt.n = stoll(value);
        else if (arg == "--q") opt.q = stoull(value);
        else if (arg == "--seed") opt.seed = stoull(value);
        else if (arg == "--distribution") opt.distribution = value;
        else if (arg == "--kind") opt.kind = value;
        else throw invalid_argument("unknown option " + arg);
    }
    if (opt.n <= 0 || opt.n > numeric_limits<Index>::max() / 9 ||
        opt.q == 0 || opt.q % 2) throw invalid_argument("require positive n, positive even q, and 9*n fitting int64");
    if (opt.distribution != "wide" && opt.distribution != "short" && opt.distribution != "local")
        throw invalid_argument("invalid distribution");
    if (opt.kind != "plain" && opt.kind != "init") throw invalid_argument("invalid kind");
    return opt;
}

struct Query { Index l, r; long long value; bool update; };

pair<Index, Index> interval(const Options& opt, mt19937_64& rng) {
    if (opt.distribution == "short") {
        const Index l = uniform_int_distribution<Index>(0, opt.n - 1)(rng);
        const Index length = uniform_int_distribution<Index>(1, min<Index>(1024, opt.n))(rng);
        return {l, l + min(length, opt.n - l)};
    }
    const Index width = opt.distribution == "local" ? min<Index>(1000000, opt.n) : opt.n;
    const Index offset = (opt.n - width) / 2;
    Index l = uniform_int_distribution<Index>(0, width - 1)(rng);
    Index r = uniform_int_distribution<Index>(0, width - 1)(rng);
    if (l > r) swap(l, r);
    return {l + offset, r + offset + 1};
}

vector<Query> generate(const Options& opt) {
    mt19937_64 rng(opt.seed);
    vector<Query> queries;
    queries.reserve(opt.q);
    const Index width = opt.distribution == "local" ? min<Index>(1000000, opt.n) : opt.n;
    const Index offset = (opt.n - width) / 2;
    for (size_t i = 0; i < opt.q; ++i) {
        Index l, r;
        if (i < opt.q / 2) {
            l = offset + uniform_int_distribution<Index>(0, width - 1)(rng);
            r = l + 1;
        } else {
            const auto range = interval(opt, rng);
            l = range.first;
            r = range.second;
        }
        queries.push_back({l, r, uniform_int_distribution<long long>(1, 9)(rng), i < opt.q / 2});
    }
    shuffle(queries.begin(), queries.end(), rng);
    return queries;
}

void checksum_add(uint64_t& checksum, long long value) {
    checksum ^= static_cast<uint64_t>(value) + UINT64_C(0x9e3779b97f4a7c15) +
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

}  // namespace

namespace {
class PointSearchReference {
    vector<Index> xs;
    vector<long long> values, prefixes;
public:
    /// @brief 点更新後の区間と累積和を独立に構築する / O(QlogQ)
    PointSearchReference(Index n, long long initial, const vector<Query>& updates) : xs{0, n} {
        for (const auto& q : updates) if (q.update) { xs.push_back(q.l); xs.push_back(q.l + 1); }
        sort(xs.begin(), xs.end());
        xs.erase(unique(xs.begin(), xs.end()), xs.end());
        values.assign(xs.size(), initial);
        for (const auto& q : updates) if (q.update)
            values[lower_bound(xs.begin(), xs.end(), q.l) - xs.begin()] = q.value;
        prefixes.resize(xs.size());
        for (size_t i = 0; i + 1 < xs.size(); ++i)
            prefixes[i + 1] = prefixes[i] + values[i] * (xs[i + 1] - xs[i]);
    }

    /// @brief [0,x)の和を返す / O(logQ)
    long long prefix(Index x) const {
        if (x == xs.back()) return prefixes.back();
        const auto i = upper_bound(xs.begin(), xs.end(), x) - xs.begin() - 1;
        return prefixes[i] + values[i] * (x - xs[i]);
    }

    /// @brief 累積和から最大の右端を求める / O(logQ)
    Index max_right(Index l, long long limit) const {
        const auto start = prefix(l);
        if (limit >= prefixes.back() - start) return xs.back();
        const auto target = start + limit;
        const auto i = upper_bound(prefixes.begin(), prefixes.end(), target) - prefixes.begin() - 1;
        return xs[i] + (target - prefixes[i]) / values[i];
    }

    /// @brief 累積和から最小の左端を求める / O(logQ)
    Index min_left(Index r, long long limit) const {
        const auto finish = prefix(r);
        if (limit >= finish) return 0;
        const auto target = finish - limit;
        const auto j = lower_bound(prefixes.begin(), prefixes.end(), target) - prefixes.begin();
        if (prefixes[j] == target) return xs[j];
        const auto remaining = target - prefixes[j - 1];
        return xs[j - 1] + remaining / values[j - 1] + (remaining % values[j - 1] != 0);
    }
};

template<class Tree>
void run_search(Tree& tree, const Options& opt, const vector<Query>& updates, long long initial) {
    const auto prepare_start = Clock::now();
    for (const auto& q : updates) if (q.update) tree.set(q.l, q.value);
    const double prepare_ms = chrono::duration<double, milli>(Clock::now() - prepare_start).count();
    mt19937_64 rng(opt.seed);
    vector<Query> probes;
    probes.reserve(opt.q);
    const auto total = tree.all_prod();
    for (size_t i = 0; i < opt.q; ++i) {
        const auto range = interval(opt, rng);
        const auto limit = uniform_int_distribution<long long>(0, total)(rng);
        probes.push_back({range.first, range.second, limit, i < opt.q / 2});
    }
    shuffle(probes.begin(), probes.end(), rng);
    const auto before = tree.node_count();
    uint64_t checksum = 0;
    const auto start = Clock::now();
    for (const auto& q : probes) {
        const auto pred = [limit = q.value](long long sum) { return sum <= limit; };
        checksum_add(checksum, q.update ? tree.max_right(q.l, pred) : tree.min_left(q.r, pred));
    }
    const double elapsed_ms = chrono::duration<double, milli>(Clock::now() - start).count();
    const auto rss = peak_rss_bytes();
    if (before != tree.node_count()) throw runtime_error("search allocated nodes");
    if (opt.verify) {
        PointSearchReference ref(opt.n, initial, updates);
        uint64_t expected = 0;
        for (const auto& q : probes)
            checksum_add(expected, q.update ? ref.max_right(q.l, q.value) : ref.min_left(q.r, q.value));
        if (expected != checksum) throw runtime_error("search prefix reference disagrees");
    }
    cout << fixed << setprecision(3)
              << "{\"kind\":\"" << opt.kind << "\",\"distribution\":\"" << opt.distribution
              << "\",\"n\":" << opt.n << ",\"q\":" << opt.q << ",\"seed\":" << opt.seed
              << ",\"initial\":" << initial << ",\"elapsed_ms\":" << elapsed_ms
              << ",\"prepare_ms\":" << prepare_ms << ",\"node_count\":" << before
              << ",\"peak_rss_bytes\":" << rss << ",\"checksum\":" << checksum
              << ",\"verified\":" << (opt.verify ? "true" : "false") << "}\n";
}
}

int main(int argc, char** argv) {
    try {
        const auto opt = parse_options(argc, argv);
        const auto updates = generate(opt);
        if (opt.kind == "plain") {
            Plain tree(opt.n);
            run_search(tree, opt, updates, 0);
        } else {
            Initial tree(opt.n, 3);
            run_search(tree, opt, updates, 3);
        }
    } catch (const exception& error) {
        cerr << error.what() << '\n';
        return 1;
    }
}
