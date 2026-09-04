#include <algorithm>
#include <cassert>
#include <cstdint>
#include <limits>
#include <random>
#include <unordered_map>
#include <utility>
#include <vector>

#include "titan_cpplib/ds/hopscotch_hash_dict.cpp"

using namespace std;

template<bool USE_HASH_FUNC>
using Dict = titan23::HopscotchHashDict<long long, USE_HASH_FUNC>;

template<bool USE_HASH_FUNC>
void verify(const Dict<USE_HASH_FUNC> &dict, const unordered_map<uint64_t, long long> &ref) {
    assert(dict.len() == (int)ref.size());
    assert(dict.empty() == ref.empty());

    unordered_map<uint64_t, long long> got;
    for (const auto &[key, val] : dict.items()) {
        assert(got.emplace(key, val).second);
    }
    assert(got == ref);

    vector<long long> a = dict.values();
    vector<long long> b;
    b.reserve(ref.size());
    for (const auto &[key, val] : ref) {
        (void)key;
        b.push_back(val);
    }
    sort(a.begin(), a.end());
    sort(b.begin(), b.end());
    assert(a == b);

    for (const auto &[key, val] : ref) {
        assert(dict.contains(key));
        assert(dict.get(key) == val);
        assert(dict.get(key, -1) == val);
        auto dat = dict.get_pos(key);
        assert(dat.second);
        assert(dict.inner_get(dat, -1) == val);
    }
}

uint64_t make_key(mt19937_64 &rng, int step) {
    int type = (int)(rng() % 5);
    if (type == 0) return rng();
    if (type == 1) return rng() % 1000;
    if (type == 2) return uint64_t(rng() % 600) << 32;
    if (type == 3) return (uint64_t(rng() % 600) << 40) | 7;
    if (step & 1) return numeric_limits<uint64_t>::max();
    return 0;
}

template<bool USE_HASH_FUNC>
void random_test(uint64_t seed) {
    mt19937_64 rng(seed);
    Dict<USE_HASH_FUNC> dict;
    unordered_map<uint64_t, long long> ref;

    for (int step = 0; step < 150000; ++step) {
        uint64_t key = make_key(rng, step);
        long long val = int(rng() % 2001) - 1000;
        int type = (int)(rng() % 10);
        if (type == 0) {
            dict.set(key, val);
            ref[key] = val;
        } else if (type == 1) {
            dict.add(key, val);
            ref[key] += val;
        } else if (type == 2) {
            auto it = ref.find(key);
            long long expected = it == ref.end() ? 0 : it->second;
            assert(dict.get(key) == expected);
            assert(dict.get(key, 1234567) == (it == ref.end() ? 1234567 : it->second));
        } else if (type == 3) {
            assert(dict.contains(key) == ref.contains(key));
            assert(dict.pos(key).second == ref.contains(key));
        } else if (type == 4) {
            auto dat = dict.get_pos(key);
            auto it = ref.find(key);
            assert(dat.second == (it != ref.end()));
            assert(dict.inner_get(dat, 9876543) == (it == ref.end() ? 9876543 : it->second));
            if (rng() & 1) {
                dict.inner_set(dat, key, val);
                ref[key] = val;
            }
        } else if (type == 5) {
            auto it = ref.find(key);
            bool expected = false;
            if (it == ref.end()) {
                ref[key] = val;
            } else if (val < it->second) {
                it->second = val;
                expected = true;
            }
            assert(dict.contains_set(key, val) == expected);
        } else if (type == 6) {
            auto it = ref.find(key);
            long long expected = it == ref.end() ? 0 : it->second;
            assert(dict[key] == expected);
            if (it == ref.end()) ref[key] = 0;
        } else if (type == 7 && step % 4093 == 0) {
            Dict<USE_HASH_FUNC> copy(dict);
            verify(copy, ref);
            Dict<USE_HASH_FUNC> assigned;
            assigned = dict;
            verify(assigned, ref);
        } else if (type == 8 && step % 997 == 0) {
            int cap = dict.inner_len();
            dict.clear();
            ref.clear();
            assert(dict.inner_len() == cap);
        } else {
            auto dat = dict.get_pos(key);
            auto it = ref.find(key);
            assert(dict.inner_get(dat) == (it == ref.end() ? 0 : it->second));
        }
        if ((step & 2047) == 0) verify(dict, ref);
    }
    verify(dict, ref);
}

void collision_test() {
    titan23::HopscotchHashDict<int, false> dict;
    for (int i = 0; i < 1000; ++i) dict.set(uint64_t(i) << 32, i + 1);
    assert(dict.len() == 1000);
    for (int i = 0; i < 1000; ++i) {
        uint64_t key = uint64_t(i) << 32;
        assert(dict.contains(key));
        assert(dict.get(key) == i + 1);
        auto dat = dict.get_pos(key);
        assert(dict.inner_get(dat, -1) == i + 1);
        dict.inner_set(dat, key, -i);
    }
    for (int i = 0; i < 1000; ++i) assert(dict.get(uint64_t(i) << 32) == -i);
    assert(dict.items().size() == 1000);
    dict.clear();
    assert(dict.empty());
    for (int i = 0; i < 1000; ++i) assert(!dict.contains(uint64_t(i) << 32));
}

void value_type_test() {
    using P = pair<long long, int>;
    titan23::HopscotchHashDict<P> dict(100);
    assert(100 <= dict.inner_len() - dict.inner_len() / 8);
    for (int i = 0; i < 100; ++i) dict.set(i, {i * i, -i});
    for (int i = 0; i < 100; ++i) assert(dict.get(i) == P(i * i, -i));
    P missing = {-1, -1};
    assert(dict.get(1000, missing) == missing);
}

struct LessOnly {
    int x = 0;

    bool operator<(const LessOnly &other) const {
        return x < other.x;
    }
};

void less_only_test() {
    titan23::HopscotchHashDict<LessOnly> dict;
    assert(!dict.contains_set(1, {5}));
    assert(dict.contains_set(1, {3}));
    assert(!dict.contains_set(1, {4}));
    assert(dict.get(1).x == 3);
}

int main() {
    for (uint64_t seed = 0; seed < 4; ++seed) {
        random_test<true>(seed * 1000003 + 7);
        random_test<false>(seed * 1000033 + 11);
    }
    collision_test();
    value_type_test();
    less_only_test();
}
