#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <limits>
#include <numeric>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

using namespace std;

namespace {

using Clock = chrono::steady_clock;
using Slot = uint32_t;

volatile uint64_t benchmark_sink = 0;

enum class Shape {
    Sibling,
    Group4,
    Deep2,
    Half,
    Shallow,
    Mixed,
};

struct Scenario {
    string name;
    int width;
    int depth;
    Shape shape;
    size_t requested_working_set;
};

struct Fixture {
    int width = 0;
    int depth = 0;
    vector<Slot> parent_ordinal;
    vector<uint32_t> old_adjacent_lcp;
    vector<uint32_t> expected_lcp;
    vector<Slot> target;
    vector<Slot> target_parent;
    vector<vector<Slot>> parent_block;
    vector<size_t> suffix_offset;
    vector<Slot> suffix_input;
    vector<Slot> slot_tour;
    vector<uint32_t> slot_leaf;
    vector<Slot> compact_parent;
    vector<uint32_t> compact_lcp;
    uint32_t compact_entry_lcp = 0;
    vector<Slot> oracle_parent;
    vector<Slot> oracle_frontier;
    vector<uint32_t> oracle_lcp;
    uint32_t oracle_entry_lcp = 0;
    vector<Slot> trace_scratch;

    size_t parent_block_bytes() const {
        return accumulate(parent_block.begin(), parent_block.end(), size_t(0), [](size_t sum, const auto &block) {
            return sum + block.capacity() * sizeof(Slot);
        });
    }

    size_t bytes() const {
        return parent_ordinal.capacity() * sizeof(Slot) + old_adjacent_lcp.capacity() * sizeof(uint32_t) +
               expected_lcp.capacity() * sizeof(uint32_t) + target.capacity() * sizeof(Slot) +
               target_parent.capacity() * sizeof(Slot) +
               suffix_offset.capacity() * sizeof(size_t) + suffix_input.capacity() * sizeof(Slot) +
               slot_tour.capacity() * sizeof(Slot) + slot_leaf.capacity() * sizeof(uint32_t) +
               compact_parent.capacity() * sizeof(Slot) + compact_lcp.capacity() * sizeof(uint32_t) +
               oracle_parent.capacity() * sizeof(Slot) + oracle_frontier.capacity() * sizeof(Slot) +
               oracle_lcp.capacity() * sizeof(uint32_t) + trace_scratch.capacity() * sizeof(Slot) +
               parent_block_bytes();
    }

    size_t slot_build_bytes() const {
        return suffix_offset.capacity() * sizeof(size_t) + suffix_input.capacity() * sizeof(Slot) +
               slot_tour.capacity() * sizeof(Slot) + slot_leaf.capacity() * sizeof(uint32_t);
    }

    size_t parent_build_bytes(bool oracle) const {
        size_t common = parent_ordinal.capacity() * sizeof(Slot) + old_adjacent_lcp.capacity() * sizeof(uint32_t) +
                        target_parent.capacity() * sizeof(Slot);
        if (oracle) {
            return common + oracle_parent.capacity() * sizeof(Slot) + oracle_frontier.capacity() * sizeof(Slot) +
                   oracle_lcp.capacity() * sizeof(uint32_t);
        }
        return common + compact_parent.capacity() * sizeof(Slot) + compact_lcp.capacity() * sizeof(uint32_t);
    }

    size_t slot_decode_bytes() const {
        return slot_tour.capacity() * sizeof(Slot) + slot_leaf.capacity() * sizeof(uint32_t) +
               trace_scratch.capacity() * sizeof(Slot);
    }

