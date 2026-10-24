#include <algorithm>
#include <chrono>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <random>
#include <string>
#include <vector>
#include "titan_cpplib/ds/range_distinct_query_static.cpp"
#include "titan_cpplib/ds/range_distinct_query_point_update.cpp"
#include "titan_cpplib/ds/range_distinct_query_dynamic.cpp"
using namespace std;

// g++ -std=c++17 -O3 -DNDEBUG -march=native -I. test/ds/benchmark/range_distinct_query_speed_benchmark.cpp -o /tmp/rdq_bench
// /tmp/rdq_bench [static|point|dynamic|edit] [n=300000] [q=300000] [sigma=n] [block=512]
// point/dynamic は同じ一点変更・区間クエリ列
// edit は挿入・削除も混ぜる

using Clock = chrono::steady_clock;

double ms(Clock::time_point a, Clock::time_point b) {
    return chrono::duration<double, milli>(b-a).count();
}

struct Query {
    int op, l, r, x;
};

template<class DS>
uint64_t run_point(DS &ds, const vector<Query> &qs) {
    uint64_t sum = 0;
    for (const auto &q : qs) {
        if (q.op == 0) sum += ds.count(q.l, q.r);
        else ds.set(q.l, q.x);
    }
    return sum;
}

int main(int argc, char **argv) {
    string mode = argc > 1 ? argv[1] : "static";
    int n = argc > 2 ? stoi(argv[2]) : 300000;
    int q = argc > 3 ? stoi(argv[3]) : 300000;
    int sigma = argc > 4 ? stoi(argv[4]) : max(1, n);
    int block = argc > 5 ? stoi(argv[5]) : 512;
    if (n < 0 || q < 0 || sigma <= 0 || block < 2 ||
        (mode != "static" && mode != "point" && mode != "dynamic" && mode != "edit")) {
        cerr << "invalid arguments\n";
        return 1;
    }
    mt19937 rng(20261024);
    vector<int> a(n);
    for (auto &x : a) x = rng() % sigma;
    vector<Query> qs;
    qs.reserve(q);
    int len = n;
    for (int i = 0; i < q; ++i) {
        int op = mode == "static" ? 0 : rng() % (mode == "edit" ? 4 : 2);
        if (!len) op = mode == "edit" ? 2 : 0;
        int l = 0, r = 0, x = rng() % sigma;
        if (op == 0) {
            l = rng() % (len+1);
            r = rng() % (len+1);
            if (l > r) swap(l, r);
        } else if (op == 2) {
            l = rng() % (len+1);
            ++len;
        } else {
            l = rng() % len;
            if (op == 3) --len;
        }
        qs.push_back({op, l, r, x});
    }

    uint64_t sum = 0;
    auto t0 = Clock::now(), t1 = t0, t2 = t0;
    if (mode == "static") {
        titan23::StaticRangeDistinctQuery<int> ds(a);
        t1 = Clock::now();
        for (const auto &q : qs) sum += ds.count(q.l, q.r);
        t2 = Clock::now();
    } else if (mode == "point") {
        titan23::PointUpdateRangeDistinctQuery<int> ds(a);
        t1 = Clock::now();
        sum = run_point(ds, qs);
        t2 = Clock::now();
    } else {
        titan23::DynamicRangeDistinctQuery<int> ds(a, block);
        t1 = Clock::now();
        if (mode == "dynamic") sum = run_point(ds, qs);
        else {
            for (const auto &q : qs) {
                if (q.op == 0) sum += ds.count(q.l, q.r);
                else if (q.op == 1) ds.set(q.l, q.x);
                else if (q.op == 2) ds.insert(q.l, q.x);
                else sum += ds.pop(q.l);
            }
        }
        t2 = Clock::now();
    }
    cout << fixed << setprecision(3)
         << "mode=" << mode << " n=" << n << " q=" << q
         << " sigma=" << sigma << " block=" << block
         << " build_ms=" << ms(t0,t1) << " run_ms=" << ms(t1,t2)
         << " checksum=" << sum << '\n';
}
