// g++ -std=c++17 -O2 -I. test/ds/persistent_wbtree_family_random.cpp -o /tmp/persistent_wbt_family_test
#include <algorithm>
#include <cassert>
#include <cstdint>
#include <iostream>
#include <limits>
#include <numeric>
#include <random>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>
#include "titan_cpplib/ds/persistent_wbtree.cpp"
#include "titan_cpplib/ds/persistent_seg_wbtree.cpp"

using namespace std;
using ll = long long;
ll sum_op(ll a, ll b) { return a + b; }
ll sum_e() { return 0; }
string concat_op(string a, string b) { return a + b; }
string concat_e() { return {}; }

template<bool Seg, class Tree>
const auto &node_at(size_t index) {
    if constexpr (Seg) return Tree::ma.tree[index];
    else return Tree::ma.d[index];
}
template<bool Seg, class Tree>
const auto &key_at(size_t index) {
    if constexpr (Seg) return Tree::ma.data[index].data;
    else return Tree::ma.keys[index];
}
template<bool Seg, class Tree, class T, class Op>
void verify(const Tree &tree, const vector<T> &expected, Op op, const T &identity) {
    const size_t count = Tree::ma.ptr;
    assert(size_t(tree.len()) == expected.size());
    assert(tree.tovector() == expected);
    for (size_t i = 0; i < expected.size(); ++i) assert(tree.get(i) == expected[i]);
    if constexpr (Seg) {
        for (int part = 0; part < 9; ++part) {
            const int l = expected.size() * part / 9;
            const int r = expected.size() * (part + 1) / 9;
            T aggregate = identity;
            for (int i = l; i < r; ++i) aggregate = op(aggregate, expected[i]);
            assert(tree.prod(l, r) == aggregate);
            assert(tree.prod(l, l) == identity);
        }
        T aggregate = identity;
        for (const T &value : expected) aggregate = op(aggregate, value);
        assert(tree.prod(0, expected.size()) == aggregate);
        tree.check();
    }
    assert(Tree::ma.ptr == count);
}

