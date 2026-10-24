/// https://github.com/titan-23/Library_cpp/blob/main/test/string/edit_distance_query_test.cpp
#include <algorithm>
#include <array>
#include <cassert>
#include <climits>
#include <deque>
#include <iostream>
#include <random>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
#include "titan_cpplib/string/edit_distance_query.cpp"

using namespace std;
using titan23::EditDistanceQuery;

template <class S, class T>
int brute_edit_distance(const S &s, int l1, int r1,
                        const T &t, int l2, int r2) {
    int n = r1 - l1, m = r2 - l2;
    vector<vector<int>> dp(n + 1, vector<int>(m + 1));
    for (int i = 0; i <= n; ++i) dp[i][0] = i;
    for (int j = 0; j <= m; ++j) dp[0][j] = j;
    for (int i = 1; i <= n; ++i) {
        for (int j = 1; j <= m; ++j) {
            dp[i][j] = min({dp[i - 1][j] + 1, dp[i][j - 1] + 1,
                            dp[i - 1][j - 1] + !(s[l1 + i - 1] == t[l2 + j - 1])});
        }
    }
    return dp[n][m];
}

void check_bound(const EditDistanceQuery &query, int l1, int r1,
                 int l2, int r2, int distance, int k) {
    int expected = distance <= k ? distance : -1;
    assert(query.distance_bounded(l1, r1, l2, r2, k) == expected);
    assert(query.within(l1, r1, l2, r2, k) == (expected >= 0));
}

void check_result(const EditDistanceQuery &query, int l1, int r1,
                  int l2, int r2, int distance) {
    assert(query.distance(l1, r1, l2, r2) == distance);
    // 成功・失敗・再度成功する順番でも内部状態が残らないことを確認する
    for (int k : {INT_MAX, INT_MIN, distance, distance - 1, 0,
                  distance + 1, -1, distance}) {
        check_bound(query, l1, r1, l2, r2, distance, k);
    }
    assert(query.distance(l1, r1, l2, r2) == distance);
}

template <class S, class T>
void check_all_ranges(const EditDistanceQuery &query, const S &s, const T &t) {
    int n = static_cast<int>(s.size()), m = static_cast<int>(t.size());
    assert(query.source_size() == n);
    assert(query.target_size() == m);
    for (int l1 = 0; l1 <= n; ++l1) {
        for (int r1 = l1; r1 <= n; ++r1) {
            for (int l2 = 0; l2 <= m; ++l2) {
                for (int r2 = l2; r2 <= m; ++r2) {
                    int d = brute_edit_distance(s, l1, r1, t, l2, r2);
                    check_result(query, l1, r1, l2, r2, d);
                }
            }
        }
    }
}

template <class S, class T>
void check_pair(const S &s, const T &t) {
    const EditDistanceQuery query(s, t);
    check_all_ranges(query, s, t);
}

template <class S, class T>
void check_string_inputs(const S &s, const T &t) {
    const EditDistanceQuery query(s, t);
    check_all_ranges(query, string_view(s), string_view(t));
}

void test_basic_and_string_inputs() {
    check_string_inputs("", "");
    check_string_inputs("abc", "");
    check_string_inputs("", "abc");
    check_string_inputs("ab", "ac");
    check_string_inputs("ab", "ba");
    check_string_inputs("kitten", "sitting");
    check_string_inputs("aaaaaa", "aaaaaaa");
    check_string_inputs("abababa", "bababab");
    check_string_inputs("abcdef", "uvwxyz");
    const char* s = "abac";
    const char* t = "cab";
    check_string_inputs(s, t);
    check_string_inputs(s, string("cab"));
    check_string_inputs(string("abac"), "cab");
    check_string_inputs(string_view("abac"), t);
    check_string_inputs(string_view(), string_view("a"));

    string source = "--abac++", target = "!!cab??";
    check_pair(string_view(source).substr(2, 4), string_view(target).substr(2, 3));
    check_pair(string("a\0b\0", 4), string("a\0c\0", 4));
    check_pair(string_view("a\0b\0", 4), string_view("ab\0", 3));
    check_string_inputs("a\0b", "a");
    check_string_inputs(string("a\0b", 3), "a");

    string high_s, high_t;
    for (int c : {0, 127, 128, 255, 1, 128}) high_s.push_back(static_cast<char>(c));
    for (int c : {128, 255, 0, 127, 1}) high_t.push_back(static_cast<char>(c));
    check_pair(high_s, high_t);
}

void test_exhaustive() {
    vector<string> strings = {""};
    for (int length = 1; length <= 3; ++length) {
        for (int mask = 0; mask < (1 << length); ++mask) {
            string s(length, 'a');
            for (int i = 0; i < length; ++i) {
                if ((mask >> i) & 1) s[i] = 'b';
            }
            strings.push_back(s);
        }
    }
    for (const auto &s : strings) {
        for (const auto &t : strings) check_pair(s, t);
    }
}

struct OrderedToken {
    int value;
    friend bool operator<(const OrderedToken &a, const OrderedToken &b) {
        return a.value < b.value;
    }
    friend bool operator==(const OrderedToken &a, const OrderedToken &b) {
        return a.value == b.value;
    }
};

struct IntSizedSequence {
    array<int, 4> values;
    int size() const { return 4; }
    const int &operator[](int index) const { return values[index]; }
};

