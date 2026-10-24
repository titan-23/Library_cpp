/// https://github.com/titan-23/Library_cpp/blob/main/test/string/edit_distance_within_test.cpp
#include <algorithm>
#include <cassert>
#include <climits>
#include <iostream>
#include <random>
#include <string>
#include <string_view>
#include <vector>
#include "titan_cpplib/string/edit_distance.cpp"

using namespace std;
using titan23::edit_distance_within;

int brute_edit_distance(string_view s, string_view t) {
    int n = static_cast<int>(s.size());
    int m = static_cast<int>(t.size());
    vector<vector<int>> dp(n + 1, vector<int>(m + 1));
    for (int i = 0; i <= n; ++i) dp[i][0] = i;
    for (int j = 0; j <= m; ++j) dp[0][j] = j;
    for (int i = 1; i <= n; ++i) {
        for (int j = 1; j <= m; ++j) {
            dp[i][j] = min({dp[i - 1][j] + 1, dp[i][j - 1] + 1,
                            dp[i - 1][j - 1] + (s[i - 1] != t[j - 1])});
        }
    }
    return dp[n][m];
}

void check(string_view s, string_view t, int k, bool expected) {
    assert(edit_distance_within(s, t, k) == expected);
    assert(edit_distance_within(t, s, k) == expected);
}

void check_thresholds(string_view s, string_view t) {
    int d = brute_edit_distance(s, t);
    int max_length = static_cast<int>(max(s.size(), t.size()));
    for (int k : {INT_MIN, -1, 0, d - 1, d, d + 1, max_length, INT_MAX}) {
        check(s, t, k, d <= k);
    }
}

void test_basic() {
    check("", "", -1, false);
    check("", "", 0, true);
    check("", "abc", 2, false);
    check("", "abc", 3, true);
    check("abc", "abc", 0, true);
    check("abc", "axc", 0, false);
    check("abc", "axc", 1, true);
    check("abc", "ac", 1, true);
    check("abc", "abxc", 1, true);
    check("kitten", "sitting", 2, false);
    check("kitten", "sitting", 3, true);
    // 隣接文字の交換は1操作ではない
    check("ab", "ba", 1, false);
    check("ab", "ba", 2, true);
    check_thresholds("abcdefgh", "ijklmnop");
    check_thresholds("abababab", "babababa");
    check_thresholds("abc", "xyzabcxyz");
    check_thresholds("", "");
    check_thresholds("", "abc");
}

void test_views_and_null_bytes() {
    string s = "--kitten++";
    string t = "!!sitting??";
    check_thresholds(string_view(s).substr(2, 6), string_view(t).substr(2, 7));
    check(string_view(), string_view(), 0, true);
    check(string_view(), "x", 0, false);

    const char a[] = {'a', '\0', 'b', '\0'};
    const char b[] = {'a', '\0', 'c', '\0'};
    const char c[] = {'a', 'b', '\0'};
    check(string_view(a, 4), string_view(b, 4), 0, false);
    check(string_view(a, 4), string_view(b, 4), 1, true);
    check(string_view(a, 4), string_view(c, 3), 1, true);
    check_thresholds(string_view(a, 4), string_view(b, 4));
    check_thresholds(string(a, 4), string(c, 3));
}

void test_exhaustive() {
    vector<string> strings = {""};
    for (int length = 1; length <= 6; ++length) {
        for (int mask = 0; mask < (1 << length); ++mask) {
            string s(length, 'a');
            for (int i = 0; i < length; ++i) {
                if ((mask >> i) & 1) s[i] = 'b';
            }
            strings.push_back(s);
        }
    }
    for (const string &s : strings) {
        for (const string &t : strings) {
            int d = brute_edit_distance(s, t);
            int max_length = static_cast<int>(max(s.size(), t.size()));
            for (int k = -1; k <= max_length + 1; ++k) {
                check(s, t, k, d <= k);
            }
        }
    }
}

string random_string(mt19937& rng, int length, bool bytes) {
    string s(length, '\0');
    for (char &c : s) {
        c = bytes ? static_cast<char>(rng() % 256)
                  : static_cast<char>('a' + rng() % 4);
    }
    return s;
}

void test_random() {
    mt19937 rng(20261024);
    for (int trial = 0; trial < 2000; ++trial) {
        bool bytes = trial % 5 == 0;
        string s = random_string(rng, rng() % 81, bytes);
        string t = random_string(rng, rng() % 81, bytes);
        check_thresholds(s, t);
    }
    for (int trial = 0; trial < 2000; ++trial) {
        string s = random_string(rng, rng() % 121, false);
        string t = s;
        int edits = rng() % 13;
        for (int step = 0; step < edits; ++step) {
            int operation = rng() % 3;
            if (t.empty() || operation == 0) {
                int pos = (int)(rng() % (t.size() + 1));
                t.insert(pos, 1, static_cast<char>('a' + rng() % 4));
            } else if (operation == 1) {
                t.erase(rng() % t.size(), 1);
            } else {
                t[rng() % t.size()] = static_cast<char>('a' + rng() % 4);
            }
        }
        check_thresholds(s, t);
    }
}

void test_long_small_thresholds() {
    const int n = 100000;
    string s(n, 'a');
    check(s, s, 0, true);
    check(s, s, INT_MIN, false);
    check(s, s, INT_MAX, true);

    string t = s;
    t.front() = 'b';
    t[n / 2] = 'b';
    t.back() = 'b';
    check(s, t, 2, false);
    check(s, t, 3, true);

    for (int k : {1, 2, 7}) {
        // 最適経路が帯の端に沿い、終端で長さの差がちょうど k になる
        t = string(k, 'b') + s;
        check(s, t, k - 1, false);
        check(s, t, k, true);
        t = s + string(k, 'b');
        check(s, t, k, true);
        t.push_back('b');
        check(s, t, k, false);
    }

    // 同じ長さでも、挿入と削除で帯の端まで移動して戻る場合
    s = "b" + string(n, 'a');
    t = string(n, 'a') + "b";
    check(s, t, 1, false);
    check(s, t, 2, true);

    // 行の最小値がしきい値を超えた後も長い接尾辞が続く
    s = string(n, 'a');
    t = "bbbb" + string(n - 4, 'a');
    check(s, t, 3, false);
    check(s, t, 4, true);
}

int main() {
    test_basic();
    test_views_and_null_bytes();
    test_exhaustive();
    test_random();
    test_long_small_thresholds();
    cout << "edit_distance_within tests passed\n";
}