template<bool Seg, bool Reversible, class Tree, class Maker, class Op>
void random_history(uint64_t seed, int initial_size, int rounds, Maker make, Op op) {
    using T = decltype(make(0));
    Tree::ma.reset();
    mt19937_64 rng(seed);
    vector<T> initial;
    for (int i = 0; i < initial_size; ++i) initial.push_back(make(i));
    vector<pair<Tree, vector<T>>> versions;
    versions.emplace_back(Tree(initial), initial);
    struct Saved {
        size_t index;
        typename Tree::MemoryAllocator::Node node;
        T key, aggregate;
        int rev;
    };
    vector<Saved> saved;
    auto capture = [&] {
        saved.clear();
        const size_t n = Tree::ma.ptr;
        for (int i = 0; i < 80; ++i) {
            const size_t index = i == 0 ? 0 : rng() % n;
            Saved entry{index, node_at<Seg, Tree>(index), key_at<Seg, Tree>(index), T{}, 0};
            if constexpr (Seg) {
                entry.aggregate = Tree::ma.data[index].data;
                entry.rev = Tree::ma.tree[index].rev;
            }
            saved.push_back(std::move(entry));
        }
    };
    auto unchanged = [&] {
        for (const Saved &old : saved) {
            const auto &node = node_at<Seg, Tree>(old.index);
            assert(node.left == old.node.left && node.right == old.node.right);
            assert(node.size == old.node.size && node.left_size == old.node.left_size);
            assert((key_at<Seg, Tree>(old.index) == old.key));
            if constexpr (Seg) {
                assert(Tree::ma.data[old.index].data == old.aggregate);
                assert(Tree::ma.tree[old.index].rev == old.rev);
            }
        }
    };
    for (int step = 0; step < rounds; ++step) {
        if (step % 97 == 0) capture();
        const size_t source = step % 3 == 0 ? rng() % versions.size() : versions.size() - 1;
        Tree current = versions[source].first;
        vector<T> expected = versions[source].second;
        const int n = expected.size();
        int kind = rng() % (Seg ? 13 : 9);
        if (kind == 0 && n >= 256) kind = 1;
        switch (kind) {
        case 0: {
            const int k = rng() % (n + 1);
            const T value = make(step + 17);
            current = current.insert(k, value);
            expected.insert(expected.begin() + k, value);
            break;
        }
        case 1:
            if (n) {
                const int k = rng() % n;
                auto result = current.pop(k);
                assert(result.second == expected[k]);
                current = result.first;
                expected.erase(expected.begin() + k);
            }
            break;
        case 2: {
            const int k = rng() % (n + 1);
            auto parts = current.split(k);
            verify<Seg>(parts.first, vector<T>(expected.begin(), expected.begin() + k), op, T{});
            verify<Seg>(parts.second, vector<T>(expected.begin() + k, expected.end()), op, T{});
            current = parts.first.merge(parts.second);
            break;
        }
        case 3: {
            const int k = rng() % (n + 1);
            current = current.split_left(k);
            expected.resize(k);
            break;
        }
        case 4: {
            const int k = rng() % (n + 1);
            current = current.split_right(k);
            expected.erase(expected.begin(), expected.begin() + k);
            break;
        }
        case 5: {
            const auto &other = versions[rng() % versions.size()];
            if (expected.size() + other.second.size() <= 256) {
                current = current.merge(other.first);
                expected.insert(expected.end(), other.second.begin(), other.second.end());
            }
            break;
        }
        case 6: {
            const size_t count = Tree::ma.ptr;
            current = current.copy();
            assert(Tree::ma.ptr == count);
            break;
        }
        case 7:
            if (n) { const int k = rng() % n; assert(current.get(k) == expected[k]); }
            break;
        case 8:
            if (n <= 128) {
                current = current.merge(current);
                const auto duplicate = expected;
                expected.insert(expected.end(), duplicate.begin(), duplicate.end());
            }
            break;
        case 9:
            if constexpr (Seg) if (n) {
                const int k = rng() % n;
                expected[k] = make(step + 71);
                current = current.set(k, expected[k]);
            }
            break;
        case 10:
            if constexpr (Seg && Reversible) {
                int l = rng() % (n + 1), r = rng() % (n + 1);
                if (l > r) swap(l, r);
                current = current.reverse(l, r);
                reverse(expected.begin() + l, expected.begin() + r);
            }
            break;
        case 11:
            if constexpr (Seg) {
                int l = rng() % (n + 1), r = rng() % (n + 1);
                if (l > r) swap(l, r);
                T aggregate{};
                for (int i = l; i < r; ++i) aggregate = op(aggregate, expected[i]);
                assert(current.prod(l, r) == aggregate);
            }
            break;
        case 12:
            if constexpr (Seg) {
                int l = rng() % (n + 1), r = rng() % (n + 1);
                if (l > r) swap(l, r);
                current = current.split_range(l, r);
                expected = vector<T>(expected.begin() + l, expected.begin() + r);
            }
            break;
        }
        unchanged();
        verify<Seg>(current, expected, op, T{});
        verify<Seg>(versions[source].first, versions[source].second, op, T{});
        const auto &past = versions[rng() % versions.size()];
        verify<Seg>(past.first, past.second, op, T{});
        versions.emplace_back(current, std::move(expected));
    }
    for (const auto &version : versions) verify<Seg>(version.first, version.second, op, T{});
    Tree rebuilt = versions.back().first;
    const auto expected = versions.back().second;
    Tree::rebuild(rebuilt);
    verify<Seg>(rebuilt, expected, op, T{});
    assert(Tree::ma.ptr == (Seg ? (expected.empty() ? 1 : 2 * expected.size()) : expected.size() + 1));
}

struct alignas(64) Tracked {
    inline static int alive = 0;
    inline static int copies_until_throw = -1;
    string text;
    Tracked() { ++alive; }
    explicit Tracked(string value) : text(std::move(value)) { ++alive; }
    Tracked(const Tracked &other) : text(other.text) {
        if (copies_until_throw == 0) throw runtime_error("requested copy failure");
        if (copies_until_throw > 0) --copies_until_throw;
        ++alive;
    }
    Tracked(Tracked &&other) noexcept : text(std::move(other.text)) { ++alive; }
    Tracked &operator=(const Tracked &) = default;
    Tracked &operator=(Tracked &&) noexcept = default;
    ~Tracked() { --alive; }
    bool operator==(const Tracked &other) const { return text == other.text; }
};
Tracked tracked_op(Tracked a, Tracked b) { return Tracked(a.text + b.text); }
Tracked tracked_e() { return {}; }

