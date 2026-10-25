// Compile with -DWBT_FAMILY_KIND=1..5 and WBT_FAMILY_HEADER set to one header.
// Kinds: WBTree, WBSegTree, LazyWBTree, PersistentWBTree, PersistentSegWBTree.
// Mixed workloads intentionally exclude set() so exact old headers compile.
// All operands are generated before timing. Mutable handles are consumed;
// persistent runs retain all Q+1 roots. Validation happens after metrics capture.
#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <limits>
#include <random>
#include <stack>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>
#if defined(__linux__) || defined(__APPLE__)
#include <sys/resource.h>
#endif

#ifndef WBT_FAMILY_KIND
#define WBT_FAMILY_KIND 1
#endif
#ifndef WBT_FAMILY_HEADER
#define WBT_FAMILY_HEADER "titan_cpplib/ds/wb_tree.cpp"
#endif
#ifndef WBT_FAMILY_VARIANT
#define WBT_FAMILY_VARIANT "production"
#endif
#include WBT_FAMILY_HEADER

namespace {
using Clock = std::chrono::steady_clock;
using Value = std::int64_t;
struct S { Value sum, count; };
S op(S a, S b) { return {a.sum + b.sum, a.count + b.count}; }
S e() { return {0, 0}; }
S mapping(Value f, S value) { return {value.sum + f * value.count, value.count}; }
Value composition(Value f, Value g) { return f + g; }
Value id() { return 0; }

#if WBT_FAMILY_KIND == 1
using Tree = titan23::WBTree<S>;
constexpr const char *family = "wb_tree";
#elif WBT_FAMILY_KIND == 2
using Tree = titan23::WBSegTree<S, op, e>;
constexpr const char *family = "wb_tree_seg";
#elif WBT_FAMILY_KIND == 3
using Tree = titan23::LazyWBTree<S, Value, op, mapping, composition, e, id>;
constexpr const char *family = "lazy_wb_tree";
#elif WBT_FAMILY_KIND == 4
using Tree = titan23::PersistentWBTree<S, int>;
constexpr const char *family = "persistent_wbtree";
#elif WBT_FAMILY_KIND == 5
using Tree = titan23::PersistentSegWBTree<int, S, op, e>;
constexpr const char *family = "persistent_seg_wbtree";
#else
#error Unsupported WBT_FAMILY_KIND
#endif
constexpr bool persistent = WBT_FAMILY_KIND >= 4;
constexpr bool has_prod = WBT_FAMILY_KIND == 2 || WBT_FAMILY_KIND == 3 || WBT_FAMILY_KIND == 5;

enum Kind { INSERT, POP, GET, SPLIT_MERGE, PROD, APPLY, REVERSE, SET, KIND_COUNT };
constexpr const char *kind_names[] = {"insert", "pop", "get", "split_merge", "prod", "apply", "reverse", "set"};
struct Query { int source = 0, l = 0, r = 0; Kind kind = GET; Value value = 0; };
struct Options {
    int n = 500000, q = 500000;
    std::uint64_t seed = 23;
    std::string mode = "latest", workload = "mixed";
    bool verify = false;
};
struct Workload {
    std::vector<S> initial;
    std::vector<Query> queries;
    std::array<std::uint64_t, KIND_COUNT> counts{};
};

Options parse_options(int argc, char **argv) {
    Options opt;
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--help") {
            std::cout << "--n N --q Q --seed SEED --mode latest|history "
                         "--workload mixed|get|prod|set --verify\n"
                         "Mutable trees support latest only. set requires a build with "
                         "WBT_FAMILY_ENABLE_SET and a family that supports set().\n";
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
        else throw std::invalid_argument("unknown option: " + arg);
    }
    if (opt.n <= 0 || opt.q < 0 || static_cast<std::int64_t>(opt.n) + opt.q > std::numeric_limits<int>::max())
        throw std::invalid_argument("require N>0, Q>=0 and N+Q<=INT_MAX");
    if (opt.mode != "latest" && opt.mode != "history") throw std::invalid_argument("invalid mode");
    if (!persistent && opt.mode != "latest") throw std::invalid_argument("mutable trees require latest mode");
    if (opt.workload != "mixed" && opt.workload != "get" && opt.workload != "prod" && opt.workload != "set")
        throw std::invalid_argument("invalid workload");
    if (opt.workload == "prod" && !has_prod) throw std::invalid_argument("this family has no prod()");
#if !defined(WBT_FAMILY_ENABLE_SET) || WBT_FAMILY_KIND == 4
    if (opt.workload == "set") throw std::invalid_argument("set workload is unavailable in this build");
#endif
    const auto length = static_cast<std::uint64_t>(opt.n) + opt.q;
    const auto value = UINT64_C(1000000) + UINT64_C(100) * opt.q;
    if (length > static_cast<std::uint64_t>(std::numeric_limits<Value>::max()) / value)
        throw std::invalid_argument("aggregate can overflow");
    if (opt.verify && length * (static_cast<std::uint64_t>(opt.q) + 1) > 5000000)
        throw std::invalid_argument("--verify requires (N+Q)*(Q+1)<=5000000");
    return opt;
}

