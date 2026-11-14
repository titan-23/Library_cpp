// Example: g++ -std=c++17 -O3 -DNDEBUG -march=native -I. this_file.cpp -o /tmp/dwts_bench
// /tmp/dwts_bench --n 300000 --q 300000 --sigma 300000 --distribution uniform
// Use --operations sum_range,weighted_median,mixed to isolate work. Each mutation starts from (a, w).
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
#include <utility>
#include <vector>
#include "titan_cpplib/ds/dynamic_wavelet_tree_sum.cpp"

namespace {

using Key = uint64_t;
using W = long long;
using Tree = titan23::DynamicWaveletTreeSum<Key, W>;
using Clock = std::chrono::steady_clock;

constexpr W max_weight = 1000000000;

// Operations before "insert" are read-only and share one structure.
const std::vector<std::string> operations = {
    "build", "access", "access_pair", "access_weight", "range_freq", "kth_smallest",
    "range_sum", "sum_lt", "sum_range", "sum_k_smallest", "sum_k_largest",
    "min_count_smallest_sum_ge", "max_count_smallest_sum_le", "weighted_median", "select",
    "insert", "pop", "set", "set_weight", "select_remove", "mixed"
};

struct Options {
    int n = 300000, q = 300000, log = 0;
    Key sigma = 0;
    uint64_t seed = 23;
    std::string distribution = "uniform";
    std::set<std::string> selected;

