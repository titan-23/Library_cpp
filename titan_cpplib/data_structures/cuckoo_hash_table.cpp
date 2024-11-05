raise NotImprementedError

#pragma GCC target("avx2")
#pragma GCC optimize("O3")
#pragma GCC optimize("unroll-loops")


#include <vector>
#include <random>
#include <cassert>
#include <iostream>
using namespace std;

class CuckooHashTable {
  private:
    using u64 = unsigned long long;
    int n;
    vector<u64> T1, T2;
    int msk, xor1, xor2;
    int size;
    int max_loop;
    const u64 NIL = 0;
    static constexpr const u64 K1 = 0x517cc1b727220a95;
    static constexpr const u64 K2 = 0xbf58476d1ce4e5b9;

    void rehash() {
        random_device rd;
        mt19937 gen(rd());
        uniform_int_distribution<int> dis(0, msk);
        xor1 = dis(gen);
        xor2 = dis(gen);
        for (u64 key : T1) {
            if (key != NIL) insert(key);
        }
        for (u64 key : T2) {
            if (key != NIL) insert(key);
        }
    }

    int bit_length(const int x) const {
        if (x == 0) return 0;
        return 32 - __builtin_clz(x);
    }

    void rebuild() {
        vector<u64> old_T1 = T1;
        vector<u64> old_T2 = T2;
        size = 0;
        msk = (1<<bit_length(T1.size()-1))-1;
        T1.resize(2*old_T1.size());
        T2.resize(2*old_T2.size());
        fill(T1.begin(), T1.end(), 0);
        fill(T2.begin(), T2.end(), 0);
        random_device rd;
        mt19937 gen(rd());
        uniform_int_distribution<int> dis(0, msk);
        xor1 = dis(gen);
        xor2 = dis(gen);
        max_loop = 3*bit_length(T1.size());
        for (u64 key : old_T1) {
            if (key != NIL) insert(key);
        }
        for (u64 key : old_T2) {
            if (key != NIL) insert(key);
        }
    }

    int h1(const u64 key) const {
        return (((((key>>32)&msk) ^ (key&msk) ^ xor1)) * (CuckooHashTable::K1 & msk)) & msk;
    }

    int h2(const u64 key) const {
        return (((((key>>32)&msk) ^ (key&msk) ^ xor2)) * (CuckooHashTable::K2 & msk)) & msk;
    }

    void innert_insert(u64 key) {
        if (contains(key)) return;
        for (int i = 0; i < max_loop; ++i) {
            swap(key, T1[h1(key)]);
            if (key == NIL) return;
            swap(key, T2[h2(key)]);
            if (key == NIL) return;
        }
        rehash();
        innert_insert(key);
    }

  public:
    CuckooHashTable() : T1(1, 0), T2(1, 0), msk(0), xor1(0), xor2(0), size(0), max_loop(1) {}

    bool contains(const u64 key) const {
        return T1[h1(key)] == key || T2[h2(key)] == key;
    }

    void insert(u64 key) {
        assert(key != NIL);
        size++;
        if (size*2 > T1.size()) rebuild();
        innert_insert(key);
    }

    void clear() {
        fill(T1.begin(), T1.end(), NIL);
        fill(T2.begin(), T2.end(), NIL);
        size = 0;
    }
};

#include "titan_cpplib/algorithm/random.cpp"
using u64 = unsigned long long;
void solve() {
    titan23::Random rnd;
    unordered_set<u64> s;
    CuckooHashTable h;
    int q = 1e7;
    for (int i = 0; i < q; ++i) {
        u64 v = rnd.rand_u64();
        s.insert(v);
        h.insert(v);
        assert(h.contains(v));
    }
}

int main() {
    ios::sync_with_stdio(false);
    cin.tie(0);
    solve();
    return 0;
}
