/// https://github.com/titan-23/Library_cpp/blob/main/titan_cpplib/ahc/tsp/tsp_iterated_local_search.cpp
#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <stdexcept>
#include <type_traits>
#include <utility>
#include <vector>
#include "titan_cpplib/ahc/tsp/tsp_local_search.cpp"
using namespace std;
namespace titan23 {
/// @brief tsp_iterated_local_search の設定 / 上限は -1 で無効 / 時間と反復回数の少なくとも一方の上限を有効にする
struct TspIteratedLocalSearchOptions {
    /// 実行時間の上限 [ms]
    double time_limit_ms = -1;
    /// Double Bridge を適用する反復の回数の上限 / 0 なら最初の局所探索だけを行う
    int64_t max_iterations = -1;
    /// Double Bridge で並べ替える 3 つの区間のそれぞれの長さの上限
    int max_kick_length = 50;
    /// 悪化を採択する確率を定める温度の初期値を、巡回路の辺の平均コストに対する比で指定する
    /// 温度は進行度に対して線形に 0 まで下げる / 0 なら悪化を採択しない
    double temperature_ratio = 0.5;
    /// 乱数のシード
    uint32_t seed = 23;
    /// 局所探索で 3 辺を交換する 3-opt も評価するか / false なら 2-opt と Or-opt だけを評価する
    bool use_three_opt = true;
};

/// @brief tsp_iterated_local_search の統計と停止した理由
struct TspIteratedLocalSearchResult {
    /// Double Bridge を適用した反復の回数
    int64_t iterations;
    /// 採択した反復の回数
    int64_t accepted_iterations;
    /// 局所探索で適用した操作の数
    int64_t applied_moves;
    double elapsed_ms;
    TspSearchStopReason stop_reason;
};

namespace tsp_iterated_local_search_detail {
inline void check_options(const TspIteratedLocalSearchOptions& options) {
    if (!isfinite(options.time_limit_ms) || (options.time_limit_ms < 0 && options.time_limit_ms != -1)) throw invalid_argument("tsp_iterated_local_search: time_limit_ms must be -1 or nonnegative");
    if (options.max_iterations < -1) throw invalid_argument("tsp_iterated_local_search: max_iterations must be -1 or nonnegative");
    if (options.max_kick_length <= 0) throw invalid_argument("tsp_iterated_local_search: max_kick_length must be positive");
    if (!isfinite(options.temperature_ratio) || options.temperature_ratio < 0) throw invalid_argument("tsp_iterated_local_search: temperature_ratio must be finite and nonnegative");
    if (options.time_limit_ms == -1 && options.max_iterations == -1) {
        throw invalid_argument("tsp_iterated_local_search: a time or iteration limit must be enabled");
    }
}

class Xorshift {
    uint64_t state_;

public:
    explicit Xorshift(uint32_t seed) : state_(0x9e3779b97f4a7c15ULL ^ ((uint64_t)seed << 1 | 1)) {
        for (int i = 0; i < 8; ++i) next();
    }

    uint32_t next() {
        state_ ^= state_ << 7;
        state_ ^= state_ >> 9;
        return (uint32_t)(state_ >> 16);
    }

