#include <algorithm>
#include <cassert>
#include <cstdint>
#include <limits>
#include <random>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "titan_cpplib/ds/universal_hash_dict.cpp"

using namespace std;
using namespace titan23;

template<bool USE_HASH_FUNC>
void verify(const UniversalHashDict<long long, USE_HASH_FUNC> &mp,
            const unordered_map<uint64_t, long long> &ref) {
    assert(mp.len() == int(ref.size()));
    assert(mp.empty() == ref.empty());
    unordered_map<uint64_t, long long> got;
    for (const auto &[key, val] : mp.items()) assert(got.emplace(key, val).second);
    assert(got == ref);

    vector<long long> vals = mp.values();
    vector<long long> want;
    want.reserve(ref.size());
    for (const auto &[key, val] : ref) {
        (void)key;
        want.push_back(val);
    }
    sort(vals.begin(), vals.end());
    sort(want.begin(), want.end());
    assert(vals == want);

    for (const auto &[key, val] : ref) {
        assert(mp.contains(key));
        assert(mp.get(key) == val);
        assert(mp.get(key, -1) == val);
        const auto dat = mp.get_pos(key);
        assert(dat.second);
        assert(mp.inner_get(dat, -1) == val);
    }
}

template<bool USE_HASH_FUNC>
void random_test(uint64_t seed) {
    mt19937_64 rng(seed);
    UniversalHashDict<long long, USE_HASH_FUNC> mp;
    unordered_map<uint64_t, long long> ref;
    const vector<uint64_t> special = {
        0,
        1,
        numeric_limits<uint64_t>::max(),
        uint64_t(1) << 63,
        (uint64_t(1) << 32) - 1,
    };

    for (int step = 0; step < 300000; ++step) {
        const uint64_t key = step % 97 == 0 ? special[rng() % special.size()] : rng();
        const long long val = int(rng() % 2001) - 1000;
        const int type = int(rng() % 10);
        if (type == 0) {
            mp.set(key, val);
            ref[key] = val;
        } else if (type == 1) {
            mp.add(key, val);
            ref[key] += val;
        } else if (type == 2) {
            const auto it = ref.find(key);
            const long long want = it == ref.end() ? 0 : it->second;
            assert(mp.get(key) == want);
            assert(mp.get(key, 1234567) == (it == ref.end() ? 1234567 : it->second));
        } else if (type == 3) {
            assert(mp.contains(key) == ref.contains(key));
            assert(mp.pos(key).second == ref.contains(key));
        } else if (type == 4) {
            const auto dat = mp.get_pos(key);
            const auto it = ref.find(key);
            assert(dat.second == (it != ref.end()));
            assert(mp.inner_get(dat, 9876543) == (it == ref.end() ? 9876543 : it->second));
            if (rng() & 1) {
                mp.inner_set(dat, key, val);
                ref[key] = val;
            }
        } else if (type == 5) {
            auto it = ref.find(key);
            bool want = false;
            if (it == ref.end()) {
                ref[key] = val;
            } else if (val < it->second) {
                it->second = val;
                want = true;
            }
            assert(mp.contains_set(key, val) == want);
        } else if (type == 6) {
            const auto it = ref.find(key);
            const long long want = it == ref.end() ? 0 : it->second;
            assert(mp[key] == want);
            if (it == ref.end()) ref[key] = 0;
        } else if (type == 7 && step % 4093 == 0) {
            UniversalHashDict<long long, USE_HASH_FUNC> copy(mp);
            verify(copy, ref);
            UniversalHashDict<long long, USE_HASH_FUNC> assigned;
            assigned = mp;
            verify(assigned, ref);
        } else if (type == 8 && step % 997 == 0) {
            const int cap = mp.inner_len();
            mp.clear();
            ref.clear();
            assert(mp.inner_len() == cap);
        } else {
            const auto dat = mp.get_pos(key);
            const auto it = ref.find(key);
            assert(mp.inner_get(dat) == (it == ref.end() ? 0 : it->second));
        }
        assert(mp.len() == int(ref.size()));
        if ((step & 4095) == 0) verify(mp, ref);
    }
    verify(mp, ref);
}

void collision_test() {
    UniversalHashDict<int, false> mp(1000);
    for (int i = 0; i < 1000; ++i) mp.set(uint64_t(i) << 32, i + 1);
    for (int i = 0; i < 1000; ++i) {
        const uint64_t key = uint64_t(i) << 32;
        assert(mp.get(key, -1) == i + 1);
        const auto dat = mp.get_pos(key);
        mp.inner_set(dat, key, -i);
    }
    for (int i = 0; i < 1000; ++i) assert(mp.get(uint64_t(i) << 32) == -i);
    mp.clear();
    for (int i = 0; i < 1000; ++i) assert(!mp.contains(uint64_t(i) << 32));
    mp.set(uint64_t(7) << 32, 123);
    assert(mp.get(uint64_t(7) << 32) == 123);
}

void sparse_clear_test() {
    UniversalHashDict<int> mp(200000);
    mp.set(0, 10);
    mp.set(numeric_limits<uint64_t>::max(), 20);
    for (int i = 1; i <= 20; ++i) mp.set(uint64_t(i) << 32, i);
    const int cap = mp.inner_len();
    mp.clear();
    assert(mp.empty() && mp.inner_len() == cap);
    assert(!mp.contains(0));
    assert(!mp.contains(numeric_limits<uint64_t>::max()));
    for (int i = 1; i <= 20; ++i) assert(!mp.contains(uint64_t(i) << 32));
    mp.set(0, 30);
    mp.set(numeric_limits<uint64_t>::max(), 40);
    assert(mp.get(0) == 30);
    assert(mp.get(numeric_limits<uint64_t>::max()) == 40);
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
        auto key_at = [=](uint64_t x) {
            ++x;
            if (type == 0) return x;
            if (type == 1) return x << 32;
            if (type == 2) return reverse_bits(x);
            return x * uint64_t(1000000007);
        };
        UniversalHashDict<int> mp;
        for (int i = 0; i < N; ++i) mp.set(key_at(uint64_t(i)), i + 1);
        assert(mp.len() == N);
        for (int i = 0; i < N; ++i) assert(mp.get(key_at(uint64_t(i)), -1) == i + 1);
    }
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
    UniversalHashDict<MoveOnly> mp;
    for (int i = 0; i < 10000; ++i) mp.set(uint64_t(i), MoveOnly(i));
    assert(mp.len() == 10000);
    for (int i = 0; i < 10000; ++i) assert(mp.contains(uint64_t(i)));
}

void value_type_test() {
    UniversalHashDict<string> mp;
    mp.set(0, "zero");
    mp.set(numeric_limits<uint64_t>::max(), "max");
    assert(mp.get(0) == "zero");
    assert(mp.get(numeric_limits<uint64_t>::max()) == "max");
    const auto copy = mp;
    assert(copy.get(0) == "zero");
    assert(copy.get(numeric_limits<uint64_t>::max()) == "max");
}

int main() {
    for (uint64_t seed = 0; seed < 4; ++seed) {
        random_test<true>(seed * 1000003 + 7);
        random_test<false>(seed * 1000033 + 11);
    }
    collision_test();
    sparse_clear_test();
    structured_test();
    move_only_test();
    value_type_test();
}
