#include <cmath>
using namespace std;

namespace titan23 {

class NormalDist {
private:
    static constexpr const double sqrt2 = 1.4142135623730951;
    static constexpr const double a1 = 0.254829592, a2 = -0.284496736, a3 = 1.421413741;
    static constexpr const double a4 = -1.453152027, a5 = 1.061405429;
    static constexpr const double p = 0.3275911;

    static double cdf_approx(double x) {
        double sign = (x < 0) ? -1.0 : 1.0;
        x = std::fabs(x) / std::sqrt(2.0);
        double t = 1.0 / (1.0 + p * x);
        double y = 1.0 - (((((a5 * t + a4) * t) + a3) * t + a2) * t + a1) * t * std::exp(-x * x);
        return 0.5 * (1.0 + sign * y);
    }

public:
    /// 正規分布 N(mu, sigma) において、x 以下となる確率を返す関数
    static double cdf(double x, double mu, double sigma) {
        return 0.5 * (1 + std::erf((x - mu) / (sigma * sqrt2)));
    }

    /// 正規分布 N(mu, sigma) において、x 以下となる確率を返す関数
    static double cdf_approx(double x, double mu, double sigma) {
        return cdf_approx((x - mu) / sigma);
    }
};
} // namespace titan23
