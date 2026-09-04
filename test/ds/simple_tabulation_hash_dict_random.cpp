#include <algorithm>
#include <cassert>
#include <cstdint>
#include <iostream>
#include <limits>
#include <random>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "titan_cpplib/ds/simple_tabulation_hash_dict.cpp"

using namespace std;
using namespace titan23;

void random_test(uint64_t seed) {
    mt19937_64 rng(seed);
    SimpleTabulationHashDict<int> mp(1);
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

    for (int qi = 0; qi < 300000; ++qi) {
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
    for (const auto &[key, val] : ref) {
        (void)val;
        assert(mp.get(key, 123456789) == 123456789);
    }
    mp.set(0, 123);
    assert(mp.get(0) == 123);
    mp.set(numeric_limits<uint64_t>::max(), 456);
    assert(mp.get(numeric_limits<uint64_t>::max()) == 456);
}

uint64_t reverse_bits(uint64_t x) {
    x = (x >> 32) | (x << 32);
    x = ((x & 0xffff0000ffff0000ULL) >> 16) | ((x & 0x0000ffff0000ffffULL) << 16);
    x = ((x & 0xff00ff00ff00ff00ULL) >> 8) | ((x & 0x00ff00ff00ff00ffULL) << 8);
    x = ((x & 0xf0f0f0f0f0f0f0f0ULL) >> 4) | ((x & 0x0f0f0f0f0f0f0f0fULL) << 4);
    x = ((x & 0xccccccccccccccccULL) >> 2) | ((x & 0x3333333333333333ULL) << 2);
    return ((x & 0xaaaaaaaaaaaaaaaaULL) >> 1) | ((x & 0x5555555555555555ULL) << 1);
}

void structured_test() {
    constexpr int N = 50000;
    for (int type = 0; type < 4; ++type) {
        SimpleTabulationHashDict<int> mp;
        for (int i = 1; i <= N; ++i) {
            uint64_t key;
            if (type == 0) key = i;
            else if (type == 1) key = uint64_t(i) << 32;
            else if (type == 2) key = reverse_bits(i);
            else key = uint64_t(i) * 1000000007ULL;
            mp.set(key, i);
        }
        if (mp.len() != N) {
            cerr << "structured type=" << type << " len=" << mp.len() << '\n';
            abort();
        }
        for (int i = 1; i <= N; ++i) {
            uint64_t key;
            if (type == 0) key = i;
            else if (type == 1) key = uint64_t(i) << 32;
            else if (type == 2) key = reverse_bits(i);
            else key = uint64_t(i) * 1000000007ULL;
            assert(mp.get(key, -1) == i);
        }
    }
}

void value_type_test() {
    SimpleTabulationHashDict<string> mp;
    mp.set(0, "zero");
    mp.set(numeric_limits<uint64_t>::max(), "max");
    assert(mp.get(0) == "zero");
    assert(mp.get(numeric_limits<uint64_t>::max()) == "max");
    auto cp = mp;
    assert(cp.get(0) == "zero");
    assert(cp.get(numeric_limits<uint64_t>::max()) == "max");
}

struct MoveOnly {
    int val = 0;

    MoveOnly() = default;
    explicit MoveOnly(int x) : val(x) {}
    MoveOnly(const MoveOnly &) = delete;
    MoveOnly &operator=(const MoveOnly &) = delete;
    MoveOnly(MoveOnly &&) = default;
    MoveOnly &operator=(MoveOnly &&) = default;
};

void move_only_test() {
    SimpleTabulationHashDict<MoveOnly> mp;
    for (int i = 1; i <= 1000; ++i) mp.set(uint64_t(i), MoveOnly(i));
    assert(mp.len() == 1000);
    for (int i = 1; i <= 1000; ++i) assert(mp.contains(uint64_t(i)));
}

void growth_copy_test() {
    SimpleTabulationHashDict<int> mp;
    for (int i = 1; i <= 10000; ++i) mp.set(uint64_t(i) * 1000000007, i);
    auto cp = mp;
    for (int i = 1; i <= 10000; ++i) assert(cp.get(uint64_t(i) * 1000000007, -1) == i);
}

int main() {
    for (uint64_t seed = 1; seed <= 3; ++seed) random_test(seed);
    structured_test();
    value_type_test();
    move_only_test();
    growth_copy_test();
}
