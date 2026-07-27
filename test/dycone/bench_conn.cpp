// 連結性のみの操作列で DyCone と DyConeSum を比較するベンチマーク
// 同一の操作列を両者に流し、答えの一致検証と実行時間の計測を行う
//
// g++ -std=c++20 -O2 -Wall -Wextra -I . -o bench_conn test/dycone/bench_conn.cpp
// ./bench_conn [n] [q] [seed] [mode]   mode: 0=両方(検証つき) 1=DyCone のみ 2=DyConeSum のみ

#include <bits/stdc++.h>
#include <sys/resource.h>
#include "titan_cpplib/ds/dy_cone.cpp"
#include "titan_cpplib/ds/dy_cone_sum.cpp"
using namespace std;
using ll = long long;

// 操作種別 0:add_edge 1:delete_edge 2:same 3:size 4:group_count
struct Op { int t, u, v; };

static double elapsed_ms(chrono::steady_clock::time_point a, chrono::steady_clock::time_point b) {
    return chrono::duration_cast<chrono::microseconds>(b - a).count() / 1000.0;
}

static long peak_rss_mb() {
    rusage ru;
    getrusage(RUSAGE_SELF, &ru);
    return ru.ru_maxrss / 1024;
}

int main(int argc, char **argv) {
    int n = argc > 1 ? atoi(argv[1]) : 1000000;
    int q = argc > 2 ? atoi(argv[2]) : 1000000;
    unsigned seed = argc > 3 ? atoi(argv[3]) : 42;
    int mode = argc > 4 ? atoi(argv[4]) : 0;

    mt19937 rng(seed);
    vector<Op> ops(q);
    {
        vector<pair<int,int>> alive;
        alive.reserve(q);
        for (int i = 0; i < q; ++i) {
            int r = rng() % 100;
            if (r < 33) {
                int u = rng() % n, v = rng() % n;
                if (u > v) swap(u, v);
                alive.push_back({u, v});
                ops[i] = {0, u, v};
            } else if (r < 55 && !alive.empty()) {
                int j = rng() % alive.size();
                auto [u, v] = alive[j];
                alive[j] = alive.back(); alive.pop_back();
                ops[i] = {1, u, v};
            } else if (r < 80) {
                ops[i] = {2, (int)(rng() % n), (int)(rng() % n)};
            } else if (r < 94) {
                ops[i] = {3, (int)(rng() % n), 0};
            } else {
                ops[i] = {4, 0, 0};
            }
        }
    }
    int num_queries = 0;
    for (const Op &o : ops) if (o.t >= 2) num_queries++;
    cout << "n=" << n << " q=" << q << " seed=" << seed
         << " (queries: " << num_queries << ")" << endl;

    vector<ll> a1, a2;

    if (mode == 0 || mode == 1) {
        auto t0 = chrono::steady_clock::now();
        titan23::DyCone dc(n, 999);
        dc.reserve(q);
        vector<int> qpos;
        qpos.reserve(num_queries);
        for (int i = 0; i < q; ++i) {
            const Op &o = ops[i];
            switch (o.t) {
                case 0: dc.add_edge(o.u, o.v); break;
                case 1: dc.delete_edge(o.u, o.v); break;
                default: qpos.push_back(i); dc.next_query(); break;
            }
        }
        auto t1 = chrono::steady_clock::now();
        a1.reserve(num_queries);
        dc.run([&] (int k) {
            const Op &o = ops[qpos[k]];
            if (o.t == 2) a1.push_back(dc.same(o.u, o.v));
            else if (o.t == 3) a1.push_back(dc.size(o.u));
            else a1.push_back(dc.group_count());
        });
        auto t2 = chrono::steady_clock::now();
        cout << "DyCone    : build " << elapsed_ms(t0, t1) << " ms, run "
             << elapsed_ms(t1, t2) << " ms, total " << elapsed_ms(t0, t2)
             << " ms, peak_rss " << peak_rss_mb() << " MB" << endl;
    }

    if (mode == 0 || mode == 2) {
        auto t0 = chrono::steady_clock::now();
        titan23::DyConeSum<ll> dc(n, 999);
        dc.reserve(q);
        vector<int> qpos;
        qpos.reserve(num_queries);
        for (int i = 0; i < q; ++i) {
            const Op &o = ops[i];
            switch (o.t) {
                case 0: dc.add_edge(o.u, o.v); break;
                case 1: dc.delete_edge(o.u, o.v); break;
                default: qpos.push_back(i); dc.next_query(); break;
            }
        }
        auto t1 = chrono::steady_clock::now();
        a2.reserve(num_queries);
        dc.run([&] (int k) {
            const Op &o = ops[qpos[k]];
            if (o.t == 2) a2.push_back(dc.same(o.u, o.v));
            else if (o.t == 3) a2.push_back(dc.size(o.u));
            else a2.push_back(dc.group_count());
        });
        auto t2 = chrono::steady_clock::now();
        cout << "DyConeSum : build " << elapsed_ms(t0, t1) << " ms, run "
             << elapsed_ms(t1, t2) << " ms, total " << elapsed_ms(t0, t2)
             << " ms, peak_rss " << peak_rss_mb() << " MB" << endl;
    }

    if (mode == 0) {
        if (a1 != a2) {
            for (int i = 0; i < (int)min(a1.size(), a2.size()); ++i) {
                if (a1[i] != a2[i]) {
                    cout << "mismatch at " << i << ": DyCone " << a1[i]
                         << ", DyConeSum " << a2[i] << endl;
                    return 1;
                }
            }
            cout << "mismatch: size " << a1.size() << " vs " << a2.size() << endl;
            return 1;
        }
        cout << "verify OK (" << a1.size() << " answers match)" << endl;
    }
    return 0;
}