Workload generate(const Options &opt) {
    std::mt19937_64 rng(opt.seed);
    auto index = [&](int maximum) { return std::uniform_int_distribution<int>(0, maximum)(rng); };
    auto value = [&] { return std::uniform_int_distribution<Value>(-1000000, 1000000)(rng); };
    std::vector<Kind> kinds = {INSERT, POP, GET, SPLIT_MERGE};
    if (has_prod) kinds.push_back(PROD);
#if WBT_FAMILY_KIND == 3
    kinds.push_back(APPLY);
    kinds.push_back(REVERSE);
#elif WBT_FAMILY_KIND == 5
    kinds.push_back(REVERSE);
#endif
    Workload work;
    work.initial.reserve(opt.n);
    for (int i = 0; i < opt.n; ++i) work.initial.push_back({value(), 1});
    work.queries.reserve(opt.q);
    std::vector<int> lengths;
    lengths.reserve(static_cast<std::size_t>(opt.q) + 1);
    lengths.push_back(opt.n);
    for (int i = 0; i < opt.q; ++i) {
        Query query;
        query.source = opt.mode == "history" ? index(i) : i;
        if (opt.workload == "get") query.kind = GET;
        else if (opt.workload == "prod") query.kind = PROD;
        else if (opt.workload == "set") query.kind = SET;
        else query.kind = kinds[index(static_cast<int>(kinds.size()) - 1)];
        const int n = lengths[query.source];
        if (!n && query.kind != INSERT && query.kind != SPLIT_MERGE) query.kind = INSERT;
        int next_length = n;
        if (query.kind == INSERT) {
            query.l = index(n);
            query.value = value();
            ++next_length;
        } else if (query.kind == POP || query.kind == GET || query.kind == SET) {
            query.l = index(n - 1);
            if (query.kind == POP) --next_length;
            if (query.kind == SET) query.value = value();
        } else if (query.kind == SPLIT_MERGE) {
            query.l = index(n);
        } else {
            query.l = index(n - 1);
            query.r = index(n - 1);
            if (query.l > query.r) std::swap(query.l, query.r);
            ++query.r;
            if (query.kind == APPLY) query.value = std::uniform_int_distribution<Value>(-100, 100)(rng);
        }
        ++work.counts[query.kind];
        work.queries.push_back(query);
        lengths.push_back(next_length);
    }
    return work;
}

S get(Tree &tree, int k) {
#if WBT_FAMILY_KIND == 1
    return tree[k];
#else
    return tree.get(k);
#endif
}
S execute(Tree &tree, const Query &query) {
    switch (query.kind) {
    case INSERT:
#if WBT_FAMILY_KIND >= 4
        tree = tree.insert(query.l, {query.value, 1});
#else
        tree.insert(query.l, {query.value, 1});
#endif
        break;
    case POP:
#if WBT_FAMILY_KIND >= 4
        { auto result = tree.pop(query.l); tree = result.first; return result.second; }
#else
        return tree.pop(query.l);
#endif
    case GET: return get(tree, query.l);
    case SPLIT_MERGE: {
        auto halves = tree.split(query.l);
#if WBT_FAMILY_KIND >= 4
        tree = halves.first.merge(halves.second);
#else
        halves.first.merge(halves.second);
        tree = std::move(halves.first);
#endif
        break;
    }
    case PROD:
#if WBT_FAMILY_KIND == 2 || WBT_FAMILY_KIND == 3 || WBT_FAMILY_KIND == 5
        return tree.prod(query.l, query.r);
#else
        throw std::logic_error("prod unsupported");
#endif
    case APPLY:
#if WBT_FAMILY_KIND == 3
        tree.apply(query.l, query.r, query.value);
        break;
#else
        throw std::logic_error("apply unsupported");
#endif
    case REVERSE:
#if WBT_FAMILY_KIND == 3
        tree.reverse(query.l, query.r);
#elif WBT_FAMILY_KIND == 5
        tree = tree.reverse(query.l, query.r);
#else
        throw std::logic_error("reverse unsupported");
#endif
        break;
    case SET:
#if defined(WBT_FAMILY_ENABLE_SET) && WBT_FAMILY_KIND != 4
#if WBT_FAMILY_KIND >= 4
        tree = tree.set(query.l, {query.value, 1});
#else
        tree.set(query.l, {query.value, 1});
#endif
        break;
#else
        throw std::logic_error("set disabled");
#endif
    default: throw std::logic_error("invalid operation");
    }
    return e();
}

