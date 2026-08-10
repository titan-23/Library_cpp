#include <algorithm>
#include <cassert>
#include <iostream>
#include <vector>

#include "titan_cpplib/ds/range_set.cpp"
#include "titan_cpplib/alg/random.cpp"

using namespace std;

// 値は [0, n) を扱う 番兵は -1 と n
const int NEG_INF = -1;

vector<pair<int, int>> to_ranges(const vector<bool> &b) {
    int n = b.size();
    vector<pair<int, int>> res;
    for (int i = 0; i < n; ++i) {
        if (!b[i]) continue;
        int j = i;
        while (j < n && b[j]) ++j;
        res.emplace_back(i, j);
        i = j;
    }
    return res;
}

void verify(const titan23::RangeSet<int> &rs, const vector<bool> &b) {
    int n = b.size();
    int POS_INF = n;
    vector<pair<int, int>> ranges = to_ranges(b);
    vector<pair<int, int>> gaps = to_ranges(vector<bool>(b.size(), true));
    {
        vector<bool> c(n);
        for (int i = 0; i < n; ++i) c[i] = !b[i];
        gaps = to_ranges(c);
    }

    assert(rs.tovector() == ranges);
    assert(rs.range_count() == (int)ranges.size());
    assert(rs.gap_count() == (int)gaps.size());
    assert(rs.empty() == ranges.empty());

    long long cnt = count(b.begin(), b.end(), true);
    assert(rs.len() == cnt);
    assert(rs.size() == cnt);

    int mn = POS_INF, mx = NEG_INF;
    for (int i = 0; i < n; ++i) {
        if (b[i]) {
            mn = min(mn, i);
            mx = max(mx, i);
        }
    }
    assert(rs.get_min() == mn);
    assert(rs.get_max() == mx);

    for (int x = 0; x < n; ++x) {
        assert(rs.contains(x) == b[x]);

        pair<int, int> want_range = {NEG_INF, NEG_INF};
        pair<int, int> want_gap = {NEG_INF, NEG_INF};
        for (const auto &[l, r] : ranges) {
            if (l <= x && x < r) want_range = {l, r};
        }
        for (const auto &[l, r] : gaps) {
            if (l <= x && x < r) want_gap = {l, r};
        }
        assert(rs.get_range(x) == want_range);
        assert(rs.get_gap(x) == want_gap);

        pair<int, int> want_next_range = {POS_INF, POS_INF};
        pair<int, int> want_prev_range = {NEG_INF, NEG_INF};
        for (const auto &[l, r] : ranges) {
            if (r > x && want_next_range.first == POS_INF) want_next_range = {l, r};
            if (l <= x) want_prev_range = {l, r};
        }
        assert(rs.next_range(x) == want_next_range);
        assert(rs.prev_range(x) == want_prev_range);

        pair<int, int> want_next_gap = {POS_INF, POS_INF};
        pair<int, int> want_prev_gap = {NEG_INF, NEG_INF};
        for (const auto &[l, r] : gaps) {
            if (r > x && want_next_gap.first == POS_INF) want_next_gap = {l, r};
            if (l <= x) want_prev_gap = {l, r};
        }
        assert(rs.next_gap(x) == want_next_gap);
        assert(rs.prev_gap(x) == want_prev_gap);

        int want_mex = POS_INF, want_rmex = NEG_INF;
        int want_ge = POS_INF, want_gt = POS_INF;
        int want_le = NEG_INF, want_lt = NEG_INF;
        for (int i = x; i < n; ++i) {
            if (!b[i]) { want_mex = i; break; }
        }
        for (int i = x; i >= 0; --i) {
            if (!b[i]) { want_rmex = i; break; }
        }
        for (int i = x; i < n; ++i) {
            if (b[i]) { want_ge = i; break; }
        }
        for (int i = x + 1; i < n; ++i) {
            if (b[i]) { want_gt = i; break; }
        }
        for (int i = x; i >= 0; --i) {
            if (b[i]) { want_le = i; break; }
        }
        for (int i = x - 1; i >= 0; --i) {
            if (b[i]) { want_lt = i; break; }
        }
        assert(rs.mex(x) == want_mex);
        assert(rs.rmex(x) == want_rmex);
        assert(rs.ge(x) == want_ge);
        assert(rs.gt(x) == want_gt);
        assert(rs.le(x) == want_le);
        assert(rs.lt(x) == want_lt);
    }

    for (int l = 0; l <= n; ++l) {
        for (int r = l; r <= n; ++r) {
            long long want_cnt = 0;
            bool want_all = true, want_any = false;
            for (int i = l; i < r; ++i) {
                want_cnt += b[i];
                want_all &= b[i];
                want_any |= b[i];
            }
            assert(rs.count_range(l, r) == want_cnt);
            assert(rs.contains(l, r) == want_all);
            assert(rs.intersects(l, r) == want_any);
            if (l < r) assert(rs.same(l, r - 1) == want_all);

            vector<pair<int, int>> want_each, got_each;
            for (const auto &[a, b2] : ranges) {
                int p = max(a, l), q = min(b2, r);
                if (p < q) want_each.emplace_back(p, q);
            }
            rs.for_each_range(l, r, [&] (int a, int b2) { got_each.emplace_back(a, b2); });
            assert(got_each == want_each);

            vector<pair<int, int>> want_gap_each, got_gap_each;
            for (const auto &[a, b2] : gaps) {
                int p = max(a, l), q = min(b2, r);
                if (p < q) want_gap_each.emplace_back(p, q);
            }
            rs.for_each_gap(l, r, [&] (int a, int b2) { got_gap_each.emplace_back(a, b2); });
            assert(got_gap_each == want_gap_each);
        }
    }
}

