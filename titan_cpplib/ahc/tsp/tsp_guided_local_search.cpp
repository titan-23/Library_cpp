/// https://github.com/titan-23/Library_cpp/blob/main/titan_cpplib/ahc/tsp/tsp_guided_local_search.cpp
#pragma once
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>
#include "titan_cpplib/ahc/tsp/tsp_local_search.cpp"
using namespace std;
namespace titan23 {
/// @brief tsp_guided_local_search の設定 / 上限は -1 で無効 / 時間とペナルティの更新回数の少なくとも一方の上限を有効にする
struct TspGuidedLocalSearchOptions {
    int64_t max_penalty_rounds = -1;
    int64_t max_evaluated_moves = -1;
    double time_limit_ms = -1;
    /// ペナルティ 1 あたりのコストを、最初の局所最適解の辺の平均コストに対する比で指定する
    long double penalty_ratio = 0.3L;
};

/// @brief tsp_guided_local_search の統計と停止した理由
struct TspGuidedLocalSearchResult {
    int64_t evaluated_moves;
    int64_t applied_moves;
    int64_t penalty_rounds;
    int64_t best_updates;
    double elapsed_ms;
    TspSearchStopReason stop_reason;
};

namespace tsp_guided_detail {
using tsp_local_search_detail::SearchTimer;

inline uint64_t edge_key(int u, int v) {
    if (u > v) swap(u, v);
    return (uint64_t)(uint32_t)u << 32 | (uint32_t)v;
}

inline int64_t get_penalty(const unordered_map<uint64_t, int64_t>& penalties, int u, int v) {
    auto it = penalties.find(edge_key(u, v));
    return it == penalties.end() ? 0 : it->second;
}

inline void increment_counter(int64_t& counter, const char* message) {
    if (counter == numeric_limits<int64_t>::max()) throw overflow_error(message);
    ++counter;
}

inline void check_options(const TspGuidedLocalSearchOptions& options) {
    if (options.max_penalty_rounds < -1) throw invalid_argument("tsp_guided_local_search: max_penalty_rounds must be -1 or nonnegative");
    if (options.max_evaluated_moves < -1) throw invalid_argument("tsp_guided_local_search: max_evaluated_moves must be -1 or nonnegative");
    if (!isfinite(options.time_limit_ms) || (options.time_limit_ms < 0 && options.time_limit_ms != -1)) throw invalid_argument("tsp_guided_local_search: time_limit_ms must be -1 or nonnegative");
    if (!isfinite(options.penalty_ratio) || options.penalty_ratio < 0) throw invalid_argument("tsp_guided_local_search: penalty_ratio must be finite and nonnegative");
    if (options.max_penalty_rounds == -1 && options.time_limit_ms == -1) {
        throw invalid_argument("tsp_guided_local_search: a time or penalty round limit must be enabled");
    }
}

/// @brief 辺のペナルティを保持するハッシュ表 / ペナルティを加えた辺だけを保存する
class EdgePenaltyTable {
public:
    static constexpr bool enabled = true;

    /// @brief expected_edges 本の辺を再配置なしで保存できる大きさで構築する
    explicit EdgePenaltyTable(size_t expected_edges) {
        size_t capacity = 64;
        while (capacity < expected_edges * 2) capacity *= 2;
        resize(capacity);
    }

    /// @brief 辺 {u, v} のペナルティを返す / 平均 O(1)
    int64_t get(int u, int v) const {
        uint64_t key = edge_key(u, v);
        size_t index = slot(key);
        while (keys_[index] != empty_key) {
            if (keys_[index] == key) return values_[index];
            index = (index + 1) & mask_;
        }
        return 0;
    }

    /// @brief 辺 {u, v} のペナルティを 1 増やす / 平均 O(1)
    void increment(int u, int v) {
        uint64_t key = edge_key(u, v);
        size_t index = slot(key);
        while (keys_[index] != empty_key) {
            if (keys_[index] == key) {
                ++values_[index];
                return;
            }
            index = (index + 1) & mask_;
        }
        keys_[index] = key;
        values_[index] = 1;
        if (++size_ * 2 > keys_.size()) grow();
    }

private:
    static constexpr uint64_t empty_key = numeric_limits<uint64_t>::max();
    vector<uint64_t> keys_;
    vector<int64_t> values_;
    size_t mask_ = 0;
    size_t size_ = 0;
    int shift_ = 0;

    size_t slot(uint64_t key) const {
        return (size_t)((key * 0x9e3779b97f4a7c15ULL) >> shift_);
    }

    // capacity は 2 の冪で 64 以上
    void resize(size_t capacity) {
        keys_.assign(capacity, empty_key);
        values_.assign(capacity, 0);
        mask_ = capacity - 1;
        shift_ = 64;
        while (((size_t)1 << (64 - shift_)) < capacity) --shift_;
    }

