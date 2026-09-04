/// https://github.com/titan-23/Library_cpp/blob/main/titan_cpplib/ds/universal_hash_dict.cpp
#pragma once

#include <bit>
#include <cassert>
#include <cstdint>
#include <random>
#include <utility>
#include <vector>
using namespace std;

namespace titan23 {

// 乱数と独立な固定入力に対し検索と更新は期待償却 O(1)
// random odd multiply-shift は衝突確率が 2 / bucket 数以下で負荷率を 1/2 以下に保つ
// USE_HASH_FUNC=false で同じ bucket に k 個集中した場合の検索は O(k)
// get_pos と pos の結果は次の変更操作まで有効
// erase は持たず要素を連続領域へ追加する
// clear と values と items は O(size)
template<typename V, bool USE_HASH_FUNC=true>
class UniversalHashDict {
private:
    using u64 = uint64_t;
    static constexpr int NONE = -1;

    struct Node { u64 key; V val; int next; };

    vector<int> head;
    vector<Node> nodes;
    int cap = 16;
    int shift = 60;
    int sz = 0;
    u64 mul = 1;

    int bucket(u64 key) const {
        if constexpr (USE_HASH_FUNC) return int((key * mul) >> shift);
        return int(key & u64(cap - 1));
    }

    void init_seed() {
        random_device rd;
        uniform_int_distribution<u64> dis;
        mul = dis(rd) | 1;
    }

    void rebuild() {
        cap *= 2;
        --shift;
        head.assign(cap, NONE);
        for (int i = 0; i < sz; ++i) {
            const int b = bucket(nodes[i].key);
            nodes[i].next = head[b];
            head[b] = i;
        }
    }

public:
    UniversalHashDict() : UniversalHashDict(0) {}

    explicit UniversalHashDict(int n) {
        assert(n >= 0);
        while (cap < n * 2) cap *= 2;
        shift = countl_zero(u64(cap - 1));
        if constexpr (USE_HASH_FUNC) init_seed();
        head.assign(cap, NONE);
        nodes.reserve(n);
    }

    pair<int, bool> get_pos(const u64 &key) const {
        const int b = bucket(key);
        for (int p = head[b]; p != NONE; p = nodes[p].next) {
            if (nodes[p].key == key) return {p, true};
        }
        return {b, false};
    }

    V get(const u64 key) const {
        const auto [p, exist] = get_pos(key);
        return exist ? nodes[p].val : V{};
    }

    V get(const u64 key, const V missing) const {
        const auto [p, exist] = get_pos(key);
        return exist ? nodes[p].val : missing;
    }

    bool contains(const u64 key) const {
        return get_pos(key).second;
    }

    pair<int, bool> pos(const u64 key) const {
        return get_pos(key);
    }

    V operator[](const u64 key) {
        const auto dat = get_pos(key);
        if (dat.second) return nodes[dat.first].val;
        V val{};
        inner_set(dat, key, val);
        return val;
    }

    V inner_get(const pair<int, bool> &dat, const V missing) const {
        return dat.second ? nodes[dat.first].val : missing;
    }

    V inner_get(const pair<int, bool> &dat) const {
        return dat.second ? nodes[dat.first].val : V{};
    }

    void inner_set(const pair<int, bool> &dat, const u64 key, V val) {
        if (dat.second) {
            nodes[dat.first].val = move(val);
            return;
        }
        int b = dat.first;
        if (sz * 2 == cap) {
            rebuild();
            b = bucket(key);
        }
        nodes.push_back({key, move(val), head[b]});
        head[b] = sz++;
    }

    void set(const u64 key, V val) {
        const auto dat = get_pos(key);
        inner_set(dat, key, move(val));
    }

    void add(const u64 key, const V val) {
        const auto dat = get_pos(key);
        if (dat.second) nodes[dat.first].val += val;
        else inner_set(dat, key, val);
    }

    bool contains_set(const u64 key, const V val) {
        const auto dat = get_pos(key);
        if (!dat.second) {
            inner_set(dat, key, val);
            return false;
        }
        if (!(val < nodes[dat.first].val)) return false;
        nodes[dat.first].val = val;
        return true;
    }

    vector<V> values() const {
        vector<V> res;
        res.reserve(sz);
        for (const Node &node : nodes) res.emplace_back(node.val);
        return res;
    }

    vector<pair<u64, V>> items() const {
        vector<pair<u64, V>> res;
        res.reserve(sz);
        for (const Node &node : nodes) res.emplace_back(node.key, node.val);
        return res;
    }

    void clear() {
        if (empty()) return;
        for (const Node &node : nodes) head[bucket(node.key)] = NONE;
        nodes.clear();
        sz = 0;
    }

    int len() const {
        return sz;
    }

    int inner_len() const {
        return cap;
    }

    bool empty() const {
        return sz == 0;
    }
};

} // namespace titan23
