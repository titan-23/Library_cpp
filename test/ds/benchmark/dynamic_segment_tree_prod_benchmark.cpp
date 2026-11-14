// 動的セグメント木の prod などを操作別に測る。g++ -std=c++17 -O2 -DNDEBUG -I. this_file.cpp -o /tmp/dst_prod_bench
// /tmp/dst_prod_bench --kind plain|init|lazy --mode prod|prod_short|update|max_right|min_left
// N=1e9 に K 個の点更新 (lazy は K 個の区間加算) をしてから、Q 回の操作を測る。準備は計測外。
// prod は両端が一様乱数の区間、prod_short は長さ 1024 以下の区間。update は既存の位置への点更新で、lazy にはない。
// max_right(l) と min_left(r) は、両端が一様乱数の区間 [l,r) の総和以下という判定で探す。答えはおよそ r と l になる。
#include <algorithm>
#include <chrono>
#include <cstdint>
#include <iostream>
#include <random>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include "titan_cpplib/ds/dynamic_lazy_segment_tree.cpp"
#include "titan_cpplib/ds/dynamic_segment_tree.cpp"
#include "titan_cpplib/ds/dynamic_segment_tree_init.cpp"

namespace {

using Index = long long;
using Clock = std::chrono::steady_clock;

long long op(long long a, long long b) { return a + b; }
long long e() { return 0; }
long long repeat(long long value, Index length) { return value * length; }
using Plain = titan23::DynamicSegmentTree<Index, long long, op, e>;
using Initial = titan23::DynamicSegmentTreeInit<Index, long long, op, e, repeat>;

struct S { long long sum; Index len; };
S lazy_op(S a, S b) { return {a.sum + b.sum, a.len + b.len}; }
S lazy_e() { return {0, 0}; }
S mapping(long long f, S s) { return {s.sum + f * s.len, s.len}; }
long long composition(long long f, long long g) { return f + g; }
long long id() { return 0; }
S lazy_repeat(S s, Index n) { return {s.sum * n, s.len * n}; }
using Lazy = titan23::DynamicLazySegmentTree<Index, S, lazy_op, lazy_e, long long, mapping, composition, id, lazy_repeat>;

struct Options {
    std::string kind = "plain", mode = "prod";
    int k = 250000, q = 1000000;
    uint64_t seed = 23;
};

Options parse_options(int argc, char **argv) {
    Options o;
    for (int i = 1; i + 1 < argc; i += 2) {
        const std::string flag = argv[i], value = argv[i + 1];
        if (flag == "--kind") o.kind = value;
        else if (flag == "--mode") o.mode = value;
        else if (flag == "--k") o.k = std::stoi(value);
        else if (flag == "--q") o.q = std::stoi(value);
        else if (flag == "--seed") o.seed = std::stoull(value);
        else throw std::invalid_argument("unknown option: " + flag);
    }
    if (o.kind == "lazy" && o.mode == "update") throw std::invalid_argument("lazy has no update mode");
    return o;
}

void checksum_add(uint64_t &checksum, const long long value) {
    checksum ^= uint64_t(value) + 0x9e3779b97f4a7c15ULL + (checksum << 6) + (checksum >> 2);
}

template<class Tree, class Prepare, class Value>
void run(const Options &o, Tree &tree, Prepare prepare, Value value) {
    constexpr Index n = 1000000000LL;
    std::mt19937_64 rng(o.seed);
    std::vector<Index> points(o.k);
    for (Index &p : points) p = rng() % n;
    prepare(rng, points);
    std::vector<std::pair<Index, Index>> queries(o.q);
    for (auto &[l, r] : queries) {
        l = rng() % n;
        if (o.mode == "prod_short") r = std::min<Index>(n, l + 1 + Index(rng() % 1024));
        else if (o.mode == "update") r = points[rng() % o.k];
        else {
            r = rng() % n;
            if (l > r) std::swap(l, r);
            ++r;
        }
    }
    const bool search = o.mode == "max_right" || o.mode == "min_left";
    std::vector<long long> limits;
    if (search) {
        for (const auto &[l, r] : queries) limits.push_back(value(tree.prod(l, r)));
    }
    uint64_t checksum = 0;
    const auto start = Clock::now();
    if (search) {
        for (int i = 0; i < o.q; ++i) {
            const auto pred = [limit = limits[i], &value](const auto &x) { return value(x) <= limit; };
            const Index answer = o.mode == "max_right" ? tree.max_right(queries[i].first, pred) : tree.min_left(queries[i].second, pred);
            checksum_add(checksum, answer);
        }
    } else if constexpr (!std::is_same_v<Tree, Lazy>) {
        if (o.mode == "update") {
            for (const auto &[l, r] : queries) tree.set(r, l % 9 + 1);
        } else {
            for (const auto &[l, r] : queries) checksum_add(checksum, value(tree.prod(l, r)));
        }
    } else {
        for (const auto &[l, r] : queries) checksum_add(checksum, value(tree.prod(l, r)));
    }
    const double ns = std::chrono::duration<double, std::nano>(Clock::now() - start).count() / o.q;
    if (o.mode == "update") checksum_add(checksum, value(tree.all_prod()));
    std::cout << "kind=" << o.kind << " mode=" << o.mode << " k=" << o.k << " q=" << o.q
              << " ns/op=" << ns << " nodes=" << tree.node_count() << " checksum=" << checksum << '\n';
}

}  // namespace

int main(int argc, char **argv) {
    const Options o = parse_options(argc, argv);
    constexpr Index n = 1000000000LL;
    auto set_points = [](auto &tree) {
        return [&tree](std::mt19937_64 &rng, const std::vector<Index> &points) {
            for (const Index p : points) tree.set(p, (long long)(rng() % 9 + 1));
        };
    };
    if (o.kind == "plain") {
        Plain tree(n);
        run(o, tree, set_points(tree), [](long long x) { return x; });
    } else if (o.kind == "init") {
        Initial tree(n, 3);
        run(o, tree, set_points(tree), [](long long x) { return x; });
    } else if (o.kind == "lazy") {
        Lazy tree(n, S{0, 1});
        run(o, tree, [&](std::mt19937_64 &rng, const std::vector<Index> &points) {
            for (const Index p : points) {
                Index q = rng() % n, l = std::min(p, q), r = std::max(p, q) + 1;
                tree.apply(l, r, (long long)(rng() % 7 + 1));
            }
        }, [](const S &x) { return x.sum; });
    } else {
        throw std::invalid_argument("unknown kind: " + o.kind);
    }
    return 0;
}