    void grow() {
        vector<uint64_t> old_keys = move(keys_);
        vector<int64_t> old_values = move(values_);
        resize(old_keys.size() * 2);
        for (size_t i = 0; i < old_keys.size(); ++i) {
            if (old_keys[i] == empty_key) continue;
            size_t index = slot(old_keys[i]);
            while (keys_[index] != empty_key) index = (index + 1) & mask_;
            keys_[index] = old_keys[i];
            values_[index] = old_values[i];
        }
    }
};
}

/// @brief 誘導局所探索で state を改善する / 終了時の state は、局所探索が収束した時点の巡回路のうちペナルティを含まないコストが最小のもの
/// 巡回路の向きは変化する場合がある / order()[0] は変化しない
/// ペナルティを加えた辺の端点だけをキューに追加し、その周囲の 2-opt だけを再評価する
template<class EdgeCost>
TspGuidedLocalSearchResult tsp_guided_local_search(
    const TspProblem<EdgeCost>& problem,
    const TspCandidates& candidates,
    TspState<typename TspProblem<EdgeCost>::Cost>& state,
    TspGuidedLocalSearchOptions options
) {
    using Cost = typename TspProblem<EdgeCost>::Cost;
    static_assert(is_convertible_v<Cost, double>, "tsp_guided_local_search: Cost must be convertible to double");
    tsp_guided_detail::check_options(options);
    if (!candidates.belongs_to(problem)) throw invalid_argument("tsp_guided_local_search: candidates and problem do not match");
    if (!state.belongs_to(problem)) throw invalid_argument("tsp_guided_local_search: state and problem do not match");
    tsp_guided_detail::SearchTimer timer;
    int n = state.size();
    if (n < 4 || candidates.candidate_count() == 0) return {0, 0, 0, 0, timer.elapsed_ms(), TspSearchStopReason::completed};
    if (options.time_limit_ms == 0) return {0, 0, 0, 0, timer.elapsed_ms(), TspSearchStopReason::time_limit};
    if (options.max_evaluated_moves == 0) return {0, 0, 0, 0, timer.elapsed_ms(), TspSearchStopReason::evaluated_move_limit};
    using Penalty = tsp_guided_detail::EdgePenaltyTable;
    // ペナルティを加えた辺を削除する 2-opt をその辺の端点からすべて評価するために、近傍を対称にする
    tsp_local_search_detail::TourSearch<EdgeCost, Penalty> search(problem, candidates, state.order(), state.total_cost(), true);
    search.set_use_or_opt(false);
    if (options.max_evaluated_moves >= 0) search.set_evaluation_limit(options.max_evaluated_moves);
    bool time_limited = options.time_limit_ms >= 0;
    auto time_over = [&] { return time_limited && timer.elapsed_ms() >= options.time_limit_ms; };
    int64_t penalty_rounds = 0, best_updates = 0;
    Cost current_cost = state.total_cost();
    Cost best_cost = current_cost;
    // has_best が false の場合は現在の巡回路が最良解であり、true の場合は best_order が最良解を保持する
    vector<int> best_order;
    bool has_best = false;
    // キューが空になるまで探索して最良解のコストを更新し、探索が終了した理由を返す
    auto descend = [&]() {
        search.reset_delta();
        bool finished = search.run(time_over);
        current_cost += search.delta_sum();
        if (current_cost < best_cost) {
            best_cost = current_cost;
            has_best = false;
            ++best_updates;
        }
        if (finished) return TspSearchStopReason::completed;
        search.clear_queue();
        return search.evaluation_limit_reached() ? TspSearchStopReason::evaluated_move_limit : TspSearchStopReason::time_limit;
    };
    auto finish = [&](TspSearchStopReason reason) {
        if (has_best) search.set_order(best_order);
        if (search.applied_moves() > 0) tsp_local_search_detail::write_back(problem, state, search.order());
        return TspGuidedLocalSearchResult{search.evaluated_moves(), search.applied_moves(), penalty_rounds, best_updates, timer.elapsed_ms(), reason};
    };
    search.push_all();
    TspSearchStopReason reason = descend();
    if (reason != TspSearchStopReason::completed) return finish(reason);
    if (options.max_penalty_rounds == 0) return finish(TspSearchStopReason::penalty_round_limit);
    double lambda = (double)options.penalty_ratio * (double)best_cost / n;
    Penalty penalties((size_t)n * 4);
    search.set_penalty(&penalties, lambda);
    vector<int> selected;
    while (true) {
        if (time_over()) return finish(TspSearchStopReason::time_limit);
        if (options.max_penalty_rounds >= 0 && penalty_rounds >= options.max_penalty_rounds) return finish(TspSearchStopReason::penalty_round_limit);
        // 現在の巡回路の辺のうち、コストを 1 + ペナルティで割った値が最大の辺すべてにペナルティを加える
        const vector<int>& order = search.order();
        double maximum_utility = -1;
        selected.clear();
        for (int i = 0; i < n; ++i) {
            int u = order[i], v = order[i + 1 == n ? 0 : i + 1];
            double utility = (double)problem.edge_cost(u, v) / (1 + (double)penalties.get(u, v));
            if (maximum_utility < utility) {
                maximum_utility = utility;
                selected.clear();
                selected.push_back(i);
            } else if (maximum_utility == utility) {
                selected.push_back(i);
            }
        }
        if (!has_best) {
            // 現在の巡回路が最良解であるため、ペナルティで変化する前に保存する
            best_order = order;
            has_best = true;
        }
        for (int i : selected) {
            int u = order[i], v = order[i + 1 == n ? 0 : i + 1];
            penalties.increment(u, v);
            search.push(u);
            search.push(v);
        }
        ++penalty_rounds;
        reason = descend();
        if (reason != TspSearchStopReason::completed) return finish(reason);
    }
}
}
