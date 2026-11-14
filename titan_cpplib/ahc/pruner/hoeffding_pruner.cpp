/// https://github.com/titan-23/Library_cpp/blob/main/titan_cpplib/ahc/pruner/hoeffding_pruner.cpp
#pragma once

#include <vector>
#include <cmath>
#include <algorithm>
using namespace std;

namespace titan23 {

/// @brief Hoeffding の不等式で、既知の最良より有意に劣るパラメータの評価を打ち切る枝刈り器
template<typename ScoreType=double>
class HoeffdingPruner {
private:
    double score_range;
    double delta;
    int min_samples;
    bool is_minimize;
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
    /// @brief 枝刈り器を構築する / seed は [0, max_seeds)、is_minimize の既定は最小化
    /// score_range は 1 シードでのスコア差の最大幅、delta は最良を誤って枝刈りする確率の上限
    HoeffdingPruner(int max_seeds, double score_range, double delta=0.05, int min_samples=5, bool is_minimize=true)
        : score_range(score_range), delta(delta), min_samples(min_samples), is_minimize(is_minimize), max_seeds(max_seeds),
          best_count(0), best_sum(0), current_count(0), current_sum(0), common_count(0), diff_sum(0.0) {

        best_scores.assign(max_seeds, 0);
        has_best_score.assign(max_seeds, false);
        current_scores.assign(max_seeds, 0);
        has_current_score.assign(max_seeds, false);
    }

    /// @brief シード seed での評価スコア score を報告する / 同じ seed を再び報告すると上書きする
    void report(int seed, ScoreType score) {
        if (!has_current_score[seed]) {
            has_current_score[seed] = true;
            current_count++;
            if (has_best_score[seed]) {
                common_count++;
                double diff = is_minimize ? (double)(score - best_scores[seed])
                                          : (double)(best_scores[seed] - score);
                diff_sum += diff;
            }
        } else {
            current_sum -= current_scores[seed];
            if (has_best_score[seed]) {
                double old_diff = is_minimize ? (double)(current_scores[seed] - best_scores[seed])
                                              : (double)(best_scores[seed] - current_scores[seed]);
                double new_diff = is_minimize ? (double)(score - best_scores[seed])
                                              : (double)(best_scores[seed] - score);
                diff_sum += (new_diff - old_diff);
            }
        }
        current_scores[seed] = score;
        current_sum += score;
    }

    /// @brief 現パラメータが最良パラメータより統計的に有意に劣り、枝刈りすべきなら true を返す
    bool should_prune() const {
        if (best_count == 0 || common_count < min_samples) return false;

        double mean_diff = diff_sum / common_count;
        double epsilon = score_range * sqrt(log(1.0 / delta) / (2.0 * common_count));

        return mean_diff > epsilon;
    }

    /// @brief 現パラメータの評価を終え、次のパラメータに備える / 枝刈りで終えたなら was_pruned を true にする
    /// 枝刈りされずに min_samples 個以上のシードを評価し、平均が最良より良いときだけ最良を置き換える
    void next_param(bool was_pruned) {
        if (!was_pruned && current_count >= min_samples) {
            bool update = false;
            if (best_count == 0) {
                update = true;
            } else {
                double current_mean = (double)current_sum / current_count;
                double best_mean = (double)best_sum / best_count;

                if (is_minimize && current_mean < best_mean) update = true;
                if (!is_minimize && current_mean > best_mean) update = true;
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
