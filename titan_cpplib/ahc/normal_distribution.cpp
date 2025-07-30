#include <iostream>
#include <cmath>
#include <cassert>
using namespace std;

namespace titan23 {

class NormalDist {
private:
    static constexpr const double sqrt2 = 1.4142135623730951;
    static constexpr const double a1 = 0.254829592, a2 = -0.284496736, a3 = 1.421413741, a4 = -1.453152027, a5 = 1.061405429;
    static constexpr const double p1 = 0.3275911;
    static constexpr const double a = 0.147; // Winitzki 定数

    static double erfinv_approx(double x) {
        assert(-1.0 <= x && x <= 1.0);
        if (x == 0) return 0;
        if (x <= -1.0) return -INFINITY;
        if (x >= 1.0) return INFINITY;
        double ln = std::log(1.0 - x * x);
        double sgn = std::copysign(1.0, x);
        double term1 = 2.0 / (M_PI * a) + ln / 2.0;
        double term2 = ln / a;
        double approx = std::sqrt(std::sqrt(term1 * term1 - term2) - term1);
        double y = sgn * approx;

        // 精度向上
        // double fx = std::erf(y) - x;
        // double dfx = (2.0 / std::sqrt(M_PI)) * std::exp(-y * y);
        // y = y - fx / dfx;

        return y;
    }

public:
    /// 正規分布 N(mu, sigma) において、x 以下となる確率を返す
    static double cdf(double x, double mu, double sigma) {
        return 0.5 * (1 + std::erf((x - mu) / (sigma * sqrt2)));
    }

    static double cdf_approx(double x) {
        double sgn = (x < 0) ? -1.0 : 1.0;
        x = std::fabs(x) / std::sqrt(2.0);
        double t = 1.0 / (1.0 + p1 * x);
        double y = 1.0 - (((((a5 * t + a4) * t) + a3) * t + a2) * t + a1) * t * std::exp(-x * x);
        return 0.5 * (1.0 + sgn * y);
    }

    /// 正規分布 N(mu, sigma) において、x 以下となる確率を返す
    static double cdf_approx(double x, double mu, double sigma) {
        return cdf_approx((x - mu) / sigma);
    }

    /// 正規分布 N(mu, sigma) における x の確率密度を返す
    static double pdf(double x, double mu, double sigma) {
        double coeff = 1.0 / (sigma * std::sqrt(2.0 * M_PI));
        double exponent = -0.5 * std::pow((x - mu) / sigma, 2);
        return coeff * std::exp(exponent);
    }

    /// 正規分布 N(mu, sigma) に対して、観測した値が区間 [a, b] に含まれる確率を返す
    static double probability_in_range(double l, double r, double mu, double sigma) {
        assert(l <= r);
        double res = cdf(r, mu, sigma) - cdf(l, mu, sigma);
        assert(res >= 0);
        return res;
    }

    /// 正規分布 N(mu, sigma) における累積確率 p に対応する x を返す
    static double inverse_cdf(double p, double mu, double sigma) {
        assert(0 <= p && p <= 1.0);
        return mu + sigma * sqrt2 * erfinv_approx(2.0 * p - 1.0);
    }
};
} // namespace titan23