void mix(std::uint64_t &hash, std::uint64_t value) {
    hash ^= value + UINT64_C(0x9e3779b97f4a7c15) + (hash << 6) + (hash >> 2);
}
void observe(std::uint64_t &hash, const Query &q, int length, S result) {
    mix(hash, q.source); mix(hash, q.kind); mix(hash, q.l); mix(hash, q.r);
    mix(hash, static_cast<std::uint64_t>(q.value)); mix(hash, length);
    mix(hash, static_cast<std::uint64_t>(result.sum)); mix(hash, static_cast<std::uint64_t>(result.count));
}
std::uint64_t validation_checksum(std::vector<Tree> &versions) {
    std::uint64_t hash = 0;
    const auto samples = std::min<std::size_t>(65, versions.size());
    for (std::size_t sample = 0; sample < samples; ++sample) {
        const std::size_t version = samples == 1 ? 0 : sample * (versions.size() - 1) / (samples - 1);
        auto &tree = versions[version];
        const int n = tree.len();
        mix(hash, version); mix(hash, n);
#if WBT_FAMILY_KIND == 2 || WBT_FAMILY_KIND == 3 || WBT_FAMILY_KIND == 5
        const auto total = tree.prod(0, n);
        mix(hash, static_cast<std::uint64_t>(total.sum)); mix(hash, total.count);
#endif
        for (int j = 0; n && j < 4; ++j) {
            const int k = (version * UINT64_C(0x9e3779b97f4a7c15) + j * UINT64_C(0x517cc1b727220a95)) % n;
            const auto value = get(tree, k);
            mix(hash, k); mix(hash, static_cast<std::uint64_t>(value.sum)); mix(hash, value.count);
        }
    }
    const auto final = versions.back().tovector();
    mix(hash, final.size());
    for (const auto value : final) {
        mix(hash, static_cast<std::uint64_t>(value.sum)); mix(hash, value.count);
    }
    return hash;
}

S oracle_execute(std::vector<Value> &values, const Query &query) {
    S result = e();
    switch (query.kind) {
    case INSERT: values.insert(values.begin() + query.l, query.value); break;
    case POP: result = {values[query.l], 1}; values.erase(values.begin() + query.l); break;
    case GET: result = {values[query.l], 1}; break;
    case SPLIT_MERGE: break;
    case PROD: for (int i = query.l; i < query.r; ++i) result = op(result, {values[i], 1}); break;
    case APPLY: for (int i = query.l; i < query.r; ++i) values[i] += query.value; break;
    case REVERSE: std::reverse(values.begin() + query.l, values.begin() + query.r); break;
    case SET: values[query.l] = query.value; break;
    default: throw std::logic_error("invalid oracle operation");
    }
    return result;
}
void verify_values(Tree &tree, const std::vector<Value> &expected, std::size_t step) {
    const auto actual = tree.tovector();
    if (tree.len() != static_cast<int>(expected.size()) || actual.size() != expected.size())
        throw std::runtime_error("length mismatch at step/version " + std::to_string(step));
    for (std::size_t i = 0; i < actual.size(); ++i) {
        if (actual[i].sum != expected[i] || actual[i].count != 1)
            throw std::runtime_error("value mismatch at step/version " + std::to_string(step) + ", index " + std::to_string(i));
    }
}
void verify(const Workload &work, std::vector<Tree> &versions, std::uint64_t checksum) {
    std::vector<Value> initial;
    for (const auto value : work.initial) initial.push_back(value.sum);
    std::uint64_t expected_checksum = 0;
    if constexpr (persistent) {
        std::vector<std::vector<Value>> oracle = {initial};
        oracle.reserve(work.queries.size() + 1);
        for (const auto &query : work.queries) {
            auto next = oracle[query.source];
            const auto result = oracle_execute(next, query);
            observe(expected_checksum, query, next.size(), result);
            oracle.push_back(std::move(next));
        }
        for (std::size_t i = 0; i < versions.size(); ++i) verify_values(versions[i], oracle[i], i);
    } else {
        auto oracle = initial;
        Tree replay(work.initial);
        verify_values(replay, oracle, 0);
        for (std::size_t i = 0; i < work.queries.size(); ++i) {
            const auto &query = work.queries[i];
            const auto expected = oracle_execute(oracle, query);
            const auto actual = execute(replay, query);
            if (actual.sum != expected.sum || actual.count != expected.count)
                throw std::runtime_error("operation result mismatch at step " + std::to_string(i + 1));
            observe(expected_checksum, query, oracle.size(), expected);
            verify_values(replay, oracle, i + 1);
        }
        verify_values(versions[0], oracle, work.queries.size());
    }
    if (expected_checksum != checksum) throw std::runtime_error("timed operation checksum differs from vector oracle");
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
template<class U> auto allocated_nodes(int) -> decltype(U::ma.ptr, std::int64_t{}) {
    return static_cast<std::int64_t>(U::ma.ptr) - 1;
}
template<class U> std::int64_t allocated_nodes(...) { return -1; }
template<int N> struct Priority : Priority<N - 1> {};
template<> struct Priority<0> {};
template<class U> auto capacity_bytes(Priority<3>) -> decltype(U::ma.capacity_bytes(), std::int64_t{}) {
    return U::ma.capacity_bytes();
}
template<class U> auto capacity_bytes(Priority<2>) -> decltype(U::ma.d.capacity(), U::ma.keys.capacity(), std::int64_t{}) {
    return U::ma.d.capacity() * sizeof(U::ma.d[0]) + U::ma.keys.capacity() * sizeof(U::ma.keys[0]);
}
template<class U> auto capacity_bytes(Priority<1>) -> decltype(U::ma.tree.capacity(), U::ma.data.capacity(), std::int64_t{}) {
    return U::ma.tree.capacity() * sizeof(U::ma.tree[0]) + U::ma.data.capacity() * sizeof(U::ma.data[0]);
}
template<class U> std::int64_t capacity_bytes(...) { return -1; }
}  // namespace

