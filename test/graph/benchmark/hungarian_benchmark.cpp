// min_cost_assignment の測定。g++ -std=c++20 -O2 -DNDEBUG -I. this_file.cpp -o /tmp/hungarian_bench
// /tmp/hungarian_bench kind n m [seed]
// kind: ll (0 以上 1e9 未満), int (1e6 未満), small (10 未満), tie3 (3 未満), neg (負), mul ((i+1)(j+1)), sum (i+j),
//       dbl ([0, 1) の一様乱数), geo (平面上の点の組の距離)
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <random>
#include <string>
#include <vector>
#include "titan_cpplib/graph/hungarian.cpp"
using namespace std;

template <class T>
void run(const vector<vector<T>> &a, const string &kind, int n, int m) {
    auto s = chrono::steady_clock::now();
    auto [c, al] = titan23::min_cost_assignment(a);
    double ms = chrono::duration<double, milli>(chrono::steady_clock::now() - s).count();
    uint64_t h = 0;
    for (int x : al) h = h * 1000003 + (uint64_t)(x + 1);
    cout << kind << " n=" << n << " m=" << m << " ms=" << ms << " cost=" << (long double)c << " allochash=" << h << '\n';
}

int main(int argc, char **argv) {
    string kind = argv[1];
    int n = atoi(argv[2]), m = atoi(argv[3]);
    uint64_t seed = argc > 4 ? atoll(argv[4]) : 1;
    mt19937_64 rng(seed);
    if (kind == "ll") {
        vector<vector<long long>> a(n, vector<long long>(m));
        for (auto &r : a) for (auto &x : r) x = rng() % 1000000000;
        run(a, kind, n, m);
    } else if (kind == "int") {
        vector<vector<int>> a(n, vector<int>(m));
        for (auto &r : a) for (auto &x : r) x = rng() % 1000000;
        run(a, kind, n, m);
    } else if (kind == "small") {
        vector<vector<int>> a(n, vector<int>(m));
        for (auto &r : a) for (auto &x : r) x = rng() % 10;
        run(a, kind, n, m);
    } else if (kind == "mul" || kind == "sum" || kind == "tie3" || kind == "neg") {
        vector<vector<long long>> a(n, vector<long long>(m));
        for (int i = 0; i < n; ++i) for (int j = 0; j < m; ++j) {
            if (kind == "mul") a[i][j] = (long long)(i + 1) * (j + 1);
            else if (kind == "sum") a[i][j] = i + j;
            else if (kind == "tie3") a[i][j] = rng() % 3;
            else a[i][j] = -(long long)(rng() % 1000000000);
        }
        run(a, kind, n, m);
    } else if (kind == "dbl") {
        vector<vector<double>> a(n, vector<double>(m));
        uniform_real_distribution<double> d(0, 1);
        for (auto &r : a) for (auto &x : r) x = d(rng);
        run(a, kind, n, m);
    } else if (kind == "geo") {
        // 2 次元の点の組の距離。AHC で多い形
        vector<pair<double, double>> p(n), q(m);
        uniform_real_distribution<double> d(0, 1000);
        for (auto &[x, y] : p) x = d(rng), y = d(rng);
        for (auto &[x, y] : q) x = d(rng), y = d(rng);
        vector<vector<double>> a(n, vector<double>(m));
        for (int i = 0; i < n; ++i) for (int j = 0; j < m; ++j) a[i][j] = hypot(p[i].first - q[j].first, p[i].second - q[j].second);
        run(a, kind, n, m);
    }
}
