#include <algorithm>
#include <cassert>
#include <iostream>
#include <vector>

#include "titan_cpplib/ds/used_set.cpp"

using namespace std;

void verify(const titan23::UsedSet &set, const vector<bool> &used) {
    int n = used.size();
    int cnt = 0;
    vector<bool> seen_use(n), seen_unuse(n);
    for (int v = 0; v < n; ++v) {
        assert(set.contains_use(v) == used[v]);
        assert(set.contains_unuse(v) != used[v]);
        cnt += used[v];
    }
    assert(set.len_use() == cnt);
    assert(set.len_unuse() == n - cnt);
    assert(set.empty_use() == (cnt == 0));
    assert(set.empty_unuse() == (cnt == n));
    for (int i = 0; i < set.len_use(); ++i) {
        int v = set.get_use(i);
        assert(used[v] && !seen_use[v]);
        seen_use[v] = true;
    }
    for (int i = 0; i < set.len_unuse(); ++i) {
        int v = set.get_unuse(i);
        assert(!used[v] && !seen_unuse[v]);
        seen_unuse[v] = true;
    }
}

void test_random(int seed) {
    int n = 100;
    titan23::Random rnd(seed);
    titan23::UsedSet set(n);
    vector<bool> used(n);
    verify(set, used);
    for (int q = 0; q < 100000; ++q) {
        int op = rnd.randrange(8);
        if (op == 0 && set.len_unuse()) {
            int v = set.rnd_get_unuse(rnd);
            set.use(v);
            used[v] = true;
        } else if (op == 1 && set.len_use()) {
            int v = set.rnd_get_use(rnd);
            set.unuse(v);
            used[v] = false;
        } else if (op == 2) {
            set.all_use();
            fill(used.begin(), used.end(), true);
        } else if (op == 3) {
            set.all_unuse();
            fill(used.begin(), used.end(), false);
        } else if (op == 4 && set.len_use()) {
            int v = set.rnd_pop_use(rnd);
            assert(used[v]);
            used[v] = false;
        } else if (op == 5 && set.len_unuse()) {
            int v = set.rnd_pop_unuse(rnd);
            assert(!used[v]);
            used[v] = true;
        } else if (op == 6 && set.len_use()) {
            assert(used[set.rnd_get_use(rnd)]);
        } else if (op == 7 && set.len_unuse()) {
            assert(!used[set.rnd_get_unuse(rnd)]);
        }
        if (q % 97 == 0) verify(set, used);
    }
    verify(set, used);
}

int main() {
    titan23::UsedSet empty;
    assert(empty.empty_use() && empty.empty_unuse());
    test_random(123456789);
    test_random(987654321);
    cout << "OK\n";
    return 0;
}
