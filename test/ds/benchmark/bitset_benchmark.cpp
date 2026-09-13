/// https://github.com/titan-23/Library_cpp/blob/main/test/ds/benchmark/bitset_benchmark.cpp
// Example (run the binaries separately, on an otherwise idle machine):
// g++ -std=c++23 -O2 -march=x86-64-v3 -I. test/ds/benchmark/bitset_benchmark.cpp -o /tmp/bitset_bench
// /tmp/bitset_bench --ms 10 --reps 5 > /tmp/bitset_bench.csv
// Also compare a portable build without -march=x86-64-v3 and an AtCoder-like
// build with -O2 -march=native. AVX2 rows appear only when __AVX2__ is defined.
// Each *_value operation directly assigns left & right or left | right to output.
// Each *_copy operation INCLUDES copying the original input to the output.
// Each *_assign operation initializes output from left BEFORE timing, then times
// only &= or |=. Its fixed right operand preserves the first result on later
// rounds, rather than saturating through a sequence of different operands.
// The shift operation copies its original input on every iteration.
// Compiler barriers apply equally to every backend and consume the entire output.
// Allocation, random generation, construction and output validation are untimed.
// Count reductions include scalar checksum accumulation inside the timed loop.
// The default input pool mostly measures warm-cache throughput. Increase --inputs
// to explore larger working sets; these measurements are CPU/compiler dependent.
#include <algorithm>
#include <array>
#include <bitset>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <limits>
#include <random>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <vector>

#include "titan_cpplib/ds/bitset.cpp"

namespace {

using Clock = std::chrono::steady_clock;
using titan23::Bitset;
using titan23::BitsetBackend;

struct Options {
    double milliseconds = 10;
    std::size_t inputs = 64;
    std::size_t iterations = 0; // Zero means calibrate separately for each row.
    std::size_t bits = 0;       // Zero selects all supported sizes.
    int repetitions = 5;
    bool basic_suite = false;
    bool std_auto_only = false;
};

// GCC and Clang barriers: the memory clobber keeps immutable-input work inside
// the loop; the memory operand forces every output byte to be materialized.
inline void clobber_memory() {
    asm volatile("" : : : "memory");
}

template<class T>
inline void consume(const T &value) {
    asm volatile("" : : "m"(value) : "memory");
}

std::uint64_t mix(std::uint64_t x) {
    x = (x ^ (x >> 30)) * 0xbf58476d1ce4e5b9ULL;
    x = (x ^ (x >> 27)) * 0x94d049bb133111ebULL;
    return x ^ (x >> 31);
}

template<std::size_t N>
using Words = std::array<std::uint64_t, (N + 63) / 64>;

template<std::size_t N>
std::bitset<N> to_std(const Words<N> &words) {
    std::bitset<N> result;
    for (std::size_t i = 0; i < N; ++i) {
        result.set(i, (words[i / 64] >> (i % 64)) & 1);
    }
    return result;
}

template<std::size_t N>
Words<N> to_words(const std::bitset<N> &bits) {
    Words<N> result{};
    for (std::size_t i = 0; i < N; ++i) {
        result[i / 64] |= std::uint64_t(bits.test(i)) << (i % 64);
    }
    return result;
}

template<std::size_t N>
struct Data {
    std::vector<Words<N>> left_words, right_words;
    std::vector<std::bitset<N>> left, right;

