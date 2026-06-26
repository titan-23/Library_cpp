#include <bits/stdc++.h>
#include "titan_cpplib/ahc/mcmc.cpp"
using namespace std;

// 真の X をノイズ付き観測からどれだけ正確に復元できるかを測る。
//   - K 変数、M 観測。各観測は density 個の非零係数を持つ疎な線形式。
//   - y = coeffs・X_true + N(0, sigma^2) で生成する。
//   - 推定平均と X_true の RMSE、最大誤差を出す。
//   - z_k = (mean_k - X_true_k) / sqrt(variance_k) の平均と標準偏差で
//     不確実性の見積りが妥当か（標準正規に近いか）を見る。

mt19937 gen_truth(12345);

struct Stat { double rmse, maxerr, z_mean, z_std; };

Stat run(int K, int M, int density, double sigma) {
    // 真の X
    normal_distribution<double> Xdist(0.0, 3.0);
    vector<double> X_true(K);
    for (int k = 0; k < K; ++k) X_true[k] = Xdist(gen_truth);

    titan23::BayesianLinearGibbsSampler sampler(K);

    normal_distribution<double> coeff_dist(0.0, 1.0);
    normal_distribution<double> noise(0.0, sigma);
    uniform_int_distribution<int> pick(0, K - 1);

    for (int m = 0; m < M; ++m) {
        // 重複しない density 個の変数を選ぶ
        set<int> idx;
        while ((int)idx.size() < min(density, K)) idx.insert(pick(gen_truth));
        vector<pair<int, double>> coeffs;
        double y = 0.0;
        for (int k : idx) {
            double c = coeff_dist(gen_truth);
            coeffs.emplace_back(k, c);
            y += c * X_true[k];
        }
        y += noise(gen_truth);
        sampler.report(y, coeffs, sigma);
    }

    auto res = sampler.estimate(20000, 4000);

    double sse = 0.0, maxerr = 0.0;
    double zsum = 0.0, zsum2 = 0.0;
    for (int k = 0; k < K; ++k) {
        double err = res.mean[k] - X_true[k];
        sse += err * err;
        maxerr = max(maxerr, fabs(err));
        double z = err / sqrt(max(res.variance[k], 1e-12));
        zsum += z;
        zsum2 += z * z;
    }
    Stat s;
    s.rmse = sqrt(sse / K);
    s.maxerr = maxerr;
    s.z_mean = zsum / K;
    s.z_std = sqrt(zsum2 / K - s.z_mean * s.z_mean);
    return s;
}

int main() {
    int K = 30;
    printf("K=%d, X_true ~ N(0, 3^2)\n", K);
    printf("%-8s %-8s %-7s %-9s %-9s %-9s %-9s\n",
           "M", "density", "sigma", "RMSE", "maxerr", "z_mean", "z_std");
    struct Case { int M, density; double sigma; };
    vector<Case> cases = {
        {15,  3, 0.1},  // 観測不足 (M < K)
        {30,  3, 0.1},  // ちょうど (M = K)
        {60,  3, 0.1},  // 余裕あり (M = 2K)
        {150, 3, 0.1},  // 大量      (M = 5K)
        {150, 3, 0.5},  // ノイズ中
        {150, 3, 2.0},  // ノイズ大
        {150, 10, 0.5}, // 密度高め
    };
    for (auto &cs : cases) {
        Stat s = run(K, cs.M, cs.density, cs.sigma);
        printf("%-8d %-8d %-7.2f %-9.4f %-9.4f %-9.3f %-9.3f\n",
               cs.M, cs.density, cs.sigma, s.rmse, s.maxerr, s.z_mean, s.z_std);
    }
    return 0;
}
