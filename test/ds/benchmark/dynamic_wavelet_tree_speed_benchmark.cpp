// Example: g++ -std=c++17 -O3 -DNDEBUG -march=native -I. this_file.cpp -o /tmp/dwt_bench
// /tmp/dwt_bench --n 300000 --q 300000 --sigma 300000 --distribution uniform
// Use --operations rank,select,mixed to isolate work. Each mutation starts from a.
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
#include <vector>
#include "titan_cpplib/ds/dynamic_wavelet_tree.cpp"

namespace {

using Key = uint64_t;
using Tree = titan23::DynamicWaveletTree<Key>;
using Clock = std::chrono::steady_clock;

const std::vector<std::string> operations = {
    "build", "access", "rank", "range_count", "range_freq", "kth_smallest",
    "prev_value", "next_value", "select", "insert", "pop", "insert_pop",
    "set", "select_remove", "mixed"
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
            std::cout << "\nskew: 90% of values in [0, min(sigma, 16)); 10% uniform.\n"
                      << "pop/select_remove use min(N,Q) deletions; insert_pop counts both calls.\n"
                      << "mixed alternates set, kth_smallest, range_freq, access (25% each).\n";
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
    Key x;
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

uint64_t hash(const std::vector<Key> &a) {
    uint64_t result = 0;
    for (Key x : a) result = result * 1000003 + x + 1;
    return result;
}

template<class F>
double measure(F &&f) {
    const auto start = Clock::now();
    f();
    return std::chrono::duration<double>(Clock::now() - start).count();
}

void print_row(const std::string &name, double seconds, int64_t count, uint64_t checksum) {
    std::cout << std::left << std::setw(16) << name << std::right
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
    std::vector<Key> a(o.n);
    for (Key &x : a) x = random_key();
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
              << " distribution=" << o.distribution << '\n';

    bool reads = false;
    for (int i = 0; i < 9; ++i) reads |= o.wants(operations[i]);
    if (reads) {
        const auto start = Clock::now();
        Tree tree(sigma, a);
        const double build = std::chrono::duration<double>(Clock::now() - start).count();
        if (o.wants("build")) print_row("build", build, o.n, hash(tree.tovector()));
        const auto read = [&](const std::string &name, auto &&query) {
            if (!o.wants(name)) return;
            uint64_t checksum = 0;
            const double seconds = measure([&] {
                for (const Query &q : queries) checksum = checksum * 1000003 + query(q);
            });
            print_row(name, seconds, o.q, checksum);
        };
        read("access", [&](const Query &q) { return tree.access(q.pos); });
        read("rank", [&](const Query &q) { return tree.rank(q.r, q.x); });
        read("range_count", [&](const Query &q) { return tree.range_count(q.l, q.r, q.x); });
        read("range_freq", [&](const Query &q) { return tree.range_freq(q.l, q.r, q.x); });
        read("kth_smallest", [&](const Query &q) { return tree.kth_smallest(q.l, q.r, q.k); });
        read("prev_value", [&](const Query &q) { return tree.prev_value(q.l, q.r, q.x) + 1; });
        read("next_value", [&](const Query &q) { return tree.next_value(q.l, q.r, q.x) + 1; });
        read("select", [&](const Query &q) { return tree.select(occurrence[q.pos], a[q.pos]); });
    }

    const auto mutate = [&](const std::string &name, int64_t count, auto &&work) {
        if (!o.wants(name)) return;
        Tree tree(sigma, a);
        uint64_t checksum = 0;
        const double seconds = measure([&] { work(tree, checksum); });
        // Make mutations observable without adding queries to their timed loop.
        checksum ^= hash(tree.tovector());
        print_row(name, seconds, count, checksum);
    };
    mutate("insert", o.q, [&](Tree &tree, uint64_t &) {
        for (const Query &q : queries) tree.insert(q.insert_pos, q.x);
    });
    mutate("pop", removals.size(), [&](Tree &tree, uint64_t &checksum) {
        for (const Removal &r : removals) checksum = checksum * 1000003 + tree.pop(r.position);
    });
    mutate("insert_pop", int64_t(o.q) * 2, [&](Tree &tree, uint64_t &checksum) {
        for (const Query &q : queries) {
            tree.insert(q.pos, q.x);
            checksum = checksum * 1000003 + tree.pop(q.pos);
        }
    });
    mutate("set", o.q, [&](Tree &tree, uint64_t &) {
        for (const Query &q : queries) tree.set(q.pos, q.x);
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
                case 0: tree.set(q.pos, q.x); break;
                case 1: checksum = checksum * 1000003 + tree.kth_smallest(q.l, q.r, q.k); break;
                case 2: checksum = checksum * 1000003 + tree.range_freq(q.l, q.r, q.x); break;
                case 3: checksum = checksum * 1000003 + tree.access(q.pos); break;
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
