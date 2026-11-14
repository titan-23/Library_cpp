#include <cassert>
#include <cstdint>
#include <iostream>
#include <limits>
#include <random>
#include <unordered_set>
#include <vector>

#include "titan_cpplib/ds/hash_set.cpp"

using namespace std;

// unordered_set と操作ごとに結果を比べる
void random_test(const uint64_t seed, const int reserve, const int queries, const uint64_t key_range) {
    mt19937_64 rng(seed);
    titan23::HashSet st = reserve < 0 ? titan23::HashSet() : titan23::HashSet(reserve);
    unordered_set<uint64_t> ref;
    const vector<uint64_t> special = {
        0, 1, numeric_limits<uint64_t>::max(), uint64_t(1) << 63, (uint64_t(1) << 32) - 1,
    };
    for (int qi = 0; qi < queries; ++qi) {
        const uint64_t key = qi % 61 == 0 ? special[rng() % special.size()] : key_range == 0 ? rng() : rng() % key_range;
        const int type = rng() % 16;
        if (type < 5) {
            st.insert(key);
            ref.insert(key);
        } else if (type < 10) {
            assert(st.contains_insert(key) == !ref.insert(key).second);
        } else if (type < 15) {
            assert(st.contains(key) == ref.contains(key));
        } else if (rng() % 200 == 0) {
            st.clear();
            ref.clear();
        }
        assert(st.len() == (int)ref.size());
        assert(st.empty() == ref.empty());
    }
    for (const uint64_t key : ref) assert(st.contains(key));
}

// 下位ビットがそろった鍵や連番など、偏った鍵の列
void structured_test() {
    constexpr int N = 200000;
    auto run = [](auto key_at) {
        titan23::HashSet st;
        for (int i = 0; i < N; ++i) assert(!st.contains_insert(key_at(i)));
        for (int i = 0; i < N; ++i) assert(st.contains(key_at(i)));
        for (int i = N; i < 2 * N; ++i) assert(!st.contains(key_at(i)));
        assert(st.len() == N);
    };
    run([](uint64_t i) { return i; });
    run([](uint64_t i) { return i << 32; });
    run([](uint64_t i) { return i << 44; });
    run([](uint64_t i) { return (i + 1) * 1000000007ULL; });
    run([](uint64_t i) { return ~i; });
}

// clear のあとに同じ集合を使い回す
void clear_reuse_test() {
    mt19937_64 rng(23);
    titan23::HashSet st(1000);
    for (int round = 0; round < 300; ++round) {
        unordered_set<uint64_t> ref;
        const int m = rng() % 3000;
        for (int i = 0; i < m; ++i) {
            const uint64_t key = rng() % 4000;
            assert(st.contains_insert(key) == !ref.insert(key).second);
        }
        for (uint64_t key = 0; key < 4000; ++key) assert(st.contains(key) == ref.contains(key));
        assert(st.len() == (int)ref.size());
        st.clear();
        assert(st.empty());
    }
}

int main() {
    random_test(1, -1, 600000, 0);
    random_test(2, -1, 600000, 3000);
    random_test(3, 100000, 600000, 200000);
    random_test(4, 0, 200000, 50);
    for (uint64_t seed = 10; seed < 400; ++seed) random_test(seed, -1, 2000, 64 + seed);
    structured_test();
    clear_reuse_test();
    cout << "HashSet random test: OK" << endl;
    return 0;
}
