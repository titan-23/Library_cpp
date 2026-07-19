#include <algorithm>
#include <cassert>
#include <cstdint>
#include <iostream>
#include <random>
#include <vector>

#include "titan_cpplib/ds/avl_tree_bit_vector.cpp"

using namespace std;

namespace {

void verify(
    const titan23::AVLTreeBitVector &bit_vector,
    const vector<uint8_t> &expected
) {
    assert(bit_vector.len() == static_cast<int>(expected.size()));
    assert(bit_vector.empty() == expected.empty());
    assert(bit_vector.tovector() == expected);

    int ones = 0;
    vector<int> zero_positions;
    vector<int> one_positions;
    vector<int> zero_prefix(expected.size() + 1);
    for (int i = 0; i < static_cast<int>(expected.size()); ++i) zero_prefix[i + 1] = zero_prefix[i] + !expected[i];
    for (int i = 0; i < static_cast<int>(expected.size()); ++i) {
        assert(bit_vector.rank0(i) == i - ones);
        assert(bit_vector.rank1(i) == ones);
        assert(bit_vector.rank(i, false) == i - ones);
        assert(bit_vector.rank(i, true) == ones);
        assert(bit_vector.access(i) == static_cast<bool>(expected[i]));

        const auto [bit, rank1] = bit_vector._access_ans_rank1(i);
        assert(bit == static_cast<bool>(expected[i]));
        assert(rank1 == ones);
        const int r = i + (expected.size() - i) / 2;
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

    const int n = static_cast<int>(expected.size());
    assert(bit_vector.rank0(n) == n - ones);
    assert(bit_vector.rank1(n) == ones);
    for (int k = 0; k < static_cast<int>(zero_positions.size()); ++k) {
        assert(bit_vector.select0(k) == zero_positions[k]);
        assert(bit_vector.select(k, false) == zero_positions[k]);
    }
    for (int k = 0; k < static_cast<int>(one_positions.size()); ++k) {
        assert(bit_vector.select1(k) == one_positions[k]);
        assert(bit_vector.select(k, true) == one_positions[k]);
    }
}

int prefix_ones(const vector<uint8_t> &a, const int r) {
    int result = 0;
    for (int i = 0; i < r; ++i) result += a[i];
    return result;
}

void test_build(mt19937_64 &rng) {
    for (int n = 0; n <= 600; ++n) {
        vector<uint8_t> a(n);
        for (uint8_t &bit : a) bit = rng() & 1;
        const titan23::AVLTreeBitVector bit_vector(a);
        verify(bit_vector, a);

        vector<uint8_t> padded(n + 7);
        for (uint8_t &bit : padded) bit = rng() & 1;
        copy(a.begin(), a.end(), padded.begin() + 3);
        const titan23::AVLTreeBitVector range_bit_vector(padded, 3, n + 3);
        verify(range_bit_vector, a);
    }
}

void test_random_operations(const uint64_t seed) {
    mt19937_64 rng(seed);
    titan23::AVLTreeBitVector bit_vector;
    bit_vector.reserve(5000);
    vector<uint8_t> expected;

    for (int query = 0; query < 100000; ++query) {
        int operation = rng() % 8;
        if (expected.empty()) operation = 0;
        if (expected.size() >= 2500 && operation <= 1) operation = 2;

        if (operation == 0) {
            const int k = rng() % (expected.size() + 1);
            const bool bit = rng() & 1;
            const int rank1 = prefix_ones(expected, k);
            assert(bit_vector._insert_and_rank1(k, bit) == rank1);
            expected.insert(expected.begin() + k, bit);
        } else if (operation == 1) {
            const int k = rng() % (expected.size() + 1);
            const bool bit = rng() & 1;
            bit_vector.insert(k, bit);
            expected.insert(expected.begin() + k, bit);
        } else if (operation == 2) {
            const int k = rng() % expected.size();
            const int rank1 = prefix_ones(expected, k);
            const bool bit = expected[k];
            const int result = bit_vector._access_pop_and_rank1(k);
            assert((result >> 1) == rank1);
            assert((result & 1) == bit);
            expected.erase(expected.begin() + k);
        } else if (operation == 3) {
            const int k = rng() % expected.size();
            const bool bit = expected[k];
            assert(bit_vector.pop(k) == bit);
            expected.erase(expected.begin() + k);
        } else if (operation == 4) {
            const int k = rng() % expected.size();
            const bool bit = rng() & 1;
            bit_vector.set(k, bit);
            expected[k] = bit;
        } else if (operation == 5) {
            const int k = rng() % expected.size();
            const int rank1 = prefix_ones(expected, k);
            const auto [bit, got_rank1] =
                bit_vector._access_ans_rank1(k);
            assert(bit == static_cast<bool>(expected[k]));
            assert(got_rank1 == rank1);
        } else if (operation == 6) {
            bool bit = rng() & 1;
            int count = bit ? prefix_ones(expected, expected.size()) : static_cast<int>(expected.size()) - prefix_ones(expected, expected.size());
            if (count == 0) {
                bit = !bit;
                count = expected.size();
            }
            const int occurrence = rng() % count;
            int position = -1;
            for (int i = 0, seen = 0; i < static_cast<int>(expected.size()); ++i) {
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

        if (query % 211 == 0) verify(bit_vector, expected);
    }
    verify(bit_vector, expected);

    while (!expected.empty()) {
        const int k = rng() % expected.size();
        const int rank1 = prefix_ones(expected, k);
        const bool bit = expected[k];
        const int result = bit_vector._access_pop_and_rank1(k);
        assert((result >> 1) == rank1);
        assert((result & 1) == bit);
        expected.erase(expected.begin() + k);
        if (expected.size() % 173 == 0) verify(bit_vector, expected);
    }
    verify(bit_vector, expected);
}

void test_adversarial_positions() {
    titan23::AVLTreeBitVector bit_vector;
    vector<uint8_t> expected;

    for (int i = 0; i < 5000; ++i) {
        const bool bit = i & 1;
        const int k = i % 3 == 0 ? 0 : static_cast<int>(expected.size());
        bit_vector.insert(k, bit);
        expected.insert(expected.begin() + k, bit);
    }
    verify(bit_vector, expected);

    bool take_front = true;
    while (!expected.empty()) {
        const int k = take_front ? 0 : static_cast<int>(expected.size()) - 1;
        assert(bit_vector.pop(k) == static_cast<bool>(expected[k]));
        expected.erase(expected.begin() + k);
        take_front = !take_front;
        if (expected.size() % 191 == 0) verify(bit_vector, expected);
    }
    verify(bit_vector, expected);

    bit_vector.clear();
    expected.clear();
    verify(bit_vector, expected);
    bit_vector.insert(0, true);
    expected.emplace_back(true);
    verify(bit_vector, expected);
}

} // namespace

int main() {
    mt19937_64 rng(0x9e3779b97f4a7c15ULL);
    test_build(rng);
    test_adversarial_positions();
    for (uint64_t seed = 0; seed < 12; ++seed) {
        test_random_operations(seed * 1000003 + 17);
    }
    cout << "AVLTreeBitVector random test: OK" << '\n';
    return 0;
}
