#include <algorithm>
#include <cassert>
#include <cstdint>
#include <iostream>
#include <limits>
#include <random>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "titan_cpplib/ds/range_distinct_query_static.cpp"
#include "titan_cpplib/ds/range_distinct_query_point_update.cpp"
#include "titan_cpplib/ds/range_distinct_query_dynamic.cpp"

using namespace std;

namespace {

struct Key {
    int id;
    string text;

    Key(int id, string text) : id(id), text(move(text)) {}
    bool operator<(const Key &other) const { return id < other.id; }
};

template<class T>
int naive(const vector<T> &a, int l, int r) {
    return set<T>(a.begin() + l, a.begin() + r).size();
}

template<class D, class T>
void verify(const D &ds, const vector<T> &a) {
    const int n = a.size();
    assert(ds.len() == n);
    assert(ds.empty() == a.empty());
    assert(ds.count(0, n) == naive(a, 0, n));
    for (int i = 0; i <= n; ++i) assert(ds.count(i, i) == 0);
    if (n <= 96) {
        for (int l = 0; l <= n; ++l) {
            set<T> seen;
            for (int r = l; r <= n; ++r) {
                assert(ds.count(l, r) == static_cast<int>(seen.size()));
                assert(ds.range_distinct(l, r) == static_cast<int>(seen.size()));
                if (r < n) seen.insert(a[r]);
            }
        }
    } else {
        for (int i = 0; i < 256; ++i) {
            int l = (i * 97) % (n + 1);
            int r = (i * 193 + 17) % (n + 1);
            if (l > r) swap(l, r);
            const int expected = naive(a, l, r);
            assert(ds.count(l, r) == expected);
            assert(ds.range_distinct(l, r) == expected);
        }
    }
}

template<class D, class T>
void verify_mutable(const D &ds, const vector<T> &a) {
    assert(ds.tovector() == a);
    for (int i = 0; i < static_cast<int>(a.size()); ++i) assert(ds.access(i) == a[i]);
    verify(ds, a);
}

void test_static() {
    verify(titan23::StaticRangeDistinctQuery<int>(), vector<int>{});
    for (int n : {0, 1, 2, 7, 8, 9, 31, 32, 33, 63, 64, 65, 127, 128}) {
        vector<long long> a(n, -7);
        verify(titan23::StaticRangeDistinctQuery<long long>(a), a);
        for (int i = 0; i < n; ++i) a[i] = (1LL << 50) + i;
        verify(titan23::StaticRangeDistinctQuery<long long>(a), a);
        for (int i = 0; i < n; ++i) a[i] = i % 7 - 3;
        verify(titan23::StaticRangeDistinctQuery<long long>(a), a);
    }
    mt19937_64 rng(23);
    for (int test = 0; test < 80; ++test) {
        vector<int> a(rng() % 65);
        for (int &x : a) x = int(rng() % 13) - 6;
        verify(titan23::StaticRangeDistinctQuery<int>(a), a);
    }
    const vector<string> a = {"", "alpha", "beta", "alpha", "", "gamma", "beta"};
    verify(titan23::StaticRangeDistinctQuery<string>(a), a);
}

template<class D>
void test_previous_links(D ds) {
    vector<int> a = {4, 7, 4, 4, 7, 9, 4, 9};
    verify_mutable(ds, a);
    for (auto [k, x] : vector<pair<int, int>>{
            {0, 7}, {6, 7}, {3, 9}, {4, 11}, {4, 11}, {0, 4},
            {1, 4}, {7, 4}, {2, 11}, {5, 11}, {0, 11}, {6, 11}}) {
        ds.set(k, x);
        a[k] = x;
        verify_mutable(ds, a);
    }
    ds.set(3, ds.access(1));
    a[3] = a[1];
    verify_mutable(ds, a);
    ds.set(0, ds.access(0));
    verify_mutable(ds, a);
}

template<class D>
void verify_keyed(const D &ds, const vector<Key> &a) {
    verify(ds, a);
    const auto values = ds.tovector();
    assert(values.size() == a.size());
    for (int i = 0; i < static_cast<int>(a.size()); ++i) {
        assert(ds.access(i).id == a[i].id && ds.access(i).text == a[i].text);
        assert(values[i].id == a[i].id && values[i].text == a[i].text);
    }
}

template<class D>
void test_ordering_only(D ds) {
    vector<Key> a = {{1, "one"}, {2, "two"}, {1, "other"}, {3, "three"}};
    verify_keyed(ds, a);
    ds.set(0, Key(1, "changed"));
    a[0] = Key(1, "changed");
    verify_keyed(ds, a);
    ds.set(1, ds.access(2));
    a[1] = a[2];
    verify_keyed(ds, a);
    ds.set(2, ds.access(0));
    a[2] = a[0];
    verify_keyed(ds, a);
}

template<class T>
void test_point(const vector<T> &values, uint64_t seed) {
    mt19937_64 rng(seed);
    for (int n : {0, 1, 8, 65}) {
        vector<T> a(n);
        for (T &x : a) x = values[rng() % values.size()];
        titan23::PointUpdateRangeDistinctQuery<T> ds(a);
        verify_mutable(ds, a);
        if (n == 0) continue;
        for (int q = 0; q < 1600; ++q) {
            int k = rng() % n;
            T x = q % 5 == 0 ? a[k] : values[rng() % values.size()];
            ds.set(k, x);
            a[k] = x;
            int l = rng() % (n + 1), r = rng() % (n + 1);
            if (l > r) swap(l, r);
            assert(ds.count(l, r) == naive(a, l, r));
            if (q % 137 == 0) verify_mutable(ds, a);
        }
        verify_mutable(ds, a);
    }
}

template<class T>
void test_dynamic(const vector<T> &values, uint64_t seed) {
    mt19937_64 rng(seed);
    vector<T> a(24);
    for (T &x : a) x = values[rng() % values.size()];
    titan23::DynamicRangeDistinctQuery<T> ds(a, 8);
    verify_mutable(ds, a);
    for (int q = 0; q < 3500; ++q) {
        int op = rng() % 6;
        if (a.empty()) op = 0;
        if (a.size() >= 96 && op <= 1) op = 2;
        if (op <= 1) {
            int k = rng() % (a.size() + 1);
            T x = values[rng() % values.size()];
            ds.insert(k, x);
            a.insert(a.begin() + k, x);
        } else if (op <= 3) {
            int k = rng() % a.size();
            T removed = ds.pop(k);
            assert(removed == a[k]);
            a.erase(a.begin() + k);
        } else {
            int k = rng() % a.size();
            T x = op == 4 ? values[rng() % values.size()] : a[k];
            ds.set(k, x);
            a[k] = x;
        }
        int l = rng() % (a.size() + 1), r = rng() % (a.size() + 1);
        if (l > r) swap(l, r);
        assert(ds.count(l, r) == naive(a, l, r));
        if (q % 71 == 0) verify_mutable(ds, a);
    }
    verify_mutable(ds, a);
}

void test_blocks() {
    vector<int> a;
    titan23::DynamicRangeDistinctQuery<int> ds(a, 8);
    verify_mutable(ds, a);
    for (int cycle = 0; cycle < 3; ++cycle) {
        for (int i = 0; i < 256; ++i) {
            int k = i % 3 == 0 ? 0 : i % 3 == 1 ? a.size() : a.size() / 2;
            int x = i % 5 - 2;
            if (!a.empty() && i % 4 == 0) {
                const int j = a.size() / 3;
                x = a[j];
                ds.insert(k, ds.access(j));
            } else {
                ds.insert(k, x);
            }
            a.insert(a.begin() + k, x);
            if (i % 63 == 0) verify_mutable(ds, a);
        }
        verify_mutable(ds, a);
        for (int i = 0; !a.empty(); ++i) {
            int k = i % 3 == 0 ? 0 : i % 3 == 1 ? a.size() - 1 : a.size() / 2;
            if (i % 7 == 0) {
                ds.set(k, i + 1000);
                a[k] = i + 1000;
            }
            int removed = ds.pop(k);
            assert(removed == a[k]);
            a.erase(a.begin() + k);
            if (i % 47 == 0) verify_mutable(ds, a);
        }
        verify_mutable(ds, a);
    }
}

void test_copy_move() {
    using D = titan23::DynamicRangeDistinctQuery<int>;
    vector<int> a(96);
    for (int i = 0; i < 96; ++i) a[i] = i % 11;
    D original(a, 8);
    for (int i = 0; i < 13; ++i) {
        int removed = original.pop(3);
        assert(removed == a[3]);
        a.erase(a.begin() + 3);
    }
    original.set(0, original.access(55));
    a[0] = a[55];
    const int x = a[55];
    original.insert(2, original.access(55));
    a.insert(a.begin() + 2, x);
    verify_mutable(original, a);
    D copied(original);
    copied.set(0, -999);
    copied.insert(7, 12345);
    vector<int> b = a;
    assert(b.size() > 7);
    b[0] = -999;
    b.insert(b.begin() + 7, 12345);
    verify_mutable(original, a);
    verify_mutable(copied, b);

    D assigned(vector<int>{-1, -1, 2}, 8);
    assigned = original;
    verify_mutable(assigned, a);
    D moved(move(copied));
    verify_mutable(moved, b);
    verify_mutable(copied, vector<int>{});
    D move_assigned(vector<int>{17}, 8);
    move_assigned = move(assigned);
    verify_mutable(move_assigned, a);
    verify_mutable(assigned, vector<int>{});
    copied = D(vector<int>{}, 8);
    assigned = D(vector<int>{}, 8);
    copied.insert(0, 9);
    assigned.insert(0, 10);
    verify_mutable(copied, vector<int>{9});
    verify_mutable(assigned, vector<int>{10});
    verify_mutable(original, a);
}

void test_large() {
    mt19937_64 rng(1000003);
    vector<long long> a(4096);
    const auto value = [&]() { return (static_cast<long long>(rng() % 39) - 19) * 1000000007LL; };
    for (auto &x : a) x = value();
    titan23::PointUpdateRangeDistinctQuery<long long> point(a);
    titan23::DynamicRangeDistinctQuery<long long> full(a);
    verify_mutable(point, a);
    verify_mutable(full, a);

    for (int q = 0; q < 4000; ++q) {
        const int k = rng() % a.size();
        const long long x = value();
        point.set(k, x);
        full.set(k, x);
        a[k] = x;
        if (q % 8 == 0) {
            int l = rng() % (a.size() + 1), r = rng() % (a.size() + 1);
            if (l > r) swap(l, r);
            const int expected = naive(a, l, r);
            assert(point.count(l, r) == expected);
            assert(full.count(l, r) == expected);
        }
    }
    verify_mutable(point, a);
    verify_mutable(full, a);

    for (int q = 0; q < 4000; ++q) {
        const int k = q % 3 == 0 ? 0 : rng() % (a.size() + 1);
        const long long x = value();
        full.insert(k, x);
        a.insert(a.begin() + k, x);
        if (q % 101 == 0) {
            int l = rng() % (a.size() + 1), r = rng() % (a.size() + 1);
            if (l > r) swap(l, r);
            assert(full.count(l, r) == naive(a, l, r));
        }
    }
    verify_mutable(full, a);
    for (int q = 0; q < 4000; ++q) {
        const int k = q % 3 == 0 ? 0 : q % 3 == 1 ? a.size() - 1 : a.size() / 2;
        const long long removed = full.pop(k);
        assert(removed == a[k]);
        a.erase(a.begin() + k);
        if (q % 101 == 0) {
            int l = rng() % (a.size() + 1), r = rng() % (a.size() + 1);
            if (l > r) swap(l, r);
            assert(full.count(l, r) == naive(a, l, r));
        }
    }
    verify_mutable(full, a);
}

} // namespace