    size_t parent_decode_bytes() const {
        return target.capacity() * sizeof(Slot) + compact_parent.capacity() * sizeof(Slot) +
               compact_lcp.capacity() * sizeof(uint32_t) + trace_scratch.capacity() * sizeof(Slot) +
               parent_block_bytes();
    }
};

struct Measurement {
    double median_ns = 0;
    double minimum_ns = 0;
    uint64_t iterations = 0;
};

uint64_t mix64(uint64_t value) {
    value += 0x9e3779b97f4a7c15ULL;
    value = (value ^ (value >> 30)) * 0xbf58476d1ce4e5b9ULL;
    value = (value ^ (value >> 27)) * 0x94d049bb133111ebULL;
    return value ^ (value >> 31);
}

void digest_add(uint64_t &digest, uint64_t value) {
    digest = mix64(digest ^ mix64(value));
}

string shape_name(Shape shape) {
    switch (shape) {
        case Shape::Sibling: return "sibling";
        case Shape::Group4: return "group4";
        case Shape::Deep2: return "deep2";
        case Shape::Half: return "half";
        case Shape::Shallow: return "shallow";
        case Shape::Mixed: return "mixed";
    }
    abort();
}

int group_size(Shape shape, int output_index) {
    switch (shape) {
        case Shape::Sibling: return numeric_limits<int>::max();
        case Shape::Group4: return 4;
        case Shape::Deep2:
        case Shape::Half:
        case Shape::Shallow: return 1;
        case Shape::Mixed: return 1 + ((output_index / 7) % 7);
    }
    abort();
}

uint32_t boundary_lcp(Shape shape, int depth, int boundary) {
    int parent_depth = depth - 1;
    switch (shape) {
        case Shape::Sibling: return (uint32_t)parent_depth;
        case Shape::Group4:
        case Shape::Deep2: return (uint32_t)max(0, parent_depth - 1);
        case Shape::Half: return (uint32_t)(parent_depth / 2);
        case Shape::Shallow: return 0;
        case Shape::Mixed: {
            int choices[] = {max(0, parent_depth - 1), 3 * parent_depth / 4, parent_depth / 2, 1, 0};
            return (uint32_t)choices[boundary % 5];
        }
    }
    abort();
}

Fixture make_fixture(const Scenario &scenario) {
    if (scenario.width < 2 || scenario.depth < 2) throw invalid_argument("width and depth must be at least 2");

    Fixture f;
    f.width = scenario.width;
    f.depth = scenario.depth;
    f.parent_ordinal.resize(f.width);

    int logical_parent_count = 0;
    int begin = 0;
    while (begin < f.width) {
        int size = group_size(scenario.shape, begin);
        int end = min(f.width, begin + size);
        for (int j = begin; j < end; ++j) f.parent_ordinal[j] = (Slot)logical_parent_count;
        ++logical_parent_count;
        begin = end;
    }

    int lead = scenario.shape == Shape::Group4 || scenario.shape == Shape::Mixed ? 1 : 0;
    int stride = scenario.shape == Shape::Group4 || scenario.shape == Shape::Mixed ? 2 : 1;
    for (Slot &ordinal : f.parent_ordinal) ordinal = (Slot)(lead + (int)ordinal * stride);
    int parent_count = lead + (logical_parent_count - 1) * stride + 1;

    f.old_adjacent_lcp.resize(max(0, parent_count - 1));
    for (int i = 0; i + 1 < parent_count; ++i) {
        f.old_adjacent_lcp[i] = boundary_lcp(scenario.shape, f.depth, i);
    }

    vector<vector<Slot>> parent_path(parent_count, vector<Slot>(f.depth));
    f.parent_block.resize(f.depth);
    f.parent_block[0].push_back(0);
    auto add_node = [&](int depth, Slot parent) {
        if (f.parent_block[depth].size() == numeric_limits<Slot>::max()) {
            throw overflow_error("slot id overflow");
        }
        Slot slot = (Slot)f.parent_block[depth].size();
        f.parent_block[depth].push_back(parent);
        return slot;
    };

    parent_path[0][0] = 0;
    for (int d = 1; d < f.depth; ++d) parent_path[0][d] = add_node(d, parent_path[0][d - 1]);
    for (int p = 1; p < parent_count; ++p) {
        uint32_t h = f.old_adjacent_lcp[p - 1];
        for (uint32_t d = 0; d <= h; ++d) parent_path[p][d] = parent_path[p - 1][d];
        for (int d = (int)h + 1; d < f.depth; ++d) {
            parent_path[p][d] = add_node(d, parent_path[p][d - 1]);
        }
    }

    f.target.resize(f.width);
    f.target_parent.resize(f.width);
    for (int j = 0; j < f.width; ++j) {
        int p = (int)f.parent_ordinal[j];
        f.target_parent[j] = parent_path[p][f.depth - 1];
        f.target[j] = (Slot)j;
    }

    f.expected_lcp.resize(f.width - 1);
    int cursor = (int)f.parent_ordinal[0];
    for (int j = 1; j < f.width; ++j) {
        int p = (int)f.parent_ordinal[j];
        if (p == cursor) {
            f.expected_lcp[j - 1] = (uint32_t)(f.depth - 1);
            continue;
        }
        uint32_t h = (uint32_t)(f.depth - 1);
        while (cursor < p) h = min(h, f.old_adjacent_lcp[cursor++]);
        f.expected_lcp[j - 1] = h;
    }

    f.suffix_offset.assign(f.width, 0);
    for (int j = 1; j < f.width; ++j) {
        int p = (int)f.parent_ordinal[j];
        int h = (int)f.expected_lcp[j - 1];
        for (int d = h + 1; d < f.depth; ++d) f.suffix_input.push_back(parent_path[p][d]);
        f.suffix_input.push_back(f.target[j]);
        f.suffix_offset[j] = f.suffix_input.size();
    }

    f.slot_tour.resize(f.suffix_input.size());
    f.slot_leaf.resize(f.width);
    f.compact_parent.resize(f.width);
    f.compact_lcp.resize(f.width - 1);
    f.oracle_parent.resize(f.width);
    f.oracle_frontier.resize(f.width);
    f.oracle_lcp.resize(f.width - 1);
    f.trace_scratch.resize((size_t)f.depth + 1);
    return f;
}

inline void compiler_barrier(const void *ptr, size_t size) {
#if defined(__GNUC__) || defined(__clang__)
    asm volatile("" : : "g"(ptr), "g"(size) : "memory");
#else
    (void)ptr;
    (void)size;
#endif
}

[[gnu::noinline]] uint64_t build_slot(Fixture &f) {
    size_t output_size = 0;
    f.slot_leaf[0] = 0;
    for (int j = 1; j < f.width; ++j) {
        size_t begin = f.suffix_offset[j - 1];
        size_t count = f.suffix_offset[j] - begin;
        copy_n(f.suffix_input.begin() + (ptrdiff_t)begin, count,
               f.slot_tour.begin() + (ptrdiff_t)output_size);
        output_size += count;
        f.slot_leaf[j] = (uint32_t)output_size;
    }
    compiler_barrier(f.slot_tour.data(), f.slot_tour.size());
    return f.slot_tour.size() + f.slot_leaf.back();
}

[[gnu::noinline]] uint64_t build_parent_compact(Fixture &f) {
    int cursor = 0;
    uint32_t entry_h = (uint32_t)(f.depth - 1);
    while (cursor < (int)f.parent_ordinal[0]) entry_h = min(entry_h, f.old_adjacent_lcp[cursor++]);
    f.compact_entry_lcp = entry_h;
    f.compact_parent[0] = f.target_parent[0];
    for (int j = 1; j < f.width; ++j) {
        int p = (int)f.parent_ordinal[j];
        f.compact_parent[j] = f.target_parent[j];
        if (p == cursor) {
            f.compact_lcp[j - 1] = (uint32_t)(f.depth - 1);
            continue;
        }
        uint32_t h = (uint32_t)(f.depth - 1);
        while (cursor < p) h = min(h, f.old_adjacent_lcp[cursor++]);
        f.compact_lcp[j - 1] = h;
    }
    compiler_barrier(f.compact_parent.data(), f.compact_parent.size());
    return f.compact_parent.back() + f.compact_lcp.back() + f.compact_entry_lcp;
}

[[gnu::noinline]] uint64_t build_parent_oracle(Fixture &f) {
    int cursor = 0;
    uint32_t entry_h = (uint32_t)(f.depth - 1);
    while (cursor < (int)f.parent_ordinal[0]) entry_h = min(entry_h, f.old_adjacent_lcp[cursor++]);
    f.oracle_entry_lcp = entry_h;
    for (int j = 0; j < f.width; ++j) {
        f.oracle_parent[j] = f.target_parent[j];
        f.oracle_frontier[j] = f.target[j];
        if (j == 0) continue;
        int p = (int)f.parent_ordinal[j];
        if (p == cursor) {
            f.oracle_lcp[j - 1] = (uint32_t)(f.depth - 1);
            continue;
        }
        uint32_t h = (uint32_t)(f.depth - 1);
        while (cursor < p) h = min(h, f.old_adjacent_lcp[cursor++]);
        f.oracle_lcp[j - 1] = h;
    }
    compiler_barrier(f.oracle_parent.data(), f.oracle_parent.size());
    return f.oracle_parent.back() + f.oracle_frontier.back() + f.oracle_lcp.back() + f.oracle_entry_lcp;
}

[[gnu::noinline]] uint64_t decode_slot(Fixture &f) {
    vector<Slot> &trace = f.trace_scratch;
    uint64_t checksum = 0;
    for (int j = 1; j < f.width; ++j) {
        size_t begin = f.slot_leaf[j - 1];
        size_t end = f.slot_leaf[j];
        int h = f.depth - (int)(end - begin);
        int d = h + 1;
        for (size_t k = begin; k < end; ++k, ++d) {
            Slot value = f.slot_tour[k];
            trace[d] = value;
            checksum += (uint64_t)value * (uint64_t)(d + 17);
        }
    }
    compiler_barrier(trace.data(), trace.size());
    return checksum;
}

[[gnu::noinline]] uint64_t decode_parent(Fixture &f) {
    vector<Slot> &trace = f.trace_scratch;
    uint64_t checksum = 0;
    for (int j = 1; j < f.width; ++j) {
        int h = (int)f.compact_lcp[j - 1];
        trace[f.depth] = f.target[j];
        checksum += (uint64_t)f.target[j] * (uint64_t)(f.depth + 17);
        Slot slot = f.compact_parent[j];
        for (int d = f.depth - 1; d > h; --d) {
            trace[d] = slot;
            checksum += (uint64_t)slot * (uint64_t)(d + 17);
            if (d > h + 1) slot = f.parent_block[d][slot];
        }
    }
    compiler_barrier(trace.data(), trace.size());
    return checksum;
}

uint64_t strong_slot_digest(const Fixture &f) {
    uint64_t digest = 0x243f6a8885a308d3ULL;
    for (int j = 1; j < f.width; ++j) {
        size_t begin = f.slot_leaf[j - 1];
        size_t end = f.slot_leaf[j];
        int h = f.depth - (int)(end - begin);
        digest_add(digest, (uint64_t)j);
        digest_add(digest, (uint64_t)h);
        for (size_t k = begin; k < end; ++k) digest_add(digest, f.slot_tour[k]);
    }
    return digest;
}

uint64_t strong_parent_digest(const Fixture &f) {
    uint64_t digest = 0x243f6a8885a308d3ULL;
    vector<Slot> trace((size_t)f.depth + 1);
    for (int j = 1; j < f.width; ++j) {
        int h = (int)f.compact_lcp[j - 1];
        Slot slot = f.target[j];
        trace[f.depth] = slot;
        slot = f.compact_parent[j];
        for (int d = f.depth - 1; d > h; --d) {
            trace[d] = slot;
            if (d > h + 1) slot = f.parent_block[d][slot];
        }
        digest_add(digest, (uint64_t)j);
        digest_add(digest, (uint64_t)h);
        for (int d = h + 1; d <= f.depth; ++d) digest_add(digest, trace[d]);
    }
    return digest;
}

Measurement summarize(vector<double> elapsed, uint64_t iterations) {
    sort(elapsed.begin(), elapsed.end());
    return {elapsed[elapsed.size() / 2], elapsed.front(), iterations};
}

template<class Kernel>
void warmup(vector<Fixture> &fixtures, Kernel kernel) {
    uint64_t local = 0;
    for (Fixture &fixture : fixtures) local += kernel(fixture);
    benchmark_sink = benchmark_sink ^ local;
}

template<class Kernel>
double measure_sample(vector<Fixture> &fixtures, uint64_t iterations, int sample, Kernel kernel) {
    uint64_t local = 0;
    auto start = Clock::now();
    for (uint64_t i = 0; i < iterations; ++i) {
        size_t index = (size_t)((i + (uint64_t)sample) % fixtures.size());
        local += kernel(fixtures[index]);
    }
    auto finish = Clock::now();
    benchmark_sink = benchmark_sink ^ local;
    return chrono::duration<double, nano>(finish - start).count() / (double)iterations;
}

vector<Scenario> scenarios() {
    constexpr size_t KiB = 1024;
    constexpr size_t MiB = 1024 * KiB;
    return {
        {"w256_d32_sibling_alloc64k", 256, 32, Shape::Sibling, 64 * KiB},
        {"w256_d32_shallow_alloc512k", 256, 32, Shape::Shallow, 512 * KiB},
        {"w4096_d64_group4_alloc4m", 4096, 64, Shape::Group4, 4 * MiB},
        {"w4096_d64_deep2_alloc32m", 4096, 64, Shape::Deep2, 32 * MiB},
        {"w4096_d64_shallow_alloc32m", 4096, 64, Shape::Shallow, 32 * MiB},
        {"w4096_d128_mixed_alloc32m", 4096, 128, Shape::Mixed, 32 * MiB},
        {"w16384_d64_sibling_alloc32m", 16384, 64, Shape::Sibling, 32 * MiB},
        {"w2048_d256_half_alloc32m", 2048, 256, Shape::Half, 32 * MiB},
    };
}

uint64_t parse_u64(string_view text, string_view option) {
    char *end = nullptr;
    string value(text);
    uint64_t result = strtoull(value.c_str(), &end, 10);
    if (!end || *end != '\0') throw invalid_argument("invalid value for " + string(option));
    return result;
}

}