template<bool Seg, class Tree>
void allocator_lifetime() {
    const int before = Tracked::alive;
    {
        typename Tree::MemoryAllocator allocator;
        const int constructed = Tracked::alive;
        allocator.reserve(17);
        allocator.reserve(17);
        allocator.reserve(1);
        assert(Tracked::alive == constructed);
        assert(allocator.cap == allocator.pool.capacity());
        const Tracked value(string(80, 'a'));
        for (int i = 0; i < 65535; ++i) {
            const int node = i && i % 7 == 0 ? allocator.copy(1) : allocator.new_node(value);
            assert(node == i + 1);
        }
        // Each allocator now stores one T object per record.
        for (int allowance = 0; allowance < 1; ++allowance) {
            for (bool copy : {false, true}) {
                const size_t position = allocator.ptr, size = allocator.pool.size();
                const int count = Tracked::alive;
                Tracked::copies_until_throw = allowance;
                bool failed = false;
                try {
                    if (copy) allocator.copy(1);
                    else allocator.new_node(value);
                } catch (const runtime_error &) { failed = true; }
                Tracked::copies_until_throw = -1;
                assert(failed && allocator.ptr == position && allocator.pool.size() == size);
                assert(Tracked::alive == count && allocator.cap == allocator.pool.capacity());
                assert(allocator.copy(1) == int(position));
            }
        }
        auto stored = [&](auto &a, size_t index) -> const Tracked& {
            if constexpr (Seg) return a.data[index].data;
            else return a.keys[index];
        };
        const Tracked *address = &stored(allocator, 1);
        assert(reinterpret_cast<uintptr_t>(address) % alignof(Tracked) == 0);
        auto moved = std::move(allocator);
        assert(&stored(moved, 1) == address && stored(moved, 1) == value);
        allocator = std::move(moved);
        assert(&stored(allocator, 1) == address);
        allocator.reset();
        const Tracked replacement(string(97, 'z'));
        for (int i = 0; i < 4099; ++i) {
            const int node = allocator.new_node(replacement);
            assert(node == i + 1 && stored(allocator, node) == replacement);
        }
    }
    assert(Tracked::alive == before);
}

void api_boundaries() {
    using B = titan23::PersistentWBTree<ll>;
    B::ma.reset();
    B::reserve(1024);
    const B b(vector<ll>{1, 2, 3});
    const B c = b.copy();
    assert(c.get(1) == 2);
    const size_t count = B::ma.ptr;
    B empty;
    assert(empty.copy().len() == 0);
    assert(empty.merge(b).tovector() == b.tovector());
    B writable = b;
    assert(writable.split(0).second.tovector() == b.tovector());
    assert(writable.split(3).first.tovector() == b.tovector());
    assert(B::ma.ptr == count);
    ostringstream out;
    out << b;
    assert(out.str() == "[1, 2, 3]");
    using S = titan23::PersistentSegWBTree<int, ll, sum_op, sum_e>;
    S::ma.reset();
    S s(vector<ll>{1, 2, 3});
    const auto root_set = s.set(1, 9);
    assert(root_set.tovector() == (vector<ll>{1, 9, 3}));
    assert(s.tovector() == (vector<ll>{1, 2, 3}));
    const size_t allocated = S::ma.ptr;
    assert(s.reverse(0, 0).tovector() == s.tovector());
    assert(s.reverse(1, 2).tovector() == s.tovector());
    assert(s.prod(0, 3) == 6 && s.prod(2, 3) == 3);
    assert(S::ma.ptr == allocated);
}

void large_shared_tree() {
    using B = titan23::PersistentWBTree<ll>;
    using S = titan23::PersistentSegWBTree<int, ll, sum_op, sum_e>;
    B::ma.reset();
    S::ma.reset();
    B basic(vector<ll>{1, 1, 1});
    S segment(vector<ll>{1, 1, 1});
    for (int i = 0; i < 29; ++i) {
        basic = basic.merge(basic);
        segment = segment.merge(segment);
    }
    const int length = 3 * (1 << 29);
    assert(basic.len() == length && segment.len() == length);
    assert(B::ma.ptr < 10000 && S::ma.ptr < 10000);
    for (int k : {0, length / 2, length - 1}) {
        assert(basic.get(k) == 1 && segment.get(k) == 1);
    }
    assert(segment.prod(0, length) == length);
    const auto basic_removed = basic.pop(0);
    const auto segment_removed = segment.pop(length - 1);
    assert(basic_removed.first.len() == length - 1 && basic_removed.second == 1);
    assert(segment_removed.first.prod(0, length - 1) == length - 1);
    assert(basic.len() == length && segment.prod(0, length) == length);
}

int main() {
    const auto number = [](int i) -> ll { return (i * 37) % 101 - 50; };
    const auto text = [](int i) { return string(1, char('a' + i % 26)); };
    api_boundaries();
    large_shared_tree();
    for (uint64_t seed : {23, 47}) {
        random_history<false, false, titan23::PersistentWBTree<ll>>(seed, 63, 2500, number, sum_op);
        random_history<true, true, titan23::PersistentSegWBTree<int, ll, sum_op, sum_e>>(seed, 64, 3000, number, sum_op);
        random_history<true, false, titan23::PersistentSegWBTree<int, string, concat_op, concat_e>>(seed, 31, 1800, text, concat_op);
    }
    random_history<false, false, titan23::PersistentWBTree<ll, unsigned>>(81, 31, 600, number, sum_op);
    random_history<true, true, titan23::PersistentSegWBTree<unsigned, ll, sum_op, sum_e>>(83, 31, 600, number, sum_op);
    allocator_lifetime<false, titan23::PersistentWBTree<Tracked>>();
    allocator_lifetime<true, titan23::PersistentSegWBTree<int, Tracked, tracked_op, tracked_e>>();
    cout << "persistent_wbtree_family: OK\n";
}
