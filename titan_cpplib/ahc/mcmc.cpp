#include <bits/stdc++.h>
using namespace std;

namespace titan23 {

/// @brief ベイズ線形回帰のためのギブスサンプラー
/// 観測モデル: y_i ~ N(sum_k(A_{i,k} * X_k), obs_sigma_i^2)、事前分布: X_k ~ N(prior_mu_k, prior_sigma_k^2)
/// report() の一回が「線形式 coeffs・X が y にほぼ等しい」一次方程式を一本足す。sigma が小さいほど強く信頼する。
/// 観測は疎形式で保持し、nnz を全観測の非零係数の総数とする。
class BayesianLinearGibbsSampler {
public:
    struct EstimationResult {
        vector<double> mean;
        vector<double> variance;
    };

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

    // 推定用の作業バッファ。再確保を避けるため呼び出し間で使い回す
    mutable vector<double> inv_var, post_var, post_sd, errors, sum_X, sum_X2;
    mutable vector<vector<pair<int, double>>> var_obs;

    // estimate_step() 用の連鎖状態。ターンをまたいで引き継ぐ
    vector<double> warm_X;
    mt19937 warm_gen;
    bool warm_inited = false;

    // 反復で変化しない量（inv_var, var_obs, post_var, post_sd）を前計算する
    void prepare_buffers() const {
        int N = observations.size();
        inv_var.assign(N, 0.0);
        for (int i = 0; i < N; ++i) {
            inv_var[i] = 1.0 / observations[i].var;
        }
        var_obs.resize(K);
        for (int k = 0; k < K; ++k) var_obs[k].clear();
        for (int i = 0; i < N; ++i) {
            for (const auto &[k, c] : observations[i].terms) {
                var_obs[k].emplace_back(i, c);
            }
        }
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
    }

    // 連鎖状態 X に対応する各観測の残差 error_i = y_i - sum_k(A_{i,k} * X_k) を計算する
    void compute_errors(const vector<double> &X) const {
        int N = observations.size();
        errors.assign(N, 0.0);
        for (int i = 0; i < N; ++i) {
            double pred = 0.0;
            for (const auto &[k, c] : observations[i].terms) {
                pred += c * X[k];
            }
            errors[i] = observations[i].y - pred;
        }
    }

    // X を1回ギブス掃きし、errors を差分更新する
    void gibbs_sweep(vector<double> &X, mt19937 &gen, normal_distribution<double> &std_normal) const {
        for (int k = 0; k < K; ++k) {
            // X_k 以外を固定したときの X_k の条件付き分布の平均を計算する
            double mean_num = prior_mu[k] / prior_var[k];
            for (const auto &[i, c] : var_obs[k]) {
                double r_ik = errors[i] + c * X[k]; // X_k を除いた残差
                mean_num += c * r_ik * inv_var[i];
            }
            double post_mu = mean_num * post_var[k];
            double new_X = post_mu + post_sd[k] * std_normal(gen);
            double delta = new_X - X[k];
            if (delta != 0.0) {
                for (const auto &[i, c] : var_obs[k]) {
                    errors[i] -= c * delta;
                }
                X[k] = new_X;
            }
        }
    }

    // 連鎖 X を gen で iterations 回回し、burn_in 以降のサンプルから事後分布を求める
    EstimationResult run_chain(vector<double> &X, mt19937 &gen, int iterations, int burn_in) const {
        prepare_buffers();
        compute_errors(X);
        sum_X.assign(K, 0.0);
        sum_X2.assign(K, 0.0);
        int valid_samples = 0;
        normal_distribution<double> std_normal(0.0, 1.0);
        for (int iter = 0; iter < iterations; ++iter) {
            gibbs_sweep(X, gen, std_normal);
            if (iter >= burn_in) {
                for (int k = 0; k < K; ++k) {
                    sum_X[k] += X[k];
                    sum_X2[k] += X[k] * X[k];
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

    /// @brief 事前分布から始めて推定する（バッチ用） / O(iterations * nnz + N + K)
    /// @param iterations サンプリングの総反復回数。目安は 5000〜20000
    /// @param burn_in 破棄する初期サンプルの数。iterations の 1〜2 割が目安 (iterations より小さいこと)
    /// @param seed 乱数シード
    EstimationResult estimate(int iterations, int burn_in = 1000, unsigned int seed = 42) const {
        assert(burn_in < iterations);
        vector<double> X = prior_mu;
        mt19937 gen(seed);
        return run_chain(X, gen, iterations, burn_in);
    }

    /// @brief 直前の連鎖状態から継続して推定する（ターンごとの再推定用） / O(iterations * nnz + N + K)
    /// 観測を追加してから呼ぶ。連鎖が前回の続きから始まるため burn_in を小さくでき、反復も少なくて済む。
    /// 初回は事前分布から始める。連鎖を初期化するには reset_chain() を呼ぶ。
    /// @param iterations サンプリングの総反復回数。ウォームスタートのため数百〜数千で足りることが多い
    /// @param burn_in 破棄する初期サンプルの数 (iterations より小さいこと)
    /// @param seed 初回のみ使う乱数シード
    EstimationResult estimate_step(int iterations, int burn_in = 200, unsigned int seed = 42) {
        assert(burn_in < iterations);
        if (!warm_inited) {
            warm_X = prior_mu;
            warm_gen.seed(seed);
            warm_inited = true;
        }
        return run_chain(warm_X, warm_gen, iterations, burn_in);
    }

    /// @brief estimate_step() の連鎖状態を初期化する。次回呼び出しは事前分布から始まる / O(1)
    void reset_chain() {
        warm_inited = false;
    }
};

} // namespace titan23