    explicit Data(std::size_t size) : left_words(size), right_words(size) {
        std::mt19937_64 random(20260913 + N);
        left.reserve(size);
        right.reserve(size);
        for (std::size_t i = 0; i < size; ++i) {
            for (std::size_t j = 0; j < left_words[i].size(); ++j) {
                auto next_word = [&] {
                    if (i % 3 == 0) return random();
                    if (i % 3 == 1) return random() & random() & random();
                    return std::uint64_t(1) << (random() % 64);
                };
                left_words[i][j] = next_word();
                right_words[i][j] = next_word();
            }
            if constexpr (N % 64 != 0) {
                const std::uint64_t mask = (std::uint64_t(1) << (N % 64)) - 1;
                left_words[i].back() &= mask;
                right_words[i].back() &= mask;
            }
            left.push_back(to_std<N>(left_words[i]));
            right.push_back(to_std<N>(right_words[i]));
        }
    }
};

enum class Operation {
    Count, CountAnd, AndCopy, OrCopy, AndValue, OrValue, AndAssign, OrAssign,
    OrShiftLeftCopy
};

const char *operation_name(Operation op) {
    switch (op) {
    case Operation::Count: return "count";
    case Operation::CountAnd: return "count_and";
    case Operation::AndCopy: return "and_copy";
    case Operation::OrCopy: return "or_copy";
    case Operation::AndValue: return "and_value";
    case Operation::OrValue: return "or_value";
    case Operation::AndAssign: return "and_assign";
    case Operation::OrAssign: return "or_assign";
    case Operation::OrShiftLeftCopy: return "or_shift_left_copy";
    }
    throw std::logic_error("unknown operation");
}

template<class B>
std::size_t count_and(const B &a, const B &b) {
    return titan23::count_and(a, b);
}

template<std::size_t N>
std::size_t count_and(const std::bitset<N> &a, const std::bitset<N> &b) {
    return (a & b).count();
}

template<class B>
void or_shift_left(B &a, std::size_t shift) {
    a.or_shift_left(shift);
}

template<std::size_t N>
void or_shift_left(std::bitset<N> &a, std::size_t shift) {
    a |= a << shift;
}

template<class B, std::size_t N>
std::vector<B> make_pool(const std::vector<Words<N>> &words,
                         const std::vector<std::bitset<N>> &standard) {
    if constexpr (std::is_same_v<B, std::bitset<N>>) {
        return standard;
    } else {
        std::vector<B> result;
        result.reserve(words.size());
        for (const auto &word : words) result.push_back(B::from_words(word));
        return result;
    }
}

template<std::size_t N>
struct Expected {
    std::vector<std::bitset<N>> bits;
    std::vector<Words<N>> words;
    std::uint64_t checksum = 0;

