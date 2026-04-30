#include <bits/stdc++.h>
using namespace std;

namespace titan23 {

/// ベイズ線形回帰のためのギブスサンプラー
/// 観測モデル: y_i ~ N(sum_k(A_{i,k} * X_k), obs_sigma_i^2)
/// 事前分布: X_k ~ N(prior_mu_k, prior_sigma_k^2)
class BayesianLinearGibbsSampler {
private:
    int K; // 変数の数
    vector<double> prior_mu;
    vector<double> prior_var;

    struct Observation {
        double y;
        vector<double> coeffs;
        double var;
    };
    vector<Observation> observations;

public:
    /// 変数の数 K を指定して初期化する
    /// 初期状態ではすべての変数が N(0, 1) の事前分布を持つ
    BayesianLinearGibbsSampler(int num_variables)
        : K(num_variables), prior_mu(K, 0.0), prior_var(K, 1.0) {}

    /// 変数 index の事前分布 N(mu, sigma) を設定する
    void set_prior(int index, double mu, double sigma) {
        assert(0 <= index && index < K);
        assert(sigma > 0.0);
        prior_mu[index] = mu;
        prior_var[index] = sigma * sigma;
    }

    /// 観測値を報告する
    /// @param y 観測された値
    /// @param coeffs 各変数に乗算される係数 (サイズはKであること)
    /// @param sigma 観測ノイズの標準偏差
    void report(double y, const vector<double>& coeffs, double sigma) {
        assert(coeffs.size() == K);
        assert(sigma > 0.0);
        observations.push_back({y, coeffs, sigma * sigma});
    }

    /// サンプリング結果を格納する構造体
    struct EstimationResult {
        vector<double> mean;     // 事後分布の平均（推定値）
        vector<double> variance; // 事後分布の分散（不確実性の指標）
    };

    /// サンプリングを実行し、事後分布の平均値と分散を返す
    /// @param iterations サンプリングの総反復回数
    /// @param burn_in 破棄する初期サンプルの数 (iterationsより小さいこと)
    EstimationResult estimate(int iterations, int burn_in = 1000, unsigned int seed = 42) const {
        assert(burn_in < iterations);
        mt19937 gen(seed);

        // 各変数の事後分布の精度（分散の逆数）の定数部分を事前計算
        vector<double> prec(K, 0.0);
        for (int k = 0; k < K; ++k) {
            prec[k] = 1.0 / prior_var[k];
            for (const auto& obs : observations) {
                prec[k] += (obs.coeffs[k] * obs.coeffs[k]) / obs.var;
            }
        }

        vector<double> current_X = prior_mu;

        // 計算の高速化のため、各観測データに対する現在の残差を管理する
        // error_i = y_i - sum_k(A_{i,k} * X_k)
        int N = observations.size();
        vector<double> errors(N, 0.0);
        for (int i = 0; i < N; ++i) {
            double pred = 0.0;
            for (int k = 0; k < K; ++k) {
                pred += observations[i].coeffs[k] * current_X[k];
            }
            errors[i] = observations[i].y - pred;
        }

        vector<double> sum_X(K, 0.0);
        vector<double> sum_X2(K, 0.0); // 分散計算用の平方和
        int valid_samples = 0;

        for (int iter = 0; iter < iterations; ++iter) {
            for (int k = 0; k < K; ++k) {
                // X_k 以外の変数を固定した場合の、X_k に対する条件付き分布の平均を計算
                double mean_num = prior_mu[k] / prior_var[k];
                for (int i = 0; i < N; ++i) {
                    if (observations[i].coeffs[k] == 0.0) continue;
                    // k番目の変数を除いた残差 r_ik = error_i + A_{i,k} * X_k
                    double r_ik = errors[i] + observations[i].coeffs[k] * current_X[k];
                    mean_num += observations[i].coeffs[k] * r_ik / observations[i].var;
                }

                double post_var = 1.0 / prec[k];
                double post_mu = mean_num * post_var;

                // 新しい値をサンプリング
                normal_distribution<double> dist(post_mu, sqrt(post_var));
                double new_X = dist(gen);

                // 残差の更新 (O(N)で完了)
                double delta = new_X - current_X[k];
                if (delta != 0.0) {
                    for (int i = 0; i < N; ++i) {
                        errors[i] -= observations[i].coeffs[k] * delta;
                    }
                    current_X[k] = new_X;
                }
            }

            // バーンイン期間終了後のサンプルを記録
            if (iter >= burn_in) {
                for (int k = 0; k < K; ++k) {
                    sum_X[k] += current_X[k];
                    sum_X2[k] += current_X[k] * current_X[k];
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
