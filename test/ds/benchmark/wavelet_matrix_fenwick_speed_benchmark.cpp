// Example: g++ -std=c++17 -O3 -DNDEBUG -march=native -I. this_file.cpp -o /tmp/wmf_bench
// /tmp/wmf_bench --n 300000 --q 300000 --sigma 300000 --distribution uniform
// Use --operations sum_range,set_weight,mixed to isolate work. Each mutation starts from (a, w).
#include "titan_cpplib/ds/wavelet_matrix_fenwick.cpp"
#include "test/ds/benchmark/wavelet_matrix_speed_common.hpp"

namespace {

// Using-declarations, not a using-directive: the library's global `using namespace std`
// would otherwise make names such as fold/read ambiguous.
using wm_bench::Clock;
using wm_bench::Input;
using wm_bench::Key;
using wm_bench::Options;
using wm_bench::Query;
using wm_bench::W;
using wm_bench::bench_key_queries;
using wm_bench::bench_weight_queries;
using wm_bench::fold;
using wm_bench::key_notes;
using wm_bench::key_operations;
using wm_bench::make_input;
using wm_bench::max_weight;
using wm_bench::measure;
using wm_bench::once;
using wm_bench::parse_options;
using wm_bench::print_header;
using wm_bench::print_row;
using wm_bench::weight_notes;
using wm_bench::weight_operations;
using Tree = titan23::WaveletMatrixFenwick<Key, W>;

const std::vector<std::string> mutation_operations = {"set_weight", "add_weight", "mixed"};

// Operations before "set_weight" are read-only and share one structure.
std::vector<std::string> make_operations() {
    std::vector<std::string> operations = {"build"};
    operations.insert(operations.end(), key_operations.begin(), key_operations.end());
    operations.insert(operations.end(), weight_operations.begin(), weight_operations.end());
    operations.insert(operations.end(), {"tovector", "toweights", "toitems"});
    operations.insert(operations.end(), mutation_operations.begin(), mutation_operations.end());
    return operations;
}

const std::vector<std::string> operations = make_operations();

uint64_t fold_tree(const Tree &tree) { return fold(tree.tovector()) * 1000003 + fold(tree.toweights()); }

int run(const Options &o) {
    const Input in = make_input(o, o.sigma);
    print_header(o, " weight=[0," + std::to_string(max_weight) + ") topk_q=" + std::to_string(std::min(o.q, o.topk_q)));

    const int read_operations = std::find(operations.begin(), operations.end(), "set_weight") - operations.begin();
    bool reads = false;
    for (int i = 0; i < read_operations; ++i) reads |= o.wants(operations[i]);
    if (reads) {
        // Uses the (sigma, keys, weights) constructor; the weight = key constructor is not benchmarked.
        const auto start = Clock::now();
        const Tree tree(o.sigma, in.a, in.w);
        const double build = std::chrono::duration<double>(Clock::now() - start).count();
        if (o.wants("build")) print_row("build", build, o.n, fold_tree(tree));
        bench_key_queries(o, in, tree);
        bench_weight_queries(o, in, tree);
        once(o, "tovector", o.n, [&] { return tree.tovector(); });
        once(o, "toweights", o.n, [&] { return tree.toweights(); });
        once(o, "toitems", o.n, [&] { return tree.toitems(); });
    }

    const auto mutate = [&](const std::string &name, int64_t count, auto &&work) {
        if (!o.wants(name)) return;
        Tree tree(o.sigma, in.a, in.w);
        uint64_t checksum = 0;
        const double seconds = measure([&] { work(tree, checksum); });
        // Make mutations observable without adding queries to their timed loop.
        checksum ^= fold_tree(tree);
        print_row(name, seconds, count, checksum);
    };
    mutate("set_weight", o.q, [&](Tree &tree, uint64_t &) {
        for (const Query &q : in.queries) tree.set_weight(q.pos, q.weight);
    });
    // Deltas are nonnegative, so every weight stays nonnegative.
    mutate("add_weight", o.q, [&](Tree &tree, uint64_t &) {
        for (const Query &q : in.queries) tree.add_weight(q.pos, q.weight);
    });
    // Budgets stay valid after updates because they only need to be nonnegative.
    mutate("mixed", o.q, [&](Tree &tree, uint64_t &checksum) {
        for (int i = 0; i < o.q; ++i) {
            const Query &q = in.queries[i];
            switch (i & 3) {
                case 0: tree.set_weight(q.pos, q.weight); break;
                case 1: checksum = checksum * 1000003 + tree.sum_k_smallest(q.l, q.r, q.k + 1); break;
                case 2: checksum = checksum * 1000003 + tree.sum_range(q.l, q.r, q.lower, q.upper); break;
                case 3: checksum = checksum * 1000003 + tree.max_count_smallest_sum_le(q.l, q.r, q.target); break;
            }
        }
    });
    return 0;
}

} // namespace

int main(int argc, char **argv) {
    try {
        return run(parse_options(argc, argv, operations,
                                 key_notes + weight_notes + "tovector/toweights/toitems are timed once; ops=N.\n"
                                 "set_weight replaces one weight; add_weight adds a delta in [0, 1000000000).\n"
                                 "mixed alternates set_weight, sum_k_smallest, sum_range, max_count_smallest_sum_le (25% each).\n",
                                 true));
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