int main(int argc, char **argv) {
    int samples = 9;
    uint64_t target_units = 8'000'000;
    string filter;
    for (int i = 1; i < argc; ++i) {
        string_view arg = argv[i];
        if (arg == "--samples" && i + 1 < argc) {
            samples = (int)parse_u64(argv[++i], arg);
        } else if (arg == "--target-units" && i + 1 < argc) {
            target_units = parse_u64(argv[++i], arg);
        } else if (arg == "--filter" && i + 1 < argc) {
            filter = argv[++i];
        } else {
            throw invalid_argument("usage: topology_microbench [--samples N] [--target-units N] [--filter TEXT]");
        }
    }
    if (samples < 3 || samples % 2 == 0) throw invalid_argument("samples must be an odd number at least 3");

    cout << "name\tshape\twidth\tdepth\trequested_bytes\tallocated_bytes\tslot_build_bytes\t"
            "compact_build_bytes\t"
            "oracle_build_bytes\tslot_decode_bytes\tparent_decode_bytes\treplicas\tsuffix_tokens\tbuild_units\t"
            "slot_build_ns\tcompact_build_ns\toracle_build_ns\tslot_decode_ns\tparent_decode_ns\t"
            "compact_build_ratio\toracle_build_ratio\tparent_decode_ratio\tdigest\n";
    cout << fixed << setprecision(3);

    for (const Scenario &scenario : scenarios()) {
        if (!filter.empty() && scenario.name.find(filter) == string::npos) continue;

        Fixture prototype = make_fixture(scenario);
        size_t copies = max<size_t>(1, (scenario.requested_working_set + prototype.bytes() - 1) / prototype.bytes());
        vector<Fixture> fixtures;
        fixtures.reserve(copies);
        fixtures.push_back(move(prototype));
        for (size_t i = 1; i < copies; ++i) fixtures.push_back(make_fixture(scenario));

        size_t working_set = 0;
        size_t slot_build_working_set = 0;
        size_t compact_build_working_set = 0;
        size_t oracle_build_working_set = 0;
        size_t slot_decode_working_set = 0;
        size_t parent_decode_working_set = 0;
        for (Fixture &f : fixtures) {
            build_slot(f);
            build_parent_compact(f);
            build_parent_oracle(f);
            if (f.compact_lcp != f.expected_lcp || f.oracle_lcp != f.expected_lcp) {
                throw runtime_error("LCP construction mismatch in " + scenario.name);
            }
            uint64_t slot_checksum = decode_slot(f);
            uint64_t parent_checksum = decode_parent(f);
            if (slot_checksum != parent_checksum) throw runtime_error("decode checksum mismatch in " + scenario.name);
            if (strong_slot_digest(f) != strong_parent_digest(f)) {
                throw runtime_error("strong digest mismatch in " + scenario.name);
            }
            working_set += f.bytes();
            slot_build_working_set += f.slot_build_bytes();
            compact_build_working_set += f.parent_build_bytes(false);
            oracle_build_working_set += f.parent_build_bytes(true);
            slot_decode_working_set += f.slot_decode_bytes();
            parent_decode_working_set += f.parent_decode_bytes();
        }

        uint64_t suffix_tokens = fixtures[0].suffix_input.size();
        uint64_t build_units = suffix_tokens + (uint64_t)scenario.width;
        uint64_t decode_units = max<uint64_t>(1, suffix_tokens);
        uint64_t build_iterations = max<uint64_t>(fixtures.size(), (target_units + build_units - 1) / build_units);
        uint64_t decode_iterations = max<uint64_t>(fixtures.size(), (target_units + decode_units - 1) / decode_units);
        for (int i = 0; i < 2; ++i) {
            warmup(fixtures, build_slot);
            warmup(fixtures, build_parent_compact);
            warmup(fixtures, build_parent_oracle);
            warmup(fixtures, decode_slot);
            warmup(fixtures, decode_parent);
        }

        array<vector<double>, 5> elapsed;
        for (auto &values : elapsed) values.reserve(samples);
        for (int sample = 0; sample < samples; ++sample) {
            for (int position = 0; position < 5; ++position) {
                int kernel = (sample + position) % 5;
                switch (kernel) {
                    case 0:
                        elapsed[0].push_back(measure_sample(fixtures, build_iterations, sample, build_slot));
                        break;
                    case 1:
                        elapsed[1].push_back(
                            measure_sample(fixtures, build_iterations, sample, build_parent_compact));
                        break;
                    case 2:
                        elapsed[2].push_back(measure_sample(fixtures, build_iterations, sample, build_parent_oracle));
                        break;
                    case 3:
                        elapsed[3].push_back(measure_sample(fixtures, decode_iterations, sample, decode_slot));
                        break;
                    case 4:
                        elapsed[4].push_back(measure_sample(fixtures, decode_iterations, sample, decode_parent));
                        break;
                }
            }
        }
        Measurement slot_build = summarize(move(elapsed[0]), build_iterations);
        Measurement compact_build = summarize(move(elapsed[1]), build_iterations);
        Measurement oracle_build = summarize(move(elapsed[2]), build_iterations);
        Measurement slot_decode = summarize(move(elapsed[3]), decode_iterations);
        Measurement parent_decode = summarize(move(elapsed[4]), decode_iterations);
        uint64_t digest = strong_slot_digest(fixtures[0]);

        cout << scenario.name << '\t' << shape_name(scenario.shape) << '\t' << scenario.width << '\t'
             << scenario.depth << '\t' << scenario.requested_working_set << '\t' << working_set << '\t'
             << slot_build_working_set << '\t'
             << compact_build_working_set << '\t' << oracle_build_working_set << '\t'
             << slot_decode_working_set << '\t' << parent_decode_working_set << '\t' << fixtures.size() << '\t'
             << suffix_tokens << '\t' << build_units << '\t' << slot_build.median_ns << '\t'
             << compact_build.median_ns << '\t'
             << oracle_build.median_ns << '\t' << slot_decode.median_ns << '\t' << parent_decode.median_ns << '\t'
             << compact_build.median_ns / slot_build.median_ns << '\t'
             << oracle_build.median_ns / slot_build.median_ns << '\t'
             << parent_decode.median_ns / slot_decode.median_ns << '\t' << hex << digest << dec << '\n';
    }
    return benchmark_sink == 0xdeadbeefcafef00dULL;
}