    bool wants(const std::string &name) const { return selected.empty() || selected.count(name); }
};

Options parse_options(int argc, char **argv) {
    Options o;
    bool sigma_given = false, log_given = false;
    for (int i = 1; i < argc; ++i) {
        const std::string flag = argv[i];
        if (flag == "--help") {
            std::cout << "--n N --q Q --sigma 1..2^60 | --log 1..60 (default sigma=N)\n"
                      << "--seed S --distribution uniform|skew\n"
                      << "--operations all|comma-separated names:\n";
            for (const auto &name : operations) std::cout << name << ' ';
            std::cout << "\nskew: 90% of keys in [0, min(sigma, 16)); 10% uniform.\n"
                      << "weights are uniform in [0, " << max_weight << "), drawn after the keys.\n"
                      << "sum_lt calls count_sum_lt. sum_k_* take k in [1, r-l].\n"
                      << "min_count/max_count use a target in [1, range_sum] precomputed from the initial weights.\n"
                      << "set_weight replaces one weight. pop/select_remove use min(N,Q) deletions.\n"
                      << "mixed alternates set, sum_k_smallest, range_freq, sum_range (25% each).\n";
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

struct Query {
    int l, r, k, pos, insert_pos;
    Key x, lower, upper;
    // total = sum of the initial weights in [l, r); target is in [1, total], or 0 if total == 0.
    W weight, total, target;
};

// Used only during input preparation, never in a timed loop.
struct Fenwick {
    std::vector<int> bit;
    explicit Fenwick(int n) : bit(n + 1) {
        for (int i = 1; i <= n; ++i) bit[i] = i & -i;
    }
    int prefix(int r) const {
        int result = 0;
        for (; r > 0; r -= r & -r) result += bit[r];
        return result;
    }
    void erase(int p) {
        for (++p; p < static_cast<int>(bit.size()); p += p & -p) --bit[p];
    }
};

struct Removal {
    Key x;
    int occurrence, position;
};

template<class U>
uint64_t hash(const std::vector<U> &a) {
    uint64_t result = 0;
    for (U x : a) result = result * 1000003 + uint64_t(x) + 1;
    return result;
}

uint64_t hash(const Tree &tree) { return hash(tree.tovector()) * 1000003 + hash(tree.toweights()); }

template<class A, class B>
uint64_t fold(const std::pair<A, B> &p) { return uint64_t(p.first) * 1000003 + uint64_t(p.second); }

template<class F>
double measure(F &&f) {
    const auto start = Clock::now();
    f();
    return std::chrono::duration<double>(Clock::now() - start).count();
}

void print_row(const std::string &name, double seconds, int64_t count, uint64_t checksum) {
    std::cout << std::left << std::setw(26) << name << std::right
              << " seconds=" << seconds << " ns/op=" << seconds * 1e9 / count
              << " ops=" << count << " checksum=" << checksum << '\n';
}

int run(const Options &o) {
    const Key sigma = o.sigma;
    std::mt19937_64 rng(o.seed);
    const auto random_key = [&]() -> Key {
        if (o.distribution == "skew" && rng() % 10 != 0) return rng() % std::min<Key>(sigma, 16);
        return rng() % sigma;
    };
    const auto random_weight = [&]() -> W { return W(rng() % uint64_t(max_weight)); };
    std::vector<Key> a(o.n);
    for (Key &x : a) x = random_key();
    std::vector<W> w(o.n);
    for (W &x : w) x = random_weight();
    std::vector<W> prefix(o.n + 1, 0);
    for (int i = 0; i < o.n; ++i) prefix[i + 1] = prefix[i] + w[i];
    std::vector<Query> queries(o.q);
    for (int i = 0; i < o.q; ++i) {
        Query &q = queries[i];
        q.l = rng() % o.n;
        q.r = rng() % o.n;
        if (q.l > q.r) std::swap(q.l, q.r);
        ++q.r;
        q.k = rng() % (q.r - q.l);
        q.pos = rng() % o.n;
        q.insert_pos = rng() % (o.n + i + 1);
        q.x = random_key();
        // [lower, upper) is never empty; upper may equal sigma.
        q.lower = random_key();
        q.upper = random_key();
        if (q.lower > q.upper) std::swap(q.lower, q.upper);
        ++q.upper;
        q.weight = random_weight();
        q.total = prefix[q.r] - prefix[q.l];
        const uint64_t fraction = rng();
        q.target = q.total > 0 ? W(1 + fraction % uint64_t(q.total)) : W(0);
    }

    // Sorting by (value, original position) avoids storage proportional to sigma.
    std::vector<int> occurrence(o.n), group_start(o.n), sorted_position(o.n), order(o.n);
    std::iota(order.begin(), order.end(), 0);
    std::sort(order.begin(), order.end(), [&](int l, int r) {
        return a[l] != a[r] ? a[l] < a[r] : l < r;
    });
    for (int i = 0, start = 0; i < o.n; ++i) {
        if (i == 0 || a[order[i]] != a[order[i - 1]]) start = i;
        occurrence[order[i]] = i - start;
        group_start[order[i]] = start;
        sorted_position[order[i]] = i;
    }
    std::vector<Removal> removals;
    if (o.wants("pop") || o.wants("select_remove")) {
        Fenwick by_value(o.n), by_position(o.n);
        std::shuffle(order.begin(), order.end(), rng);
        removals.reserve(std::min(o.n, o.q));
        for (int i = 0; i < std::min(o.n, o.q); ++i) {
            const int p = order[i], s = sorted_position[p];
            removals.push_back({a[p], by_value.prefix(s) - by_value.prefix(group_start[p]), by_position.prefix(p)});
            by_value.erase(s);
            by_position.erase(p);
        }
    }

    std::cout << std::fixed << std::setprecision(6)
              << "N=" << o.n << " Q=" << o.q << " sigma=" << sigma << " log=" << o.log << " seed=" << o.seed
              << " distribution=" << o.distribution << " weight=[0," << max_weight << ")\n";

    const int read_operations = std::find(operations.begin(), operations.end(), "insert") - operations.begin();
    bool reads = false;
    for (int i = 0; i < read_operations; ++i) reads |= o.wants(operations[i]);
    if (reads) {
        const auto start = Clock::now();
        Tree tree(sigma, a, w);
        const double build = std::chrono::duration<double>(Clock::now() - start).count();
        if (o.wants("build")) print_row("build", build, o.n, hash(tree));
        const auto read = [&](const std::string &name, auto &&query) {
            if (!o.wants(name)) return;
            uint64_t checksum = 0;
            const double seconds = measure([&] {
                for (const Query &q : queries) checksum = checksum * 1000003 + query(q);
            });
            print_row(name, seconds, o.q, checksum);
        };
        read("access", [&](const Query &q) { return tree.access(q.pos); });
        read("access_pair", [&](const Query &q) { return fold(tree.access_pair(q.pos)); });
        read("access_weight", [&](const Query &q) { return tree.access_weight(q.pos); });
        read("range_freq", [&](const Query &q) { return tree.range_freq(q.l, q.r, q.x); });
        read("kth_smallest", [&](const Query &q) { return tree.kth_smallest(q.l, q.r, q.k); });
        read("range_sum", [&](const Query &q) { return tree.range_sum(q.l, q.r); });
        read("sum_lt", [&](const Query &q) { return fold(tree.count_sum_lt(q.l, q.r, q.x)); });
        read("sum_range", [&](const Query &q) { return tree.sum_range(q.l, q.r, q.lower, q.upper); });
        read("sum_k_smallest", [&](const Query &q) { return tree.sum_k_smallest(q.l, q.r, q.k + 1); });
        read("sum_k_largest", [&](const Query &q) { return tree.sum_k_largest(q.l, q.r, q.k + 1); });
        read("min_count_smallest_sum_ge", [&](const Query &q) {
            return tree.min_count_smallest_sum_ge(q.l, q.r, q.target) + 1;
        });
        read("max_count_smallest_sum_le", [&](const Query &q) {
            return tree.max_count_smallest_sum_le(q.l, q.r, q.target);
        });
        // A range whose weights are all 0 has no weighted median; total is precomputed.
        read("weighted_median", [&](const Query &q) {
            return q.total > 0 ? tree.weighted_median(q.l, q.r) + 1 : Key(0);
        });
        read("select", [&](const Query &q) { return tree.select(occurrence[q.pos], a[q.pos]); });
    }

    const auto mutate = [&](const std::string &name, int64_t count, auto &&work) {
        if (!o.wants(name)) return;
        Tree tree(sigma, a, w);
        uint64_t checksum = 0;
        const double seconds = measure([&] { work(tree, checksum); });
        // Make mutations observable without adding queries to their timed loop.
        checksum ^= hash(tree);
        print_row(name, seconds, count, checksum);
    };
    mutate("insert", o.q, [&](Tree &tree, uint64_t &) {
        for (const Query &q : queries) tree.insert(q.insert_pos, q.x, q.weight);
    });
    mutate("pop", removals.size(), [&](Tree &tree, uint64_t &checksum) {
        for (const Removal &r : removals) checksum = checksum * 1000003 + fold(tree.pop(r.position));
    });
    mutate("set", o.q, [&](Tree &tree, uint64_t &) {
        for (const Query &q : queries) tree.set(q.pos, q.x, q.weight);
    });
    mutate("set_weight", o.q, [&](Tree &tree, uint64_t &) {
        for (const Query &q : queries) tree.set_weight(q.pos, q.weight);
    });
    mutate("select_remove", removals.size(), [&](Tree &tree, uint64_t &checksum) {
        for (const Removal &r : removals) {
            checksum = checksum * 1000003 + tree.select_remove(r.occurrence, r.x);
        }
    });
    mutate("mixed", o.q, [&](Tree &tree, uint64_t &checksum) {
        for (int i = 0; i < o.q; ++i) {
            const Query &q = queries[i];
            switch (i & 3) {
                case 0: tree.set(q.pos, q.x, q.weight); break;
                case 1: checksum = checksum * 1000003 + tree.sum_k_smallest(q.l, q.r, q.k + 1); break;
                case 2: checksum = checksum * 1000003 + tree.range_freq(q.l, q.r, q.x); break;
                case 3: checksum = checksum * 1000003 + tree.sum_range(q.l, q.r, q.lower, q.upper); break;
            }
        }
    });
    return 0;
}

} // namespace

int main(int argc, char **argv) {
    try {
        return run(parse_options(argc, argv));
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
