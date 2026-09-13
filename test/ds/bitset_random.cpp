#include <array>
#include <bitset>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <random>
#include <type_traits>
#include <utility>
#include <vector>

#include "titan_cpplib/ds/bitset.cpp"

namespace {

const char* phase = "initialization";
std::size_t current_size = 0;
int current_backend = 0;

void check(bool condition, const char* detail) {
    if (!condition) {
        std::cerr << "Bitset test failed: N=" << current_size
                  << " backend=" << current_backend << " phase=" << phase
                  << " check=" << detail << '\n';
        std::exit(1);
    }
}

template <std::size_t N, titan23::BitsetBackend Backend>
void verify(const titan23::Bitset<N, Backend>& actual,
            const std::bitset<N>& expected) {
    check(actual.size() == N, "size");
    check(actual.len() == N, "len");
    check(actual.count() == expected.count(), "count");
    check(actual.any() == expected.any(), "any");
    check(actual.none() == expected.none(), "none");
    check(actual.all() == expected.all(), "all");
    for (std::size_t i = 0; i < N; ++i) {
        check(actual.test(i) == expected[i], "test");
        check(actual.access(i) == expected[i], "access");
        check(actual[i] == expected[i], "const operator[]");
    }
    if constexpr (N % 64 != 0) {
        check((actual.words().back() >> (N % 64)) == 0, "unused tail bits");
    }
}

template <std::size_t N>
std::vector<std::size_t> boundary_shifts() {
    return {0, 1, 2, 31, 32, 63, 64, 65, 127, 128, 129,
            255, 256, 257, N == 0 ? 0 : N - 1, N, N + 1,
            std::numeric_limits<std::size_t>::max()};
}

template <std::size_t N, titan23::BitsetBackend Backend>
std::pair<titan23::Bitset<N, Backend>, std::bitset<N>>
random_pair(std::mt19937_64& rng) {
    using Bitset = titan23::Bitset<N, Backend>;
    std::array<std::uint64_t, Bitset::word_count> words{};
    for (auto& word : words) word = rng();
    std::bitset<N> reference;
    for (std::size_t i = 0; i < N; ++i) {
        reference[i] = (words[i / 64] >> (i % 64)) & 1;
    }
    return {Bitset::from_words(words), reference};
}

template <std::size_t N, titan23::BitsetBackend Backend>
void check_shifts(const titan23::Bitset<N, Backend>& source,
                  const std::bitset<N>& reference, std::size_t shift) {
    auto actual = source;
    check(&(actual <<= shift) == &actual, "left shift assignment return");
    verify(actual, reference << shift);
    verify(source << shift, reference << shift);

    actual = source;
    check(&(actual >>= shift) == &actual, "right shift assignment return");
    verify(actual, reference >> shift);
    verify(source >> shift, reference >> shift);

    actual = source;
    check(&actual.or_shift_left(shift) == &actual, "fused left shift return");
    verify(actual, reference | (reference << shift));
    actual = source;
    check(&actual.or_shift_right(shift) == &actual, "fused right shift return");
    verify(actual, reference | (reference >> shift));
}

template <std::size_t N, titan23::BitsetBackend Backend>
void construction_and_boundaries(std::mt19937_64& rng) {
    using Bitset = titan23::Bitset<N, Backend>;
    static_assert(Bitset::word_count == (N + 63) / 64);
    static_assert(std::is_same_v<decltype(std::declval<Bitset&>().words()),
                                const std::array<std::uint64_t,
                                                 Bitset::word_count>&>);
    phase = "construction and whole-bitset mutation";
    Bitset actual;
    std::bitset<N> reference;
    verify(actual, reference);
    check(&actual.set() == &actual, "set all return");
    reference.set();
    verify(actual, reference);
    check(&actual.flip() == &actual, "flip all return");
    reference.flip();
    verify(actual, reference);
    actual.set();
    check(&actual.reset() == &actual, "reset all return");
    reference.reset();
    verify(actual, reference);

    for (std::uint64_t value : {std::uint64_t{0}, std::uint64_t{1},
                               std::uint64_t{1} << 63,
                               std::numeric_limits<std::uint64_t>::max()}) {
        verify(Bitset(value), std::bitset<N>(value));
    }
    phase = "from_words masks unused bits";
    std::array<std::uint64_t, Bitset::word_count> words{};
    words.fill(std::numeric_limits<std::uint64_t>::max());
    verify(Bitset::from_words(words), reference.set());

    phase = "boundary shift counts";
    for (int pattern = 0; pattern < 8; ++pattern) {
        auto [bits, expected] = random_pair<N, Backend>(rng);
        if (pattern == 0) {
            bits.reset();
            expected.reset();
        } else if (pattern == 1) {
            bits.set();
            expected.set();
        }
        verify(bits, expected);
        verify(~bits, ~expected);
        for (std::size_t shift : boundary_shifts<N>()) {
            check_shifts(bits, expected, shift);
        }
    }

    phase = "single-bit shifts across word and vector boundaries";
    for (std::size_t i = 0; i < N; ++i) {
        // Exhaustive positions and shifts for short bitsets; sample positions
        // next to each 64-bit boundary for long ones.
        if constexpr (N > 129) {
            if (i % 64 > 1 && i % 64 < 62 && i + 1 != N) continue;
        }
        Bitset bits;
        std::bitset<N> expected;
        bits.set(i);
        expected.set(i);
        if constexpr (N <= 129) {
            for (std::size_t shift = 0; shift <= N + 1; ++shift) {
                check_shifts(bits, expected, shift);
            }
        } else {
            for (std::size_t shift : boundary_shifts<N>()) {
                check_shifts(bits, expected, shift);
            }
        }
    }
}

template <std::size_t N, titan23::BitsetBackend Backend>
void random_operations(std::mt19937_64& rng) {
    using Bitset = titan23::Bitset<N, Backend>;
    auto [actual, reference] = random_pair<N, Backend>(rng);
    const auto shifts = boundary_shifts<N>();
    const int rounds = N < 1024 ? 1500 : 500;
    phase = "random operation sequences";
    for (int iteration = 0; iteration < rounds; ++iteration) {
        auto [other, other_reference] = random_pair<N, Backend>(rng);
        const std::size_t shift = (rng() & 1)
            ? shifts[rng() % shifts.size()]
            : rng() % (N + 130);
        switch (rng() % 17) {
        case 0:
            check(&(actual &= other) == &actual, "and assignment return");
            reference &= other_reference;
            break;
        case 1:
            check(&(actual |= other) == &actual, "or assignment return");
            reference |= other_reference;
            break;
        case 2:
            check(&(actual ^= other) == &actual, "xor assignment return");
            reference ^= other_reference;
            break;
        case 3:
            actual <<= shift;
            reference <<= shift;
            break;
        case 4:
            actual >>= shift;
            reference >>= shift;
            break;
        case 5:
            actual.or_shift_left(shift);
            reference |= reference << shift;
            break;
        case 6:
            actual.or_shift_right(shift);
            reference |= reference >> shift;
            break;
        case 7:
            actual.flip();
            reference.flip();
            break;
        case 8:
            actual = other;
            reference = other_reference;
            break;
        case 9:
            if constexpr (N > 0) {
                const std::size_t position = rng() % N;
                const bool value = rng() & 1;
                check(&actual.set(position, value) == &actual, "set bit return");
                reference.set(position, value);
            }
            break;
        case 10:
            if constexpr (N > 0) {
                const std::size_t position = rng() % N;
                check(&actual.reset(position) == &actual, "reset bit return");
                reference.reset(position);
            }
            break;
        case 11:
            if constexpr (N > 0) {
                const std::size_t position = rng() % N;
                check(&actual.flip(position) == &actual, "flip bit return");
                reference.flip(position);
            }
            break;
        case 12:
            if constexpr (N > 0) {
                const std::size_t position = rng() % N;
                const std::size_t source = rng() % N;
                actual[position] = actual[source];
                reference[position] = reference[source];
                actual[position] = actual[position];
                actual[source] = true;
                reference[source] = true;
                actual[position] = false;
                reference[position] = false;
                check(static_cast<bool>(actual[source]) == reference[source],
                      "mutable proxy conversion");
            }
            break;
        case 13: {
            const Bitset copy = actual;
            actual &= actual;
            check(actual == copy, "self and");
            actual |= actual;
            check(actual == copy, "self or");
            actual ^= actual;
            reference.reset();
            break;
        }
        case 14:
            actual.set();
            reference.set();
            break;
        case 15:
            actual.reset();
            reference.reset();
            break;
        case 16:
            check_shifts(actual, reference, shift);
            break;
        }
        verify(actual, reference);
        check(actual.count_and(other) == (reference & other_reference).count(),
              "count_and");
        check(actual.intersects(other) == (reference & other_reference).any(),
              "intersects");
        check(actual.count_and(actual) == reference.count(), "self count_and");
        check(actual.intersects(actual) == reference.any(), "self intersects");
        check((actual == other) == (reference == other_reference), "equal");
        check((actual != other) == (reference != other_reference), "not equal");
        check(actual == actual, "self equal");
        check(!(actual != actual), "self not equal");
        if (iteration % 16 == 0) {
            verify(actual & other, reference & other_reference);
            verify(actual | other, reference | other_reference);
            verify(actual ^ other, reference ^ other_reference);
            verify(~actual, ~reference);
        }
    }
}

template <class... Bits>
concept HasCountAnd = requires(const Bits&... bits) {
    { titan23::count_and(bits...) } -> std::same_as<std::size_t>;
};

template <class First, class... Rest>
concept HasMemberCountAnd = requires(const First& first, const Rest&... rest) {
    { first.count_and(rest...) } -> std::same_as<std::size_t>;
};

template <class... Bits>
concept HasBitOr = requires(const Bits&... bits) { titan23::bit_or(bits...); };

template <class... Bits>
concept HasAnyXor = requires(const Bits&... bits) {
    { titan23::any_xor(bits...) } -> std::same_as<bool>;
};

template <class... Bits>
concept HasAllAnd = requires(const Bits&... bits) {
    { titan23::all_and(bits...) } -> std::same_as<bool>;
};

using CountCheckBitset = titan23::Bitset<65>;
using CountCheckDifferentSize = titan23::Bitset<64>;
using CountCheckDifferentBackend = titan23::Bitset<65, titan23::BitsetBackend::Scalar>;
static_assert(!HasCountAnd<>);
static_assert(!HasCountAnd<CountCheckBitset>);
static_assert(HasCountAnd<CountCheckBitset, CountCheckBitset>);
static_assert(!HasCountAnd<CountCheckBitset, CountCheckDifferentSize>);
static_assert(!HasCountAnd<CountCheckBitset, CountCheckBitset,
                          CountCheckDifferentSize>);
static_assert(!HasCountAnd<CountCheckBitset, CountCheckDifferentBackend>);
static_assert(!HasCountAnd<CountCheckBitset, CountCheckBitset,
                          CountCheckDifferentBackend>);
static_assert(!HasCountAnd<CountCheckBitset, int>);
static_assert(!HasMemberCountAnd<CountCheckBitset>);
static_assert(HasMemberCountAnd<CountCheckBitset, CountCheckBitset>);
static_assert(!HasMemberCountAnd<CountCheckBitset, CountCheckDifferentSize>);
static_assert(!HasMemberCountAnd<CountCheckBitset, CountCheckBitset,
                                CountCheckDifferentSize>);
static_assert(!HasMemberCountAnd<CountCheckBitset, CountCheckDifferentBackend>);
static_assert(!HasMemberCountAnd<CountCheckBitset, CountCheckBitset,
                                CountCheckDifferentBackend>);
static_assert(!HasMemberCountAnd<CountCheckBitset, int>);
static_assert(HasBitOr<CountCheckBitset, CountCheckBitset>);
static_assert(!HasBitOr<> && !HasBitOr<CountCheckBitset>);
static_assert(!HasBitOr<CountCheckBitset, CountCheckDifferentSize>);
static_assert(!HasBitOr<CountCheckBitset, CountCheckBitset,
                       CountCheckDifferentBackend>);
static_assert(HasAnyXor<CountCheckBitset, CountCheckBitset>);
static_assert(!HasAnyXor<> && !HasAnyXor<CountCheckBitset>);
static_assert(!HasAnyXor<CountCheckBitset, CountCheckBitset,
                        CountCheckDifferentSize>);
static_assert(!HasAnyXor<CountCheckBitset, CountCheckDifferentBackend>);
static_assert(HasAllAnd<CountCheckBitset, CountCheckBitset>);
static_assert(!HasAllAnd<> && !HasAllAnd<CountCheckBitset>);
static_assert(!HasAllAnd<CountCheckBitset, CountCheckDifferentSize>);
static_assert(!HasAllAnd<CountCheckBitset, CountCheckBitset,
                        CountCheckDifferentBackend>);

template <std::size_t N, titan23::BitsetBackend Backend, std::size_t... I>
void check_bitwise_pack(const std::array<titan23::Bitset<N, Backend>, 8>& bits,
                        const std::array<std::bitset<N>, 8>& references,
                        std::index_sequence<I...>) {
    constexpr std::size_t operands = sizeof...(I) + 1;
    std::bitset<N> expected_and = references[0];
    std::bitset<N> expected_or = references[0];
    std::bitset<N> expected_xor = references[0];
    ((expected_and &= references[I + 1]), ...);
    ((expected_or |= references[I + 1]), ...);
    ((expected_xor ^= references[I + 1]), ...);
    const std::size_t count = expected_and.count();
    check(titan23::count_and(bits[0], bits[I + 1]...) == count,
          "qualified variadic count_and");
    check(count_and(bits[0], bits[I + 1]...) == count,
          "ADL variadic count_and");
    check(bits[0].count_and(bits[I + 1]...) == count,
          "member variadic count_and");
    check(titan23::count_and(bits[operands - 1], bits[operands - 2 - I]...)
              == count,
          "count_and operand order");
    check(titan23::count_and(bits[0], (static_cast<void>(I), bits[0])...)
              == references[0].count(),
          "count_and repeated operand");
    check(titan23::count_or(bits[0], bits[I + 1]...) == expected_or.count(),
          "variadic count_or");
    check(titan23::count_xor(bits[0], bits[I + 1]...) == expected_xor.count(),
          "variadic count_xor");
    check(titan23::any_and(bits[0], bits[I + 1]...) == expected_and.any(),
          "variadic any_and");
    check(titan23::any_or(bits[0], bits[I + 1]...) == expected_or.any(),
          "variadic any_or");
    check(any_xor(bits[0], bits[I + 1]...) == expected_xor.any(),
          "ADL variadic any_xor");
    check(all_and(bits[0], bits[I + 1]...) == expected_and.all(),
          "ADL variadic all_and");
    check(titan23::all_or(bits[0], bits[I + 1]...) == expected_or.all(),
          "variadic all_or");
    check(titan23::all_xor(bits[0], bits[I + 1]...) == expected_xor.all(),
          "variadic all_xor");
    verify(titan23::bit_and(bits[0], bits[I + 1]...), expected_and);
    verify(titan23::bit_or(bits[0], bits[I + 1]...), expected_or);
    verify(titan23::bit_xor(bits[0], bits[I + 1]...), expected_xor);
    verify(titan23::bit_or(bits[operands - 1], bits[operands - 2 - I]...),
           expected_or);
    verify(titan23::bit_xor(bits[operands - 1], bits[operands - 2 - I]...),
           expected_xor);
    verify(titan23::bit_and(bits[0], (static_cast<void>(I), bits[0])...),
           references[0]);
    verify(titan23::bit_or(bits[0], (static_cast<void>(I), bits[0])...),
           references[0]);
    const std::bitset<N> repeated_xor = operands % 2
        ? references[0] : std::bitset<N>{};
    verify(titan23::bit_xor(bits[0], (static_cast<void>(I), bits[0])...),
           repeated_xor);
    check(titan23::count_xor(bits[0], (static_cast<void>(I), bits[0])...)
              == repeated_xor.count(),
          "count_xor repeated operand parity");
}

template <std::size_t N, titan23::BitsetBackend Backend>
void variadic_bitwise_operations() {
    current_size = N;
    current_backend = static_cast<int>(Backend);
    phase = "variadic bitwise operations";
    std::mt19937_64 rng(0x37d6d1c97ULL + N);
    for (int iteration = 0; iteration < 96; ++iteration) {
        std::array<titan23::Bitset<N, Backend>, 8> bits;
        std::array<std::bitset<N>, 8> references;
        for (std::size_t i = 0; i < bits.size(); ++i) {
            auto [actual, reference] = random_pair<N, Backend>(rng);
            if (iteration >= 1 && iteration <= 18) {
                actual.set();
                reference.set();
            }
            if (iteration == 2 ||
                (iteration >= 3 && iteration <= 10 &&
                 i == static_cast<std::size_t>(iteration - 3))) {
                actual.reset();
                reference.reset();
            }
            if constexpr (N > 0) {
                if (iteration >= 11 && iteration <= 18 &&
                    i == static_cast<std::size_t>(iteration - 11)) {
                    actual.reset(N - 1);
                    reference.reset(N - 1);
                }
            }
            bits[i] = actual;
            references[i] = reference;
        }
        const auto before = bits;
        check_bitwise_pack(bits, references, std::make_index_sequence<1>{});
        check_bitwise_pack(bits, references, std::make_index_sequence<2>{});
        check_bitwise_pack(bits, references, std::make_index_sequence<3>{});
        check_bitwise_pack(bits, references, std::make_index_sequence<7>{});
        check(titan23::count_and(bits[0], bits[1], bits[0], bits[1]) ==
                  (references[0] & references[1]).count(),
              "count_and repeated pair");
        check(titan23::count(bits[0]) == references[0].count(), "free count");
        check(titan23::any(bits[0]) == references[0].any(), "free any");
        check(titan23::all(bits[0]) == references[0].all(), "free all");
        check(titan23::none(bits[0]) == references[0].none(), "free none");
        check(count(bits[0]) == references[0].count(), "ADL count");
        check(titan23::any(bits[0]) == references[0].any(), "qualified any");
        check(all(bits[0]) == references[0].all(), "ADL all");
        check(none(bits[0]) == references[0].none(), "ADL none");
        check(bits == before, "free algorithms leave inputs unchanged");
        for (std::size_t i = 0; i < bits.size(); ++i) verify(bits[i], references[i]);
    }
}

template <std::size_t N, titan23::BitsetBackend Backend>
void run_size() {
    current_size = N;
    current_backend = static_cast<int>(Backend);
    std::mt19937_64 rng(0x9c41f7c42ULL + N);
    construction_and_boundaries<N, Backend>(rng);
    random_operations<N, Backend>(rng);
}

template <titan23::BitsetBackend Backend>
void run_backend() {
    run_size<0, Backend>();
    run_size<1, Backend>();
    run_size<63, Backend>();
    run_size<64, Backend>();
    run_size<65, Backend>();
    run_size<127, Backend>();
    run_size<128, Backend>();
    run_size<129, Backend>();
    run_size<255, Backend>();
    run_size<256, Backend>();
    run_size<257, Backend>();
    run_size<511, Backend>();
    run_size<512, Backend>();
    run_size<513, Backend>();
    run_size<959, Backend>();
    run_size<960, Backend>();
    run_size<961, Backend>();
    run_size<1984, Backend>();
    run_size<1985, Backend>();
    run_size<4096, Backend>();
    run_size<4097, Backend>();
    variadic_bitwise_operations<0, Backend>();
    variadic_bitwise_operations<65, Backend>();
    variadic_bitwise_operations<257, Backend>();
    variadic_bitwise_operations<961, Backend>();
    variadic_bitwise_operations<1985, Backend>();
    variadic_bitwise_operations<4097, Backend>();
}

} // namespace

int main() {
    run_backend<titan23::BitsetBackend::Auto>();
    run_backend<titan23::BitsetBackend::Scalar>();
#if defined(__AVX2__)
    run_backend<titan23::BitsetBackend::Avx2>();
#endif
    std::cout << "Bitset differential tests passed\n";
}
