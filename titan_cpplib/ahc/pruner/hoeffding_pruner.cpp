#include <vector>
#include <cmath>
#include <algorithm>
using namespace std;

namespace titan23 {

template<typename ScoreType=double>
class HoeffdingPruner {
private:
    double score_range;
    double delta;
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

    int common_count;
    double diff_sum;

public:
    /// @brief コンストラクタ
    /// @param max_seeds 評価に使用するシードの最大数
    /// @param score_range 1つのシードに対するスコア差の最大幅（理論上の上限 - 下限）
    /// @param delta 誤って最良候補を枝刈りしてしまう確率の上限（有意水準に相当、例: 0.05）
    /// @param min_samples 枝刈り判定を開始する最低限の共通シード数
    /// @param is_maximize 最大化問題の場合はtrue、最小化問題の場合はfalseを指定します
    HoeffdingPruner(int max_seeds, double score_range, double delta=0.05, int min_samples=5, bool is_maximize=true)
        : score_range(score_range), delta(delta), min_samples(min_samples), is_maximize(is_maximize), max_seeds(max_seeds),
          best_count(0), best_sum(0), current_count(0), current_sum(0), common_count(0), diff_sum(0.0) {

        best_scores.assign(max_seeds, 0);
        has_best_score.assign(max_seeds, false);
        current_scores.assign(max_seeds, 0);
        has_current_score.assign(max_seeds, false);
    }

    /// @brief シードに対する評価スコアを報告します
    /// @param seed 評価に使用したシード値
    /// @param score 評価関数の結果スコア
    void report(int seed, ScoreType score) {
        if (!has_current_score[seed]) {
            has_current_score[seed] = true;
            current_count++;
            if (has_best_score[seed]) {
                common_count++;
                double diff = is_maximize ? (double)(best_scores[seed] - score) 
                                          : (double)(score - best_scores[seed]);
                diff_sum += diff;
            }
        } else {
            current_sum -= current_scores[seed];
            if (has_best_score[seed]) {
                double old_diff = is_maximize ? (double)(best_scores[seed] - current_scores[seed]) 
                                              : (double)(current_scores[seed] - best_scores[seed]);
                double new_diff = is_maximize ? (double)(best_scores[seed] - score) 
                                              : (double)(score - best_scores[seed]);
                diff_sum += (new_diff - old_diff);
            }
        }
        current_scores[seed] = score;
        current_sum += score;
    }

    /// @brief 現在のパラメータが過去のベストパラメータに対して統計的に有意に劣っているかを判定します
    /// @return 枝刈りすべき（劣っている）場合はtrue、評価を継続すべき場合はfalseを返します
    bool should_prune() const {
        if (best_count == 0 || common_count < min_samples) return false;

        double mean_diff = diff_sum / common_count;
        double epsilon = score_range * sqrt(log(1.0 / delta) / (2.0 * common_count));

        return mean_diff > epsilon;
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
        common_count = 0;
        diff_sum = 0.0;
    }
};
} //namespace titan23
