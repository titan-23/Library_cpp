/// https://github.com/titan-23/Library_cpp/blob/main/test/string/edit_distance_test.cpp
#include <algorithm>
#include <array>
#include <cassert>
#include <climits>
#include <cstddef>
#include <deque>
#include <iostream>
#include <optional>
#include <random>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>
#include "titan_cpplib/string/edit_distance.cpp"

using namespace std;
using namespace titan23;

template <class S, class T>
int brute_edit_distance(const S &s, const T &t) {
    int n = (int)s.size(), m = (int)t.size();
    vector<vector<int>> dp(n + 1, vector<int>(m + 1));
    for (int i = 0; i <= n; ++i) dp[i][0] = i;
    for (int j = 0; j <= m; ++j) dp[0][j] = j;
    for (int i = 1; i <= n; ++i) {
        for (int j = 1; j <= m; ++j) {
            dp[i][j] = min({dp[i - 1][j] + 1, dp[i][j - 1] + 1,
                            dp[i - 1][j - 1] + !(s[i - 1] == t[j - 1])});
        }
    }
    return dp.back().back();
}

template <class S, class T>
void verify_restore(const S &s, const T &t, int distance,
                    const vector<EditOperation> &operations) {
    assert((int)operations.size() == distance);
    int n = (int)s.size(), m = (int)t.size();
    int i = 0, j = 0;
    for (const auto &operation : operations) {
        assert(i <= operation.source_index && operation.source_index <= n);
        assert(j <= operation.target_index && operation.target_index <= m);
        // 添字は編集後の配列ではなく、元の列における位置を表す
        while (i < operation.source_index && j < operation.target_index) {
            assert(s[i] == t[j]);
            ++i;
            ++j;
        }
        assert(i == operation.source_index && j == operation.target_index);
        switch (operation.type) {
            case EditOperationType::Insert:
                assert(j < m);
                ++j;
                break;
            case EditOperationType::Delete:
                assert(i < n);
                ++i;
                break;
            case EditOperationType::Replace:
                assert(i < n && j < m);
                assert(!(s[i] == t[j]));
                ++i;
                ++j;
                break;
            default:
                assert(false);
        }
    }
    while (i < n && j < m) {
        assert(s[i] == t[j]);
        ++i;
        ++j;
    }
    assert(i == n && j == m);
}

void verify_deterministic(const vector<EditOperation> &a,
                          const vector<EditOperation> &b) {
    assert(a.size() == b.size());
    for (int i = 0; i < (int)a.size(); ++i) {
        assert(a[i].type == b[i].type);
        assert(a[i].source_index == b[i].source_index);
        assert(a[i].target_index == b[i].target_index);
    }
}

template <class S, class T>
void check_bound(const S &s, const T &t, int distance, int k) {
    int expected = distance <= k ? distance : -1;
    assert(edit_distance_bounded(s, t, k) == expected);
    assert(edit_distance_bounded(t, s, k) == expected);
    assert(edit_distance_within(s, t, k) == (expected != -1));
    assert(edit_distance_within(t, s, k) == (expected != -1));
}

template <class S, class T>
void verify_restore_bound(const S &s, const T &t, int d, int k,
                          const optional<vector<EditOperation>> &res) {
    const bool ok = k >= 0 && d <= k;
    assert(res.has_value() == ok);
    if (res) verify_restore(s, t, d, *res);
}

template <class S, class T>
void check_restore_bound(const S &s, const T &t, int d, int k) {
    verify_restore_bound(s, t, d, k, edit_distance_restore_bounded(s, t, k));
    verify_restore_bound(t, s, d, k, edit_distance_restore_bounded(t, s, k));
}

template <class S, class T>
void check_pair(const S &s, const T &t) {
    int distance = brute_edit_distance(s, t);
    assert(edit_distance(s, t) == distance);
    assert(edit_distance(t, s) == distance);
    auto forward = edit_distance_restore(s, t);
    auto backward = edit_distance_restore(t, s);
    verify_restore(s, t, distance, forward);
    verify_restore(t, s, distance, backward);
    verify_deterministic(forward, edit_distance_restore(s, t));
    verify_deterministic(backward, edit_distance_restore(t, s));
    int d = distance;
    for (int k : {INT_MIN, -1, 0, 1, d - 1, d, d + 1, INT_MAX}) {
        check_bound(s, t, distance, k);
        auto a = edit_distance_restore_bounded(s, t, k);
        auto b = edit_distance_restore_bounded(t, s, k);
        verify_restore_bound(s, t, distance, k, a);
        verify_restore_bound(t, s, distance, k, b);
        if (a) verify_deterministic(forward, *a);
        if (b) verify_deterministic(backward, *b);
    }
}

