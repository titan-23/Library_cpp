/// https://github.com/titan-23/Library_cpp/blob/main/test/ds/benchmark/persistent_lazy_wbtree_benchmark.cpp
// g++ -std=c++17 -O3 -DNDEBUG -I. this_file.cpp -o /tmp/plwbt_bench
// /tmp/plwbt_bench --n 500000 --q 500000 --mode latest --seed 23
// /tmp/plwbt_bench --n 64 --q 1000 --mode history --verify
// /tmp/plwbt_bench --workload access --n 500000 --q 500000
// /tmp/plwbt_bench --workload get --prepare-ranges 1000 --n 500000 --q 500000
// Override PERSISTENT_LAZY_WBTREE_HEADER to compile the preserved baseline.
// Define PLWBT_BENCH_DISABLE_SET for the raw baseline, whose set() does not
// compile. Accessor variants can use --workload get|set|access; get and set
// start from a clean tree, access selects set/get/apply/reverse uniformly.
// Each process measures one fresh allocator. All Q + 1 versions are retained;
// read-only queries retain an alias. Operations are selected uniformly among
// apply/prod/reverse/insert/pop/split_merge/get. split_merge rejoins in order.
// Initial values, query operands, source versions, and sizes are generated
// before timing. Build and query phases are timed separately. --verify runs a
// vector oracle and checks every retained version after collecting metrics.
// A separate validation checksum samples full aggregates and point values
// from the last version and up to 64 prior versions after collecting metrics.
// Baseline reads may allocate during this validation; these allocations and
// their runtime/RSS are intentionally excluded from the benchmark metrics.
// get/prod can first apply alternating range additions/reversals to version 0.
// Preparation uses a separate RNG and timer; timed query operands stay unchanged.
#include <algorithm>
#include <array>
#include <chrono>
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

#ifndef PERSISTENT_LAZY_WBTREE_HEADER
#define PERSISTENT_LAZY_WBTREE_HEADER "titan_cpplib/ds/persistent_lazy_wbtree.cpp"
#endif
#include PERSISTENT_LAZY_WBTREE_HEADER

#ifndef PERSISTENT_LAZY_WBTREE_BENCH_LABEL
#define PERSISTENT_LAZY_WBTREE_BENCH_LABEL "optimized"
#endif

