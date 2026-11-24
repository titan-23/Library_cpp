/// https://github.com/titan-23/Library_cpp/blob/main/titan_cpplib/ahc/tsp/atsp_iterated_local_search.cpp
#pragma once
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <limits>
#include <queue>
#include <stdexcept>
#include <type_traits>
#include <utility>
#include <vector>
#include "titan_cpplib/ahc/tsp/tsp_iterated_local_search.cpp"
#include "titan_cpplib/graph/hungarian.cpp"
using namespace std;
namespace titan23 {
/// @brief 非対称 TSP の各点の近傍候補 / make_atsp_candidates で作る
/// 点 u から出る辺のコストが小さい点と、点 u に入る辺のコストが小さい点を、それぞれコストの昇順に保持する
struct AtspCandidates {
    int node_count = 0;
    /// 1 点あたりの候補数
    int candidate_count = 0;
    /// out[u * candidate_count + i] は edge_cost(u, v) が i 番目に小さい点 v
    vector<int> out;
    /// in[u * candidate_count + i] は edge_cost(v, u) が i 番目に小さい点 v
    vector<int> in;
    const void* problem_id = nullptr;
};

/// @brief 各点の近傍候補を c = min(requested_count, n - 1) 個ずつ求める / O(n^2) 回の edge_cost の呼び出しと、最悪 O(n^2 c) の挿入
/// コストが同じ点は点番号の小さい順に並べる
template<class EdgeCost>
AtspCandidates make_atsp_candidates(const TspProblem<EdgeCost>& problem, int requested_count) {
    using Cost = typename TspProblem<EdgeCost>::Cost;
    if (requested_count < 0) throw invalid_argument("make_atsp_candidates: requested_count must be nonnegative");
    int n = problem.node_count();
    int c = min(requested_count, n - 1);
    AtspCandidates candidates;
    candidates.node_count = n;
    candidates.candidate_count = c;
    candidates.problem_id = static_cast<const void*>(&problem);
    if (c == 0) return candidates;
    candidates.out.resize((size_t)n * (size_t)c);
    candidates.in.resize((size_t)n * (size_t)c);
    // u の昇順に全ての辺 (u, v) を走査し、出る辺の上位と入る辺の上位へ同時に挿入する
    vector<Cost> out_cost((size_t)c);
    vector<Cost> in_cost((size_t)n * (size_t)c);
    vector<int> in_count((size_t)n, 0);
    auto insert = [c](int* nodes, Cost* costs, int& count, int node, const Cost& cost) {
        if (count == c && !(cost < costs[count - 1])) return;
        int i = count < c ? count++ : count - 1;
        while (i > 0 && cost < costs[i - 1]) {
            costs[i] = costs[i - 1];
            nodes[i] = nodes[i - 1];
            --i;
        }
        costs[i] = cost;
        nodes[i] = node;
    };
    for (int u = 0; u < n; ++u) {
        int out_count = 0;
        int* out_nodes = candidates.out.data() + (size_t)u * (size_t)c;
        for (int v = 0; v < n; ++v) {
            if (u == v) continue;
            Cost cost = problem.edge_cost(u, v);
            insert(out_nodes, out_cost.data(), out_count, v, cost);
            insert(candidates.in.data() + (size_t)v * (size_t)c, in_cost.data() + (size_t)v * (size_t)c, in_count[v], u, cost);
        }
    }
    return candidates;
}

namespace atsp_detail {
// 割当の閉路を大きい順に併合して 1 本の巡回路にする / 閉路数が上限を超えた場合と中断時は空の列を返す
template<class EdgeCost, class Stop>
vector<int> merge_cycles(vector<int> next_node, int start, int max_cycles, EdgeCost cost, Stop stop) {
    using Weight = decltype(cost(0, 0));
    int n = (int)next_node.size();
    vector<int> cycle_of((size_t)n, -1);
    vector<int> cycle_size;
    for (int s = 0; s < n; ++s) {
        if (cycle_of[s] != -1) continue;
        int size = 0;
        for (int v = s; cycle_of[v] == -1; v = next_node[v]) {
            cycle_of[v] = (int)cycle_size.size();
            ++size;
        }
        cycle_size.push_back(size);
    }
    int cycle_count = (int)cycle_size.size();
    if (cycle_count > max_cycles) return {};
    vector<int> cycle_order((size_t)cycle_count);
    for (int i = 0; i < cycle_count; ++i) cycle_order[i] = i;
    stable_sort(cycle_order.begin(), cycle_order.end(), [&](int a, int b) { return cycle_size[a] > cycle_size[b]; });
    // main_nodes に併合済みの閉路の全点を保持する
    vector<vector<int>> members((size_t)cycle_count);
    for (int v = 0; v < n; ++v) members[cycle_of[v]].push_back(v);
    vector<int> main_nodes = members[cycle_order[0]];
    for (int index = 1; index < cycle_count; ++index) {
        const vector<int>& other = members[cycle_order[index]];
        int best_i = -1, best_j = -1;
        Weight best_delta{};
        for (int i : main_nodes) {
            if (stop()) return {};
            int next_i = next_node[i];
            Weight cost_i = cost(i, next_i);
            for (int j : other) {
                int next_j = next_node[j];
                Weight delta = cost(i, next_j) + cost(j, next_i) - cost_i - cost(j, next_j);
                if (best_i == -1 || delta < best_delta) {
                    best_delta = delta;
                    best_i = i;
                    best_j = j;
                }
            }
        }
        swap(next_node[best_i], next_node[best_j]);
        main_nodes.insert(main_nodes.end(), other.begin(), other.end());
    }
    vector<int> order;
    order.reserve((size_t)n);
    int v = start;
    for (int i = 0; i < n; ++i) {
        order.push_back(v);
        v = next_node[v];
    }
    return order;
}

template<class EdgeCost>
using AssignmentWeight = conditional_t<is_floating_point_v<typename TspProblem<EdgeCost>::Cost>,
    common_type_t<double, typename TspProblem<EdgeCost>::Cost>, long long>;

template<class EdgeCost, class Stop = hungarian_detail::NeverStop>
vector<int> assignment_order(const TspProblem<EdgeCost>& problem, int start, int max_cycles,
        Stop stop = {}, const vector<int>* feasible = nullptr) {
    using Weight = AssignmentWeight<EdgeCost>;
    int n = problem.node_count();
    if (n < 3) {
        vector<int> order = {start};
        for (int v = 0; v < n; ++v) if (v != start) order.push_back(v);
        return order;
    }
    int stride = hungarian_detail::padded<Weight>(n);
    vector<Weight> weight((size_t)n * stride);
    auto at = [&](int u, int v) -> Weight& { return weight[(size_t)u * stride + v]; };
    for (int u = 0; u < n; ++u) {
        if ((u & 15) == 0 && stop()) return {};
        for (int v = 0; v < n; ++v) if (u != v) at(u, v) = (Weight)problem.edge_cost(u, v);
    }
    // 既知の巡回路のコストを上界にして、自己ループを割当から除く
    using Sum = conditional_t<is_integral_v<Weight>, __int128_t, long double>;
    Sum bound{};
    for (int i = 0; i < n; ++i) {
        int j = i + 1 == n ? 0 : i + 1;
        bound += at(feasible ? (*feasible)[i] : i, feasible ? (*feasible)[j] : j);
    }
    Weight forbidden;
    if constexpr (is_integral_v<Weight>) {
        forbidden = bound < numeric_limits<Weight>::max() ? (Weight)(bound + 1) : numeric_limits<Weight>::max();
    } else {
        forbidden = nextafter((Weight)bound, numeric_limits<Weight>::infinity());
    }
    for (int u = 0; u < n; ++u) at(u, u) = forbidden;
    vector<int> next_node = hungarian_detail::solve(weight.data(), n, n, stride, stop);
    if (next_node.empty()) return {};
    return merge_cycles(move(next_node), start, max_cycles, [&](int u, int v) -> Sum { return at(u, v); }, stop);
}
}

/// @brief 割当問題の最適解の閉路を併合して、全点を巡る巡回順を作る / 返す巡回順の先頭は start
/// 割当問題の最適値が最適な巡回路のコストに近い問題では、最近傍法より良い初期解になる / 辺のコストに構造のない問題が該当する
/// ハンガリアン法の O(n^3) と、併合の O(n^2) / メモリは O(n^2)
template<class EdgeCost>
vector<int> make_atsp_assignment_order(const TspProblem<EdgeCost>& problem, int start) {
    if (start < 0 || start >= problem.node_count()) throw invalid_argument("make_atsp_assignment_order: start is out of range");
    return atsp_detail::assignment_order(problem, start, problem.node_count());
}

/// @brief atsp_iterated_local_search の設定 / 上限は -1 で無効 / 時間と反復回数の少なくとも一方の上限を有効にする
struct AtspIteratedLocalSearchOptions {
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
    /// 区間を反転する操作で反転する区間の長さの上限 / 0 なら区間を反転する操作を使用しない
    int max_reverse_length = 200;
};

/// @brief atsp_iterated_local_search の統計と停止した理由
struct AtspIteratedLocalSearchResult {
    /// Double Bridge を適用した反復の回数
    int64_t iterations;
    /// 採択した反復の回数
    int64_t accepted_iterations;
    /// 局所探索で適用した操作の数
    int64_t applied_moves;
    double elapsed_ms;
    TspSearchStopReason stop_reason;
};

namespace atsp_detail {
/// @brief 向きのある巡回路を配列で保持し、候補表とキューを使用して、区間の入れ替えと短い区間の反転を適用する探索用の内部クラス
/// 巡回路は開始位置を区別しない / 区間の反転では、区間内の各辺の向きによるコストの差を reverse_gain_ から求める
template<class EdgeCost>
class TourSearch {
public:
    using Cost = typename TspProblem<EdgeCost>::Cost;

