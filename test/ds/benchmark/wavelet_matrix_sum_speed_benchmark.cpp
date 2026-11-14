// Example: g++ -std=c++17 -O3 -DNDEBUG -march=native -I. this_file.cpp -o /tmp/wms_bench
// /tmp/wms_bench --n 300000 --q 300000 --sigma 300000 --distribution uniform
// Use --operations sum_range,sum_k_smallest,weighted_median to isolate work. The structure is built once.
#include "titan_cpplib/ds/wavelet_matrix_sum.cpp"
#include "test/ds/benchmark/wavelet_matrix_speed_common.hpp"

namespace {

// Using-declarations, not a using-directive: the library's global `using namespace std`
// would otherwise make names such as fold/read ambiguous.
using wm_bench::Clock;
using wm_bench::Input;
using wm_bench::Key;
using wm_bench::Options;
using wm_bench::W;
using wm_bench::bench_key_queries;
using wm_bench::bench_weight_queries;
using wm_bench::fold;
using wm_bench::key_notes;
using wm_bench::key_operations;
using wm_bench::make_input;
using wm_bench::max_weight;
using wm_bench::once;
using wm_bench::parse_options;
using wm_bench::print_header;
using wm_bench::print_row;
using wm_bench::weight_notes;
using wm_bench::weight_operations;
using Tree = titan23::WaveletMatrixSum<Key, W>;

std::vector<std::string> make_operations() {
    std::vector<std::string> operations = {"build"};
    operations.insert(operations.end(), key_operations.begin(), key_operations.end());
    operations.insert(operations.end(), weight_operations.begin(), weight_operations.end());
    operations.insert(operations.end(), {"tovector", "toweights", "toitems"});
    return operations;
}

const std::vector<std::string> operations = make_operations();

uint64_t fold_tree(const Tree &tree) { return fold(tree.tovector()) * 1000003 + fold(tree.toweights()); }

int run(const Options &o) {
    const Input in = make_input(o, o.sigma);
    print_header(o, " weight=[0," + std::to_string(max_weight) + ") topk_q=" + std::to_string(std::min(o.q, o.topk_q)));

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
    return 0;
}

} // namespace

int main(int argc, char **argv) {
    try {
        return run(parse_options(argc, argv, operations,
                                 key_notes + weight_notes + "tovector/toweights/toitems are timed once; ops=N.\n", true));
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
