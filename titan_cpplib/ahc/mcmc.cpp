#include <bits/stdc++.h>
using namespace std;

namespace titan23 {

/// @brief ベイズ線形回帰のためのギブスサンプラー
/// 観測モデル: y_i ~ N(sum_k(A_{i,k} * X_k), obs_sigma_i^2)、事前分布: X_k ~ N(prior_mu_k, prior_sigma_k^2)
/// report() の一回が「線形式 coeffs・X が y にほぼ等しい」一次方程式を一本足す。sigma が小さいほど強く信頼する。
/// 観測は疎形式で保持し、nnz を全観測の非零係数の総数とする。
class BayesianLinearGibbsSampler {
private:
    int K;
    vector<double> prior_mu;
    vector<double> prior_var;

    struct Observation {
        double y;
        double var;
        vector<pair<int, double>> terms; // (変数index, 係数) の非零項のみ
    };
    vector<Observation> observations;

    mutable vector<double> inv_var, post_var, post_sd, now_X, errors, sum_X, sum_X2;
    mutable vector<vector<pair<int, double>>> var_obs;

public:
    /// @brief 変数の数 K を指定して初期化する。初期の事前分布は全変数 N(0, 1) / O(K)
    BayesianLinearGibbsSampler(int num_variables)
        : K(num_variables), prior_mu(K, 0.0), prior_var(K, 1.0) {}

    /// @brief 変数 index の事前分布 N(mu, sigma) を設定する / O(1)
    void set_prior(int index, double mu, double sigma) {
        assert(0 <= index && index < K);
        assert(sigma > 0.0);
        prior_mu[index] = mu;
        prior_var[index] = sigma * sigma;
    }

    /// @brief 観測値を疎形式で報告する / O(係数の個数)
    /// @param y 観測された値
    /// @param coeffs 非零の (変数index, 係数) の列
    /// @param sigma 観測ノイズの標準偏差
    void report(double y, const vector<pair<int, double>> &coeffs, double sigma) {
        assert(sigma > 0.0);
        for (const auto &[k, c] : coeffs) {
            assert(0 <= k && k < K);
        }
        observations.push_back({y, sigma * sigma, coeffs});
    }

    /// @brief 観測値を密形式で報告する / O(K)
    /// @param y 観測された値
    /// @param coeffs 各変数に乗算される係数 (サイズはKであること)
    /// @param sigma 観測ノイズの標準偏差
    void report(double y, const vector<double> &coeffs, double sigma) {
        assert((int)coeffs.size() == K);
        vector<pair<int, double>> terms;
        for (int k = 0; k < K; ++k) {
            if (coeffs[k] != 0.0) terms.emplace_back(k, coeffs[k]);
        }
        report(y, terms, sigma);
    }

    struct EstimationResult {
        vector<double> mean;
        vector<double> variance;
    };

    /// @brief サンプリングを実行し、事後分布の平均値と分散を返す / O(iterations * nnz + N + K)
    /// @param iterations サンプリングの総反復回数。目安は 5000〜20000
    /// @param burn_in 破棄する初期サンプルの数。iterations の 1〜2 割が目安 (iterations より小さいこと)
    /// @param seed 乱数シード
    EstimationResult estimate(int iterations, int burn_in = 1000, unsigned int seed = 42) const {
        assert(burn_in < iterations);
        mt19937 gen(seed);

        int N = observations.size();

        inv_var.assign(N, 0.0);
        for (int i = 0; i < N; ++i) {
            inv_var[i] = 1.0 / observations[i].var;
        }

        // 変数ごとに、その変数を含む観測の (観測index, 係数) を集める
        var_obs.resize(K);
        for (int k = 0; k < K; ++k) var_obs[k].clear();
        for (int i = 0; i < N; ++i) {
            for (const auto &[k, c] : observations[i].terms) {
                var_obs[k].emplace_back(i, c);
            }
        }

        // 事後分布の分散・標準偏差は反復で変化しないため前計算する
        post_var.resize(K);
        post_sd.resize(K);
        for (int k = 0; k < K; ++k) {
            double prec = 1.0 / prior_var[k];
            for (const auto &[i, c] : var_obs[k]) {
                prec += c * c * inv_var[i];
            }
            post_var[k] = 1.0 / prec;
            post_sd[k] = sqrt(post_var[k]);
        }

        now_X = prior_mu;

        // 各観測の残差 error_i = y_i - sum_k(A_{i,k} * X_k) を持ち、差分で更新する
        errors.assign(N, 0.0);
        for (int i = 0; i < N; ++i) {
            double pred = 0.0;
            for (const auto &[k, c] : observations[i].terms) {
                pred += c * now_X[k];
            }
            errors[i] = observations[i].y - pred;
        }

        sum_X.assign(K, 0.0);
        sum_X2.assign(K, 0.0);
        int valid_samples = 0;

        normal_distribution<double> std_normal(0.0, 1.0);

        for (int iter = 0; iter < iterations; ++iter) {
            for (int k = 0; k < K; ++k) {
                // X_k 以外を固定したときの X_k の条件付き分布の平均を計算する
                double mean_num = prior_mu[k] / prior_var[k];
                for (const auto &[i, c] : var_obs[k]) {
                    double r_ik = errors[i] + c * now_X[k]; // X_k を除いた残差
                    mean_num += c * r_ik * inv_var[i];
                }
                double post_mu = mean_num * post_var[k];
                double new_X = post_mu + post_sd[k] * std_normal(gen);

                double delta = new_X - now_X[k];
                if (delta != 0.0) {
                    for (const auto &[i, c] : var_obs[k]) {
                        errors[i] -= c * delta;
                    }
                    now_X[k] = new_X;
                }
            }

            if (iter >= burn_in) {
                for (int k = 0; k < K; ++k) {
                    sum_X[k] += now_X[k];
                    sum_X2[k] += now_X[k] * now_X[k];
                }
                valid_samples++;
            }
        }

        EstimationResult result;
        result.mean.assign(K, 0.0);
        result.variance.assign(K, 0.0);
        for (int k = 0; k < K; ++k) {
            result.mean[k] = sum_X[k] / valid_samples;
            double mean_sq = sum_X2[k] / valid_samples;
            result.variance[k] = mean_sq - result.mean[k] * result.mean[k]; // E[X^2] - (E[X])^2
        }
        return result;
    }
};

} // namespace titan23
