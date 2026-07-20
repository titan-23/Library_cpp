#include <algorithm>
#include <cassert>
#include <cstdint>
#include <iostream>
#include <random>
#include <vector>

#include "titan_cpplib/ds/b_tree_bit_vector_sum.cpp"

using namespace std;

namespace {

using Weight = long long;
using BitVector = titan23::BTreeBitVectorSum<Weight>;

int rank1(const vector<uint8_t> &bits, const int r) {
    int result = 0;
    for (int i = 0; i < r; ++i) result += bits[i];
    return result;
}

int select_bit(const vector<uint8_t> &bits, int k, const bool bit) {
    for (int i = 0; i < static_cast<int>(bits.size()); ++i) {
        if (bits[i] == bit && k-- == 0) return i;
    }
    return -1;
}

int min_count_sum_ge(const vector<uint8_t> &bits, const vector<Weight> &weights, const int l, const int r, const bool bit, Weight target) {
    if (target <= 0) return 0;
    int count = 0;
    for (int i = l; i < r; ++i) {
        if (bits[i] != bit) continue;
        ++count;
        target -= weights[i];
        if (target <= 0) return count;
    }
    return -1;
}

void verify(const BitVector &tree, const BitVector::Sequence &sequence, const vector<uint8_t> &bits, const vector<Weight> &weights, mt19937_64 &rng) {
    assert(tree.len(sequence) == static_cast<int>(bits.size()));
    const auto [actual_bits, actual_weights] = tree.tovector(sequence);
    assert(actual_bits == bits);
    assert(actual_weights == weights);
    Weight all_sum = 0;
    for (const Weight weight : weights) all_sum += weight;
    assert(tree.all_sum(sequence) == all_sum);

    for (int query = 0; query < 100; ++query) {
        int l = static_cast<int>(rng() % (bits.size() + 1));
        int r = static_cast<int>(rng() % (bits.size() + 1));
        if (l > r) swap(l, r);
        if (bits.empty()) continue;

        const auto data = tree.range_data(sequence, l, r);
        int l0 = 0;
        int r0 = 0;
        Weight sum0 = 0;
        Weight sum1 = 0;
        for (int i = 0; i < l; ++i) l0 += !bits[i];
        r0 = l0;
        for (int i = l; i < r; ++i) {
            r0 += !bits[i];
            if (bits[i]) {
                sum1 += weights[i];
            } else {
                sum0 += weights[i];
            }
        }
        assert(data.l0 == l0);
        assert(data.r0 == r0);
        assert(data.sum0 == sum0);
        assert(data.sum1 == sum1);

        const int k = static_cast<int>(rng() % bits.size());
        const auto access = tree.access(sequence, k);
        assert(access.bit == bits[k]);
        assert(access.rank1 == rank1(bits, k));
        assert(access.weight == weights[k]);

        const bool bit = rng() & 1;
        const int bit_count = count(bits.begin(), bits.end(), bit);
        if (bit_count > 0) {
            const int occurrence = static_cast<int>(rng() % bit_count);
            assert(tree.select(sequence, occurrence, bit) == select_bit(bits, occurrence, bit));
        }

        const Weight branch_sum = bit ? sum1 : sum0;
        const Weight target = static_cast<Weight>(rng() % (branch_sum + 5));
        assert(tree.min_count_sum_ge(sequence, l, r, bit, target) == min_count_sum_ge(bits, weights, l, r, bit, target));
    }
}

void run(const uint64_t seed) {
    mt19937_64 rng(seed);
    vector<uint8_t> bits(200);
    vector<Weight> weights(200);
    for (int i = 0; i < 200; ++i) {
        bits[i] = rng() & 1;
        weights[i] = rng() % 20;
    }
    vector<int> order(bits.size());
    for (int i = 0; i < static_cast<int>(order.size()); ++i) order[i] = i;

    BitVector tree;
    auto sequence = tree.build(bits, order, weights, 0, bits.size());
    for (int query = 0; query < 30000; ++query) {
        int operation = rng() % 8;
        if (bits.empty()) operation = 0;
        if (bits.size() >= 500 && operation == 0) operation = 1;

        if (operation == 0) {
            const int k = rng() % (bits.size() + 1);
            const bool bit = rng() & 1;
            const Weight weight = rng() % 20;
            assert(tree.insert(sequence, k, bit, weight) == rank1(bits, k));
            bits.insert(bits.begin() + k, bit);
            weights.insert(weights.begin() + k, weight);
        } else if (operation == 1) {
            const int k = rng() % bits.size();
            const auto result = tree.pop(sequence, k);
            assert(result.bit == bits[k]);
            assert(result.rank1 == rank1(bits, k));
            assert(result.weight == weights[k]);
            bits.erase(bits.begin() + k);
            weights.erase(weights.begin() + k);
        } else if (operation == 2) {
            const int k = rng() % bits.size();
            const bool bit = rng() & 1;
            const Weight weight = rng() % 20;
            const auto result = tree.set(sequence, k, bit, weight);
            assert(result.bit == bits[k]);
            assert(result.rank1 == rank1(bits, k));
            assert(result.weight == weights[k]);
            bits[k] = bit;
            weights[k] = weight;
        } else if (operation == 3) {
            const bool bit = rng() & 1;
            const int bit_count = count(bits.begin(), bits.end(), bit);
            if (bit_count > 0) {
                const int occurrence = rng() % bit_count;
                const int position = select_bit(bits, occurrence, bit);
                assert(tree.select_pop(sequence, occurrence, bit) == position);
                bits.erase(bits.begin() + position);
                weights.erase(weights.begin() + position);
            }
        } else if (operation == 4) {
            const int k = rng() % bits.size();
            const Weight delta = rng() % 20;
            const auto result = tree.add_weight(sequence, k, delta);
            assert(result.bit == bits[k]);
            assert(result.rank1 == rank1(bits, k));
            assert(result.weight == weights[k]);
            weights[k] += delta;
        }

        if (query % 131 == 0) verify(tree, sequence, bits, weights, rng);
    }
    verify(tree, sequence, bits, weights, rng);
}

void stress_rebalance() {
    BitVector tree;
    BitVector::Sequence sequence;
    vector<uint8_t> bits;
    vector<Weight> weights;
    for (int i = 0; i < 20000; ++i) {
        tree.insert(sequence, tree.len(sequence), i & 1, i % 17);
        bits.emplace_back(i & 1);
        weights.emplace_back(i % 17);
    }
    for (int i = 0; i < 10000; ++i) {
        const int k = (i * 7919) % bits.size();
        const auto result = tree.pop(sequence, k);
        assert(result.bit == bits[k]);
        assert(result.weight == weights[k]);
        bits.erase(bits.begin() + k);
        weights.erase(weights.begin() + k);
    }
    const auto [actual_bits, actual_weights] = tree.tovector(sequence);
    assert(actual_bits == bits);
    assert(actual_weights == weights);
}

} // namespace

int main() {
    for (uint64_t seed = 0; seed < 8; ++seed) run(seed * 1000003 + 71);
    stress_rebalance();
    cout << "B-tree bit vector sum random test: OK" << '\n';
    return 0;
}
