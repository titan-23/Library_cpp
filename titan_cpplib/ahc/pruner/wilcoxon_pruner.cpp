/// https://github.com/titan-23/Library_cpp/blob/main/titan_cpplib/ahc/pruner/wilcoxon_pruner.cpp
#pragma once

#include <vector>
#include <cmath>
#include <algorithm>
#include <numeric>
using namespace std;

namespace titan23 {

/// @brief 同じシードでの評価を Wilcoxon 符号順位検定で比べ、既知の最良に有意に劣るパラメータの評価を打ち切る枝刈り器
/// パラメータごとに report と should_prune を繰り返し、評価を終えたら next_param を呼ぶ
template<typename ScoreType=double>
class WilcoxonPruner {
private:
    double p_threshold;
    int min_samples;
    bool is_minimize;
    int max_seeds;

    vector<ScoreType> best_scores;
    vector<bool> has_best_score;
    int best_count;
    ScoreType best_sum;

    vector<ScoreType> now_scores;
    vector<bool> has_now_score;
    int now_count;
    ScoreType now_sum;

    vector<pair<double, double>> workspace_diffs;

public:
     /// @brief 枝刈り器を構築する / seed は [0, max_seeds)、p_threshold は有意水準、is_minimize の既定は最小化
     WilcoxonPruner(int max_seeds, double p_threshold=0.1, int min_samples=5, bool is_minimize=true)
        : p_threshold(p_threshold), min_samples(min_samples), is_minimize(is_minimize), max_seeds(max_seeds), best_count(0), best_sum(0), now_count(0), now_sum(0) {
        best_scores.assign(max_seeds, 0);
        has_best_score.assign(max_seeds, false);
        now_scores.assign(max_seeds, 0);
        has_now_score.assign(max_seeds, false);
        workspace_diffs.reserve(max_seeds);
    }

    /// @brief シード seed での評価スコア score を報告する / 同じ seed を再び報告すると上書きする
    void report(int seed, ScoreType score) {
        if (!has_now_score[seed]) {
            has_now_score[seed] = true;
            now_count++;
        } else {
            now_sum -= now_scores[seed];
        }
        now_scores[seed] = score;
        now_sum += score;
    }

    /// @brief 現パラメータが最良パラメータより統計的に有意に劣り、枝刈りすべきなら true を返す
    bool should_prune() {
        if (best_count == 0 || now_count < min_samples) return false;
        workspace_diffs.clear();
        for (int i = 0; i < max_seeds; ++i) {
            if (has_now_score[i] && has_best_score[i]) {
                double diff = is_minimize ? (now_scores[i] - best_scores[i])
                                          : (best_scores[i] - now_scores[i]);
                if (abs(diff) > 1e-9) {
                    workspace_diffs.push_back({abs(diff), diff});
                }
            }
        }
        int valid_n = workspace_diffs.size();
        if (valid_n < min_samples) return false;
        sort(workspace_diffs.begin(), workspace_diffs.end(), [] (const auto& a, const auto& b) { return a.first < b.first; });
        double w_plus = 0;
        double var_correction = 0;
        for (int i = 0; i < valid_n;) {
            int j = i;
            while (j < valid_n && abs(workspace_diffs[j].first - workspace_diffs[i].first) < 1e-9) j++;
            long long t = j - i;
            double avg_rank = (i + 1 + j) / 2.0;
            for (int k = i; k < j; ++k) {
                if (workspace_diffs[k].second > 0) w_plus += avg_rank;
            }
            if (t > 1) var_correction += (t * t * t - t) / 48.0;
            i = j;
        }
        double expected_w = valid_n * (valid_n + 1) / 4.0;
        double var_w = valid_n * (valid_n + 1) * (2 * valid_n + 1) / 24.0 - var_correction;
        if (var_w <= 0) return false;
        double z = (w_plus - expected_w) / sqrt(var_w);
        double p_value = 0.5 * erfc(z / sqrt(2.0));
        return p_value < p_threshold;
    }

    /// @brief 現パラメータの評価を終え、次のパラメータに備える / 枝刈りで終えたなら was_pruned を true にする
    /// 枝刈りされずに min_samples 個以上のシードを評価し、平均が最良より良いときだけ最良を置き換える
    void next_param(bool was_pruned) {
        if (!was_pruned && now_count >= min_samples) {
            bool update = false;
            if (best_count == 0) {
                update = true;
            } else {
                double now_mean = (double)now_sum / now_count;
                double best_mean = (double)best_sum / best_count;
                if (is_minimize && now_mean < best_mean) update = true;
                if (!is_minimize && now_mean > best_mean) update = true;
            }
            if (update) {
                best_scores = now_scores;
                has_best_score = has_now_score;
                best_count = now_count;
                best_sum = now_sum;
            }
        }
        fill(has_now_score.begin(), has_now_score.end(), false);
        now_count = 0;
        now_sum = 0;
    }
};
} //namespace titan23