namespace {
using Clock = std::chrono::steady_clock;
using Value = std::int64_t;
struct S { Value sum, count; };
S op(S a, S b) { return {a.sum + b.sum, a.count + b.count}; }
S e() { return {0, 0}; }
S mapping(Value f, S x) { return {x.sum + f * x.count, x.count}; }
Value composition(Value f, Value g) { return f + g; }
Value id() { return 0; }
using Tree = titan23::PersistentLazyWBTree<int, S, Value, op, mapping,
                                          composition, e, id>;

template<class Allocator>
auto allocator_capacity_bytes(const Allocator& allocator, int)
    -> decltype(allocator.capacity_bytes()) {
    return allocator.capacity_bytes();
}

template<class Allocator>
std::size_t allocator_capacity_bytes(const Allocator& allocator, long) {
    return allocator.tree.capacity() * sizeof(typename Allocator::Node)
        + allocator.data.capacity() * sizeof(typename Allocator::Data);
}

enum Kind { APPLY, PROD, REVERSE, INSERT, POP, SPLIT_MERGE, GET, SET, KIND_COUNT };
constexpr const char* kind_names[] = {
    "apply", "prod", "reverse", "insert", "pop", "split_merge", "get", "set"
};

struct Options {
    int n = 500000, q = 500000;
    std::uint64_t seed = 23;
    std::string mode = "latest";
    std::string workload = "mixed";
    int reserve = 0;
    int prepare_ranges = 0;
    bool verify = false;
};

Options parse_options(int argc, char** argv) {
    Options opt;
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--help") {
            std::cout << "--n N --q Q --seed SEED --mode latest|history "
                         "--workload mixed|get|prod|set|access --reserve NODES "
                         "--prepare-ranges N --verify\n"
                         "Defaults: N=Q=500000, seed=23, mode=latest, no reservation.\n"
                         "All historical versions remain live. Query generation, "
                         "reservation and verification are outside timed phases.\n"
                         "--prepare-ranges is for get/prod only; alternating range "
                         "updates/reversals have a separate preparation timer.\n"
                         "--verify is limited to (N+Q)*(Q+1) <= 5000000.\n";
            std::exit(0);
        }
        if (arg == "--verify") { opt.verify = true; continue; }
        if (i + 1 == argc) throw std::invalid_argument("missing value for " + arg);
        const std::string value = argv[++i];
        if (arg == "--n") opt.n = std::stoi(value);
        else if (arg == "--q") opt.q = std::stoi(value);
        else if (arg == "--seed") opt.seed = std::stoull(value);
        else if (arg == "--mode") opt.mode = value;
        else if (arg == "--workload") opt.workload = value;
        else if (arg == "--reserve") opt.reserve = std::stoi(value);
        else if (arg == "--prepare-ranges") opt.prepare_ranges = std::stoi(value);
        else throw std::invalid_argument("unknown option " + arg);
    }
    if (opt.n <= 0 || opt.q < 0 || opt.reserve < 0 || opt.prepare_ranges < 0 ||
        static_cast<std::int64_t>(opt.n) + opt.q > std::numeric_limits<int>::max())
        throw std::invalid_argument("require N > 0, Q >= 0, reserve >= 0, prepare-ranges >= 0, N+Q <= INT_MAX");
    if (opt.mode != "latest" && opt.mode != "history")
        throw std::invalid_argument("mode must be latest or history");
    if (opt.workload != "mixed" && opt.workload != "get" && opt.workload != "prod" &&
        opt.workload != "set" && opt.workload != "access")
        throw std::invalid_argument("workload must be mixed, get, prod, set or access");
    if (opt.prepare_ranges && opt.workload != "get" && opt.workload != "prod")
        throw std::invalid_argument("--prepare-ranges requires workload get or prod");
#ifdef PLWBT_BENCH_DISABLE_SET
    if (opt.workload == "set" || opt.workload == "access")
        throw std::invalid_argument("raw baseline does not support set/access workloads");
#endif
    const auto max_length = static_cast<std::uint64_t>(opt.n) + opt.q;
    const auto max_value = UINT64_C(1000000) + UINT64_C(100) *
        (static_cast<std::uint64_t>(opt.q) + opt.prepare_ranges);
    if (max_length > static_cast<std::uint64_t>(std::numeric_limits<Value>::max()) / max_value)
        throw std::invalid_argument("N and Q can overflow the 64-bit aggregate");
    if (opt.verify && max_length * (static_cast<std::uint64_t>(opt.q) + 1) > 5000000)
        throw std::invalid_argument("--verify requires (N+Q)*(Q+1) <= 5000000");
    return opt;
}

struct Query {
    int source, l, r;
    Kind kind;
    Value value;
};

struct Workload {
    std::vector<S> initial;
    std::vector<Query> preparation;
    std::vector<Query> queries;
    std::array<std::uint64_t, KIND_COUNT> counts{};
};

