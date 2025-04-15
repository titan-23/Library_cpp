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

    static double cdf_approx(double x) {
        double sgn = (x < 0) ? -1.0 : 1.0;
        x = std::fabs(x) / std::sqrt(2.0);
        double t = 1.0 / (1.0 + p1 * x);
        double y = 1.0 - (((((a5 * t + a4) * t) + a3) * t + a2) * t + a1) * t * std::exp(-x * x);
        return 0.5 * (1.0 + sgn * y);
    }

    static double erfinv(double x) {
        /// 逆誤差関数 erfinv の近似計算
        assert(-1.0 <= x && x <= 1.0);
        double a = 0.0;
        double b = 0.0;
        double t = x;
        double y = t;
        double epsilon = 1e-10;
        while (true) {
            t = (2.0 * y + x / (sqrt2 * std::exp(-y * y))) / 2.0;
            if (std::fabs(t - y) < epsilon) break;
            y = t;
        }
        return y;
    }

public:
    /// 正規分布 N(mu, sigma) において、x 以下となる確率を返す
    static double cdf(double x, double mu, double sigma) {
        return 0.5 * (1 + std::erf((x - mu) / (sigma * sqrt2)));
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
        assert(cdf(r, mu, sigma) - cdf(l, mu, sigma) >= 0);
        return cdf(r, mu, sigma) - cdf(l, mu, sigma);
    }

    /// 正規分布 N(mu, sigma) における累積確率 p に対応する x を返す
    static double inverse_cdf(double p, double mu, double sigma) {
        assert(0 <= p && p <= 1.0);
        return mu + sigma * sqrt2 * erfinv(2.0 * p - 1.0);
    }
};
} // namespace titan23