int main() {
    test_static();
    const vector<int> links = {4, 7, 4, 4, 7, 9, 4, 9};
    test_previous_links(titan23::PointUpdateRangeDistinctQuery<int>(links));
    test_previous_links(titan23::DynamicRangeDistinctQuery<int>(links, 8));
    const vector<Key> keys = {{1, "one"}, {2, "two"}, {1, "other"}, {3, "three"}};
    verify(titan23::StaticRangeDistinctQuery<Key>(keys), keys);
    test_ordering_only(titan23::PointUpdateRangeDistinctQuery<Key>(keys));
    test_ordering_only(titan23::DynamicRangeDistinctQuery<Key>(keys, 8));
    titan23::DynamicRangeDistinctQuery<Key> keyed(keys, 8);
    vector<Key> keyed_values = keys;
    const Key x = keyed_values[2];
    keyed.insert(0, keyed.access(2));
    keyed_values.insert(keyed_values.begin(), x);
    verify_keyed(keyed, keyed_values);
    Key removed = keyed.pop(2);
    assert(removed.id == keyed_values[2].id && removed.text == keyed_values[2].text);
    keyed_values.erase(keyed_values.begin() + 2);
    verify_keyed(keyed, keyed_values);
    const vector<long long> numbers = {
        numeric_limits<long long>::min(), numeric_limits<long long>::max(),
        -(1LL << 60), 1LL << 60, -1, 0, 1, 3, 7
    };
    const vector<string> words = {"", "a", "b", "alpha", "beta", "alpha beta"};
    test_point(numbers, 123);
    test_point(words, 456);
    for (uint64_t seed : {17, 999, 2026}) test_dynamic(numbers, seed);
    test_dynamic(words, 314159);
    test_blocks();
    test_copy_move();
    test_large();
    cout << "Range distinct query test: OK\n";
}