Workload generate(const Options& opt) {
    std::mt19937_64 rng(opt.seed);
    const auto index = [&](int maximum) {
        return std::uniform_int_distribution<int>(0, maximum)(rng);
    };
    const auto initial_value = [&]() {
        return std::uniform_int_distribution<Value>(-1000000, 1000000)(rng);
    };
    Workload work;
    work.initial.reserve(opt.n);
    for (int i = 0; i < opt.n; ++i) work.initial.push_back({initial_value(), 1});
    work.queries.reserve(opt.q);
    std::vector<int> lengths;
    lengths.reserve(static_cast<std::size_t>(opt.q) + 1);
    lengths.push_back(opt.n);
    for (int i = 0; i < opt.q; ++i) {
        Query q{};
        q.source = opt.mode == "latest" ? i : index(i);
        if (opt.workload == "get") q.kind = GET;
        else if (opt.workload == "prod") q.kind = PROD;
        else if (opt.workload == "set") q.kind = SET;
        else if (opt.workload == "access") {
            constexpr Kind access_kinds[] = {SET, GET, APPLY, REVERSE};
            q.kind = access_kinds[index(3)];
        } else q.kind = static_cast<Kind>(index(6));
        const int n = lengths[q.source];
        // The preserved baseline mishandles no-op updates to an empty root.
        // Generate a valid insertion instead when an operation needs elements.
        if (n == 0 && q.kind != INSERT && q.kind != SPLIT_MERGE) q.kind = INSERT;
        int next_length = n;
        if (q.kind == APPLY || q.kind == PROD || q.kind == REVERSE) {
            q.l = index(n - 1);
            q.r = index(n - 1);
            if (q.l > q.r) std::swap(q.l, q.r);
            ++q.r;
            if (q.kind == APPLY)
                q.value = std::uniform_int_distribution<Value>(-100, 100)(rng);
        } else if (q.kind == INSERT) {
            q.l = index(n);
            q.value = initial_value();
            ++next_length;
        } else if (q.kind == POP || q.kind == GET || q.kind == SET) {
            q.l = index(n - 1);
            if (q.kind == POP) --next_length;
            if (q.kind == SET) q.value = initial_value();
        } else {
            q.l = index(n);
        }
        ++work.counts[q.kind];
        work.queries.push_back(q);
        lengths.push_back(next_length);
    }
    // Keep preparation independent of every draw used for initial/query data.
    std::mt19937_64 preparation_rng(opt.seed ^ UINT64_C(0xd1b54a32d192ed03));
    work.preparation.reserve(opt.prepare_ranges);
    for (int i = 0; i < opt.prepare_ranges; ++i) {
        Query q{};
        q.kind = (i & 1) ? REVERSE : APPLY;
        q.l = std::uniform_int_distribution<int>(0, opt.n - 1)(preparation_rng);
        q.r = std::uniform_int_distribution<int>(0, opt.n - 1)(preparation_rng);
        if (q.l > q.r) std::swap(q.l, q.r);
        ++q.r;
        if (q.kind == APPLY) {
            q.value = std::uniform_int_distribution<Value>(-100, 100)(preparation_rng);
            if (q.value == 0) q.value = 1;
        }
        work.preparation.push_back(q);
    }
    return work;
}

void mix(std::uint64_t& hash, std::uint64_t value) {
    hash ^= value + UINT64_C(0x9e3779b97f4a7c15) + (hash << 6) + (hash >> 2);
}

void observe(std::uint64_t& hash, const Query& q, int length, S result) {
    mix(hash, q.source);
    mix(hash, q.kind);
    mix(hash, length);
    mix(hash, static_cast<std::uint64_t>(result.sum));
    mix(hash, static_cast<std::uint64_t>(result.count));
}

std::int64_t peak_rss_bytes() {
#if defined(__linux__) || defined(__APPLE__)
    rusage usage{};
    if (getrusage(RUSAGE_SELF, &usage) != 0) return -1;
#if defined(__APPLE__)
    return usage.ru_maxrss;
#else
    return static_cast<std::int64_t>(usage.ru_maxrss) * 1024;
#endif
#else
    return -1;
#endif
}

