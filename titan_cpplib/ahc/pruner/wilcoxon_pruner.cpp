#include <vector>
#include <cmath>
#include <algorithm>
#include <numeric>
using namespace std;

namespace titan23 {

template<typename ScoreType=double>
class WilcoxonPruner {
private:
    double p_threshold;
    int min_samples;
    bool is_maximize;
    int max_seeds;

    vector<ScoreType> best_scores;
    vector<bool> has_best_score;
    int best_count;
    ScoreType best_sum;

    vector<ScoreType> current_scores;
    vector<bool> has_current_score;
    int current_count;
    ScoreType current_sum;

    vector<pair<double, double>> workspace_diffs;

public:
     /// @brief
     /// @param max_seeds 評価に使用するシードの最大数 内部の配列サイズを事前確保するために使用します
     /// @param p_threshold 枝刈りを判定する有意水準
     /// @param min_samples 枝刈り判定を開始する最低限の共通シード数
     /// @param is_maximize 最大化問題の場合はtrue、最小化問題の場合はfalseを指定します
     WilcoxonPruner(int max_seeds, double p_threshold=0.1, int min_samples=5, bool is_maximize=true)
        : p_threshold(p_threshold), min_samples(min_samples), is_maximize(is_maximize), max_seeds(max_seeds),
          best_count(0), best_sum(0), current_count(0), current_sum(0) {

        best_scores.assign(max_seeds, 0);
        has_best_score.assign(max_seeds, false);
        current_scores.assign(max_seeds, 0);
        has_current_score.assign(max_seeds, false);
        workspace_diffs.reserve(max_seeds);
    }

    /// @brief シードに対する評価スコアを報告します
    /// @param seed 評価に使用したシード値
    /// @param score 評価関数の結果スコア
    void report(int seed, ScoreType score) {
        if (!has_current_score[seed]) {
            has_current_score[seed] = true;
            current_count++;
        } else {
            current_sum -= current_scores[seed];
        }
        current_scores[seed] = score;
        current_sum += score;
    }

    /// @brief 現在のパラメータが過去のベストパラメータに対して統計的に有意に劣っているかを判定します
    /// @return 枝刈りすべき（劣っている）場合はtrue、評価を継続すべき場合はfalseを返します
    bool should_prune() {
        if (best_count == 0 || current_count < min_samples) return false;
        workspace_diffs.clear();
        for (int i = 0; i < max_seeds; ++i) {
            if (has_current_score[i] && has_best_score[i]) {
                double diff = is_maximize ? (best_scores[i] - current_scores[i])
                                          : (current_scores[i] - best_scores[i]);
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

    /// @brief 現在のパラメータの評価を終了し、次のパラメータ評価の準備を行います
    /// @param was_pruned 途中で枝刈りされて終了した場合はtrue、完走した場合はfalseを指定します
    void next_param(bool was_pruned) {
        if (!was_pruned && current_count >= min_samples) {
            bool update = false;
            if (best_count == 0) {
                update = true;
            } else {
                double current_mean = (double)current_sum / current_count;
                double best_mean = (double)best_sum / best_count;

                if (is_maximize && current_mean > best_mean) update = true;
                if (!is_maximize && current_mean < best_mean) update = true;
            }
            if (update) {
                best_scores = current_scores;
                has_best_score = has_current_score;
                best_count = current_count;
                best_sum = current_sum;
            }
        }
        fill(has_current_score.begin(), has_current_score.end(), false);
        current_count = 0;
        current_sum = 0;
    }
};
} //namespace titan23
