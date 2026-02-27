#include <vector>
#include <algorithm>
#include <numeric>
using namespace std;

namespace titan23 {

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
    /// @brief コンストラクタ
    /// @param initial_candidates 初期候補の総数
    /// @param is_maximize 最大化問題の場合はtrue、最小化問題の場合はfalse
    /// @param eta ラウンドごとの削減率（デフォルト2: 毎回半分に枝刈り）
    SuccessiveHalvingPruner(int initial_candidates, bool is_maximize=true, int eta=2)
        : initial_candidates(initial_candidates), is_maximize(is_maximize), eta(eta) {

        sum_scores.assign(initial_candidates, 0);
        eval_counts.assign(initial_candidates, 0);
        survivors.resize(initial_candidates);
        iota(survivors.begin(), survivors.end(), 0);
        is_active.assign(initial_candidates, true);
    }

    /// @brief 候補の評価スコアを報告します
    /// @param id 候補のID (0 ~ initial_candidates-1)
    /// @param score 評価関数の結果スコア
    void report(int id, ScoreType score) {
        if (!is_active[id]) return;
        sum_scores[id] += score;
        eval_counts[id]++;
    }

    /// @brief 現在の生存者を指定された割合(eta)で枝刈りします
    /// @return 枝刈り後の生存者IDリスト
    const vector<int>& prune() {
        int next_size = max(1, (int)survivors.size() / eta);
        return prune_to(next_size);
    }

    /// @brief 生存者が指定数になるまで下位を枝刈りします
    /// @param keep_count 残す候補の数
    /// @return 枝刈り後の生存者IDリスト
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

    /// @brief 現在の生存者IDリストを取得します
    const vector<int>& get_survivors() const {
        return survivors;
    }

    /// @brief 指定した候補が生き残っているかを判定します
    bool is_surviving(int id) const {
        return is_active[id];
    }

    /// @brief 状態をリセットし、別の探索で再利用可能にします
    void clear() {
        fill(sum_scores.begin(), sum_scores.end(), 0);
        fill(eval_counts.begin(), eval_counts.end(), 0);
        survivors.resize(initial_candidates);
        iota(survivors.begin(), survivors.end(), 0);
        fill(is_active.begin(), is_active.end(), true);
    }
};
} //namespace titan23
