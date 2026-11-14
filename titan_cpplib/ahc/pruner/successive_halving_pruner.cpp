/// https://github.com/titan-23/Library_cpp/blob/main/titan_cpplib/ahc/pruner/successive_halving_pruner.cpp
#pragma once

#include <vector>
#include <algorithm>
#include <numeric>
using namespace std;

namespace titan23 {

/// @brief 平均スコアが下位の候補を段階的に削る逐次半減法の枝刈り器
template<typename ScoreType=double>
class SuccessiveHalvingPruner {
private:
    int initial_candidates;
    bool is_maximize;
    int eta;

    vector<ScoreType> sum_scores;
    vector<int> eval_counts;
    vector<int> survivors;
    vector<bool> is_active;

public:
    /// @brief 候補数 initial_candidates で構築する / is_maximize の既定は最大化
    /// eta はラウンドごとの削減率で、既定の 2 では毎回半分に枝刈りする
    SuccessiveHalvingPruner(int initial_candidates, bool is_maximize=true, int eta=2)
        : initial_candidates(initial_candidates), is_maximize(is_maximize), eta(eta) {

        sum_scores.assign(initial_candidates, 0);
        eval_counts.assign(initial_candidates, 0);
        survivors.resize(initial_candidates);
        iota(survivors.begin(), survivors.end(), 0);
        is_active.assign(initial_candidates, true);
    }

    /// @brief 候補 id の評価スコア score を報告する / id は [0, initial_candidates)、枝刈り済みの候補は無視する
    void report(int id, ScoreType score) {
        if (!is_active[id]) return;
        sum_scores[id] += score;
        eval_counts[id]++;
    }

    /// @brief 生存者を 1/eta に枝刈りし、生存者の id の一覧を返す / 少なくとも 1 個は残す
    const vector<int>& prune() {
        int next_size = max(1, (int)survivors.size() / eta);
        return prune_to(next_size);
    }

    /// @brief 生存者が keep_count 個になるまで下位を枝刈りし、生存者の id の一覧を返す
    const vector<int>& prune_to(int keep_count) {
        if (keep_count >= (int)survivors.size()) return survivors;
        auto comp = [&] (int a, int b) {
            double mean_a = eval_counts[a] == 0 ? 0 : (double)sum_scores[a] / eval_counts[a];
            double mean_b = eval_counts[b] == 0 ? 0 : (double)sum_scores[b] / eval_counts[b];
            if (is_maximize) return mean_a > mean_b;
            return mean_a < mean_b;
        };
        nth_element(survivors.begin(), survivors.begin() + keep_count, survivors.end(), comp);
        for (int i = keep_count; i < (int)survivors.size(); ++i) {
            is_active[survivors[i]] = false;
        }
        survivors.resize(keep_count);
        return survivors;
    }

    /// @brief 現在の生存者の id の一覧を返す
    const vector<int>& get_survivors() const {
        return survivors;
    }

    /// @brief 候補 id が生き残っているかを返す
    bool is_surviving(int id) const {
        return is_active[id];
    }

    /// @brief 状態を初期化し、別の探索で再利用できるようにする
    void clear() {
        fill(sum_scores.begin(), sum_scores.end(), 0);
        fill(eval_counts.begin(), eval_counts.end(), 0);
        survivors.resize(initial_candidates);
        iota(survivors.begin(), survivors.end(), 0);
        fill(is_active.begin(), is_active.end(), true);
    }
};
} //namespace titan23
