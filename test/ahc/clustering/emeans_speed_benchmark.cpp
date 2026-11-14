// e-means の測定。g++ -std=c++20 -O2 -DNDEBUG -I. this_file.cpp -o /tmp/emeans_bench
// /tmp/emeans_bench n d k [polish] [seed] [ptype]
// k 個の中心のまわりに正規分布で n 点を置く。polish は Hamerly 法の反復の上限 (既定 100)
// ptype: vec (vector<double>), arr2 (array<double, 2>), int (vector<int>)
#include <array>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <random>
#include <string>
#include <vector>
#include "titan_cpplib/ahc/clustering/emeans.cpp"
using namespace std;

template <class Point>
void run(const vector<Point> &pts, int k, int polish, uint32_t seed, const string &tag) {
    titan23::EMeansOptions o;
    o.seed = seed;
    o.polish_iterations = polish;
    auto s = chrono::steady_clock::now();
    auto r = titan23::emeans(pts, k, o);
    double ms = chrono::duration<double, milli>(chrono::steady_clock::now() - s).count();
    uint64_t h = 0;
    for (int x : r.labels) h = h * 1000003 + (uint64_t)x;
    cout << tag << " ms=" << ms << " cost=" << r.total_cost << " iter=" << r.iterations << " labelhash=" << h << '\n';
}

int main(int argc, char **argv) {
    int n = atoi(argv[1]), d = atoi(argv[2]), k = atoi(argv[3]);
    int polish = argc > 4 ? atoi(argv[4]) : 100;
    uint32_t seed = argc > 5 ? atoi(argv[5]) : 1;
    string ptype = argc > 6 ? argv[6] : "vec";
    mt19937_64 rng(12345);
    // 中心のまわりに正規分布で点を置く
    int g = max(1, k);
    vector<vector<double>> mu(g, vector<double>(d));
    uniform_real_distribution<double> U(0, 1000);
    for (auto &c : mu) for (auto &x : c) x = U(rng);
    normal_distribution<double> N(0, 40);
    string tag = "n=" + to_string(n) + " d=" + to_string(d) + " k=" + to_string(k) + " polish=" + to_string(polish) + " " + ptype;
    if (ptype == "vec") {
        vector<vector<double>> pts(n, vector<double>(d));
        for (auto &p : pts) { auto &c = mu[rng() % g]; for (int a = 0; a < d; ++a) p[a] = c[a] + N(rng); }
        run(pts, k, polish, seed, tag);
    } else if (ptype == "arr2") {
        vector<array<double, 2>> pts(n);
        for (auto &p : pts) { auto &c = mu[rng() % g]; for (int a = 0; a < 2; ++a) p[a] = c[a] + N(rng); }
        run(pts, k, polish, seed, tag);
    } else {
        vector<vector<int>> pts(n, vector<int>(d));
        for (auto &p : pts) { auto &c = mu[rng() % g]; for (int a = 0; a < d; ++a) p[a] = (int)(c[a] + N(rng)); }
        run(pts, k, polish, seed, tag);
    }
}
