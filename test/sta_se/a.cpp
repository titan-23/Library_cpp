#include <bits/stdc++.h>
using namespace std;

#include "titan_cpplib/data_structures/static_set.cpp"
#include "titan_cpplib/data_structures/static_multiset.cpp"

void solve() {
    int n;
    cin >> n;
    vector<int> A(n);
    for (int i = 0; i < n; ++i) {
        cin >> A[i];
    }

    titan23::StaticMultiset<int> s(A, -1);

    vector<int> B = A;
    sort(B.begin(), B.end());

    // for set
    // B.erase(unique(B.begin(), B.end()), B.end());

    auto get_index = [&] (int key) -> int {
        int cnt = 0;
        for (int a : B) {
            if (a < key) ++cnt;
        }
        return cnt;
    };

    auto get_index_right = [&] (int key) -> int {
        int cnt = 0;
        for (int a : B) {
            if (a <= key) ++cnt;
        }
        return cnt;
    };

    auto get_count = [&] (int key) -> int {
        int cnt = 0;
        for (int a : B) {
            if (a == key) ++cnt;
        }
        return cnt;
    };

    auto get_count_range = [&] (int lower, int upper) -> int {
        int cnt = 0;
        for (int a : B) {
            if (lower <= a && a < upper) ++cnt;
        }
        return cnt;
    };

    auto get_contains = [&] (int key) -> bool {
        for (int a : B) {
            if (a == key) return true;
        }
        return false;
    };

    int q;
    cin >> q;
    while (--q) {
        int c;
        cin >> c;
        if (c == 0) { // index
            int key;
            cin >> key;
            assert(s.index(key) == get_index(key));
        } else if (c == 1) { // index_right
            int key;
            cin >> key;
            assert(s.index_right(key) == get_index_right(key));
        } else if (c == 2) { // count
            int key;
            cin >> key;
            assert(s.count(key) == get_count(key));
        } else if (c == 3) { // count_range
            int lower, upper;
            cin >> lower >> upper;
            assert(s.count_range(lower, upper) == get_count_range(lower, upper));
        } else if (c == 4) { // contains
            int key;
            cin >> key;
            assert(s.contains(key) == get_contains(key));
        }
    }

    cout << "test ok !" << endl;
}

int main() {
    ios::sync_with_stdio(false);
    cin.tie(0);
    solve();
    return 0;
}
