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

template<class BitVector>
void verify(const BitVector &bit_vector, const vector<uint8_t> &expected) {
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
    assert(bit_vector._rank0_pair(0, 0) == make_pair(0, 0));
    assert(bit_vector._rank0_pair(n, n) == make_pair(n - ones, n - ones));
    assert(bit_vector._rank0_pair(0, n) == make_pair(0, n - ones));
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
    const vector<int> sizes = {
        0, 1, 2, 63, 64, 126, 127, 128, 129, 507, 508, 509,
        1015, 1016, 1017, 2032, 4063, 4064, 4065, 8127, 8128, 8129,
        12191, 12192, 12193, 16255, 16256, 16257, 20000, 300000
    };
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

void test_small_patterns() {
    // Every bit position is exercised on both sides of the 64-bit boundary.
    for (int n = 0; n <= 128; ++n) {
        for (const uint8_t background : {0, 1}) {
            vector<uint8_t> a(n, background);
            verify(titan23::BTreeBitVector(a), a);
            for (int p = 0; p < n; ++p) {
                a[p] ^= 1;
                verify(titan23::BTreeBitVector(a), a);
                a[p] ^= 1;
            }
        }
    }

    // Exhaust all paired endpoints separately from the larger pattern sweep.
    for (const int n : {0, 1, 63, 64, 65, 126, 127, 128, 129}) {
        vector<uint8_t> a(n);
        vector<int> zeros(n + 1);
        for (int i = 0; i < n; ++i) {
            a[i] = (i % 3 == 0);
            zeros[i + 1] = zeros[i] + !a[i];
        }
        titan23::BTreeBitVector bit_vector(a);
        for (int mode = 0; mode < 2; ++mode) {
            if (mode == 1) bit_vector.reserve(4096);
            for (int l = 0; l <= n; ++l) {
                for (int r = l; r <= n; ++r) {
                    assert(bit_vector._rank0_pair(l, r) == make_pair(zeros[l], zeros[r]));
                }
            }
        }
    }
}

void verify_value_semantics(const titan23::BTreeBitVector &original, const vector<uint8_t> &expected) {
    titan23::BTreeBitVector copied(original);
    verify(copied, expected);
    if (!expected.empty()) {
        copied.set(0, !expected[0]);
        assert(original.access(0) == static_cast<bool>(expected[0]));
        copied.set(0, expected[0]);
    }
    titan23::BTreeBitVector assigned(vector<uint8_t>(256, 1));
    assigned = original;
    verify(assigned, expected);
    titan23::BTreeBitVector moved(move(copied));
    verify(moved, expected);
    titan23::BTreeBitVector move_assigned(vector<uint8_t>(256, 0));
    move_assigned = move(assigned);
    verify(move_assigned, expected);
    // Both small and tree moved-from objects must remain usable.
    copied.clear();
    assigned.clear();
    copied.insert(0, true);
    assigned.insert(0, false);
    verify(copied, vector<uint8_t>{1});
    verify(assigned, vector<uint8_t>{0});
}

void test_reserved_small_transitions() {
    for (const int n : {0, 1, 63, 64, 65, 126, 127, 128}) {
        vector<uint8_t> expected(n);
        for (int i = 0; i < n; ++i) expected[i] = (i % 3 == 1);
        titan23::BTreeBitVector bit_vector(expected);
        bit_vector.reserve(4096);
        verify(bit_vector, expected);
        verify_value_semantics(bit_vector, expected);

        while (expected.size() < 129) {
            const int k = expected.size() / 2;
            const bool bit = (expected.size() % 3 == 0);
            const int rank1 = prefix_ones(expected, k);
            const int result = bit_vector._insert_and_rank1(k, bit);
            assert(result == rank1);
            expected.insert(expected.begin() + k, bit);
            verify(bit_vector, expected);
        }
        verify_value_semantics(bit_vector, expected);
        while (!expected.empty()) {
            const int k = expected.size() / 2;
            const bool bit = expected[k];
            const int rank1 = prefix_ones(expected, k);
            if (expected.size() & 1) {
                const int result = bit_vector._access_pop_and_rank1(k);
                assert(result == ((rank1 << 1) | bit));
            } else {
                const int occurrence = bit ? rank1 : k - rank1;
                const int result = bit_vector._select_pop(occurrence, bit);
                assert(result == k);
            }
            expected.erase(expected.begin() + k);
            verify(bit_vector, expected);
            if (expected.size() == 128 || expected.size() == 127 || expected.size() == 64 || expected.empty()) {
                verify_value_semantics(bit_vector, expected);
            }
        }
        bit_vector.reserve(4096);
        verify(bit_vector, expected);
        bit_vector.insert(0, true);
        verify(bit_vector, vector<uint8_t>{1});
        const bool removed = bit_vector.pop(0);
        assert(removed);
        verify(bit_vector, expected);
    }
}

void test_core_lifecycle() {
    titan23::BTreeBitVectorCore core;
    vector<uint8_t> expected;
    verify(core, expected);
    core.reserve(0);
    verify(core, expected);
    core.clear();
    verify(core, expected);

    // The wrapper demotes before reaching zero. Exercise Core's own empty root,
    // free-list reuse, and recreation of its pools after clear directly.
    for (const int n : {10000, 3000, 1000}) {
        if (n == 1000) {
            core.clear();
            verify(core, expected);
        }
        core.reserve(n);
        for (int i = 0; i < n; ++i) {
            const int k = i % 3 == 0 ? expected.size() / 2 : expected.size();
            const bool bit = (i % 5 < 2);
            core.insert(k, bit);
            expected.insert(expected.begin() + k, bit);
        }
        verify(core, expected);
        titan23::BTreeBitVectorCore copied(core);
        verify(copied, expected);

        for (int i = 0; !expected.empty(); ++i) {
            const int k = i % 3 == 0 ? 0 : i % 3 == 1 ? expected.size() / 2 : expected.size() - 1;
            const bool removed = core.pop(k);
            assert(removed == static_cast<bool>(expected[k]));
            expected.erase(expected.begin() + k);
            if (expected.size() % 997 == 0) verify(core, expected);
        }
        verify(core, expected);
        titan23::BTreeBitVectorCore empty_copy(core);
        verify(empty_copy, expected);
        empty_copy.insert(0, true);
        verify(empty_copy, vector<uint8_t>{1});
    }
    core.clear();
    core.clear();
    verify(core, expected);
}

} // namespace

int main() {
    mt19937_64 rng(0x9e3779b97f4a7c15ULL);
    test_build(rng);
    test_small_mode();
    test_small_patterns();
    test_reserved_small_transitions();
    test_core_lifecycle();
    test_split_and_merge();
    for (uint64_t seed = 0; seed < 8; ++seed) test_random_operations(seed * 1000003 + 17);
    cout << "BTreeBitVector random test: OK" << '\n';
    return 0;
}