int main(int argc, char **argv) {
    try {
        const auto options = parse_options(argc, argv);
        const auto workload = generate(options);
        std::vector<Tree> versions;
        versions.reserve(persistent ? static_cast<std::size_t>(options.q) + 1 : 1);
        const auto build_start = Clock::now();
        versions.emplace_back(workload.initial);
        const auto build_end = Clock::now();
        const auto nodes_after_build = allocated_nodes<Tree>(0);
        std::uint64_t checksum = 0;
        const auto query_start = Clock::now();
        for (const auto &query : workload.queries) {
#if WBT_FAMILY_KIND >= 4
                auto next = versions[query.source];
                const auto result = execute(next, query);
                observe(checksum, query, next.len(), result);
                versions.push_back(std::move(next));
#else
                const auto result = execute(versions[0], query);
                observe(checksum, query, versions[0].len(), result);
#endif
        }
        const auto query_end = Clock::now();
        const auto node_count = allocated_nodes<Tree>(0);
        const auto capacity = capacity_bytes<Tree>(Priority<3>{});
        const auto rss = peak_rss_bytes();
        const auto sampled_checksum = validation_checksum(versions);
        if (options.verify) verify(workload, versions, checksum);
        std::cout << std::fixed << std::setprecision(9)
                  << "{\"variant\":\"" << WBT_FAMILY_VARIANT << "\",\"family\":\"" << family
                  << "\",\"mode\":\"" << options.mode << "\",\"workload\":\"" << options.workload
                  << "\",\"n\":" << options.n << ",\"q\":" << options.q << ",\"seed\":" << options.seed
                  << ",\"retained_versions\":" << versions.size()
                  << ",\"build_seconds\":" << std::chrono::duration<double>(build_end - build_start).count()
                  << ",\"query_seconds\":" << std::chrono::duration<double>(query_end - query_start).count()
                  << ",\"checksum\":\"" << checksum << "\",\"validation_checksum\":\"" << sampled_checksum
                  << "\",\"nodes_after_build\":" << nodes_after_build << ",\"node_count\":" << node_count
                  << ",\"allocated_capacity_bytes\":" << capacity << ",\"peak_rss_bytes\":" << rss
                  << ",\"verified\":" << (options.verify ? "true" : "false") << ",\"operations\":{";
        for (int i = 0; i < KIND_COUNT; ++i) {
            if (i) std::cout << ',';
            std::cout << '"' << kind_names[i] << "\":" << workload.counts[i];
        }
        std::cout << "}}\n";
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