    /// @brief 巡回順 order と候補表 candidates から構築する / 候補のうち巡回路に含まれない点は除く / O(node_count + n c)
    TourSearch(const TspProblem<EdgeCost>& problem, const AtspCandidates& candidates, const vector<int>& order, Cost total_cost, int max_reverse_length)
        : problem_(problem), n_((int)order.size()), k_(max(candidates.candidate_count, 1)), order_(order), position_((size_t)problem.node_count(), -1),
          out_((size_t)problem.node_count() * (size_t)k_, -1), in_((size_t)problem.node_count() * (size_t)k_, -1),
          out_cost_(out_.size(), Cost{}), in_cost_(in_.size(), Cost{}), reverse_gain_(max_reverse_length >= 2 ? (size_t)problem.node_count() : 0, Cost{}), max_reverse_length_(max_reverse_length),
          queue_(order.size()), in_queue_((size_t)problem.node_count(), 0) {
        for (int i = 0; i < n_; ++i) position_[order_[i]] = i;
        if (max_reverse_length_ >= 2) {
            for (int i = 0; i < n_; ++i) refresh(i);
        }
        int c = candidates.candidate_count;
        for (int u : order_) {
            size_t out_index = (size_t)u * (size_t)k_, in_index = out_index;
            for (int i = 0; i < c; ++i) {
                int v = candidates.out[(size_t)u * (size_t)c + i];
                if (position_[v] >= 0) {
                    out_[out_index] = v;
                    out_cost_[out_index++] = problem.edge_cost(u, v);
                }
                v = candidates.in[(size_t)u * (size_t)c + i];
                if (position_[v] >= 0) {
                    in_[in_index] = v;
                    in_cost_[in_index++] = problem.edge_cost(v, u);
                }
            }
        }
        // 浮動小数点数では、丸め誤差だけで生じた負の差分を改善として扱わない
        if constexpr (is_floating_point_v<Cost>) epsilon_ = total_cost / (Cost)n_ * (Cost)1e-10;
    }

    /// @brief 現在の巡回順を返す
    const vector<int>& order() const { return order_; }
    /// @brief 巡回順全体を order に置き換える / order は構築時と同じ点の集合 / O(n)
    void set_order(const vector<int>& order) {
        order_ = order;
        for (int i = 0; i < n_; ++i) position_[order_[i]] = i;
        if (max_reverse_length_ >= 2) {
            for (int i = 0; i < n_; ++i) refresh(i);
        }
    }
    /// @brief 評価した操作の数を返す
    int64_t evaluated_moves() const { return evaluated_moves_; }
    /// @brief 適用した操作の数を返す
    int64_t applied_moves() const { return applied_moves_; }
    /// @brief reset_delta の後に適用した操作のコストの差分の総和を返す
    Cost delta_sum() const { return delta_sum_; }
    /// @brief delta_sum を 0 にする
    void reset_delta() { delta_sum_ = Cost{}; }

    /// @brief 点 v をキューに追加する / 既にキューにある場合は何もしない / O(1)
    void push(int v) {
        if (in_queue_[v]) return;
        in_queue_[v] = 1;
        int tail = queue_head_ + queue_size_;
        if (tail >= n_) tail -= n_;
        queue_[tail] = v;
        ++queue_size_;
    }
    /// @brief 巡回路の全点をキューに追加する / O(n)
    void push_all() {
        for (int v : order_) push(v);
    }
    /// @brief キューを空にする
    void clear_queue() {
        while (queue_size_ > 0) {
            in_queue_[queue_[queue_head_]] = 0;
            if (++queue_head_ == n_) queue_head_ = 0;
            --queue_size_;
        }
    }

