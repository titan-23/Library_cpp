// Example: g++ -std=c++17 -O3 -DNDEBUG -march=native -I. this_file.cpp -o /tmp/wm_bench
// /tmp/wm_bench --n 300000 --q 300000 --sigma 300000 --distribution uniform
// Use --operations kth_smallest,range_freq,topk to isolate work. The structure is built once.
#include "titan_cpplib/ds/wavelet_matrix.cpp"
#include "test/ds/benchmark/wavelet_matrix_speed_common.hpp"

namespace {

// Using-declarations, not a using-directive: the library's global `using namespace std`
// would otherwise make names such as fold/read ambiguous.
using wm_bench::Clock;
using wm_bench::Input;
using wm_bench::Key;
using wm_bench::Options;
using wm_bench::bench_key_queries;
using wm_bench::fold;
using wm_bench::key_notes;
using wm_bench::key_operations;
using wm_bench::make_input;
using wm_bench::once;
using wm_bench::parse_options;
using wm_bench::print_header;
using wm_bench::print_row;
using Tree = titan23::WaveletMatrix<Key>;

std::vector<std::string> make_operations() {
    std::vector<std::string> operations = {"build"};
    operations.insert(operations.end(), key_operations.begin(), key_operations.end());
    operations.push_back("tovector");
    return operations;
}

const std::vector<std::string> operations = make_operations();

int run(const Options &o) {
    const Input in = make_input(o, o.sigma);
    print_header(o, " topk_q=" + std::to_string(std::min(o.q, o.topk_q)));

    const auto start = Clock::now();
    const Tree tree(o.sigma, in.a);
    const double build = std::chrono::duration<double>(Clock::now() - start).count();
    if (o.wants("build")) print_row("build", build, o.n, fold(tree.tovector()));
    bench_key_queries(o, in, tree);
    once(o, "tovector", o.n, [&] { return tree.tovector(); });
    return 0;
}

} // namespace

int main(int argc, char **argv) {
    try {
        return run(parse_options(argc, argv, operations, key_notes + "tovector is timed once; ops=N.\n", true));
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
