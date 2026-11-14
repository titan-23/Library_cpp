// Shared option parsing, input generation and reporting for the static wavelet matrix speed benchmarks:
//   wavelet_matrix_speed_benchmark.cpp, wavelet_matrix_bit_speed_benchmark.cpp,
//   wavelet_matrix_sum_speed_benchmark.cpp, wavelet_matrix_fenwick_speed_benchmark.cpp,
//   wavelet_matrix_2d_speed_benchmark.cpp
// The 1D benchmarks draw keys, weights and queries in the same order, so one --seed gives every
// 1D benchmark the same input (except that the bit benchmark caps `upper`, see make_input).
// Nothing here runs inside a timed loop except read/once, which only wrap the measured calls.
#pragma once
#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <limits>
#include <numeric>
#include <random>
#include <set>
#include <stdexcept>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

namespace wm_bench {

using Key = uint64_t;
using W = long long;
using Clock = std::chrono::steady_clock;

constexpr W max_weight = 1000000000;
constexpr long long quantile_denominator = 1000;
constexpr int topk_k = 3;

struct Options {
    int n = 300000, q = 300000, log = 0, topk_q = 1000;
    Key sigma = 0;
    uint64_t seed = 23;
    std::string distribution = "uniform";
    std::set<std::string> selected;