    /// @brief キューが空になるまで、キューの点を端点とする改善操作を適用する
    /// 128 点を処理するたびに stop() を呼び出し、true が返された場合は中断して false を返す / キューが空になった場合は true を返す
    template<class Stop>
    bool run(Stop&& stop) {
        int counter = 0;
        while (queue_size_ > 0) {
            if ((++counter & 127) == 0 && stop()) return false;
            int v = queue_[queue_head_];
            if (++queue_head_ == n_) queue_head_ = 0;
            --queue_size_;
            in_queue_[v] = 0;
            improve_node(v);
        }
        return true;
    }

    /// @brief 区間の入れ替えの記録を開始する / それまでの記録は破棄する
    void start_log() {
        logging_ = true;
        log_.clear();
    }
    /// @brief 記録した入れ替えと反転を逆順に元に戻して、記録の開始時の巡回順に戻す / 記録は保持する
    void undo_log() {
        for (size_t i = log_.size(); i-- > 0;) {
            if (log_[i].second_length < 0) reverse_block(log_[i].start, log_[i].first_length);
            else rotate_blocks(log_[i].start, log_[i].second_length, log_[i].first_length);
        }
    }
    /// @brief undo_log で戻した入れ替えと反転を再び適用する
    void redo_log() {
        for (size_t i = 0; i < log_.size(); ++i) {
            if (log_[i].second_length < 0) reverse_block(log_[i].start, log_[i].first_length);
            else rotate_blocks(log_[i].start, log_[i].first_length, log_[i].second_length);
        }
    }

    /// @brief 位置 start の直後に続く長さ length1, length2, length3 の区間 B, C, D を D, C, B の順に並べ替える Double Bridge を適用する
    /// length1 + length2 + length3 < n とする / evaluate が true ならコストの差分を delta_sum に加え、端点をキューに追加する
    /// 適用後に同じ start と長さ length3, length2, length1 で呼び出すと元の巡回順に戻る / O(length1 + length2 + length3)
    void double_bridge(int start, int length1, int length2, int length3, bool evaluate) {
        int total = length1 + length2 + length3;
        int first = wrap(start + 1);
        int second = wrap(first + length1);
        int third = wrap(second + length2);
        if (evaluate) {
            int before = order_[start], after = order_[wrap(third + length3)];
            int b_first = order_[first], b_last = order_[wrap(first + length1 - 1)];
            int c_first = order_[second], c_last = order_[wrap(second + length2 - 1)];
            int d_first = order_[third], d_last = order_[wrap(third + length3 - 1)];
            Cost delta{};
            delta += cost(before, d_first);
            delta += cost(d_last, c_first);
            delta += cost(c_last, b_first);
            delta += cost(b_last, after);
            delta -= cost(before, b_first);
            delta -= cost(b_last, c_first);
            delta -= cost(c_last, d_first);
            delta -= cost(d_last, after);
            delta_sum_ += delta;
            push(before);
            push(b_first);
            push(b_last);
            push(c_first);
            push(c_last);
            push(d_first);
            push(d_last);
            push(after);
        }
        work_.resize(total);
        copy_block(third, length3, work_.data());
        copy_block(second, length2, work_.data() + length3);
        copy_block(first, length1, work_.data() + length3 + length2);
        write_block(first, total);
        // 始点が変化した辺は、並べ替えた範囲の直前の点と、並べ替え後の 3 区間の末尾の点から出る辺
        if (max_reverse_length_ >= 2) {
            refresh(start);
            refresh(wrap(start + length3));
            refresh(wrap(start + length3 + length2));
            refresh(wrap(start + total));
        }
    }

private:
    // second_length が -1 の場合は、位置 start から始まる長さ first_length の区間の反転
    struct LogEntry {
        int start;
        int first_length;
        int second_length;
    };

    const TspProblem<EdgeCost>& problem_;
    int n_;
    int k_;
    vector<int> order_;
    vector<int> position_;
    // 点 u の候補は out_[u * k_] と in_[u * k_] から始まり、候補が k_ 個未満の場合は -1 で終わる
    vector<int> out_;
    vector<int> in_;
    vector<Cost> out_cost_;
    vector<Cost> in_cost_;
    // reverse_gain_[u] は、u から次の点 v への辺を逆向きにしたときのコストの増加量 cost(v, u) - cost(u, v)
    vector<Cost> reverse_gain_;
    int max_reverse_length_;
    // 現在の巡回路で順方向と逆方向のコストが異なる辺の数
    int asymmetric_edges_ = 0;
    vector<int> queue_;
    vector<unsigned char> in_queue_;
    int queue_head_ = 0;
    int queue_size_ = 0;
    vector<LogEntry> log_;
    bool logging_ = false;
    Cost epsilon_{};
    Cost delta_sum_{};
    int64_t evaluated_moves_ = 0;
    int64_t applied_moves_ = 0;
    vector<int> work_;

    // 辺 u -> v のコスト
    Cost cost(int u, int v) const { return problem_.edge_cost(u, v); }
    bool improves(const Cost& delta) const { return delta < -epsilon_; }
    int next(int v) const {
        int i = position_[v] + 1;
        return order_[i == n_ ? 0 : i];
    }
    int prev(int v) const {
        int i = position_[v];
        return order_[i == 0 ? n_ - 1 : i - 1];
    }
    // from から巡回路の向きに進んで v に達するまでの辺の数
    int offset(int from, int v) const {
        int result = position_[v] - position_[from];
        return result < 0 ? result + n_ : result;
    }
    int wrap(int index) const { return index >= n_ ? index - n_ : index; }
    // 位置 index の点から出る辺について reverse_gain_ を求め直す
    void refresh(int index) {
        int u = order_[index];
        int v = order_[index + 1 == n_ ? 0 : index + 1];
        asymmetric_edges_ -= reverse_gain_[u] != Cost{};
        reverse_gain_[u] = cost(v, u) - cost(u, v);
        asymmetric_edges_ += reverse_gain_[u] != Cost{};
    }
    // first から巡回路の向きに length 点続く区間を反転したときの、区間内の辺のコストの増加量 / O(length)
    Cost reverse_delta(int first, int length) const {
        if (asymmetric_edges_ == 0) return Cost{};
        Cost result{};
        int index = position_[first];
        int tail = min(length - 1, n_ - index);
        for (int i = index; i < index + tail; ++i) result += reverse_gain_[order_[i]];
        for (int i = 0; i < length - 1 - tail; ++i) result += reverse_gain_[order_[i]];
        return result;
    }

