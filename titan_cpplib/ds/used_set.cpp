#pragma once

#include <cassert>
#include "titan_cpplib/ds/index_set.cpp"
#include "titan_cpplib/alg/random.cpp"
using namespace std;

namespace titan23 {

class UsedSet {
private:
    int u;
    titan23::IndexSet used, unused;

public:
    /// 空の UsedSet を構築する / `O(1)`
    UsedSet() : u(0) {}

    /// `[0, u)` をすべて未使用として構築する / `O(u)`
    UsedSet(int u) : u(u), used(u), unused(u) {
        assert(u >= 0);
        for (int i = 0; i < u; ++i) {
            unused.add(i);
        }
    }

    /// 未使用の `v` を使用済みにする / `O(1)`
    void use(int v) {
        assert(0 <= v && v < u && unused.contains(v));
        used.add(v);
        unused.remove(v);
    }

    /// 使用済みの `v` を未使用にする / `O(1)`
    void unuse(int v) {
        assert(0 <= v && v < u && used.contains(v));
        used.remove(v);
        unused.add(v);
    }

    /// すべて使用済みにする / `O(len_unuse())`
    void all_use() {
        if (unused.empty()) return;
        for (int v : unused.que) {
            used.add(v);
            unused.pos[v] = -1;
        }
        unused.que.clear();
    }

    /// すべて未使用にする / `O(len_use())`
    void all_unuse() {
        if (used.empty()) return;
        for (int v : used.que) {
            unused.add(v);
            used.pos[v] = -1;
        }
        used.que.clear();
    }

    /// 使用済み集合の `i` 番目を返す / `O(1)`
    int get_use(int i) const {
        assert(0 <= i && i < len_use());
        return used.get(i);
    }

    /// 未使用集合の `i` 番目を返す / `O(1)`
    int get_unuse(int i) const {
        assert(0 <= i && i < len_unuse());
        return unused.get(i);
    }

    /// `v` が使用済みか判定する / `O(1)`
    bool contains_use(int v) const {
        assert(0 <= v && v < u);
        return used.contains(v);
    }

    /// `v` が未使用か判定する / `O(1)`
    bool contains_unuse(int v) const {
        assert(0 <= v && v < u);
        return unused.contains(v);
    }

    /// 使用済み集合が空か判定する / `O(1)`
    bool empty_use() const { return used.empty(); }

    /// 未使用集合が空か判定する / `O(1)`
    bool empty_unuse() const { return unused.empty(); }

    /// 使用済みの要素数を返す / `O(1)`
    int len_use() const { return used.len(); }

    /// 未使用の要素数を返す / `O(1)`
    int len_unuse() const { return unused.len(); }

    /// 使用済み集合からランダムに1要素を返す / `O(1)`
    int rnd_get_from_use(titan23::Random &rnd) const {
        assert(!empty_use());
        return used.get(rnd.randrange(len_use()));
    }

    /// 未使用集合からランダムに1要素を返す / `O(1)`
    int rnd_get_from_unuse(titan23::Random &rnd) const {
        assert(!empty_unuse());
        return unused.get(rnd.randrange(len_unuse()));
    }

    /// 使用済み集合からランダムに1要素を取り出し、未使用にして返す / `O(1)`
    int rnd_pop_from_use(titan23::Random &rnd) {
        int v = rnd_get_from_use(rnd);
        unuse(v);
        return v;
    }

    /// 未使用集合からランダムに1要素を取り出し、使用済みにして返す / `O(1)`
    int rnd_pop_from_unuse(titan23::Random &rnd) {
        int v = rnd_get_from_unuse(rnd);
        use(v);
        return v;
    }

    friend ostream& operator<<(ostream& os, const UsedSet &ust) {
        os << "[UsedSet] used : " << ust.used << " / unused : " << ust.unused;
        return os;
    }
};
} // namespace titan23
