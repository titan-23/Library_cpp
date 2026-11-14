#include <algorithm>
#include <cassert>
#include <cstdint>
#include <iostream>
#include <numeric>
#include <random>
#include <vector>

#include "titan_cpplib/ds/b_tree_sum_sequence.cpp"

using namespace std;

namespace {

using Weight = long long;
using Pool = titan23::BTreeSumSequence<Weight>;

// 1 つの領域に複数の列を置き、各操作を素朴な配列と比べる
void run(const uint64_t seed, const int sequence_count, const int max_size, const int steps) {
    mt19937_64 rng(seed);
    auto random_int = [&](const int lo, const int hi) { return lo + int(rng() % uint64_t(hi - lo + 1)); };
    auto random_weight = [&]() { return Weight(rng() % 2000001) - 1000000; };

    Pool pool;
    vector<Pool::Sequence> sequences(sequence_count);
    vector<vector<Weight>> naive(sequence_count);
    for (int s = 0; s < sequence_count; ++s) {
        const int n = random_int(0, max_size / 2);
        naive[s].resize(n);
        for (Weight &w : naive[s]) w = random_weight();
        vector<int> order(n);
        iota(order.begin(), order.end(), 0);
        sequences[s] = pool.build(order, naive[s], 0, n);
    }

    auto check_all = [&](const int s) {
        const vector<Weight> &a = naive[s];
        assert(pool.len(sequences[s]) == int(a.size()));
        assert(pool.empty(sequences[s]) == a.empty());
        assert(pool.tovector(sequences[s]) == a);
        assert(pool.all_sum(sequences[s]) == accumulate(a.begin(), a.end(), Weight(0)));
    };

    for (int step = 0; step < steps; ++step) {
        const int s = random_int(0, sequence_count - 1);
        vector<Weight> &a = naive[s];
        Pool::Sequence &seq = sequences[s];
        const int n = a.size();
        const int type = random_int(0, 7);
        if (type <= 1 && n < max_size) {
            const int k = random_int(0, n);
            const Weight w = random_weight();
            pool.insert(seq, k, w);
            a.insert(a.begin() + k, w);
        } else if (type == 2 && n > 0) {
            const int k = random_int(0, n - 1);
            assert(pool.pop(seq, k) == a[k]);
            a.erase(a.begin() + k);
        } else if (type == 3 && n > 0) {
            const int k = random_int(0, n - 1);
            const Weight d = random_weight();
            assert(pool.add(seq, k, d) == a[k]);
            a[k] += d;
        } else if (type == 4 && n > 0) {
            const int k = random_int(0, n - 1);
            const Weight w = random_int(0, 3) == 0 ? a[k] : random_weight();
            assert(pool.set(seq, k, w) == a[k]);
            a[k] = w;
        } else if (type == 5 && n > 0) {
            const int k = random_int(0, n - 1);
            assert(pool.access(seq, k) == a[k]);
        } else if (type == 6) {
            const int r = random_int(0, n);
            assert(pool.prefix_sum(seq, r) == accumulate(a.begin(), a.begin() + r, Weight(0)));
        } else {
            int l = random_int(0, n), r = random_int(0, n);
            if (l > r) swap(l, r);
            assert(pool.range_sum(seq, l, r) == accumulate(a.begin() + l, a.begin() + r, Weight(0)));
        }
        if (step % 997 == 0) check_all(s);
    }
    for (int s = 0; s < sequence_count; ++s) check_all(s);

    // 全要素を削除してから作り直し、解放した節点の再利用を確かめる
    for (int s = 0; s < sequence_count; ++s) {
        while (!naive[s].empty()) {
            const int k = random_int(0, int(naive[s].size()) - 1);
            assert(pool.pop(sequences[s], k) == naive[s][k]);
            naive[s].erase(naive[s].begin() + k);
        }
        assert(pool.empty(sequences[s]));
        assert(pool.range_sum(sequences[s], 0, 0) == 0);
    }
    for (int s = 0; s < sequence_count; ++s) {
        const int n = random_int(0, max_size);
        naive[s].resize(n);
        for (Weight &w : naive[s]) w = random_weight();
        vector<int> order(n);
        iota(order.begin(), order.end(), 0);
        shuffle(order.begin(), order.end(), rng);
        vector<Weight> expected(n);
        for (int i = 0; i < n; ++i) expected[i] = naive[s][order[i]];
        sequences[s] = pool.build(order, naive[s], 0, n);
        naive[s] = expected;
        check_all(s);
    }
}

}  // namespace

int main() {
    // 小さい列では葉 1 つの場合を、大きい列では分割と併合を多く通す
    run(1, 8, 40, 200000);
    run(2, 4, 3000, 300000);
    run(3, 2, 20000, 200000);
    cout << "B-tree sum sequence random test: OK" << endl;
    return 0;
}