    // 位置 start から始まる長さ length の区間を反転する / 区間は配列の末尾から先頭へ続いてもよい / O(length)
    void reverse_block(int start, int length) {
        // 区間内の辺 u -> v は v -> u になるため、v の reverse_gain_ は u の値の符号を反転した値になる
        // 末尾側から更新することで、まだ参照していない値を残す
        if (asymmetric_edges_ != 0) {
            int index = wrap(start + length - 1);
            // 内部の差を 1 点ずつ移すと、先頭の値が重複し、末尾の値が除かれる
            asymmetric_edges_ += (reverse_gain_[order_[start]] != Cost{}) - (reverse_gain_[order_[index]] != Cost{});
            for (int t = length - 1; t > 0; --t) {
                int previous_index = index == 0 ? n_ - 1 : index - 1;
                reverse_gain_[order_[index]] = -reverse_gain_[order_[previous_index]];
                index = previous_index;
            }
        }
        int i = start, j = wrap(start + length - 1);
        for (int count = length >> 1; count > 0; --count) {
            int u = order_[i], v = order_[j];
            order_[i] = v;
            position_[v] = i;
            order_[j] = u;
            position_[u] = j;
            if (++i == n_) i = 0;
            if (--j < 0) j = n_ - 1;
        }
        // 区間の直前の点と、反転後の区間の末尾の点から出る辺は、辺の端点が変化する
        refresh(start == 0 ? n_ - 1 : start - 1);
        refresh(wrap(start + length - 1));
    }
    // first から巡回路の向きに length 点続く区間を反転し、記録する
    void reverse_segment(int first, int length) {
        int start = position_[first];
        reverse_block(start, length);
        if (logging_) log_.push_back({start, length, -1});
    }

    // 巡回路上の連続区間をコピーする / 配列の末尾をまたぐ場合は 2 区間に分ける
    void copy_block(int start, int length, int* dest) const {
        int tail = min(length, n_ - start);
        copy_n(order_.data() + start, tail, dest);
        copy_n(order_.data(), length - tail, dest + tail);
    }

    // 作業領域から巡回路に書き戻し、位置が変化した範囲の逆引きを更新する
    void write_block(int start, int length) {
        int tail = min(length, n_ - start);
        copy_n(work_.data(), tail, order_.data() + start);
        copy_n(work_.data() + tail, length - tail, order_.data());
        for (int i = start; i < start + tail; ++i) position_[order_[i]] = i;
        for (int i = 0; i < length - tail; ++i) position_[order_[i]] = i;
    }

    // 隣接する 2 区間の順序を交換する / O(first_length + second_length)
    void rotate_blocks(int start, int first_length, int second_length) {
        int length = first_length + second_length;
        work_.resize(length);
        copy_block(wrap(start + first_length), second_length, work_.data());
        copy_block(start, first_length, work_.data() + second_length);
        write_block(start, length);
        // 始点が変化した辺は、入れ替えた範囲の直前の点と、入れ替え後の 2 区間の末尾の点から出る辺
        if (max_reverse_length_ >= 2) {
            refresh(start == 0 ? n_ - 1 : start - 1);
            refresh(wrap(start + second_length - 1));
            refresh(wrap(start + first_length + second_length - 1));
        }
    }

    // 区間 B = [b, ..., c] と、その直後の区間 C = [next(c), ..., e] を入れ替える
    // 巡回路を A, B, C の 3 区間とみなし、最も長い区間を動かさずに残りの 2 区間を入れ替える / 巡回路としては同じ結果になる
    void swap_segments(int b, int c, int e) {
        int d = next(c);
        int f = next(e);
        int length_b = offset(b, c) + 1;
        int length_c = offset(d, e) + 1;
        int length_a = n_ - length_b - length_c;
        int start, first_length, second_length;
        if (length_a >= length_b && length_a >= length_c) {
            start = position_[b];
            first_length = length_b;
            second_length = length_c;
        } else if (length_b >= length_c) {
            start = position_[d];
            first_length = length_c;
            second_length = length_a;
        } else {
            start = position_[f];
            first_length = length_a;
            second_length = length_b;
        }
        rotate_blocks(start, first_length, second_length);
        if (logging_) log_.push_back({start, first_length, second_length});
    }

    void apply_swap(int a, int b, int c, int d, int e, int f, const Cost& delta) {
        swap_segments(b, c, e);
        delta_sum_ += delta;
        ++applied_moves_;
        push(a);
        push(b);
        push(c);
        push(d);
        push(e);
        push(f);
    }