    // [0, n) の整数
    int below(int n) {
        return (int)(((uint64_t)next() * (uint64_t)n) >> 32);
    }
};
}

/// @brief 2-opt、Or-opt、3-opt の局所探索と Double Bridge を繰り返す反復局所探索で state を改善する
/// 終了時の state は探索中に見つけたコストが最小の巡回路になる / 巡回路の向きは変化する場合がある / order()[0] は変化しない
/// 1 回の反復は、近い位置の 4 辺を交換する Double Bridge を適用し、その端点の周囲だけを局所探索で改善する
template<class EdgeCost>
TspIteratedLocalSearchResult tsp_iterated_local_search(
    const TspProblem<EdgeCost>& problem,
    const TspCandidates& candidates,
    TspState<typename TspProblem<EdgeCost>::Cost>& state,
    TspIteratedLocalSearchOptions options
) {
    using Cost = typename TspProblem<EdgeCost>::Cost;
    static_assert(is_convertible_v<Cost, double>, "tsp_iterated_local_search: Cost must be convertible to double");
    tsp_iterated_local_search_detail::check_options(options);
    if (!candidates.belongs_to(problem)) throw invalid_argument("tsp_iterated_local_search: candidates and problem do not match");
    if (!state.belongs_to(problem)) throw invalid_argument("tsp_iterated_local_search: state and problem do not match");
    tsp_local_search_detail::SearchTimer timer;
    int n = state.size();
    if (n < 4 || candidates.candidate_count() == 0) return {0, 0, 0, timer.elapsed_ms(), TspSearchStopReason::completed};
    bool time_limited = options.time_limit_ms >= 0;
    auto time_over = [&] { return time_limited && timer.elapsed_ms() >= options.time_limit_ms; };
    if (time_over()) return {0, 0, 0, timer.elapsed_ms(), TspSearchStopReason::time_limit};
    tsp_local_search_detail::TourSearch<EdgeCost> search(problem, candidates, state.order(), state.total_cost(), false);
    search.set_use_three_opt(options.use_three_opt);
    int64_t iterations = 0, accepted_iterations = 0;
    TspSearchStopReason reason = TspSearchStopReason::completed;
    search.push_all();
    if (!search.run(time_over)) {
        search.clear_queue();
        reason = TspSearchStopReason::time_limit;
    }
    int max_length = min(options.max_kick_length, (n - 1) / 3);
    // best_order は、現在の巡回路よりコストが小さい巡回路を保存している場合だけ有効
    vector<int> best_order;
    bool has_best = false;
    // 現在の巡回路のコストから、探索中に見つけた最小のコストを引いた値
    Cost excess{};
    if (reason == TspSearchStopReason::completed && n >= 8) {
        tsp_iterated_local_search_detail::Xorshift random(options.seed);
        double base_temperature = options.temperature_ratio * (double)state.total_cost() / n;
        while (true) {
            if (options.max_iterations >= 0 && iterations >= options.max_iterations) {
                reason = TspSearchStopReason::iteration_limit;
                break;
            }
            double now = time_limited ? timer.elapsed_ms() : 0;
            if (time_limited && now >= options.time_limit_ms) {
                reason = TspSearchStopReason::time_limit;
                break;
            }
            int start = random.below(n);
            int length1 = 1 + random.below(max_length);
            int length2 = 1 + random.below(max_length);
            int length3 = 1 + random.below(max_length);
            search.reset_delta();
            search.start_log();
            search.double_bridge(start, length1, length2, length3, true);
            bool finished = search.run(time_over);
            if (!finished) search.clear_queue();
            Cost delta = search.delta_sum();
            bool accept = finished && !(Cost{} < delta);
            if (finished && !accept && base_temperature > 0) {
                double progress = 0;
                if (time_limited) progress = now / options.time_limit_ms;
                if (options.max_iterations >= 0) progress = max(progress, (double)iterations / (double)options.max_iterations);
                double temperature = base_temperature * (1 - progress);
                double u = ((double)random.next() + 0.5) / 4294967296.0;
                accept = (double)delta <= -temperature * log(u);
            }
            if (accept) {
                if (Cost{} < delta && !has_best && !(Cost{} < excess)) {
                    // 最小のコストの巡回路から悪化する場合は、この反復を適用する前の巡回順を保存する
                    search.undo_log();
                    search.double_bridge(start, length3, length2, length1, false);
                    best_order = search.order();
                    search.double_bridge(start, length1, length2, length3, false);
                    search.redo_log();
                    has_best = true;
                }
                excess += delta;
                if (excess < Cost{}) {
                    excess = Cost{};
                    has_best = false;
                }
                ++accepted_iterations;
            } else {
                search.undo_log();
                search.double_bridge(start, length3, length2, length1, false);
            }
            if (!finished) {
                reason = TspSearchStopReason::time_limit;
                break;
            }
            ++iterations;
        }
    }
    if (has_best && Cost{} < excess) search.set_order(best_order);
    tsp_local_search_detail::write_back(problem, state, search.order());
    return {iterations, accepted_iterations, search.applied_moves(), timer.elapsed_ms(), reason};
}

/// @brief 点数 node_count と辺のコストを返す関数 edge_cost から、反復局所探索で巡回順を求めて返す / 返す巡回順の先頭は start
/// time_limit_ms は候補表と初期解の作成を含む実行時間の上限 [ms] / 候補表と初期解の作成は O(n^2) で、途中で中断しない
template<class EdgeCost>
vector<int> solve_tsp(int node_count, EdgeCost edge_cost, double time_limit_ms, int start = 0, uint32_t seed = 23) {
    tsp_local_search_detail::SearchTimer timer;
    TspProblem<EdgeCost> problem(node_count, move(edge_cost));
    TspCandidates candidates = make_tsp_candidates(problem, 10);
    auto state = problem.make_nearest_neighbor_state(start);
    TspIteratedLocalSearchOptions options;
    options.time_limit_ms = max(0.0, time_limit_ms - timer.elapsed_ms());
    options.seed = seed;
    tsp_iterated_local_search(problem, candidates, state, options);
    return state.order();
}
}