template <class S, class T>
void check_string_overloads(const S &s, const T &t) {
    string_view sv(s), tv(t);
    int distance = brute_edit_distance(sv, tv);
    assert(edit_distance(s, t) == distance);
    assert(edit_distance(t, s) == distance);
    int d = distance;
    for (int k : {-1, 0, d - 1, d, d + 1, INT_MAX}) {
        check_bound(s, t, distance, k);
        verify_restore_bound(sv, tv, distance, k,
                             edit_distance_restore_bounded(s, t, k));
        verify_restore_bound(tv, sv, distance, k,
                             edit_distance_restore_bounded(t, s, k));
    }
    verify_restore(sv, tv, distance, edit_distance_restore(s, t));
    verify_restore(tv, sv, distance, edit_distance_restore(t, s));
}

void test_strings() {
    static_assert(is_same_v<decltype(edit_distance("a", "b")), int>);
    static_assert(is_same_v<decltype(edit_distance_bounded("a", "b", 1)), int>);
    static_assert(is_same_v<decltype(edit_distance_restore("a", "b")),
                           vector<EditOperation>>);
    static_assert(is_same_v<decltype(edit_distance_restore_bounded("a", "b", 1)),
                           optional<vector<EditOperation>>>);
    static_assert(is_same_v<decltype(EditOperation::source_index), int>);
    static_assert(is_same_v<decltype(EditOperation::target_index), int>);

    check_pair(string(), string());
    check_pair(string(), string("abc"));
    check_pair(string("abc"), string("abc"));
    check_pair(string("abc"), string("xyz"));
    check_pair(string("kitten"), string("sitting"));
    check_pair(string("ab"), string("ba"));
    check_pair(string("ababab"), string("bababa"));
    check_pair(string("abcxyz"), string("xyzabc"));

    const char* source = "kitten";
    const char* target = "sitting";
    check_string_overloads("kitten", "sitting");
    check_string_overloads(source, target);
    check_string_overloads(source, string("sitting"));
    check_string_overloads(string("kitten"), "sitting");
    check_string_overloads(string_view("kitten"), target);
    check_string_overloads("kitten", string_view("sitting"));
    check_string_overloads(string(), "");
    check_string_overloads(string_view(), "abc");

    string s = "--kitten++", t = "!!sitting??";
    check_pair(string_view(s).substr(2, 6), string_view(t).substr(2, 7));
    check_pair(string_view(), string_view());
    check_pair(string("a\0b\0", 4), string("a\0c\0", 4));
    check_pair(string_view("a\0b\0", 4), string_view("ab\0", 3));
    check_string_overloads(string("a\0b", 3), "a");
    check_string_overloads("a\0b", "a");
}

void test_exhaustive() {
    vector<string> strings = {""};
    for (int length = 1; length <= 5; ++length) {
        for (int mask = 0; mask < (1 << length); ++mask) {
            string s(length, 'a');
            for (int i = 0; i < length; ++i) {
                if ((mask >> i) & 1) s[i] = 'b';
            }
            strings.push_back(s);
        }
    }
    for (const auto &s : strings) {
        for (const auto &t : strings) {
            check_pair(s, t);
            int distance = brute_edit_distance(s, t);
            int max_length = static_cast<int>(max(s.size(), t.size()));
            for (int k = 0; k <= max_length + 1; ++k) {
                check_bound(s, t, distance, k);
                check_restore_bound(s, t, distance, k);
            }
        }
    }
}

// 要素のコピーや operator!= を要求しないことを C++17 でも確認する
struct EqualityOnlyToken {
    int value;
    explicit EqualityOnlyToken(int value) : value(value) {}
    EqualityOnlyToken(const EqualityOnlyToken &) = delete;
    EqualityOnlyToken &operator=(const EqualityOnlyToken &) = delete;
    EqualityOnlyToken(EqualityOnlyToken &&) = default;
    EqualityOnlyToken &operator=(EqualityOnlyToken &&) = default;
    friend bool operator==(const EqualityOnlyToken &a, const EqualityOnlyToken &b) {
        return a.value == b.value;
    }
};

struct IntSizedSequence {
    array<int, 3> values;
    int size() const { return 3; }
    const int &operator[](int index) const { return values[index]; }
};

void test_generic_sequences() {
    check_pair(vector<int>(), deque<int>());
    check_pair(vector<int>{-5, 3, 3, 0, 8}, deque<int>{3, -5, 0, 8, 2});
    check_pair(array<int, 4>{-1, INT_MIN, 0, INT_MAX}, vector<int>{INT_MIN, 0, 6});
    check_pair(array<int, 0>{}, deque<int>{-3, 5});
    check_pair(vector<long long>{-1000000000000LL, 7, 9},
               array<long long, 4>{9, -1000000000000LL, 7, 9});
    check_pair(vector<int>{1, 2, 3}, deque<long>{1, 3, 4});
    check_pair(IntSizedSequence{{2, -5, 7}}, vector<int>{2, 7, 6, 6});
    check_pair(vector<bool>{true, false, false, true},
               array<bool, 3>{false, true, false});
    check_pair(u32string(U"こんにちは世界"), u32string(U"こんばんは世界！"));
    check_pair(u32string({U'猫', U'\0', U'犬'}), u32string({U'猫', U'鳥'}));

    vector<EqualityOnlyToken> s, t;
    for (int value : {1, 2, 1, 3, 5}) s.emplace_back(value);
    for (int value : {2, 1, 8, 3}) t.emplace_back(value);
    check_pair(s, t);
    array<EqualityOnlyToken, 2> a = {EqualityOnlyToken(1), EqualityOnlyToken(2)};
    check_pair(a, s);
}