void test_generic_sequences() {
    check_pair(vector<int>(), vector<int>());
    check_pair(vector<int>{INT_MIN, -5, 0, INT_MAX, 8},
               vector<int>{INT_MAX, 0, -5, 7});
    check_pair(deque<int>{-1, 4, -1, 9}, array<int, 3>{4, 9, 2});
    check_pair(IntSizedSequence{{2, -3, 2, 7}}, vector<int>{-3, 7, 2});
    check_pair(array<int, 0>{}, vector<int>{1, 2});
    check_pair(vector<long long>{LLONG_MIN, -1000000000000LL, LLONG_MAX},
               array<long long, 4>{LLONG_MAX, LLONG_MIN, 7, LLONG_MAX});
    check_pair(vector<bool>{false, true, true, false}, array<bool, 3>{true, false, true});
    check_pair(vector<OrderedToken>{{3}, {-5}, {3}, {8}},
               deque<OrderedToken>{{-5}, {7}, {8}});
    check_pair(u32string(U"日本語の列"), u32string(U"日本の文字"));
}

void test_random() {
    mt19937 rng(2026102401);
    for (int trial = 0; trial < 100; ++trial) {
        string s(rng() % 9, '\0'), t(rng() % 9, '\0');
        for (char &c : s) c = static_cast<char>(rng() % 4);
        for (char &c : t) c = static_cast<char>(rng() % 4);
        check_pair(s, t);
    }

    for (int trial = 0; trial < 80; ++trial) {
        vector<int> s(rng() % 31), t(rng() % 31);
        for (int &value : s) value = static_cast<int>(rng() % 11) - 5;
        for (int &value : t) value = static_cast<int>(rng() % 11) - 5;
        const EditDistanceQuery query(s, t);
        const EditDistanceQuery reverse(t, s);
        for (int step = 0; step < 100; ++step) {
            int l1 = rng() % (s.size() + 1), r1 = rng() % (s.size() + 1);
            int l2 = rng() % (t.size() + 1), r2 = rng() % (t.size() + 1);
            if (l1 > r1) swap(l1, r1);
            if (l2 > r2) swap(l2, r2);
            int d = brute_edit_distance(s, l1, r1, t, l2, r2);
            check_result(query, l1, r1, l2, r2, d);
            check_result(reverse, l2, r2, l1, r1, d);
            int k = static_cast<int>(rng() % 35) - 2;
            check_bound(query, l1, r1, l2, r2, d, k);
        }
    }
}

EditDistanceQuery query_from_local_inputs() {
    string s = "abracadabra", t = "cadabraabra";
    return EditDistanceQuery(string_view(s), string_view(t));
}

void test_input_ownership() {
    string s = "kitten", t = "sitting";
    const string original_s = s, original_t = t;
    const EditDistanceQuery query(s, t);
    s.assign(10000, 'x');
    t.clear();
    t.shrink_to_fit();
    check_all_ranges(query, original_s, original_t);

    const auto local_query = query_from_local_inputs();
    check_all_ranges(local_query, string("abracadabra"), string("cadabraabra"));
    const EditDistanceQuery temporary_query(string("abba"), string("baba"));
    check_all_ranges(temporary_query, string("abba"), string("baba"));

    vector<int> a = {-1, 3, 8}, b = {3, 8, 2, -1};
    const vector<int> original_a = a, original_b = b;
    const EditDistanceQuery integer_query(a, b);
    a.assign(10000, 42);
    b.clear();
    b.shrink_to_fit();
    check_all_ranges(integer_query, original_a, original_b);
}

void test_large_preprocessing_and_small_distances() {
    constexpr int n = 100000;
    string s(n, 'a'), t = s;
    const array<int, 3> changes = {123, n / 2, n - 1};
    for (int pos : changes) t[pos] = 'b';
    const EditDistanceQuery query(s, t);
    assert(query.source_size() == n);
    assert(query.target_size() == n);
    // 同じ範囲同士なら、t に含まれる b の個数が編集距離になる
    for (const auto &range : vector<pair<int, int>>{
             {0, n}, {0, 123}, {123, 124}, {124, n / 2},
             {n / 2, n}, {n - 1, n}, {n, n}, {100, 90000}}) {
        int left = range.first, right = range.second;
        int distance = 0;
        for (int pos : changes) distance += left <= pos && pos < right;
        assert(query.distance(left, right, left, right) == distance);
        for (int k : {-1, 0, distance - 1, distance, distance + 1}) {
            check_bound(query, left, right, left, right, distance, k);
        }
    }
    // 長い一致区間の途中に置いた終端を LCE が越えないことを確認する
    check_result(query, 1000, 40000, 1001, 40000, 1);
    check_result(query, 2000, 30000, 2005, 30000, 5);
    check_result(query, 0, 1, n - 1, n, 1);
    check_bound(query, 0, n, n, n, n, 3);
    check_bound(query, n, n, 0, n, n, 3);
}

struct SizedSequence {
    long long n;
    long long size() const { return n; }
    int operator[](int) const {
        assert(false);
        return 0;
    }
};

void test_size_limit() {
    // 区切りの 1 要素も含めて int の範囲に収まる必要がある
    for (const auto &lens : vector<pair<long long, long long>>{
             {INT_MAX, 0}, {0, INT_MAX}, {INT_MAX - 1, 1},
             {INT_MAX / 2, INT_MAX / 2 + 1}}) {
        bool caught = false;
        try {
            const EditDistanceQuery query(SizedSequence{lens.first},
                                          SizedSequence{lens.second});
        } catch (const length_error &) {
            caught = true;
        }
        assert(caught);
    }
}

int main() {
    test_basic_and_string_inputs();
    test_exhaustive();
    test_generic_sequences();
    test_random();
    test_input_ownership();
    test_large_preprocessing_and_small_distances();
    test_size_limit();
    cout << "edit_distance_query tests passed\n";
}