void test_update(int seed) {
    int n = 20;
    titan23::Random rnd(seed);
    titan23::RangeSet<int> rs(NEG_INF, n);
    vector<bool> b(n, false);
    verify(rs, b);
    for (int q = 0; q < 300; ++q) {
        int op = rnd.randrange(8);
        int l = rnd.randrange(0, n);
        int r = rnd.randrange(l, n + 1);
        int x = rnd.randrange(n);
        if (op == 0) {
            long long want = 0;
            for (int i = l; i < r; ++i) {
                if (!b[i]) ++want;
                b[i] = true;
            }
            assert(rs.add(l, r) == want);
        } else if (op == 1) {
            long long want = 0;
            for (int i = l; i < r; ++i) {
                if (b[i]) ++want;
                b[i] = false;
            }
            assert(rs.remove(l, r) == want);
        } else if (op == 2) {
            assert(rs.add(x) == !b[x]);
            b[x] = true;
        } else if (op == 3) {
            assert(rs.discard(x) == b[x]);
            b[x] = false;
        } else if (op == 4) {
            if (!b[x]) continue;
            rs.remove(x);
            b[x] = false;
        } else if (op == 5) {
            b[x] = !b[x];
            assert(rs.toggle(x) == b[x]);
        } else if (op == 6) {
            long long before = count(b.begin(), b.end(), true);
            long long got = rs.flip(l, r);
            for (int i = l; i < r; ++i) {
                b[i] = !b[i];
            }
            assert(got == count(b.begin(), b.end(), true) - before);
        } else {
            if (rnd.randrange(2) == 0) {
                rs.clear();
                fill(b.begin(), b.end(), false);
            } else {
                rs.fill();
                fill(b.begin(), b.end(), true);
            }
        }
        verify(rs, b);
    }
}

void test_setop(int seed) {
    int n = 20;
    titan23::Random rnd(seed);
    for (int t = 0; t < 200; ++t) {
        titan23::RangeSet<int> s(NEG_INF, n), u(NEG_INF, n);
        vector<bool> x(n), y(n);
        for (int i = 0; i < n; ++i) {
            if (rnd.randrange(2)) {
                x[i] = true;
                s.add(i);
            }
            if (rnd.randrange(2)) {
                y[i] = true;
                u.add(i);
            }
        }
        vector<bool> wor(n), wand(n), wsub(n), wxor(n), wnot(n);
        bool inter = false, subset = true;
        for (int i = 0; i < n; ++i) {
            wor[i] = x[i] || y[i];
            wand[i] = x[i] && y[i];
            wsub[i] = x[i] && !y[i];
            wxor[i] = x[i] != y[i];
            wnot[i] = !x[i];
            if (x[i] && y[i]) inter = true;
            if (x[i] && !y[i]) subset = false;
        }
        verify(s | u, wor);
        verify(s & u, wand);
        verify(s - u, wsub);
        verify(s ^ u, wxor);
        verify(s.complement(), wnot);

        assert(s.intersects(u) == inter);
        assert(s.is_subset_of(u) == subset);
        assert(u.is_superset_of(s) == subset);
        assert((s == u) == (x == y));
        assert((s != u) == (x != y));

        titan23::RangeSet<int> v = s;
        v |= u;
        verify(v, wor);
        v = s;
        v &= u;
        verify(v, wand);
        v = s;
        v -= u;
        verify(v, wsub);
        v = s;
        v ^= u;
        verify(v, wxor);
    }
}

void test_build(int seed) {
    int n = 20;
    titan23::Random rnd(seed);
    for (int t = 0; t < 200; ++t) {
        vector<bool> b(n);
        vector<pair<int, int>> ranges;
        int m = rnd.randrange(1, 6);
        for (int i = 0; i < m; ++i) {
            int l = rnd.randrange(0, n);
            int r = rnd.randrange(l, n + 1);
            ranges.emplace_back(l, r);
            for (int j = l; j < r; ++j) {
                b[j] = true;
            }
        }
        rnd.shuffle(ranges);
        verify(titan23::RangeSet<int>(ranges, NEG_INF, n), b);

        vector<int> a;
        vector<bool> c(n);
        int k = rnd.randrange(1, 10);
        for (int i = 0; i < k; ++i) {
            int x = rnd.randrange(n);
            a.emplace_back(x);
            c[x] = true;
        }
        verify(titan23::RangeSet<int>(a, NEG_INF, n), c);
    }
}

int main() {
    for (int seed = 0; seed < 10; ++seed) {
        test_update(seed);
        test_setop(seed);
        test_build(seed);
    }
    cerr << "OK." << endl;
    return 0;
}