std::uint64_t validation_checksum(std::vector<Tree>& versions) {
    std::uint64_t hash = 0;
    const auto sample = [&](std::size_t index) {
        auto& tree = versions[index];
        const int n = tree.len();
        const S total = tree.prod(0, n);
        mix(hash, index);
        mix(hash, n);
        mix(hash, static_cast<std::uint64_t>(total.sum));
        mix(hash, static_cast<std::uint64_t>(total.count));
        if (n) {
            const int k = (static_cast<std::uint64_t>(index) * UINT64_C(0x9e3779b97f4a7c15)) % n;
            const S point = tree.get(k);
            mix(hash, k);
            mix(hash, static_cast<std::uint64_t>(point.sum));
            mix(hash, static_cast<std::uint64_t>(point.count));
        }
    };
    const auto count = std::min<std::size_t>(64, versions.size() - 1);
    for (std::size_t i = 0; i < count; ++i)
        sample(count == 1 ? 0 : i * (versions.size() - 2) / (count - 1));
    sample(versions.size() - 1);
    return hash;
}

void verify(const Workload& work, std::vector<Tree>& versions, std::uint64_t checksum) {
    std::vector<std::vector<Value>> oracle;
    oracle.reserve(versions.size());
    oracle.emplace_back();
    for (const auto x : work.initial) oracle.back().push_back(x.sum);
    for (const auto& q : work.preparation) {
        if (q.kind == APPLY) {
            for (int j = q.l; j < q.r; ++j) oracle[0][j] += q.value;
        } else {
            std::reverse(oracle[0].begin() + q.l, oracle[0].begin() + q.r);
        }
    }
    std::uint64_t expected_checksum = 0;
    for (const auto& q : work.queries) {
        auto values = oracle[q.source];
        S result = e();
        switch (q.kind) {
        case APPLY:
            for (int j = q.l; j < q.r; ++j) values[j] += q.value;
            break;
        case PROD:
            for (int j = q.l; j < q.r; ++j) result = op(result, {values[j], 1});
            break;
        case REVERSE:
            std::reverse(values.begin() + q.l, values.begin() + q.r);
            break;
        case INSERT:
            values.insert(values.begin() + q.l, q.value);
            break;
        case POP:
            result = {values[q.l], 1};
            values.erase(values.begin() + q.l);
            break;
        case GET:
            result = {values[q.l], 1};
            break;
        case SET:
            values[q.l] = q.value;
            break;
        case SPLIT_MERGE:
            break;
        default:
            throw std::logic_error("invalid operation");
        }
        observe(expected_checksum, q, static_cast<int>(values.size()), result);
        oracle.push_back(std::move(values));
    }
    if (expected_checksum != checksum) throw std::runtime_error("oracle checksum mismatch");
    for (std::size_t i = 0; i < versions.size(); ++i) {
        versions[i].check();
        const auto actual = versions[i].tovector();
        if (actual.size() != oracle[i].size())
            throw std::runtime_error("version length mismatch: " + std::to_string(i));
        for (std::size_t j = 0; j < actual.size(); ++j) {
            if (actual[j].sum != oracle[i][j] || actual[j].count != 1)
                throw std::runtime_error("version value mismatch: " + std::to_string(i));
        }
    }
}
}  // namespace

