#include <algorithm>
#include <cassert>
#include <cstdint>
#include <limits>
#include <random>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "titan_cpplib/ds/linear_hash_dict.cpp"

using namespace std;
using namespace titan23;

template<bool USE_HASH_FUNC>
void random_test(uint64_t seed) {
    mt19937_64 rng(seed);
    LinearHashDict<int, USE_HASH_FUNC> mp(200000);
    unordered_map<uint64_t, int> ref;
    ref.reserve(200000);

    vector<uint64_t> special = {
        0,
        1,
        numeric_limits<uint64_t>::max(),
        uint64_t(1) << 63,
        (uint64_t(1) << 32) - 1,
    };
    for (int i = 0; i < int(special.size()); ++i) {
        mp.set(special[i], i + 10);
        ref[special[i]] = i + 10;
    }

    for (int qi = 0; qi < 600000; ++qi) {
        uint64_t key = qi % 97 == 0 ? special[rng() % special.size()] : rng();
        int type = rng() % 8;
        if (type == 0) {
            int val = int(rng() % 1000001) - 500000;
            mp.set(key, val);
            ref[key] = val;
        } else if (type == 1) {
            int val = int(rng() % 101) - 50;
            mp.add(key, val);
            ref[key] += val;
        } else if (type == 2) {
            const auto dat = mp.get_pos(key);
            auto it = ref.find(key);
            assert(dat.second == (it != ref.end()));
            int want = it == ref.end() ? 123456789 : it->second;
            assert(mp.inner_get(dat, 123456789) == want);
        } else if (type == 3) {
            const auto dat = mp.pos(key);
            int val = int(rng() % 1000001) - 500000;
            mp.inner_set(dat, key, val);
            ref[key] = val;
        } else if (type == 4) {
            auto it = ref.find(key);
            int want = it == ref.end() ? -123456789 : it->second;
            assert(mp.get(key, -123456789) == want);
        } else if (type == 5) {
            assert(mp.contains(key) == ref.contains(key));
        } else if (type == 6) {
            int val = int(rng() % 1000001) - 500000;
            auto it = ref.find(key);
            bool want = it != ref.end() && val < it->second;
            assert(mp.contains_set(key, val) == want);
            if (it == ref.end()) ref[key] = val;
            else if (val < it->second) it->second = val;
        } else {
            auto it = ref.find(key);
            int want = it == ref.end() ? 0 : it->second;
            assert(mp[key] == want);
            if (it == ref.end()) ref[key] = 0;
        }
        assert(mp.len() == int(ref.size()));
        assert(mp.empty() == ref.empty());
    }

    auto items = mp.items();
    assert(items.size() == ref.size());
    for (const auto &[key, val] : items) assert(ref.at(key) == val);
    auto vals = mp.values();
    vector<int> want;
    want.reserve(ref.size());
    for (const auto &[key, val] : ref) want.push_back(val);
    sort(vals.begin(), vals.end());
    sort(want.begin(), want.end());
    assert(vals == want);

    mp.clear();
    assert(mp.empty());
    assert(mp.len() == 0);
    for (const auto &item : ref) assert(mp.get(item.first, 123456789) == 123456789);
    mp.set(0, 123);
    assert(mp.get(0) == 123);
}

void collision_test() {
    LinearHashDict<int, false> mp(2048);
    for (int i = 0; i < 1000; ++i) mp.set(uint64_t(i + 1) << 12, i);
    for (int i = 0; i < 1000; ++i) assert(mp.get(uint64_t(i + 1) << 12, -1) == i);
}

void structured_test() {
    constexpr int N = 100000;
    auto run = [](auto key_at) {
        LinearHashDict<int> mp(N);
        for (int i = 0; i < N; ++i) mp.set(key_at(i), i + 1);
        for (int i = 0; i < N; ++i) assert(mp.get(key_at(i), -1) == i + 1);
    };
    run([](uint64_t i) { return i + 1; });
    run([](uint64_t i) { return (i + 1) << 32; });
    run([](uint64_t i) { return (i + 1) * 1000000007ULL; });
    run([](uint64_t i) { return ((i + 1) << 32) ^ ((i + 1) * (i + 1)); });
}

void value_type_test() {
    LinearHashDict<string> mp;
    mp.set(0, "zero");
    mp.set(numeric_limits<uint64_t>::max(), "max");
    assert(mp.get(0) == "zero");
    assert(mp.get(numeric_limits<uint64_t>::max()) == "max");
    auto cp = mp;
    assert(cp.get(0) == "zero");
    assert(cp.get(numeric_limits<uint64_t>::max()) == "max");
}

int main() {
    for (uint64_t seed = 1; seed <= 3; ++seed) {
        random_test<true>(seed);
        random_test<false>(seed + 10);
    }
    collision_test();
    structured_test();
    value_type_test();
}