    // t を端点とする改善操作を探し、最初に見つけた操作を適用する / 適用した場合は true
    // 評価する操作はすべて、巡回路上でこの順に現れる辺 a -> b, c -> d, e -> f を削除し、辺 a -> d, e -> b, c -> f を追加する
    // 結果は、区間 B = [b, ..., c] と区間 C = [d, ..., e] の入れ替えになる
    bool improve_node(int t) {
        const int* out = out_.data() + (size_t)t * (size_t)k_;
        const Cost* out_cost = out_cost_.data() + (size_t)t * (size_t)k_;
        const int* in = in_.data() + (size_t)t * (size_t)k_;
        const Cost* in_cost = in_cost_.data() + (size_t)t * (size_t)k_;
        {
            // a = t から出る辺 a -> b を a -> d に置き換え、続けて c から出る辺 c -> d を c -> f に置き換える
            int a = t, b = next(a);
            Cost cost_ab = cost(a, b);
            for (int i = 0; i < k_; ++i) {
                int d = out[i];
                if (d < 0) break;
                Cost gain1 = cost_ab - out_cost[i];
                if (!(Cost{} < gain1)) break;
                int c = prev(d);
                int offset_d = offset(b, d);
                Cost gain1_removed = gain1 + cost(c, d);
                const int* out_c = out_.data() + (size_t)c * (size_t)k_;
                const Cost* out_cost_c = out_cost_.data() + (size_t)c * (size_t)k_;
                for (int j = 0; j < k_; ++j) {
                    int f = out_c[j];
                    if (f < 0) break;
                    Cost gain2 = gain1_removed - out_cost_c[j];
                    if (!(Cost{} < gain2)) break;
                    if (offset(b, f) <= offset_d) continue;
                    int e = prev(f);
                    ++evaluated_moves_;
                    Cost delta = cost(e, b) - cost(e, f) - gain2;
                    if (!improves(delta)) continue;
                    apply_swap(a, b, c, d, e, f, delta);
                    return true;
                }
            }
        }
        {
            // b = t に入る辺 a -> b を e -> b に置き換え、続けて f に入る辺 e -> f を c -> f に置き換える
            int b = t, a = prev(b);
            Cost cost_ab = cost(a, b);
            for (int i = 0; i < k_; ++i) {
                int e = in[i];
                if (e < 0) break;
                Cost gain1 = cost_ab - in_cost[i];
                if (!(Cost{} < gain1)) break;
                int f = next(e);
                int offset_e = offset(b, e);
                Cost gain1_removed = gain1 + cost(e, f);
                const int* in_f = in_.data() + (size_t)f * (size_t)k_;
                const Cost* in_cost_f = in_cost_.data() + (size_t)f * (size_t)k_;
                for (int j = 0; j < k_; ++j) {
                    int c = in_f[j];
                    if (c < 0) break;
                    Cost gain2 = gain1_removed - in_cost_f[j];
                    if (!(Cost{} < gain2)) break;
                    if (offset(b, c) >= offset_e) continue;
                    int d = next(c);
                    ++evaluated_moves_;
                    Cost delta = cost(a, d) - cost(c, d) - gain2;
                    if (!improves(delta)) continue;
                    apply_swap(a, b, c, d, e, f, delta);
                    return true;
                }
            }
        }
        // Or-opt: 長さ 1 から 3 の区間 S = [first, ..., last] を辺 e -> f の間へ移動する
        // 辺 p -> first, last -> q, e -> f を削除し、辺 p -> q, e -> first, last -> f を追加する
        // 同じ区間を反転して移す場合も、元の位置から取り除く差分は共通
        Cost forward_removal[4]{}, backward_removal[4]{};
        {
            // t を S の先頭とし、t に入る辺の候補から e を選ぶ
            int first = t, last = t;
            int p = prev(first);
            Cost cost_p_first = cost(p, first);
            for (int length = 1; length <= 3; ++length) {
                if (length > 1) last = next(last);
                int q = next(last);
                if (last == p || q == p) break;
                Cost remove_gain = cost_p_first + cost(last, q) - cost(p, q);
                forward_removal[length] = remove_gain;
                if (!(Cost{} < remove_gain)) continue;
                for (int i = 0; i < k_; ++i) {
                    int e = in[i];
                    if (e < 0) break;
                    Cost cost_e_first = in_cost[i];
                    if (!(cost_e_first < remove_gain)) break;
                    if (e == p || offset(first, e) < length) continue;
                    int f = next(e);
                    ++evaluated_moves_;
                    Cost delta = cost_e_first + cost(last, f) - cost(e, f) - remove_gain;
                    if (!improves(delta)) continue;
                    apply_swap(p, first, last, q, e, f, delta);
                    return true;
                }
            }
        }
        {
            // t を S の末尾とし、t から出る辺の候補から f を選ぶ
            int first = t, last = t;
            int q = next(last);
            Cost cost_last_q = cost(last, q);
            for (int length = 1; length <= 3; ++length) {
                if (length > 1) first = prev(first);
                int p = prev(first);
                if (first == q || p == q) break;
                Cost remove_gain = cost(p, first) + cost_last_q - cost(p, q);
                backward_removal[length] = remove_gain;
                if (!(Cost{} < remove_gain)) continue;
                for (int i = 0; i < k_; ++i) {
                    int f = out[i];
                    if (f < 0) break;
                    Cost cost_last_f = out_cost[i];
                    if (!(cost_last_f < remove_gain)) break;
                    if (f == q || offset(first, f) < length) continue;
                    int e = prev(f);
                    ++evaluated_moves_;
                    Cost delta = cost_last_f + cost(e, first) - cost(e, f) - remove_gain;
                    if (!improves(delta)) continue;
                    apply_swap(p, first, last, q, e, f, delta);
                    return true;
                }
            }
        }
        if (max_reverse_length_ < 2) return false;
        // 2-opt: 辺 a -> b, c -> d を削除し、区間 [b, ..., c] を反転して、辺 a -> c, b -> d を追加する
        {
            // a = t から出る辺 a -> b を a -> c に置き換える
            int a = t, b = next(a);
            Cost cost_ab = cost(a, b);
            for (int i = 0; i < k_; ++i) {
                int c = out[i];
                if (c < 0) break;
                Cost gain1 = cost_ab - out_cost[i];
                if (!(Cost{} < gain1)) break;
                int length = offset(b, c) + 1;
                if (length > max_reverse_length_) continue;
                int d = next(c);
                ++evaluated_moves_;
                Cost delta = cost(b, d) - cost(c, d) - gain1 + reverse_delta(b, length);
                if (!improves(delta)) continue;
                apply_reverse(a, b, c, d, length, delta);
                return true;
            }
        }
        {
            // d = t に入る辺 c -> d を b -> d に置き換える
            int d = t, c = prev(d);
            Cost cost_cd = cost(c, d);
            for (int i = 0; i < k_; ++i) {
                int b = in[i];
                if (b < 0) break;
                Cost gain1 = cost_cd - in_cost[i];
                if (!(Cost{} < gain1)) break;
                int length = offset(b, c) + 1;
                if (length > max_reverse_length_) continue;
                int a = prev(b);
                ++evaluated_moves_;
                Cost delta = cost(a, c) - cost(a, b) - gain1 + reverse_delta(b, length);
                if (!improves(delta)) continue;
                apply_reverse(a, b, c, d, length, delta);
                return true;
            }
        }
        // 反転 Or-opt: 長さ 2 または 3 の区間 S = [first, ..., last] を反転して辺 e -> f の間へ移動する
        // 辺 p -> first, last -> q, e -> f を削除し、辺 p -> q, e -> last, first -> f を追加する
        {
            // t を S の末尾とし、t に入る辺の候補から e を選ぶ
            int first = t, last = t;
            int q = next(last);
            for (int length = 2; length <= 3; ++length) {
                first = prev(first);
                int p = prev(first);
                if (first == q || p == q) break;
                Cost remove_gain = backward_removal[length];
                if (!(Cost{} < remove_gain)) continue;
                Cost inside = reverse_delta(first, length);
                for (int i = 0; i < k_; ++i) {
                    int e = in[i];
                    if (e < 0) break;
                    Cost cost_e_last = in_cost[i];
                    if (!(cost_e_last < remove_gain)) break;
                    if (e == p || offset(first, e) < length) continue;
                    int f = next(e);
                    ++evaluated_moves_;
                    Cost delta = cost_e_last + cost(first, f) - cost(e, f) - remove_gain + inside;
                    if (!improves(delta)) continue;
                    apply_reversed_swap(p, first, last, q, e, f, length, delta);
                    return true;
                }
            }
        }
        {
            // t を S の先頭とし、t から出る辺の候補から f を選ぶ
            int first = t, last = t;
            int p = prev(first);
            for (int length = 2; length <= 3; ++length) {
                last = next(last);
                int q = next(last);
                if (last == p || q == p) break;
                Cost remove_gain = forward_removal[length];
                if (!(Cost{} < remove_gain)) continue;
                Cost inside = reverse_delta(first, length);
                for (int i = 0; i < k_; ++i) {
                    int f = out[i];
                    if (f < 0) break;
                    Cost cost_first_f = out_cost[i];
                    if (!(cost_first_f < remove_gain)) break;
                    if (f == q || offset(first, f) < length) continue;
                    int e = prev(f);
                    ++evaluated_moves_;
                    Cost delta = cost_first_f + cost(e, last) - cost(e, f) - remove_gain + inside;
                    if (!improves(delta)) continue;
                    apply_reversed_swap(p, first, last, q, e, f, length, delta);
                    return true;
                }
            }
        }
        return false;
    }

    void apply_reverse(int a, int b, int c, int d, int length, const Cost& delta) {
        reverse_segment(b, length);
        delta_sum_ += delta;
        ++applied_moves_;
        push(a);
        push(b);
        push(c);
        push(d);
    }

