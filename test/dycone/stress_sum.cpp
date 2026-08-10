// DyConeSum の正当性ストレステスト
// ランダムな操作列を生成し、毎回 DSU を作り直す愚直解と照合する
// 自己ループ・多重辺・削除後の再追加・成分加算を含む
//
// g++ -std=c++20 -O2 -Wall -Wextra -I . -o stress_sum test/dycone/stress_sum.cpp
// ./stress_sum [iters]

#include <bits/stdc++.h>
#include "titan_cpplib/ds/dy_cone_sum.cpp"
using namespace std;
using ll = long long;

struct DSU {
    vector<int> p;
    DSU(int n) : p(n, -1) {}
    int f(int x) { while (p[x] >= 0) x = p[x]; return x; }
    bool u(int a, int b) {
        a = f(a); b = f(b);
        if (a == b) return false;
        if (p[a] > p[b]) swap(a, b);
        p[a] += p[b]; p[b] = a;
        return true;
    }
};

// 操作種別 0:add_edge 1:delete_edge 2:add_point 3:add_group
//          4:sum 5:size 6:same 7:group_count
struct Op { int t, u, v; ll x; };

int main(int argc, char **argv) {
    int iters = argc > 1 ? atoi(argv[1]) : 2000;
    mt19937 rng(123);
    for (int it = 0; it < iters; ++it) {
        int n = rng() % 40 + 1;
        int q = rng() % 120 + 1;
        if (it % 10 == 0) { n = rng() % 300 + 1; q = rng() % 600 + 1; }

        vector<Op> ops;
        vector<pair<int,int>> alive;
        for (int i = 0; i < q; ++i) {
            int r = rng() % 100;
            if (r < 25) {
                int u, v;
                if (!alive.empty() && rng() % 10 == 0) {
                    tie(u, v) = alive[rng() % alive.size()]; // 多重辺
                } else if (rng() % 20 == 0) {
                    u = v = rng() % n; // 自己ループ
                } else {
                    u = rng() % n; v = rng() % n;
                }
                if (u > v) swap(u, v);
                alive.push_back({u, v});
                ops.push_back({0, u, v, 0});
            } else if (r < 45 && !alive.empty()) {
                int j = rng() % alive.size();
                auto [u, v] = alive[j];
                alive[j] = alive.back(); alive.pop_back();
                ops.push_back({1, u, v, 0});
            } else if (r < 57) {
                ops.push_back({2, (int)(rng() % n), 0, (ll)(rng() % 201) - 100});
            } else if (r < 69) {
                ops.push_back({3, (int)(rng() % n), 0, (ll)(rng() % 201) - 100});
            } else if (r < 80) {
                ops.push_back({4, (int)(rng() % n), 0, 0});
            } else if (r < 88) {
                ops.push_back({5, (int)(rng() % n), 0, 0});
            } else if (r < 96) {
                ops.push_back({6, (int)(rng() % n), (int)(rng() % n), 0});
            } else {
                ops.push_back({7, 0, 0, 0});
            }
        }

        // DyConeSum
        titan23::DyConeSum<ll> dc(n, rng());
        vector<int> qpos; // k 番目のクエリの ops 添字
        for (int i = 0; i < (int)ops.size(); ++i) {
            const Op &o = ops[i];
            switch (o.t) {
                case 0: dc.add_edge(o.u, o.v); break;
                case 1: dc.delete_edge(o.u, o.v); break;
                case 2: dc.add_point(o.u, o.x); break;
                case 3: dc.add_group(o.u, o.x); break;
                default: qpos.push_back(i); dc.next_query(); break;
            }
        }
        vector<ll> got;
        dc.run([&] (int k) {
            const Op &o = ops[qpos[k]];
            if (o.t == 4) got.push_back(dc.sum(o.u));
            else if (o.t == 5) got.push_back(dc.size(o.u));
            else if (o.t == 6) got.push_back(dc.same(o.u, o.v));
            else got.push_back(dc.group_count());
        });

        // 愚直解
        vector<ll> want;
        vector<ll> val(n, 0);
        vector<pair<int,int>> cur;
        for (const Op &o : ops) {
            if (o.t == 0) {
                cur.push_back({o.u, o.v});
                continue;
            }
            if (o.t == 1) {
                for (int j = (int)cur.size() - 1; j >= 0; --j) {
                    if (cur[j] == make_pair(o.u, o.v)) { cur.erase(cur.begin() + j); break; }
                }
                continue;
            }
            if (o.t == 2) {
                val[o.u] += o.x;
                continue;
            }
            DSU d(n);
            int gc = n;
            for (auto &[a, b] : cur) if (d.u(a, b)) gc--;
            if (o.t == 3) {
                for (int x = 0; x < n; ++x) if (d.f(x) == d.f(o.u)) val[x] += o.x;
            } else if (o.t == 4) {
                ll s = 0;
                for (int x = 0; x < n; ++x) if (d.f(x) == d.f(o.u)) s += val[x];
                want.push_back(s);
            } else if (o.t == 5) {
                int c = 0;
                for (int x = 0; x < n; ++x) if (d.f(x) == d.f(o.u)) c++;
                want.push_back(c);
            } else if (o.t == 6) {
                want.push_back(d.f(o.u) == d.f(o.v));
            } else {
                want.push_back(gc);
            }
        }

        if (got != want) {
            cout << "WA at iter " << it << " (n=" << n << ", q=" << q << ")\n";
            for (int i = 0; i < (int)min(got.size(), want.size()); ++i) {
                if (got[i] != want[i]) {
                    cout << "  query " << i << " (op " << ops[qpos[i]].t << "): got "
                         << got[i] << ", want " << want[i] << "\n";
                }
            }
            return 1;
        }
    }
    cout << "stress OK: " << iters << " cases" << endl;
    return 0;
}