    Expected(const Data<N> &data, Operation op, std::size_t shift) {
        for (std::size_t i = 0; i < data.left.size(); ++i) {
            if (op == Operation::Count) {
                checksum += data.left[i].count();
            } else if (op == Operation::CountAnd) {
                checksum += (data.left[i] & data.right[i]).count();
            } else {
                auto value = data.left[i];
                if (op == Operation::AndCopy || op == Operation::AndValue
                    || op == Operation::AndAssign) {
                    value &= data.right[i];
                } else if (op == Operation::OrCopy || op == Operation::OrValue
                           || op == Operation::OrAssign) {
                    value |= data.right[i];
                } else value |= value << shift;
                bits.push_back(value);
                words.push_back(to_words<N>(value));
                for (auto word : words.back()) checksum = mix(checksum ^ word);
            }
        }
    }
};

struct Measurement {
    double seconds;
    std::uint64_t checksum;
};

// The operation is a template argument, so no operation-selection branch is
// included in the inner loop. Shift counts remain runtime values.
template<Operation Op, class B>
Measurement measure(const std::vector<B> &left, const std::vector<B> &right,
                    std::vector<B> &output, std::size_t rounds, std::size_t shift) {
    if constexpr (Op == Operation::AndAssign || Op == Operation::OrAssign) {
        output = left; // Setup is outside the measured interval for assignments.
    }
    std::uint64_t checksum = 0;
    clobber_memory();
    const auto start = Clock::now();
    for (std::size_t round = 0; round < rounds; ++round) {
        for (std::size_t i = 0; i < left.size(); ++i) {
            clobber_memory();
            if constexpr (Op == Operation::Count) {
                checksum += left[i].count();
            } else if constexpr (Op == Operation::CountAnd) {
                checksum += count_and(left[i], right[i]);
            } else {
                if constexpr (Op == Operation::AndValue) {
                    output[i] = left[i] & right[i];
                } else if constexpr (Op == Operation::OrValue) {
                    output[i] = left[i] | right[i];
                } else {
                    if constexpr (Op != Operation::AndAssign && Op != Operation::OrAssign) {
                        output[i] = left[i];
                    }
                    if constexpr (Op == Operation::AndCopy || Op == Operation::AndAssign) {
                        output[i] &= right[i];
                    } else if constexpr (Op == Operation::OrCopy || Op == Operation::OrAssign) {
                        output[i] |= right[i];
                    } else or_shift_left(output[i], shift);
                }
                consume(output[i]);
            }
        }
    }
    consume(checksum);
    const auto finish = Clock::now();
    return {std::chrono::duration<double>(finish - start).count(), checksum};
}

template<Operation Op, class B, std::size_t N>
void validate(const Measurement &result, const std::vector<B> &output,
              const Expected<N> &expected, std::size_t rounds) {
    if constexpr (Op == Operation::Count || Op == Operation::CountAnd) {
        if (result.checksum != expected.checksum * rounds) {
            throw std::runtime_error("count checksum differs from std::bitset");
        }
    } else {
        for (std::size_t i = 0; i < output.size(); ++i) {
            bool equal;
            if constexpr (std::is_same_v<B, std::bitset<N>>) {
                equal = output[i] == expected.bits[i];
            } else {
                equal = output[i].words() == expected.words[i];
            }
            if (!equal) throw std::runtime_error("output differs from std::bitset");
        }
    }
}

template<Operation Op, class B, std::size_t N>
void run_row(const char *backend, const Data<N> &data, const Expected<N> &expected,
             std::size_t shift, const Options &options) {
    const auto left = make_pool<B, N>(data.left_words, data.left);
    const auto right = make_pool<B, N>(data.right_words, data.right);
    std::vector<B> output(left.size());
    // Warm up and validate before any reported measurement.
    auto run = [&](std::size_t rounds) {
        const auto result = measure<Op>(left, right, output, rounds, shift);
        validate<Op>(result, output, expected, rounds);
        return result.seconds;
    };
    std::size_t rounds = options.iterations ? options.iterations : 1;
    double seconds = run(rounds);
    if (!options.iterations) {
        // Calibrate from at least 1 ms to avoid using a single clock tick as an
        // estimate. An explicit --iterations gives identical rounds to all rows.
        while (seconds < 0.001 && rounds <= (std::size_t(1) << 28)) {
            rounds *= 2;
            seconds = run(rounds);
        }
        const double estimated = rounds * (options.milliseconds / 1000) / seconds;
        rounds = static_cast<std::size_t>(std::max(1.0, std::min(estimated, 1e9)));
    }
    std::vector<double> samples;
    for (int repetition = 0; repetition < options.repetitions; ++repetition) {
        samples.push_back(run(rounds) * 1e9 / rounds / left.size());
    }
    std::sort(samples.begin(), samples.end());
    const auto mid = samples.size() / 2;
    const double median = samples.size() % 2 ? samples[mid]
                                            : (samples[mid - 1] + samples[mid]) / 2;
    std::cout << N << ',' << operation_name(Op) << ',' << shift << ',' << backend
              << ',' << options.inputs << ',' << rounds << ',' << options.repetitions
              << ',' << median << ',' << expected.checksum << '\n';
}

template<Operation Op, std::size_t N>
void compare(const Data<N> &data, std::size_t shift, const Options &options) {
    const Expected<N> expected(data, Op, shift);
    run_row<Op, std::bitset<N>>("std", data, expected, shift, options);
    if (!options.std_auto_only) {
        run_row<Op, Bitset<N, BitsetBackend::Scalar>>("scalar", data, expected, shift, options);
#if defined(__AVX2__)
        run_row<Op, Bitset<N, BitsetBackend::Avx2>>("avx2", data, expected, shift, options);
#endif
    }
    run_row<Op, Bitset<N, BitsetBackend::Auto>>("auto", data, expected, shift, options);
}

template<std::size_t N>
void run_size(const Options &options) {
    if (options.bits && options.bits != N) return;
    const Data<N> data(options.inputs);
    compare<Operation::Count>(data, 0, options);
    if (!options.basic_suite) {
        compare<Operation::CountAnd>(data, 0, options);
        compare<Operation::AndCopy>(data, 0, options);
        compare<Operation::OrCopy>(data, 0, options);
    }
    compare<Operation::AndValue>(data, 0, options);
    compare<Operation::OrValue>(data, 0, options);
    compare<Operation::AndAssign>(data, 0, options);
    compare<Operation::OrAssign>(data, 0, options);
    if (options.basic_suite) return;
    for (std::size_t shift : {std::size_t(0), std::size_t(1), std::size_t(17),
                             std::size_t(63), std::size_t(64), std::size_t(65),
                             N / 2 + 7, N}) {
        compare<Operation::OrShiftLeftCopy>(data, shift, options);
    }
}

std::size_t positive_integer(const std::string &value) {
    if (value.empty() || value.find_first_not_of("0123456789") != std::string::npos) {
        throw std::invalid_argument("expected a positive integer: " + value);
    }
    const auto number = std::stoull(value);
    if (!number || number > std::numeric_limits<std::size_t>::max()) {
        throw std::invalid_argument("integer is outside the supported range");
    }
    return static_cast<std::size_t>(number);
}

Options parse_options(int argc, char **argv) {
    Options options;
    for (int i = 1; i < argc; ++i) {
        const std::string key = argv[i];
        if (key == "--help") {
            std::cout << "Usage: bitset_benchmark [--ms 10] [--reps 5] [--inputs 64]\n"
                         "       [--iterations ROUNDS] [--bits 128|256|512|1024|2048|4096|4097|65536]\n"
                         "       [--suite basic|all] [--backends std-auto|all]\n"
                         "--ms sets target milliseconds per sample; --iterations overrides it.\n"
                         "One round visits all inputs. *_copy includes the input copy.\n"
                         "*_value directly assigns left & right or left | right to output.\n"
                         "*_assign initializes output before timing, then repeats &= or |= with fixed rhs.\n"
                         "basic runs count, and_value, or_value, and_assign and or_assign.\n"
                         "Defaults: --suite all --backends all.\n"
                         "CSV reports median ns/operation; all results are validated.\n";
            std::exit(0);
        }
        if (++i == argc) throw std::invalid_argument("missing value for " + key);
        const std::string value = argv[i];
        if (key == "--ms") {
            std::size_t used = 0;
            options.milliseconds = std::stod(value, &used);
            if (used != value.size() || !std::isfinite(options.milliseconds)
                || options.milliseconds <= 0 || options.milliseconds > 60000) {
                throw std::invalid_argument("--ms must be in (0, 60000]");
            }
        } else if (key == "--inputs") options.inputs = positive_integer(value);
        else if (key == "--iterations") options.iterations = positive_integer(value);
        else if (key == "--bits") options.bits = positive_integer(value);
        else if (key == "--suite") {
            if (value != "basic" && value != "all") {
                throw std::invalid_argument("--suite must be basic or all");
            }
            options.basic_suite = value == "basic";
        } else if (key == "--backends") {
            if (value != "std-auto" && value != "all") {
                throw std::invalid_argument("--backends must be std-auto or all");
            }
            options.std_auto_only = value == "std-auto";
        } else if (key == "--reps") {
            const auto repetitions = positive_integer(value);
            if (repetitions > 1000) throw std::invalid_argument("--reps must be <= 1000");
            options.repetitions = static_cast<int>(repetitions);
        } else throw std::invalid_argument("unknown option: " + key);
    }
    if (options.bits && options.bits != 128 && options.bits != 256 && options.bits != 512
        && options.bits != 1024 && options.bits != 2048 && options.bits != 4096
        && options.bits != 4097 && options.bits != 65536) {
        throw std::invalid_argument("unsupported --bits size (see --help)");
    }
    return options;
}

} // namespace

int main(int argc, char **argv) {
    try {
        const Options options = parse_options(argc, argv);
        std::cerr << "Compiler: " << __VERSION__ << "; AVX2: "
#if defined(__AVX2__)
                  << "yes"
#else
                  << "no"
#endif
                  << "; seed: 20260913 + N; mixed 1/2, 1/8 and 1/64 densities\n";
        std::cout << "bits,operation,shift,backend,inputs,rounds,repetitions,median_ns,checksum\n"
                  << std::fixed << std::setprecision(3);
        run_size<128>(options);
        run_size<256>(options);
        run_size<512>(options);
        run_size<1024>(options);
        run_size<2048>(options);
        run_size<4096>(options);
        run_size<4097>(options);
        run_size<65536>(options);
    } catch (const std::exception &error) {
        std::cerr << "bitset_benchmark: " << error.what() << '\n';
        return 1;
    }
}
