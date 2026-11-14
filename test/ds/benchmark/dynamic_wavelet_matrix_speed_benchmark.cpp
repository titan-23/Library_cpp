// Example: g++ -std=c++17 -O3 -DNDEBUG -march=native -I. this_file.cpp -o /tmp/dwm_bench
// /tmp/dwm_bench --n 300000 --q 300000 --sigma 300000 --distribution uniform
// Same options, queries and output as dynamic_wavelet_tree_speed_benchmark.cpp, measured on
// titan23::DynamicWaveletMatrix<uint64_t>. select_remove is unsupported and skipped.
// Checksums of the shared operations match the tree benchmark for the same options.
#define DWT_BENCH_DYNAMIC_WAVELET_MATRIX
#include "dynamic_wavelet_tree_speed_benchmark.cpp"