string random_string(mt19937 &rng, int length) {
    string s(length, '\0');
    for (char &c : s) c = static_cast<char>(rng() % 5);
    return s;
}

void test_random() {
    mt19937 rng(230024);
    for (int trial = 0; trial < 500; ++trial) {
        auto s = random_string(rng, rng() % 31);
        auto t = random_string(rng, rng() % 31);
        check_pair(s, t);
    }
    for (int trial = 0; trial < 500; ++trial) {
        auto s = random_string(rng, rng() % 61);
        auto t = s;
        int count = rng() % 10;
        for (int i = 0; i < count; ++i) {
            int operation = rng() % 3;
            if (t.empty() || operation == 0) {
                t.insert(rng() % (t.size() + 1), 1, static_cast<char>(rng() % 5));
            } else if (operation == 1) {
                t.erase(rng() % t.size(), 1);
            } else {
                t[rng() % t.size()] = static_cast<char>(rng() % 5);
            }
        }
        check_pair(s, t);
    }
    for (int trial = 0; trial < 300; ++trial) {
        vector<int> s(rng() % 41);
        deque<int> t(rng() % 41);
        for (int &value : s) value = static_cast<int>(rng() % 11) - 5;
        for (int &value : t) value = static_cast<int>(rng() % 11) - 5;
        check_pair(s, t);
    }
}

void test_long_bounded_and_skewed_exact() {
    constexpr int n = 100000;
    string s(n, 'a'), t = s;
    check_restore_bound(s, t, 0, 0);
    t.front() = t[n / 2] = t.back() = 'b';
    check_bound(s, t, 3, 2);
    check_bound(s, t, 3, 3);
    check_bound(s, t, 3, 4);
    check_restore_bound(s, t, 3, 2);
    check_restore_bound(s, t, 3, 3);
    check_restore_bound(s, t, 3, 4);
    t = "bbbbbbb" + s;
    check_bound(s, t, 7, 6);
    check_bound(s, t, 7, 7);
    check_restore_bound(s, t, 7, 6);
    check_restore_bound(s, t, 7, 7);
    t.push_back('b');
    check_bound(s, t, 8, 7);
    check_bound(s, t, 8, 8);
    check_restore_bound(s, t, 8, 8);
    check_restore_bound(s, string(n, 'b'), n, 2);

    vector<int> a(n, -1), b = a;
    b.front() = b.back() = 2;
    check_bound(a, b, 2, 1);
    check_bound(a, b, 2, 2);
    check_restore_bound(a, b, 2, 2);

    assert(edit_distance(s, "aaa") == n - 3);
    assert(edit_distance("aaa", s) == n - 3);
    check_bound(s, "aaa", n - 3, n - 4);
    check_bound(s, "aaa", n - 3, n - 3);
    check_bound(s, "aaa", n - 3, INT_MAX);
    assert(edit_distance(s, "") == n);
    check_restore_bound(s, string("aaa"), n - 3, INT_MAX);
    check_restore_bound(s, string(), n, n - 1);
    check_restore_bound(s, string(), n, n);
}

struct HugeSequence {
    size_t n;
    size_t size() const { return n; }
    int operator[](int) const {
        assert(false);
        return 0;
    }
};

template <class F>
void expect_length_error(F f) {
    bool caught = false;
    try {
        f();
    } catch (const length_error &) {
        caught = true;
    }
    assert(caught);
}

void test_size_limit() {
    const vector<int> empty;
    auto check = [](const auto &s, const auto &t) {
        expect_length_error([&] { (void)edit_distance(s, t); });
        expect_length_error([&] { (void)edit_distance_bounded(s, t, INT_MAX); });
        expect_length_error([&] { (void)edit_distance_within(s, t, INT_MAX); });
        expect_length_error([&] { (void)edit_distance_restore(s, t); });
        expect_length_error([&] { (void)edit_distance_restore_bounded(s, t, INT_MAX); });
    };
    // 巨大な領域を確保せず、int への変換前に長さを拒否することを確認する
    for (size_t n : {(size_t)INT_MAX, (size_t)INT_MAX + 1}) {
        const HugeSequence s{n};
        check(s, empty);
        check(empty, s);
    }
    const HugeSequence s{(size_t)INT_MAX - 1};
    assert(edit_distance(s, empty) == INT_MAX - 1);
    assert(edit_distance(empty, s) == INT_MAX - 1);
    assert(edit_distance_bounded(s, empty, INT_MAX) == INT_MAX - 1);
    assert(edit_distance_bounded(empty, s, INT_MAX - 2) == -1);
    assert(edit_distance_within(s, empty, INT_MAX));
}

int main() {
    test_strings();
    test_exhaustive();
    test_generic_sequences();
    test_random();
    test_long_bounded_and_skewed_exact();
    test_size_limit();
    cout << "edit_distance tests passed\n";
}