int main(int argc, char** argv) {
    try {
        const auto opt = parse_options(argc, argv);
        const auto work = generate(opt);
        std::vector<Tree> versions;
        versions.reserve(static_cast<std::size_t>(opt.q) + 1);
        if (opt.reserve) Tree::ma.reserve(opt.reserve);

        const auto build_start = Clock::now();
        versions.emplace_back(work.initial);
        const auto build_end = Clock::now();
        const auto nodes_after_build = Tree::ma.ptr - 1;
        double prepare_seconds = 0;
        if (!work.preparation.empty()) {
            const auto prepare_start = Clock::now();
            for (const auto& q : work.preparation) {
                if (q.kind == APPLY) versions[0] = versions[0].apply(q.l, q.r, q.value);
                else versions[0] = versions[0].reverse(q.l, q.r);
            }
            prepare_seconds = std::chrono::duration<double>(Clock::now() - prepare_start).count();
        }
        const auto nodes_after_prepare = Tree::ma.ptr - 1;
        std::uint64_t checksum = 0;
        const auto query_start = Clock::now();
        for (const auto& q : work.queries) {
            auto next = versions[q.source];
            S result = e();
            switch (q.kind) {
            case APPLY:
                next = next.apply(q.l, q.r, q.value);
                break;
            case PROD:
                result = next.prod(q.l, q.r);
                break;
            case REVERSE:
                next = next.reverse(q.l, q.r);
                break;
            case INSERT:
                next = next.insert(q.l, {q.value, 1});
                break;
            case POP: {
                auto popped = next.pop(q.l);
                next = popped.first;
                result = popped.second;
                break;
            }
            case SPLIT_MERGE: {
                auto halves = next.split(q.l);
                next = halves.first.merge(halves.second);
                break;
            }
            case GET:
                result = next.get(q.l);
                break;
            case SET:
#ifdef PLWBT_BENCH_DISABLE_SET
                throw std::logic_error("set is disabled in this build");
#else
                next = next.set(q.l, {q.value, 1});
                break;
#endif
            default:
                throw std::logic_error("invalid operation");
            }
            versions.push_back(next);
            observe(checksum, q, next.len(), result);
        }
        const auto query_end = Clock::now();
        const double build_seconds = std::chrono::duration<double>(build_end - build_start).count();
        const double query_seconds = std::chrono::duration<double>(query_end - query_start).count();
        const auto node_count = Tree::ma.ptr - 1;
        const auto tree_capacity = Tree::ma.tree.capacity();
        const auto data_capacity = Tree::ma.data.capacity();
        const auto node_bytes = sizeof(typename Tree::MemoryAllocator::Node);
        const auto data_bytes = sizeof(typename Tree::MemoryAllocator::Data);
        const auto allocated_capacity_bytes = allocator_capacity_bytes(Tree::ma, 0);
        const auto rss = peak_rss_bytes();
        const auto sampled_checksum = validation_checksum(versions);
        if (opt.verify) verify(work, versions, checksum);

        std::cout << std::fixed << std::setprecision(9)
                  << "{\"variant\":\"" << PERSISTENT_LAZY_WBTREE_BENCH_LABEL
                  << "\",\"mode\":\"" << opt.mode << "\",\"workload\":\"" << opt.workload
                  << "\",\"n\":" << opt.n
                  << ",\"q\":" << opt.q << ",\"seed\":" << opt.seed
                  << ",\"reserve_nodes\":" << opt.reserve
                  << ",\"prepare_ranges\":" << opt.prepare_ranges
                  << ",\"retained_versions\":" << versions.size()
                  << ",\"build_seconds\":" << build_seconds
                  << ",\"prepare_seconds\":" << prepare_seconds
                  << ",\"query_seconds\":" << query_seconds
                  << ",\"checksum\":\"" << checksum << "\""
                  << ",\"validation_checksum\":\"" << sampled_checksum << "\""
                  << ",\"nodes_after_build\":" << nodes_after_build
                  << ",\"nodes_after_prepare\":" << nodes_after_prepare
                  << ",\"node_count\":" << node_count
                  << ",\"tree_capacity\":" << tree_capacity
                  << ",\"data_capacity\":" << data_capacity
                  << ",\"node_bytes\":" << node_bytes
                  << ",\"data_bytes\":" << data_bytes
                  << ",\"allocated_capacity_bytes\":" << allocated_capacity_bytes
                  << ",\"peak_rss_bytes\":" << rss
                  << ",\"verified\":" << (opt.verify ? "true" : "false")
                  << ",\"operations\":{";
        for (int i = 0; i < KIND_COUNT; ++i) {
            if (i) std::cout << ',';
            std::cout << '"' << kind_names[i] << "\":" << work.counts[i];
        }
        std::cout << "}}\n";
    } catch (const std::exception& err) {
        std::cerr << err.what() << '\n';
        return 1;
    }
}
