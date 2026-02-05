#pragma once

#include <vector>
#include <random>
#include <cassert>

using namespace std;

// HashSet
namespace titan23 {

class HashSet {
private:
    using u64 = unsigned long long;
    static constexpr const int M = 2;
    vector<u64> exist;
    vector<u64> keys;
    int msk, xor_;
    int size;

    // static constexpr const u64 K = 0x517cc1b727220a95;
    // constexpr int hash(const u64 &key) const {
    //     return (((((key>>32)&msk) ^ (key&msk) ^ xor_)) * HashSet::K) & msk;
    // }

    constexpr int hash(const u64 &key) const {
        u64 h = key ^ xor_;
        h = (h ^ (h >> 30)) * 0xbf58476d1ce4e5b9;
        h = (h ^ (h >> 27)) * 0x94d049bb133111eb;
        h = h ^ (h >> 31);
        return h & msk;
    }

    pair<int, bool> get_pos(const u64 &key) const {
        int h = hash(key);
        while (true) {
            if (!(exist[h>>6]>>(h&63)&1)) return {h, false};
            if (keys[h] == key) return {h, true};
            h = (h + 1) & msk;
        }
    }

    int bit_length(const int x) const {
        if (x == 0) return 0;
        return 32 - __builtin_clz(x);
    }

    void rebuild() {
        vector<u64> old_exist = exist;
        vector<u64> old_keys = keys;
        exist.resize(HashSet::M*old_exist.size()+1);
        fill(exist.begin(), exist.end(), 0);
        keys.resize(HashSet::M*old_keys.size());
        size = 0;
        msk = (1<<bit_length(keys.size()-1))-1;
        random_device rd;
        mt19937 gen(rd());
        uniform_int_distribution<int> dis(0, msk);
        xor_ = dis(gen);
        for (int i = 0; i < (int)old_keys.size(); ++i) {
            if (old_exist[i>>6]>>(i&63)&1) {
                insert(old_keys[i]);
            }
        }
    }

public:
    HashSet() : exist(1, 0), keys(1), msk(0), xor_(0), size(0) {}

    HashSet(const int n) {
        int s = 1<<bit_length(n);
        s *= HashSet::M;
        assert(s > 0);
        exist.resize((s>>6)+1, 0);
        keys.resize(s);
        msk = (1<<bit_length(keys.size()-1))-1;
        random_device rd;
        mt19937 gen(rd());
        uniform_int_distribution<int> dis(0, msk);
        xor_ = dis(gen);
        size = 0;
    }

    bool contains(const u64 key) const {
        return get_pos(key).second;
    }

    void insert(const u64 key) {
        const auto [pos, is_exist] = get_pos(key);
        if (is_exist) return;
        exist[pos>>6] |= 1ull<<(pos&63);
        keys[pos] = key;
        ++size;
        if (HashSet::M*size > keys.size()) rebuild();
    }

    //! keyがすでにあればtrue, なければ挿入してfalse / `O(1)`
    bool contains_insert(const u64 key) {
        const auto [pos, is_exist] = get_pos(key);
        if (is_exist) return true;
        exist[pos>>6] |= 1ull<<(pos&63);
        keys[pos] = key;
        ++size;
        if (HashSet::M*size > keys.size()) rebuild();
        return false;
    }

    //! 全ての要素を削除する / `O(n/w)`
    void clear() {
        if (empty()) return;
        this->size = 0;
        fill(exist.begin(), exist.end(), 0);
    }

    bool empty() const {
        return size == 0;
    }

    int len() const {
        return size;
    }
};
} // namespaced titan23