    bool wants(const std::string &name) const { return selected.empty() || selected.count(name); }
    bool wants_any(const std::vector<std::string> &names) const {
        for (const auto &name : names) if (wants(name)) return true;
        return false;
    }
};

// `notes` is appended to --help. --topk-q is accepted only when `has_topk` is true.
inline Options parse_options(int argc, char **argv, const std::vector<std::string> &operations,
                             const std::string &notes, bool has_topk) {
    Options o;
    bool sigma_given = false, log_given = false;
    for (int i = 1; i < argc; ++i) {
        const std::string flag = argv[i];
        if (flag == "--help") {
            std::cout << "--n N --q Q --sigma 1..2^60 | --log 1..60 (default sigma=N)\n"
                      << "--seed S --distribution uniform|skew\n";
            if (has_topk) std::cout << "--topk-q T (default 1000): topk runs on the first min(Q, T) queries\n";
            std::cout << "--operations all|comma-separated names:\n";
            for (const auto &name : operations) std::cout << name << ' ';
            std::cout << "\nskew: 90% of values in [0, min(sigma, 16)); 10% uniform.\n" << notes;
            std::exit(0);
        }
        if (++i == argc) throw std::invalid_argument("missing value for " + flag);
        const std::string value = argv[i];
        if (flag == "--n") o.n = std::stoi(value);
        else if (flag == "--q") o.q = std::stoi(value);
        else if (flag == "--log") {
            o.log = std::stoi(value);
            log_given = true;
        } else if (flag == "--sigma") {
            o.sigma = std::stoull(value);
            sigma_given = true;
        }
        else if (flag == "--seed") o.seed = std::stoull(value);
        else if (flag == "--distribution") o.distribution = value;
        else if (has_topk && flag == "--topk-q") o.topk_q = std::stoi(value);
        else if (flag == "--operations") {
            o.selected.clear();
            if (value != "all") {
                size_t start = 0;
                do {
                    const size_t end = value.find(',', start);
                    const std::string name = value.substr(start, end - start);
                    if (std::find(operations.begin(), operations.end(), name) == operations.end()) {
                        throw std::invalid_argument("unknown operation: " + name);
                    }
                    o.selected.insert(name);
                    if (end == std::string::npos) break;
                    start = end + 1;
                } while (true);
            }
        } else throw std::invalid_argument("unknown option: " + flag);
    }
    if (o.n <= 0 || o.q <= 0 || int64_t(o.n) + o.q >= std::numeric_limits<int>::max()) {
        throw std::invalid_argument("N and Q must be positive, with N+Q < INT_MAX");
    }
    if (o.topk_q <= 0) throw std::invalid_argument("topk-q must be positive");
    if (sigma_given && log_given) throw std::invalid_argument("--sigma and --log are mutually exclusive");
    if (log_given) {
        if (o.log < 1 || o.log > 60) throw std::invalid_argument("log must be in [1,60]");
        o.sigma = Key(1) << o.log;
    } else if (!sigma_given) {
        o.sigma = o.n;
    }
    if (o.sigma == 0 || o.sigma > (Key(1) << 60)) throw std::invalid_argument("sigma must be in [1,2^60]");
    o.log = 0;
    for (Key top = o.sigma - 1; top != 0; top >>= 1) ++o.log;
    if (o.distribution != "uniform" && o.distribution != "skew") {
        throw std::invalid_argument("distribution must be uniform or skew");
    }
    return o;
}

inline Key random_key(std::mt19937_64 &rng, const Options &o) {
    if (o.distribution == "skew" && rng() % 10 != 0) return rng() % std::min<Key>(o.sigma, 16);
    return rng() % o.sigma;
}

inline W random_weight(std::mt19937_64 &rng) { return W(rng() % uint64_t(max_weight)); }

inline void print_header(const Options &o, const std::string &extra) {
    std::cout << std::fixed << std::setprecision(6)
              << "N=" << o.n << " Q=" << o.q << " sigma=" << o.sigma << " log=" << o.log << " seed=" << o.seed
              << " distribution=" << o.distribution << extra << '\n';
}

// Checksums. Every value is folded as uint64_t, so -1 results wrap deterministically.
// fold(vector) equals the reference benchmarks' hash(vector).
template<class U>
uint64_t fold(const U &x) { return uint64_t(x); }

template<class A, class B>
uint64_t fold(const std::pair<A, B> &p) { return fold(p.first) * 1000003 + fold(p.second); }

template<class A, class B, class C>
uint64_t fold(const std::tuple<A, B, C> &t) {
    return (fold(std::get<0>(t)) * 1000003 + fold(std::get<1>(t))) * 1000003 + fold(std::get<2>(t));
}

template<class U>
uint64_t fold(const std::vector<U> &a) {
    uint64_t result = 0;
    for (const U &x : a) result = result * 1000003 + fold(x) + 1;
    return result;
}

template<class F>
double measure(F &&f) {
    const auto start = Clock::now();
    f();
    return std::chrono::duration<double>(Clock::now() - start).count();
}

inline void print_row(const std::string &name, double seconds, int64_t count, uint64_t checksum) {
    std::cout << std::left << std::setw(26) << name << std::right
              << " seconds=" << seconds << " ns/op=" << seconds * 1e9 / count
              << " ops=" << count << " checksum=" << checksum << '\n';
}

struct Query {
    int l, r, k, pos;
    Key x, lower, upper;
    // total = sum of the initial weights in [l, r); target is in [1, total], or 0 if total == 0.
    W weight, total, target;
    long long numerator;  // weighted_quantile ratio numerator / quantile_denominator, in [1, 1000]
    uint64_t value_fraction;
    // Filled from the initial keys after all random draws.
    int value_count;       // #{i in [l, r) : lower <= a[i] < upper}
    int value_k;           // in [0, value_count), or -1 if value_count == 0
    int range_select_k;    // a[l + k] is the range_select_k-th occurrence of its key in [l, r)
    Key range_select_x;    // a[l + k]
};

struct Input {
    std::vector<Key> a;
    std::vector<W> w;
    std::vector<Query> queries;
    std::vector<int> occurrence;  // occurrence[i] = #{j < i : a[j] == a[i]}
};

// Used only during input preparation, never in a timed loop.
struct CountFenwick {
    std::vector<int> bit;
    explicit CountFenwick(int n) : bit(n + 1, 0) {}
    void add(int p) {
        for (++p; p < static_cast<int>(bit.size()); p += p & -p) ++bit[p];
    }
    int prefix(int r) const {
        int result = 0;
        for (; r > 0; r -= r & -r) result += bit[r];
        return result;
    }
};

// Offline sweep over positions: value_count for every query in O((N+Q)log N).
inline void fill_value_counts(const std::vector<Key> &a, std::vector<Query> &queries) {
    std::vector<Key> values(a);
    std::sort(values.begin(), values.end());
    values.erase(std::unique(values.begin(), values.end()), values.end());
    const auto index = [&](Key x) { return int(std::lower_bound(values.begin(), values.end(), x) - values.begin()); };
    std::vector<std::pair<int, int>> events;  // (position, query * 2 + is_right)
    events.reserve(queries.size() * 2);
    for (int i = 0; i < static_cast<int>(queries.size()); ++i) {
        queries[i].value_count = 0;
        events.emplace_back(queries[i].l, 2 * i);
        events.emplace_back(queries[i].r, 2 * i + 1);
    }
    std::sort(events.begin(), events.end());
    CountFenwick counter(values.size());
    int inserted = 0;
    for (const auto &[position, id] : events) {
        while (inserted < position) counter.add(index(a[inserted++]));
        Query &q = queries[id / 2];
        const int c = counter.prefix(index(q.upper)) - counter.prefix(index(q.lower));
        q.value_count += (id & 1) ? c : -c;
    }
}

// Every query satisfies 0 <= l < r <= N, k in [0, r-l), pos in [0, N), x < sigma,
// lower <= upper <= upper_limit. upper_limit must be at least sigma-1; WaveletMatrix<T, LOG>
// from wavelet_matrix_bit.cpp cannot take upper = 2^LOG, so its benchmark passes 2^LOG - 1.
inline Input make_input(const Options &o, Key upper_limit) {
    std::mt19937_64 rng(o.seed);
    Input in;
    in.a.resize(o.n);
    for (Key &x : in.a) x = random_key(rng, o);
    in.w.resize(o.n);
    for (W &x : in.w) x = random_weight(rng);
    std::vector<W> prefix(o.n + 1, 0);
    for (int i = 0; i < o.n; ++i) prefix[i + 1] = prefix[i] + in.w[i];
    in.queries.resize(o.q);
    for (Query &q : in.queries) {
        q.l = rng() % o.n;
        q.r = rng() % o.n;
        if (q.l > q.r) std::swap(q.l, q.r);
        ++q.r;
        q.k = rng() % (q.r - q.l);
        q.pos = rng() % o.n;
        q.x = random_key(rng, o);
        q.lower = random_key(rng, o);
        q.upper = random_key(rng, o);
        if (q.lower > q.upper) std::swap(q.lower, q.upper);
        ++q.upper;
        q.upper = std::min(q.upper, upper_limit);
        q.weight = random_weight(rng);
        q.total = prefix[q.r] - prefix[q.l];
        const uint64_t fraction = rng();
        q.target = q.total > 0 ? W(1 + fraction % uint64_t(q.total)) : W(0);
        q.numerator = 1 + rng() % quantile_denominator;
        q.value_fraction = rng();
    }

    // Sorting by (value, original position) avoids storage proportional to sigma.
    std::vector<int> group_start(o.n), sorted_position(o.n), order(o.n);
    in.occurrence.resize(o.n);
    std::iota(order.begin(), order.end(), 0);
    std::sort(order.begin(), order.end(), [&](int l, int r) {
        return in.a[l] != in.a[r] ? in.a[l] < in.a[r] : l < r;
    });
    for (int i = 0, start = 0; i < o.n; ++i) {
        if (i == 0 || in.a[order[i]] != in.a[order[i - 1]]) start = i;
        in.occurrence[order[i]] = i - start;
        group_start[order[i]] = start;
        sorted_position[order[i]] = i;
    }
    for (Query &q : in.queries) {
        const int p = q.l + q.k;
        const auto first = order.begin() + group_start[p];
        const int before_l = std::lower_bound(first, order.begin() + sorted_position[p], q.l) - first;
        q.range_select_k = in.occurrence[p] - before_l;
        q.range_select_x = in.a[p];
    }
    fill_value_counts(in.a, in.queries);
    for (Query &q : in.queries) {
        q.value_k = q.value_count > 0 ? int(q.value_fraction % uint64_t(q.value_count)) : -1;
    }
    return in;
}

// Times `query` over the first `count` queries and prints one row.
template<class Q, class F>
void read(const Options &o, const std::string &name, const std::vector<Q> &queries, int count, F &&query) {
    if (!o.wants(name)) return;
    uint64_t checksum = 0;
    const double seconds = measure([&] {
        for (int i = 0; i < count; ++i) checksum = checksum * 1000003 + fold(query(queries[i]));
    });
    print_row(name, seconds, count, checksum);
}

// Times a single call (tovector and friends); the result is hashed after timing.
template<class F>
void once(const Options &o, const std::string &name, int64_t count, F &&f) {
    if (!o.wants(name)) return;
    decltype(f()) result;
    const double seconds = measure([&] { result = f(); });
    print_row(name, seconds, count, fold(result));
}

// Queries shared by every 1D static wavelet matrix.
const std::vector<std::string> key_operations = {
    "access", "rank", "range_count", "range_freq", "range_freq2", "kth_smallest", "kth_largest",
    "prev_value", "next_value", "select", "range_select", "has_majority", "topk",
    "kth_index_in_value_range", "next_index_in_value_range", "prev_index_in_value_range"
};

const std::string key_notes =
    "range_freq counts keys < x; range_freq2 counts keys in [lower, upper).\n"
    "select uses an existing occurrence of a[pos]; range_select asks for a[l+k] inside [l, r).\n"
    "topk asks for k=3 on the first min(Q, topk-q) queries.\n"
    "kth_index_in_value_range uses k in [0, count) precomputed offline; empty value ranges add 0.\n"
    "prev_value/next_value/*_index_in_value_range add 1 so that -1 folds to 0.\n";

template<class Tree>
void bench_key_queries(const Options &o, const Input &in, const Tree &tree) {
    const std::vector<Query> &queries = in.queries;
    const std::vector<Key> &a = in.a;
    const std::vector<int> &occurrence = in.occurrence;
    read(o, "access", queries, o.q, [&](const Query &q) { return tree.access(q.pos); });
    read(o, "rank", queries, o.q, [&](const Query &q) { return tree.rank(q.r, q.x); });
    read(o, "range_count", queries, o.q, [&](const Query &q) { return tree.range_count(q.l, q.r, q.x); });
    read(o, "range_freq", queries, o.q, [&](const Query &q) { return tree.range_freq(q.l, q.r, q.x); });
    read(o, "range_freq2", queries, o.q, [&](const Query &q) { return tree.range_freq(q.l, q.r, q.lower, q.upper); });
    read(o, "kth_smallest", queries, o.q, [&](const Query &q) { return tree.kth_smallest(q.l, q.r, q.k); });
    read(o, "kth_largest", queries, o.q, [&](const Query &q) { return tree.kth_largest(q.l, q.r, q.k); });
    read(o, "prev_value", queries, o.q, [&](const Query &q) { return tree.prev_value(q.l, q.r, q.x) + 1; });
    read(o, "next_value", queries, o.q, [&](const Query &q) { return tree.next_value(q.l, q.r, q.x) + 1; });
    read(o, "select", queries, o.q, [&](const Query &q) { return tree.select(occurrence[q.pos], a[q.pos]); });
    read(o, "range_select", queries, o.q, [&](const Query &q) {
        return tree.range_select(q.l, q.r, q.range_select_k, q.range_select_x);
    });
    // has_majority requires l < r, which every query satisfies.
    read(o, "has_majority", queries, o.q, [&](const Query &q) { return tree.has_majority(q.l, q.r); });
    read(o, "topk", queries, std::min(o.q, o.topk_q), [&](const Query &q) { return tree.topk(q.l, q.r, topk_k); });
    read(o, "kth_index_in_value_range", queries, o.q, [&](const Query &q) {
        return q.value_count > 0 ? tree.kth_index_in_value_range(q.l, q.r, q.lower, q.upper, q.value_k) + 1 : 0;
    });
    read(o, "next_index_in_value_range", queries, o.q, [&](const Query &q) {
        return tree.next_index_in_value_range(q.l, q.r, q.lower, q.upper) + 1;
    });
    read(o, "prev_index_in_value_range", queries, o.q, [&](const Query &q) {
        return tree.prev_index_in_value_range(q.l, q.r, q.lower, q.upper) + 1;
    });
}

// Queries shared by WaveletMatrixSum and WaveletMatrixFenwick.
const std::vector<std::string> weight_operations = {
    "access_weight", "access_pair", "range_sum", "count_sum_lt", "sum_lt", "sum_range",
    "sum_k_smallest", "sum_k_largest", "max_right_smallest", "max_right_largest",
    "max_count_smallest_sum_le", "max_count_largest_sum_le", "weighted_quantile",
    "weighted_quantile_ratio", "weighted_median", "min_count_smallest_sum_ge", "min_count_largest_sum_ge"
};

const std::string weight_notes =
    "weights are uniform in [0, 1000000000), drawn after the keys.\n"
    "count_sum_lt/sum_lt use x; sum_range uses [lower, upper). sum_k_* take k in [1, r-l].\n"
    "max_right_*/max_count_*/min_count_* use a target in [1, range_sum] (0 if range_sum is 0)\n"
    "precomputed from the initial weights; max_right_* use pred(sum) = sum <= target.\n"
    "weighted_quantile uses that target; weighted_quantile_ratio uses numerator/1000 with\n"
    "numerator in [1, 1000]. weighted_* add 0 without calling when range_sum is 0.\n";

template<class Tree>
void bench_weight_queries(const Options &o, const Input &in, const Tree &tree) {
    const std::vector<Query> &queries = in.queries;
    read(o, "access_weight", queries, o.q, [&](const Query &q) { return tree.access_weight(q.pos); });
    read(o, "access_pair", queries, o.q, [&](const Query &q) { return tree.access_pair(q.pos); });
    read(o, "range_sum", queries, o.q, [&](const Query &q) { return tree.range_sum(q.l, q.r); });
    read(o, "count_sum_lt", queries, o.q, [&](const Query &q) { return tree.count_sum_lt(q.l, q.r, q.x); });
    read(o, "sum_lt", queries, o.q, [&](const Query &q) { return tree.sum_lt(q.l, q.r, q.x); });
    read(o, "sum_range", queries, o.q, [&](const Query &q) { return tree.sum_range(q.l, q.r, q.lower, q.upper); });
    read(o, "sum_k_smallest", queries, o.q, [&](const Query &q) { return tree.sum_k_smallest(q.l, q.r, q.k + 1); });
    read(o, "sum_k_largest", queries, o.q, [&](const Query &q) { return tree.sum_k_largest(q.l, q.r, q.k + 1); });
    read(o, "max_right_smallest", queries, o.q, [&](const Query &q) {
        const W budget = q.target;
        return tree.max_right_smallest(q.l, q.r, [budget](W sum) { return sum <= budget; });
    });
    read(o, "max_right_largest", queries, o.q, [&](const Query &q) {
        const W budget = q.target;
        return tree.max_right_largest(q.l, q.r, [budget](W sum) { return sum <= budget; });
    });
    read(o, "max_count_smallest_sum_le", queries, o.q, [&](const Query &q) {
        return tree.max_count_smallest_sum_le(q.l, q.r, q.target);
    });
    read(o, "max_count_largest_sum_le", queries, o.q, [&](const Query &q) {
        return tree.max_count_largest_sum_le(q.l, q.r, q.target);
    });
    // A range whose weights are all 0 has no quantile; total is precomputed.
    read(o, "weighted_quantile", queries, o.q, [&](const Query &q) {
        return q.total > 0 ? tree.weighted_quantile(q.l, q.r, q.target) + 1 : Key(0);
    });
    read(o, "weighted_quantile_ratio", queries, o.q, [&](const Query &q) {
        return q.total > 0 ? tree.weighted_quantile(q.l, q.r, q.numerator, quantile_denominator) + 1 : Key(0);
    });
    read(o, "weighted_median", queries, o.q, [&](const Query &q) {
        return q.total > 0 ? tree.weighted_median(q.l, q.r) + 1 : Key(0);
    });
    read(o, "min_count_smallest_sum_ge", queries, o.q, [&](const Query &q) {
        return tree.min_count_smallest_sum_ge(q.l, q.r, q.target) + 1;
    });
    read(o, "min_count_largest_sum_ge", queries, o.q, [&](const Query &q) {
        return tree.min_count_largest_sum_ge(q.l, q.r, q.target) + 1;
    });
}

} // namespace wm_bench
