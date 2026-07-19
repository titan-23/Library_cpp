#include <algorithm>
#include <cassert>
#include <cstdint>
#include <iostream>
#include <random>
#include <utility>
#include <vector>

#include "titan_cpplib/ds/b_tree_bit_vector.cpp"

using namespace std;

namespace {

int prefix_ones(const vector<uint8_t> &a, const int r) {
    int result = 0;
    for (int i = 0; i < r; ++i) result += a[i];
    return result;
}

void verify(const titan23::BTreeBitVector &bit_vector, const vector<uint8_t> &expected) {
    const int n = expected.size();
    assert(bit_vector.len() == n);
    assert(bit_vector.empty() == expected.empty());
    assert(bit_vector.tovector() == expected);

    int ones = 0;
    vector<int> zero_positions;
    vector<int> one_positions;
    vector<int> zero_prefix(n + 1);
    for (int i = 0; i < n; ++i) zero_prefix[i + 1] = zero_prefix[i] + !expected[i];
    for (int i = 0; i < n; ++i) {
        assert(bit_vector.rank0(i) == i - ones);
        assert(bit_vector.rank1(i) == ones);
        assert(bit_vector.rank(i, false) == i - ones);
        assert(bit_vector.rank(i, true) == ones);
        assert(bit_vector.access(i) == static_cast<bool>(expected[i]));
        const auto [bit, rank1] = bit_vector._access_ans_rank1(i);
        assert(bit == static_cast<bool>(expected[i]));
        assert(rank1 == ones);
        const int r = i + (n - i) / 2;
        const auto [l0, r0] = bit_vector._rank0_pair(i, r);
        assert(l0 == zero_prefix[i]);
        assert(r0 == zero_prefix[r]);
        if (expected[i]) {
            one_positions.emplace_back(i);
            ++ones;
        } else {
            zero_positions.emplace_back(i);
        }
    }

    assert(bit_vector.rank0(n) == n - ones);
    assert(bit_vector.rank1(n) == ones);
    for (int k = 0; k < n - ones; ++k) {
        assert(bit_vector.select0(k) == zero_positions[k]);
        assert(bit_vector.select(k, false) == zero_positions[k]);
    }
    for (int k = 0; k < ones; ++k) {
        assert(bit_vector.select1(k) == one_positions[k]);
        assert(bit_vector.select(k, true) == one_positions[k]);
    }
}

void test_build(mt19937_64 &rng) {
    const vector<int> sizes = {0, 1, 2, 63, 64, 126, 127, 128, 129, 507, 508, 509, 1015, 1016, 1017, 2032, 16255, 16256, 16257, 20000};
    for (const int n : sizes) {
        vector<uint8_t> a(n);
        for (uint8_t &bit : a) bit = rng() & 1;
        const titan23::BTreeBitVector bit_vector(a);
        verify(bit_vector, a);

        vector<uint8_t> padded(n + 7);
        for (uint8_t &bit : padded) bit = rng() & 1;
        copy(a.begin(), a.end(), padded.begin() + 3);
        const titan23::BTreeBitVector range_bit_vector(padded, 3, n + 3);
        verify(range_bit_vector, a);
    }

    for (const int n : {1, 127, 1024, 20000}) {
        const vector<uint8_t> zeros(n, 0);
        const vector<uint8_t> ones(n, 1);
        verify(titan23::BTreeBitVector(zeros), zeros);
        verify(titan23::BTreeBitVector(ones), ones);
    }
}

void test_random_operations(const uint64_t seed) {
    mt19937_64 rng(seed);
    titan23::BTreeBitVector bit_vector;
    bit_vector.reserve(30000);
    vector<uint8_t> expected;

    for (int query = 0; query < 120000; ++query) {
        int operation = rng() % 9;
        if (expected.empty()) operation = 0;
        if (expected.size() >= 30000 && operation <= 2) operation = 3;

        if (operation <= 1) {
            const int k = rng() % (expected.size() + 1);
            const bool bit = rng() & 1;
            const int rank1 = prefix_ones(expected, k);
            if (operation == 0) {
                assert(bit_vector._insert_and_rank1(k, bit) == rank1);
            } else {
                bit_vector.insert(k, bit);
            }
            expected.insert(expected.begin() + k, bit);
        } else if (operation == 2) {
            const int k = rng() % (expected.size() + 1);
            const bool bit = rng() & 1;
            bit_vector.insert(k, bit);
            expected.insert(expected.begin() + k, bit);
        } else if (operation == 3) {
            const int k = rng() % expected.size();
            const int rank1 = prefix_ones(expected, k);
            const bool bit = expected[k];
            const int result = bit_vector._access_pop_and_rank1(k);
            assert((result >> 1) == rank1);
            assert((result & 1) == bit);
            expected.erase(expected.begin() + k);
        } else if (operation == 4) {
            const int k = rng() % expected.size();
            assert(bit_vector.pop(k) == static_cast<bool>(expected[k]));
            expected.erase(expected.begin() + k);
        } else if (operation == 5) {
            const int k = rng() % expected.size();
            const bool bit = rng() & 1;
            bit_vector.set(k, bit);
            expected[k] = bit;
        } else if (operation == 6) {
            const int k = rng() % expected.size();
            const int rank1 = prefix_ones(expected, k);
            const auto [bit, got_rank1] = bit_vector._access_ans_rank1(k);
            assert(bit == static_cast<bool>(expected[k]));
            assert(got_rank1 == rank1);
        } else if (operation == 7) {
            bool bit = rng() & 1;
            int count = bit ? prefix_ones(expected, expected.size()) : expected.size() - prefix_ones(expected, expected.size());
            if (count == 0) {
                bit = !bit;
                count = expected.size();
            }
            const int occurrence = rng() % count;
            int position = -1;
            const int n = expected.size();
            for (int i = 0, seen = 0; i < n; ++i) {
                if (expected[i] != bit) continue;
                if (seen++ == occurrence) {
                    position = i;
                    break;
                }
            }
            assert(bit_vector._select_pop(occurrence, bit) == position);
            expected.erase(expected.begin() + position);
        } else {
            const int k = rng() % expected.size();
            const int rank1 = prefix_ones(expected, k);
            const bool old = expected[k];
            const bool bit = rng() & 1;
            const auto [got_old, got_rank1] = bit_vector._access_set_and_rank1(k, bit);
            assert(got_old == old);
            assert(got_rank1 == rank1);
            expected[k] = bit;
        }

        if (query % 401 == 0) verify(bit_vector, expected);
    }
    verify(bit_vector, expected);

    while (!expected.empty()) {
        const int k = rng() & 1 ? 0 : expected.size() - 1;
        assert(bit_vector.pop(k) == static_cast<bool>(expected[k]));
        expected.erase(expected.begin() + k);
        if (expected.size() % 257 == 0) verify(bit_vector, expected);
    }
    verify(bit_vector, expected);

    for (int i = 0; i < 5000; ++i) {
        const bool bit = rng() & 1;
        bit_vector.insert(i, bit);
        expected.emplace_back(bit);
    }
    verify(bit_vector, expected);
}

void test_split_and_merge() {
    titan23::BTreeBitVector bit_vector;
    vector<uint8_t> expected;
    bit_vector.reserve(50000);

    for (int i = 0; i < 40000; ++i) {
        const int k = i % 4 == 0 ? 0 : expected.size();
        const bool bit = (i * 17 + 3) & 1;
        bit_vector.insert(k, bit);
        expected.insert(expected.begin() + k, bit);
    }
    verify(bit_vector, expected);

    for (int i = 0; i < 30000; ++i) {
        const int k = i % 3 == 0 ? 0 : i % 3 == 1 ? expected.size() - 1 : expected.size() / 2;
        assert(bit_vector.pop(k) == static_cast<bool>(expected[k]));
        expected.erase(expected.begin() + k);
        if (i % 997 == 0) verify(bit_vector, expected);
    }
    verify(bit_vector, expected);

    bit_vector.clear();
    expected.clear();
    verify(bit_vector, expected);
    bit_vector.insert(0, true);
    expected.emplace_back(true);
    verify(bit_vector, expected);
}

void test_small_mode() {
    mt19937_64 rng(123456789);
    titan23::BTreeBitVector bit_vector;
    vector<uint8_t> expected;

    for (int i = 0; i < 127; ++i) {
        const bool bit = rng() & 1;
        bit_vector.insert(i, bit);
        expected.emplace_back(bit);
    }
    verify(bit_vector, expected);

    for (int cycle = 0; cycle < 500; ++cycle) {
        const int k = rng() % (expected.size() + 1);
        const bool bit = rng() & 1;
        bit_vector.insert(k, bit);
        expected.insert(expected.begin() + k, bit);
        verify(bit_vector, expected);

        const int erase = rng() % expected.size();
        assert(bit_vector.pop(erase) == static_cast<bool>(expected[erase]));
        expected.erase(expected.begin() + erase);
        verify(bit_vector, expected);
    }

    titan23::BTreeBitVector small_copy = bit_vector;
    verify(small_copy, expected);
    small_copy.set(0, !expected[0]);
    assert(small_copy.access(0) != bit_vector.access(0));

    bit_vector.insert(0, true);
    expected.insert(expected.begin(), true);
    titan23::BTreeBitVector large_copy;
    large_copy = bit_vector;
    verify(large_copy, expected);
    titan23::BTreeBitVector moved = move(large_copy);
    verify(moved, expected);
}

} // namespace

int main() {
    mt19937_64 rng(0x9e3779b97f4a7c15ULL);
    test_build(rng);
    test_small_mode();
    test_split_and_merge();
    for (uint64_t seed = 0; seed < 8; ++seed) test_random_operations(seed * 1000003 + 17);
    cout << "BTreeBitVector random test: OK" << '\n';
    return 0;
}
