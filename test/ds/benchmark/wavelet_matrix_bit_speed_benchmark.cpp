// Example: g++ -std=c++17 -O3 -DNDEBUG -march=native -I. this_file.cpp -o /tmp/wmb_bench
// /tmp/wmb_bench --n 300000 --q 300000 --sigma 300000 --distribution uniform
// Use --operations kth_smallest,range_freq,topk to isolate work. The structure is built once.
// WaveletMatrix<T, log> takes log as a template argument; log = bit_length(sigma-1) is dispatched
// at run time to an instantiation for 0..60, so it has the same depth as wavelet_matrix.cpp.
// The 61 instantiations make compilation take about 25 s at -O2.
#include "titan_cpplib/ds/wavelet_matrix_bit.cpp"
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

constexpr int max_log = 60;

std::vector<std::string> make_operations() {
    std::vector<std::string> operations = {"build"};
    operations.insert(operations.end(), key_operations.begin(), key_operations.end());
    operations.push_back("tovector");
    return operations;
}

const std::vector<std::string> operations = make_operations();

template<int LOG>
void run_log(const Options &o, const Input &in) {
    using Tree = titan23::WaveletMatrix<Key, LOG>;
    const auto start = Clock::now();
    const Tree tree(in.a);
    const double build = std::chrono::duration<double>(Clock::now() - start).count();
    if (o.wants("build")) print_row("build", build, o.n, fold(tree.tovector()));
    bench_key_queries(o, in, tree);
    once(o, "tovector", o.n, [&] { return tree.tovector(); });
}

template<int LOG>
void dispatch(const Options &o, const Input &in) {
    if constexpr (LOG > max_log) {
        throw std::invalid_argument("log must be at most 60");
    } else {
        if (o.log == LOG) return run_log<LOG>(o, in);
        dispatch<LOG + 1>(o, in);
    }
}

int run(const Options &o) {
    // WaveletMatrix<T, LOG>::range_freq ignores bits at or above LOG, so upper = 2^LOG would count
    // as 0. Capping upper at 2^LOG - 1 changes the input only when sigma is a power of two.
    const Key upper_limit = std::min(o.sigma, (Key(1) << o.log) - 1);
    const Input in = make_input(o, upper_limit);
    print_header(o, " topk_q=" + std::to_string(std::min(o.q, o.topk_q)) + " upper_limit=" + std::to_string(upper_limit));
    dispatch<0>(o, in);
    return 0;
}

} // namespace

int main(int argc, char **argv) {
    try {
        return run(parse_options(argc, argv, operations,
                                 key_notes + "tovector is timed once; ops=N. sum() asserts false and is not benchmarked.\n"
                                 "upper is capped at 2^log - 1 (printed as upper_limit).\n", true));
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
