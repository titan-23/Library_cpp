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

/// @brief u64 キーの連鎖法ハッシュ表 / erase は持たない
/// USE_HASH_FUNC=false では同じ bucket に k 個集中すると検索が O(k) になる
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
    /// @brief 空の表を作る
    UniversalHashDict() : UniversalHashDict(0) {}

    /// @brief n 要素を入れる前提で領域を確保する / O(n)
    explicit UniversalHashDict(int n) {
        assert(n >= 0);
        while (cap < n * 2) cap *= 2;
        shift = countl_zero(u64(cap - 1));
        if constexpr (USE_HASH_FUNC) init_seed();
        head.assign(cap, NONE);
        nodes.reserve(n);
    }

    /// @brief key の位置と有無を返す / 結果は次の変更操作まで有効 / 期待 O(1)
    pair<int, bool> get_pos(const u64 &key) const {
        const int b = bucket(key);
        for (int p = head[b]; p != NONE; p = nodes[p].next) {
            if (nodes[p].key == key) return {p, true};
        }
        return {b, false};
    }

    /// @brief key の値を返す / なければ V{} / 期待 O(1)
    V get(const u64 key) const {
        const auto [p, exist] = get_pos(key);
        return exist ? nodes[p].val : V{};
    }

    /// @brief key の値を返す / なければ missing / 期待 O(1)
    V get(const u64 key, const V missing) const {
        const auto [p, exist] = get_pos(key);
        return exist ? nodes[p].val : missing;
    }

    /// @brief key を含むかを返す / 期待 O(1)
    bool contains(const u64 key) const {
        return get_pos(key).second;
    }

    /// @brief get_pos と同じ / 結果は次の変更操作まで有効 / 期待 O(1)
    pair<int, bool> pos(const u64 key) const {
        return get_pos(key);
    }

    /// @brief key の値を返す / なければ V{} を追加する / 参照ではなく値を返す / 期待償却 O(1)
    V operator[](const u64 key) {
        const auto dat = get_pos(key);
        if (dat.second) return nodes[dat.first].val;
        V val{};
        inner_set(dat, key, val);
        return val;
    }

    /// @brief get_pos の結果 dat が指す値を返す / なければ missing / O(1)
    V inner_get(const pair<int, bool> &dat, const V missing) const {
        return dat.second ? nodes[dat.first].val : missing;
    }

    /// @brief get_pos の結果 dat が指す値を返す / なければ V{} / O(1)
    V inner_get(const pair<int, bool> &dat) const {
        return dat.second ? nodes[dat.first].val : V{};
    }

    /// @brief get_pos の結果 dat を使い、key の値を val にする / 償却 O(1)
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

    /// @brief key の値を val にする / 期待償却 O(1)
    void set(const u64 key, V val) {
        const auto dat = get_pos(key);
        inner_set(dat, key, move(val));
    }

    /// @brief key の値に val を加える / なければ val で追加する / 期待償却 O(1)
    void add(const u64 key, const V val) {
        const auto dat = get_pos(key);
        if (dat.second) nodes[dat.first].val += val;
        else inner_set(dat, key, val);
    }

    /// @brief val が今の値より小さければ更新して true を返す / key がなければ val で追加して false を返す / 期待償却 O(1)
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

    /// @brief 値を追加順に並べて返す / O(size)
    vector<V> values() const {
        vector<V> res;
        res.reserve(sz);
        for (const Node &node : nodes) res.emplace_back(node.val);
        return res;
    }

    /// @brief (key, 値) の組を追加順に並べて返す / O(size)
    vector<pair<u64, V>> items() const {
        vector<pair<u64, V>> res;
        res.reserve(sz);
        for (const Node &node : nodes) res.emplace_back(node.key, node.val);
        return res;
    }

    /// @brief すべての要素を削除する / bucket 数は保つ / O(size)
    void clear() {
        if (empty()) return;
        for (const Node &node : nodes) head[bucket(node.key)] = NONE;
        nodes.clear();
        sz = 0;
    }

    /// @brief 要素数を返す / O(1)
    int len() const {
        return sz;
    }

    /// @brief bucket 数を返す / O(1)
    int inner_len() const {
        return cap;
    }

    /// @brief 空かを返す / O(1)
    bool empty() const {
        return sz == 0;
    }
};

} // namespace titan23