    // 区間 S = [first, ..., last] を e と f の間へ移動してから反転する
    void apply_reversed_swap(int p, int first, int last, int q, int e, int f, int length, const Cost& delta) {
        swap_segments(first, last, e);
        reverse_segment(first, length);
        delta_sum_ += delta;
        ++applied_moves_;
        push(p);
        push(first);
        push(last);
        push(q);
        push(e);
        push(f);
    }
};

inline void check_options(const AtspIteratedLocalSearchOptions& options) {
    if (!isfinite(options.time_limit_ms) || (options.time_limit_ms < 0 && options.time_limit_ms != -1)) throw invalid_argument("atsp_iterated_local_search: time_limit_ms must be -1 or nonnegative");
    if (options.max_iterations < -1) throw invalid_argument("atsp_iterated_local_search: max_iterations must be -1 or nonnegative");
    if (options.max_kick_length <= 0) throw invalid_argument("atsp_iterated_local_search: max_kick_length must be positive");
    if (!isfinite(options.temperature_ratio) || options.temperature_ratio < 0) throw invalid_argument("atsp_iterated_local_search: temperature_ratio must be finite and nonnegative");
    if (options.time_limit_ms == -1 && options.max_iterations == -1) {
        throw invalid_argument("atsp_iterated_local_search: a time or iteration limit must be enabled");
    }
}
}

/// @brief 巡回順 order のコストを返す / 辺は order[i] から order[i + 1] への向きで、末尾の点から先頭の点への辺を含む / O(n)
template<class EdgeCost>
typename TspProblem<EdgeCost>::Cost atsp_tour_cost(const TspProblem<EdgeCost>& problem, const vector<int>& order) {
    typename TspProblem<EdgeCost>::Cost total{};
    int n = (int)order.size();
    if (n < 2) return total;
    for (int i = 0; i < n; ++i) total += problem.edge_cost(order[i], order[i + 1 == n ? 0 : i + 1]);
    return total;
}

/// @brief 非対称 TSP の巡回順 order を反復局所探索で改善する / 辺のコストは edge_cost(u, v) と edge_cost(v, u) が異なってよい
/// order は重複しない点の列で、辺は order[i] から order[i + 1] への向き / 終了時の order は探索中に見つけたコストが最小の巡回順で、order[0] は変化しない
/// 局所探索は区間の入れ替えと Or-opt を評価し、設定に応じて 2-opt と反転 Or-opt を加える
template<class EdgeCost>
AtspIteratedLocalSearchResult atsp_iterated_local_search(
    const TspProblem<EdgeCost>& problem,
    const AtspCandidates& candidates,
    vector<int>& order,
    AtspIteratedLocalSearchOptions options
) {
    using Cost = typename TspProblem<EdgeCost>::Cost;
    static_assert(is_convertible_v<Cost, double>, "atsp_iterated_local_search: Cost must be convertible to double");
    atsp_detail::check_options(options);
    if (candidates.problem_id != static_cast<const void*>(&problem)) throw invalid_argument("atsp_iterated_local_search: candidates and problem do not match");
    tsp_local_search_detail::SearchTimer timer;
    int n = (int)order.size();
    {
        vector<unsigned char> seen((size_t)problem.node_count(), 0);
        for (int v : order) {
            if (v < 0 || v >= problem.node_count()) throw invalid_argument("atsp_iterated_local_search: order contains an out-of-range node");
            if (seen[v]) throw invalid_argument("atsp_iterated_local_search: order contains a duplicate node");
            seen[v] = 1;
        }
    }
    if (n < 4 || candidates.candidate_count == 0) return {0, 0, 0, timer.elapsed_ms(), TspSearchStopReason::completed};
    bool time_limited = options.time_limit_ms >= 0;
    auto time_over = [&] { return time_limited && timer.elapsed_ms() >= options.time_limit_ms; };
    if (time_over()) return {0, 0, 0, timer.elapsed_ms(), TspSearchStopReason::time_limit};
    Cost initial_cost = atsp_tour_cost(problem, order);
    if (options.max_reverse_length < 0) throw invalid_argument("atsp_iterated_local_search: max_reverse_length must be nonnegative");
    atsp_detail::TourSearch<EdgeCost> search(problem, candidates, order, initial_cost, options.max_reverse_length);
    int64_t iterations = 0, accepted_iterations = 0;
    TspSearchStopReason reason = TspSearchStopReason::completed;
    search.reset_delta();
    search.push_all();
    if (!search.run(time_over)) {
        search.clear_queue();
        reason = TspSearchStopReason::time_limit;
    }
    Cost current_cost = initial_cost + search.delta_sum();
    int max_length = min(options.max_kick_length, (n - 1) / 3);
    // best_order は、現在の巡回路よりコストが小さい巡回路を保存している場合だけ有効
    vector<int> best_order;
    bool has_best = false;
    // 現在の巡回路のコストから、探索中に見つけた最小のコストを引いた値
    Cost excess{};
    if (reason == TspSearchStopReason::completed && n >= 8) {
        tsp_iterated_local_search_detail::Xorshift random(options.seed);
        double base_temperature = options.temperature_ratio * (double)current_cost / n;
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
            // 中断した反復でも、既に最良解を改善していればその巡回路を採用する
            bool accept = finished ? delta <= Cost{} : delta < -excess;
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
                if (finished) ++accepted_iterations;
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
    vector<int> result = search.order();
    rotate(result.begin(), find(result.begin(), result.end(), order[0]), result.end());
    order = move(result);
    return {iterations, accepted_iterations, search.applied_moves(), timer.elapsed_ms(), reason};
}

namespace atsp_detail {
// 始点から最近傍法で巡回順を作る / 候補内に次の訪問先がない場合だけ全点を走査する
template<class EdgeCost>
vector<int> nearest_order(const TspProblem<EdgeCost>& problem, const AtspCandidates& candidates,
                          int start) {
    int node_count = problem.node_count();
    if (start < 0 || start >= node_count) throw invalid_argument("solve_atsp: start is out of range");
    vector<unsigned char> visited(node_count, 0);
    vector<int> order;
    order.reserve(problem.node_count());
    order.push_back(start);
    visited[start] = 1;
    while ((int)order.size() < node_count) {
        int u = order.back(), next = -1;
        size_t offset = (size_t)u * candidates.candidate_count;
        for (int j = 0; j < candidates.candidate_count; ++j) {
            int v = candidates.out[offset + j];
            if (!visited[v]) {
                next = v;
                break;
            }
        }
        if (next < 0) {
            typename TspProblem<EdgeCost>::Cost best{};
            for (int v = 0; v < node_count; ++v) if (!visited[v]) {
                auto cost = problem.edge_cost(u, v);
                if (next < 0 || cost < best) {
                    next = v;
                    best = cost;
                }
            }
        }
        order.push_back(next);
        visited[next] = 1;
    }
    return order;
}

// 最小の閉路を近傍候補または初期巡回路の辺で別の閉路に併合する / O(n k log n)
template<class EdgeCost, class Stop>
vector<int> merge_candidate_cycles(const TspProblem<EdgeCost>& problem, const AtspCandidates& candidates,
                                   const vector<int>& seed, vector<int> next, Stop stop) {
    using Cost = typename TspProblem<EdgeCost>::Cost;
    using Sum = conditional_t<is_integral_v<Cost>, __int128_t, long double>;
    int n = (int)next.size(), k = candidates.candidate_count;
    vector<int> component(n, -1), size(n), prev(n), seed_next(n);
    vector<Cost> arc(n);
    using Entry = pair<int, int>;
    priority_queue<Entry, vector<Entry>, greater<Entry>> queue;
    for (int i = 0; i < n; ++i) {
        prev[next[i]] = i;
        seed_next[seed[i]] = seed[i + 1 == n ? 0 : i + 1];
        arc[i] = problem.edge_cost(i, next[i]);
        if (component[i] >= 0) continue;
        for (int v = i; component[v] < 0; v = next[v]) {
            component[v] = i;
            ++size[i];
        }
        queue.emplace(size[i], i);
    }
    if ((int)queue.size() > n / 8) return {};
    while (!queue.empty()) {
        auto [count, id] = queue.top();
        queue.pop();
        if (size[id] != count) continue;
        if (count == n) break;
        int best_u = -1, best_w = -1;
        Sum best_delta{};
        auto consider = [&](int u, int w) {
            if (component[w] == id) return;
            Sum delta = (Sum)problem.edge_cost(u, next[w]) - arc[u]
                      + problem.edge_cost(w, next[u]) - arc[w];
            if (best_u < 0 || delta < best_delta) {
                best_delta = delta;
                best_u = u;
                best_w = w;
            }
        };
        int u = id;
        do {
            if (stop()) return {};
            // 初期巡回路は必ずこの閉路から出る辺を含む
            consider(u, prev[seed_next[u]]);
            for (int j = 0; j < k; ++j) {
                int v = candidates.out[(size_t)u * k + j];
                consider(u, prev[v]);
                consider(u, candidates.in[(size_t)next[u] * k + j]);
            }
            u = next[u];
        } while (u != id);
        int other = component[best_w];
        do {
            component[u] = other;
            u = next[u];
        } while (u != id);
        size[id] = 0;
        size[other] += count;
        queue.emplace(size[other], other);
        swap(next[best_u], next[best_w]);
        prev[next[best_u]] = best_u;
        prev[next[best_w]] = best_w;
        arc[best_u] = problem.edge_cost(best_u, next[best_u]);
        arc[best_w] = problem.edge_cost(best_w, next[best_w]);
    }
    vector<int> result(n);
    int v = seed.front();
    for (int& u : result) {
        u = v;
        v = next[v];
    }
    return result;
}

// 入辺と出辺の候補に現在の巡回路の辺を加え、オークション法で閉路被覆を求める
// 各点が行き先を 1 つ確保し、競合した場合は元の所有者が別の行き先を選ぶ
template<class EdgeCost, class Stop>
vector<int> candidate_assignment_order(const TspProblem<EdgeCost>& problem, const AtspCandidates& candidates,
                                       const vector<int>& order, Stop stop) {
    using Cost = typename TspProblem<EdgeCost>::Cost;
    struct Edge { int to; Cost cost; };
    int n = (int)order.size(), k = candidates.candidate_count;
    vector<vector<Edge>> edges(n);
    vector<long double> spreads(n);
    for (int i = 0; i < n; ++i) {
        if ((i & 127) == 0 && stop()) return {};
        int u = order[i];
        const int* out = candidates.out.data() + (size_t)u * k;
        edges[u].reserve(2 * k + 1);
        for (int j = 0; j < k; ++j) edges[u].push_back({out[j], problem.edge_cost(u, out[j])});
        spreads[u] = (long double)edges[u].back().cost - edges[u].front().cost;
        int v = order[i + 1 == n ? 0 : i + 1];
        if (find(out, out + k, v) == out + k) edges[u].push_back({v, problem.edge_cost(u, v)});
    }
    for (int i = 0; i < n; ++i) {
        if ((i & 127) == 0 && stop()) return {};
        int v = order[i], previous = order[i == 0 ? n - 1 : i - 1];
        for (int j = 0; j < k; ++j) {
            int u = candidates.in[(size_t)v * k + j];
            const int* out = candidates.out.data() + (size_t)u * k;
            if (u != previous && find(out, out + k, v) == out + k) {
                edges[u].push_back({v, problem.edge_cost(u, v)});
            }
        }
    }
    // 典型的な候補間のコスト差に比例した最小入札幅を使用する
    nth_element(spreads.begin(), spreads.begin() + n / 2, spreads.end());
    long double epsilon = spreads[n / 2] / 128;
    if (!(epsilon > 0) || stop()) return {};
    vector<long double> price(n, 0);
    vector<int> owner(n, -1), successor(n, -1);
    // 全ての点が先に 1 回ずつ入札し、行き先を失った点は次の周で入札する
    vector<int> pending(n), displaced;
    for (int u = 0; u < n; ++u) pending[u] = u;
    for (int64_t bids = 0; !pending.empty() && bids < 32LL * n;) {
        displaced.clear();
        for (int u : pending) {
            if ((bids++ & 127) == 0 && stop()) return {};
            long double best = numeric_limits<long double>::infinity(), second = best;
            int v = -1;
            for (const Edge& edge : edges[u]) {
                long double value = ((long double)edge.cost - edges[u].front().cost) + price[edge.to];
                if (value < best) {
                    second = best;
                    best = value;
                    v = edge.to;
                } else if (value < second) {
                    second = value;
                }
            }
            if (!isfinite(second)) return {};
            price[v] += (second - best) + epsilon;
            if (owner[v] >= 0) displaced.push_back(owner[v]);
            owner[v] = u;
            successor[u] = v;
        }
        pending.swap(displaced);
    }
    // 入札回数の上限に達した場合は、未割当の点と未使用の行き先だけを貪欲に対応させる
    if (!pending.empty()) {
        vector<int> available;
        for (int v = 0; v < n; ++v) if (owner[v] < 0) available.push_back(v);
        for (int u : pending) {
            if (stop()) return {};
            int best = -1;
            Cost best_cost{};
            for (int j = 0; j < (int)available.size(); ++j) {
                if (available[j] == u) continue;
                Cost cost = problem.edge_cost(u, available[j]);
                if (best < 0 || cost < best_cost) {
                    best = j;
                    best_cost = cost;
                }
            }
            if (best < 0) return {};
            successor[u] = available[best];
            available[best] = available.back();
            available.pop_back();
        }
    }
    auto quick = merge_candidate_cycles(problem, candidates, order, successor, stop);
    if (quick.empty()) return {};
    auto merged = merge_cycles(move(successor), order.front(), n / 8,
        [&](int u, int v) { return (long double)problem.edge_cost(u, v); }, stop);
    if (merged.empty()) return quick;
    auto total = [&](const vector<int>& tour) {
        using Sum = conditional_t<is_integral_v<Cost>, __int128_t, long double>;
        Sum sum{};
        for (int i = 0; i < n; ++i) sum += problem.edge_cost(tour[i], tour[i + 1 == n ? 0 : i + 1]);
        return sum;
    };
    return total(merged) <= total(quick) ? move(merged) : move(quick);
}

// 残り時間に応じて全辺または近傍候補から割当を求め、巡回路のコストが下がる場合に order を更新する
template<class EdgeCost>
void improve_initial_order(const TspProblem<EdgeCost>& problem, const AtspCandidates& candidates,
        vector<int>& order, double time_limit_ms, double setup_ms, int free_start) {
    int n = problem.node_count(), c = candidates.candidate_count;
    if (n < 8 || c == 0 || (int)order.size() != n || !(time_limit_ms > 0)) return;
    // 終点自由の経路で始点に戻る辺はコスト 0 に設定するため、相互最近傍の判定から除く
    auto nearest = [&](int u) {
        const int* out = candidates.out.data() + (size_t)u * c;
        if (out[0] != free_start) return out[0];
        return c > 1 ? out[1] : -1;
    };
    int mutual = 0;
    for (int u = 0; u < n; ++u) {
        if (u == free_start) continue;
        int v = nearest(u);
        if (v >= 0 && nearest(v) == u) ++mutual;
    }
    if (mutual > (n - (free_start >= 0)) / 4) return;
    tsp_local_search_detail::SearchTimer timer;
    auto keep_better = [&](vector<int> candidate) {
        if (candidate.empty()) return;
        auto incumbent = atsp_tour_cost(problem, order);
        typename TspProblem<EdgeCost>::Cost total{};
        for (int i = 0; i < n; ++i) {
            auto cost = problem.edge_cost(candidate[i], candidate[i + 1 == n ? 0 : i + 1]);
            if (cost >= incumbent - total) return;
            total += cost;
        }
        if (total < incumbent) order = move(candidate);
    };
    // 候補辺で先に巡回路を作り、全辺の割当を中断しても初期解として使用できるようにする
    auto sparse_stop = [&] { return timer.elapsed_ms() >= 0.4 * time_limit_ms; };
    keep_better(candidate_assignment_order(problem, candidates, order, sparse_stop));
    double spent = timer.elapsed_ms();
    double remaining = time_limit_ms - spent;
    // 初期化に費やした時間の 3 倍以上が残る場合だけ、追加の割当に残り時間の半分までを割く
    if (remaining < 3 * (setup_ms + spent)) return;
    using Weight = AssignmentWeight<EdgeCost>;
    constexpr size_t matrix_budget = 32ULL << 20;
    if ((size_t)n > matrix_budget / sizeof(Weight) / n
        || (size_t)hungarian_detail::padded<Weight>(n) > matrix_budget / sizeof(Weight) / n) return;
    auto dense_stop = [&] { return timer.elapsed_ms() >= spent + remaining / 2; };
    keep_better(assignment_order(problem, order.front(), n / 8, dense_stop, &order));
}

template<class EdgeCost>
vector<int> solve_cycle(int node_count, EdgeCost edge_cost, double time_limit_ms, int start, uint32_t seed, int free_start) {
    tsp_local_search_detail::SearchTimer timer;
    TspProblem<EdgeCost> problem(node_count, move(edge_cost));
    AtspCandidates candidates = make_atsp_candidates(problem, 10);
    vector<int> order = nearest_order(problem, candidates, start);
    double setup_ms = timer.elapsed_ms();
    improve_initial_order(problem, candidates, order, time_limit_ms - setup_ms, setup_ms, free_start);
    AtspIteratedLocalSearchOptions options;
    options.time_limit_ms = max(0.0, time_limit_ms - timer.elapsed_ms());
    options.seed = seed;
    atsp_iterated_local_search(problem, candidates, order, options);
    return order;
}
}

/// @brief 非対称 TSP の巡回順を求めて返す / 辺は返す列の順の向きで、末尾の点から先頭の点への辺を含む / 返す巡回順の先頭は start
/// time_limit_ms は候補表と初期解の作成を含む実行時間の上限 [ms] / 候補表と最近傍法の巡回順は必ず作成する
/// 初期化の実行時間と残り時間に応じて、全辺または近傍候補の割当からも初期解を求める
template<class EdgeCost>
vector<int> solve_atsp(int node_count, EdgeCost edge_cost, double time_limit_ms, int start = 0, uint32_t seed = 23) {
    return atsp_detail::solve_cycle(node_count, move(edge_cost), time_limit_ms, start, seed, -1);
}

/// @brief start から始まり全点を 1 回ずつ通る、戻らない経路の訪問順を求めて返す / 辺のコストは非対称でよい
/// goal が 0 以上なら経路の終点を goal に固定し、-1 なら終点を固定しない / node_count >= 2 で、goal は start と異なること
/// 返す列の先頭は start で、長さは node_count / time_limit_ms は候補表と初期解の作成を含む実行時間の上限 [ms]
/// 終点を固定する場合は始点と終点を 1 点にまとめ、終点が自由な場合は始点に戻る辺のコストを 0 にする
template<class EdgeCost>
vector<int> solve_atsp_path(int node_count, EdgeCost edge_cost, double time_limit_ms, int start, int goal = -1, uint32_t seed = 23) {
    using Cost = remove_cvref_t<invoke_result_t<const EdgeCost&, int, int>>;
    if (node_count < 2) throw invalid_argument("solve_atsp_path: node_count must be at least 2");
    if (start < 0 || start >= node_count) throw invalid_argument("solve_atsp_path: start is out of range");
    if (goal < -1 || goal >= node_count || goal == start) throw invalid_argument("solve_atsp_path: goal must be -1 or a node other than start");
    if (node_count == 2) return {start, 1 - start};
    bool fixed = goal >= 0;
    int anchor = start - (fixed && goal < start);
    auto original = [=](int v) { return v + (fixed && v >= goal); };
    // 固定終点への入辺を anchor の入辺に、始点からの出辺を anchor の出辺にする
    auto path_cost = [&](int u, int v) -> Cost {
        if (v == anchor) return fixed ? edge_cost(original(u), goal) : Cost{};
        return edge_cost(original(u), original(v));
    };
    vector<int> order = atsp_detail::solve_cycle(node_count - fixed, path_cost, time_limit_ms, anchor, seed,
                                                fixed ? -1 : anchor);
    if (fixed) {
        for (int& v : order) v = original(v);
        order.push_back(goal);
    }
    return order;
}
}
