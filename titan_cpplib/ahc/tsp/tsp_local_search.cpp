/// https://github.com/titan-23/Library_cpp/blob/main/titan_cpplib/ahc/tsp/tsp_local_search.cpp
#pragma once
#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <type_traits>
#include <utility>
#include <vector>
#include "titan_cpplib/ahc/tsp/tsp_symmetric_moves.cpp"
using namespace std;
namespace titan23 {
/// @brief 探索を停止した理由
enum class TspSearchStopReason {
    completed,
    time_limit,
    evaluated_move_limit,
    penalty_round_limit,
    iteration_limit,
};

/// @brief 2-opt 局所探索の設定
struct TspLocalSearchOptions {
    /// 評価する操作の数の上限 / -1 なら上限なし
    int64_t max_evaluated_moves = -1;
    /// true ならコストが下がる操作を見つけた時点で適用し、false なら一巡のうちコストの差分が最小の操作を適用する
    bool first_improvement = true;
};

/// @brief 2-opt 局所探索の結果
struct TspLocalSearchResult {
    /// 評価した操作の数
    int64_t evaluated_moves;
    /// 適用した操作の数
    int64_t applied_moves;
    /// 局所最適解に達したか / 評価回数の上限で停止した場合は false
    bool locally_optimal;
};

namespace tsp_local_search_detail {
inline void check_options(const TspLocalSearchOptions& options) {
    if (options.max_evaluated_moves < -1) throw invalid_argument("tsp_two_opt_local_search: max_evaluated_moves must be -1 or nonnegative");
}

inline int previous_index(int index, int n) {
    return index == 0 ? n - 1 : index - 1;
}

// 位置 edge1, edge2 から始まる辺を削除する 2-opt について、反転する区間 [left, right] を求める / 隣接する辺なら false
inline bool make_two_opt_indices(int edge1, int edge2, int n, int& left, int& right) {
    if (edge1 == edge2 || (edge1 + 1) % n == edge2 || (edge2 + 1) % n == edge1) return false;
    if (edge1 > edge2) swap(edge1, edge2);
    left = edge1 + 1;
    right = edge2;
    return true;
}

class SearchTimer {
    chrono::steady_clock::time_point start_;

public:
    SearchTimer() : start_(chrono::steady_clock::now()) {}

    double elapsed_ms() const {
        return chrono::duration<double, milli>(chrono::steady_clock::now() - start_).count();
    }
};

/// @brief 辺のペナルティを使用しないことを示す TourSearch のテンプレート引数
struct NoEdgePenalty {
    static constexpr bool enabled = false;

    int64_t get(int, int) const { return 0; }
};

/// @brief 巡回路を配列で保持し、候補表とキューを使用して 2-opt、Or-opt、3-opt を適用する探索用の内部クラス
/// 巡回路は向きと開始位置を区別しない / 区間の反転では、反転する区間とその補集合のうち短い方を反転する
/// Penalty::enabled が true の場合は、set_penalty で指定した辺のペナルティを 2-opt の評価に加えることができる
template<class EdgeCost, class Penalty = NoEdgePenalty>
class TourSearch {
public:
    using Cost = typename TspProblem<EdgeCost>::Cost;

    /// @brief 巡回順 order と候補表 candidates から構築する / O(node_count + n c^2)
    /// 点 u の近傍は、u の候補点のうち巡回路に含まれる点とする / symmetric が true なら、u を候補に含む点も u の近傍に加える
    TourSearch(const TspProblem<EdgeCost>& problem, const TspCandidates& candidates, const vector<int>& order, Cost total_cost, bool symmetric)
        : problem_(problem), n_((int)order.size()), order_(order), position_((size_t)problem.node_count(), -1),
          candidate_begin_((size_t)problem.node_count() + 1, 0), queue_((size_t)order.size()), in_queue_((size_t)problem.node_count(), 0) {
        for (int i = 0; i < n_; ++i) position_[order_[i]] = i;
        // v の近傍に u を加える必要があるか / v が u の候補で、u が v の候補でない場合
        auto needs_reverse = [&](int u, int v) {
            if (!symmetric) return false;
            for (int w : candidates[v]) if (w == u) return false;
            return true;
        };
        for (int u : order_) {
            for (int v : candidates[u]) if (position_[v] >= 0) {
                ++candidate_begin_[u + 1];
                if (needs_reverse(u, v)) ++candidate_begin_[v + 1];
            }
        }
        for (size_t u = 0; u + 1 < candidate_begin_.size(); ++u) candidate_begin_[u + 1] += candidate_begin_[u];
        candidate_.resize((size_t)candidate_begin_.back());
        candidate_cost_.resize((size_t)candidate_begin_.back());
        vector<int> fill_position(candidate_begin_.begin(), candidate_begin_.end() - 1);
        auto add = [&](int u, int v, const Cost& edge_cost) {
            candidate_[fill_position[u]] = v;
            candidate_cost_[fill_position[u]] = edge_cost;
            ++fill_position[u];
        };
        for (int u : order_) {
            for (int v : candidates[u]) if (position_[v] >= 0) {
                Cost edge_cost = problem.edge_cost(u, v);
                add(u, v, edge_cost);
                if (needs_reverse(u, v)) add(v, u, edge_cost);
            }
        }
        // 各点の近傍をコストの昇順に並べる / 挿入ソート
        for (int u : order_) {
            int begin = candidate_begin_[u], end = candidate_begin_[u + 1];
            for (int i = begin + 1; i < end; ++i) {
                int v = candidate_[i];
                Cost edge_cost = candidate_cost_[i];
                int j = i;
                while (j > begin && edge_cost < candidate_cost_[j - 1]) {
                    candidate_[j] = candidate_[j - 1];
                    candidate_cost_[j] = candidate_cost_[j - 1];
                    --j;
                }
                candidate_[j] = v;
                candidate_cost_[j] = edge_cost;
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
    }
    /// @brief Or-opt を使用するかを設定する
    void set_use_or_opt(bool use_or_opt) { use_or_opt_ = use_or_opt; }
    /// @brief 3 辺を交換する 3-opt を使用するかを設定する
    void set_use_three_opt(bool use_three_opt) { use_three_opt_ = use_three_opt; }
    /// @brief true にすると、候補点への辺が削除する辺より長い 2-opt も評価する
    void set_exhaustive(bool exhaustive) { exhaustive_ = exhaustive; }
    /// @brief 評価する操作の数の上限を設定する
    void set_evaluation_limit(int64_t limit) { evaluation_limit_ = limit; }
    /// @brief 評価回数の上限に達して探索を中断したかを返す
    bool evaluation_limit_reached() const { return limit_reached_; }
    /// @brief 2-opt の評価に、辺 {u, v} ごとに lambda * penalty->get(u, v) を加える / penalty が nullptr なら加えない
    /// ペナルティを加える間は、候補点への辺の長さによる打ち切りと Or-opt を行わない
    void set_penalty(const Penalty* penalty, double lambda) {
        penalty_ = penalty;
        lambda_ = lambda;
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
    /// 128 点を処理するたびに stop() を呼び出し、true が返された場合は中断する
    /// キューが空になった場合は true、中断した場合と評価回数の上限に達した場合は false を返す
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
            if (limit_reached_) return false;
        }
        return true;
    }

    /// @brief 反転の記録を開始する / それまでの記録は破棄する
    void start_log() {
        logging_ = true;
        log_.clear();
    }
    /// @brief 記録した反転を逆順に適用して、記録の開始時の巡回順に戻す / 記録は保持する
    void undo_log() {
        for (size_t i = log_.size(); i-- > 0;) reverse_range(log_[i].first, log_[i].second);
    }
    /// @brief undo_log で戻した反転を再び適用する
    void redo_log() {
        for (size_t i = 0; i < log_.size(); ++i) reverse_range(log_[i].first, log_[i].second);
    }

    /// @brief 位置 start の直後に続く長さ length1, length2, length3 の区間 B, C, D を D, C, B の順に並べ替える Double Bridge を適用する
    /// length1 + length2 + length3 < n とする / evaluate が true ならコストの差分を delta_sum に加え、端点をキューに追加する
    /// 適用後に同じ start と長さ length3, length2, length1 で呼び出すと元の巡回順に戻る / O(length1 + length2 + length3)
    void double_bridge(int start, int length1, int length2, int length3, bool evaluate) {
        int total = length1 + length2 + length3;
        work_.resize((size_t)total);
        int first_index = start + 1;
        if (first_index == n_) first_index = 0;
        int index = first_index;
        for (int t = 0; t < total; ++t) {
            work_[t] = order_[index];
            if (++index == n_) index = 0;
        }
        if (evaluate) {
            int before = order_[start];
            int after = order_[index];
            int b_first = work_[0], b_last = work_[length1 - 1];
            int c_first = work_[length1], c_last = work_[length1 + length2 - 1];
            int d_first = work_[length1 + length2], d_last = work_[total - 1];
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
        index = first_index;
        auto write = [&](int from, int length) {
            for (int t = 0; t < length; ++t) {
                int v = work_[from + t];
                order_[index] = v;
                position_[v] = index;
                if (++index == n_) index = 0;
            }
        };
        write(length1 + length2, length3);
        write(length1, length2);
        write(0, length1);
    }

private:
    const TspProblem<EdgeCost>& problem_;
    int n_;
    vector<int> order_;
    vector<int> position_;
    // 点 u の近傍は candidate_ の [candidate_begin_[u], candidate_begin_[u + 1]) で、コストの昇順に並ぶ
    vector<int> candidate_begin_;
    vector<int> candidate_;
    vector<Cost> candidate_cost_;
    vector<int> queue_;
    vector<unsigned char> in_queue_;
    int queue_head_ = 0;
    int queue_size_ = 0;
    // 適用した反転の開始位置と長さ
    vector<pair<int, int>> log_;
    bool logging_ = false;
    bool use_or_opt_ = true;
    bool use_three_opt_ = false;
    bool exhaustive_ = false;
    Cost epsilon_{};
    Cost delta_sum_{};
    int64_t evaluated_moves_ = 0;
    int64_t applied_moves_ = 0;
    int64_t evaluation_limit_ = numeric_limits<int64_t>::max();
    bool limit_reached_ = false;
    const Penalty* penalty_ = nullptr;
    double lambda_ = 0;
    vector<int> work_;

    Cost cost(int u, int v) const { return problem_.edge_cost(u, v); }
    bool improves(const Cost& delta) const { return delta < -epsilon_; }
    bool penalized() const {
        if constexpr (Penalty::enabled) return penalty_ != nullptr;
        else return false;
    }
    int next(int v) const {
        int i = position_[v] + 1;
        return order_[i == n_ ? 0 : i];
    }
    int prev(int v) const {
        int i = position_[v];
        return order_[i == 0 ? n_ - 1 : i - 1];
    }

    // 位置 i から始まる長さ length の区間を反転する / 区間は配列の末尾から先頭へ続いてもよい
    void reverse_range(int i, int length) {
        int j = i + length - 1;
        if (j >= n_) j -= n_;
        for (int count = length >> 1; count > 0; --count) {
            int a = order_[i], b = order_[j];
            order_[i] = b;
            position_[b] = i;
            order_[j] = a;
            position_[a] = j;
            if (++i == n_) i = 0;
            if (--j < 0) j = n_ - 1;
        }
    }

    // 配列の順方向に x から y へ進む経路を反転する
    // 補集合の方が短い場合は補集合を反転する / 巡回路としては同じ結果になる
    void reverse_path(int x, int y) {
        int i = position_[x], j = position_[y];
        int length = j - i;
        if (length < 0) length += n_;
        ++length;
        if (length * 2 > n_) {
            i = j + 1;
            if (i == n_) i = 0;
            length = n_ - length;
        }
        if (length < 2) return;
        reverse_range(i, length);
        if (logging_) log_.emplace_back(i, length);
    }

    // 辺 {u1, u2} と {v1, v2} を削除し、辺 {u1, v1} と {u2, v2} を追加する
    // 巡回路上で u1 から u2 への向きと v1 から v2 への向きが同じであること / 2 辺が点を共有する場合は何もしない
    void exchange(int u1, int u2, int v1, int v2) {
        if (u1 == v2 || u2 == v1) return;
        if (next(u1) == u2) reverse_path(u2, v1);
        else reverse_path(u1, v2);
    }

    // 辺 {a, b}, {c, d}, {e, f} を削除し、辺 {a, c}, {d, e}, {f, b} を追加する 3-opt を探し、最初に見つけた改善操作を適用する
    // b は a から向き direction に 1 つ進んだ点、c は a の近傍、gain1 は cost(a, b) - cost(a, c) で正の値
    // d は c の隣の点、e は d の近傍で、gain1 + cost(c, d) - cost(d, e) が正のものだけを評価する
    bool improve_three_opt(int a, int b, int c, Cost gain1, int direction) {
        auto forward = [&](int v) { return direction ? next(v) : prev(v); };
        auto backward = [&](int v) { return direction ? prev(v) : next(v); };
        // b から a と反対の向きに進んで v に達するまでの辺の数
        auto offset = [&](int v) {
            int result = direction ? position_[v] - position_[b] : position_[b] - position_[v];
            return result < 0 ? result + n_ : result;
        };
        for (int type = 0; type < 2; ++type) {
            // type 0: d は c の先の点 / 辺 {a, c}, {b, d} を追加する 2-opt の後に、辺 {b, d} と {e, f} を交換する 2-opt を続ける操作
            // type 1: d は c の手前の点 / b から d までの区間を e と f の間へ移動する操作
            int d = type == 0 ? forward(c) : backward(c);
            if (d == a) continue;
            Cost gain1_removed = gain1 + cost(c, d);
            int segment_last = offset(type == 0 ? c : d);
            const int* candidate = candidate_.data() + candidate_begin_[d];
            const Cost* candidate_cost = candidate_cost_.data() + candidate_begin_[d];
            int candidate_count = candidate_begin_[d + 1] - candidate_begin_[d];
            for (int i = 0; i < candidate_count; ++i) {
                int e = candidate[i];
                Cost gain2 = gain1_removed - candidate_cost[i];
                if (!(Cost{} < gain2)) break;
                if (e == b || e == c) continue;
                bool inside = offset(e) <= segment_last;
                if (type == 0) {
                    // 最初の 2-opt で b から c までの区間が反転するため、区間の中では f の向きが逆になる
                    int f = inside ? forward(e) : backward(e);
                    if (f == d) continue;
                    if (evaluated_moves_ >= evaluation_limit_) {
                        limit_reached_ = true;
                        return false;
                    }
                    ++evaluated_moves_;
                    Cost delta = cost(f, b) - cost(e, f) - gain2;
                    if (!improves(delta)) continue;
                    exchange(a, b, c, d);
                    exchange(d, b, e, f);
                    delta_sum_ += delta;
                    ++applied_moves_;
                    push(a);
                    push(b);
                    push(c);
                    push(d);
                    push(e);
                    push(f);
                    return true;
                }
                if (inside) continue;
                for (int side = 0; side < 2; ++side) {
                    // side 0: f は e の先の点 / 区間を反転して e と f の間へ挿入する
                    // side 1: f は e の手前の点 / 区間の向きを維持して f と e の間へ挿入する
                    if (side == 0 && e == a) continue;
                    int f = side == 0 ? forward(e) : backward(e);
                    if (evaluated_moves_ >= evaluation_limit_) {
                        limit_reached_ = true;
                        return false;
                    }
                    ++evaluated_moves_;
                    Cost delta = cost(f, b) - cost(e, f) - gain2;
                    if (!improves(delta)) continue;
                    if (side == 0) {
                        exchange(a, b, e, f);
                        exchange(a, e, c, d);
                    } else {
                        exchange(a, b, d, c);
                        exchange(a, d, f, e);
                        exchange(a, f, c, b);
                    }
                    delta_sum_ += delta;
                    ++applied_moves_;
                    push(a);
                    push(b);
                    push(c);
                    push(d);
                    push(e);
                    push(f);
                    return true;
                }
            }
        }
        return false;
    }

    // t1 を端点とする改善操作を探し、最初に見つけた操作を適用する / 適用した場合は true
    bool improve_node(int t1) {
        const int* candidate = candidate_.data() + candidate_begin_[t1];
        const Cost* candidate_cost = candidate_cost_.data() + candidate_begin_[t1];
        int candidate_count = candidate_begin_[t1 + 1] - candidate_begin_[t1];
        for (int direction = 0; direction < 2; ++direction) {
            // 2-opt: 辺 {t1, t2} と {t3, t4} を削除し、辺 {t1, t3} と {t2, t4} を追加する
            int t2 = direction ? next(t1) : prev(t1);
            Cost d12 = cost(t1, t2);
            for (int i = 0; i < candidate_count; ++i) {
                int t3 = candidate[i];
                Cost d13 = candidate_cost[i];
                if (!exhaustive_ && !penalized() && !(d13 < d12)) break;
                if (evaluated_moves_ >= evaluation_limit_) {
                    limit_reached_ = true;
                    return false;
                }
                ++evaluated_moves_;
                int t4 = direction ? next(t3) : prev(t3);
                Cost delta = d13 + cost(t2, t4) - d12 - cost(t3, t4);
                bool improved;
                if (penalized()) {
                    int64_t penalty_delta = penalty_->get(t1, t3) + penalty_->get(t2, t4) - penalty_->get(t1, t2) - penalty_->get(t3, t4);
                    improved = (double)delta + lambda_ * (double)penalty_delta < -(double)epsilon_;
                } else {
                    improved = improves(delta);
                }
                if (improved) {
                    exchange(t1, t2, t3, t4);
                    delta_sum_ += delta;
                    ++applied_moves_;
                    push(t1);
                    push(t2);
                    push(t3);
                    push(t4);
                    return true;
                }
                if (use_three_opt_ && !penalized() && d13 < d12 && improve_three_opt(t1, t2, t3, d12 - d13, direction)) return true;
            }
            if (!use_or_opt_ || penalized()) continue;
            // Or-opt: t1 から t2 と反対の向きに続く長さ 1 から 3 の区間 S を、候補点 c とその隣の点 e の間へ移動する
            // 辺 {p, s[0]}, {last, q}, {c, e} を削除し、辺 {p, q}, {c, s[0]}, {last, e} を追加する
            int p = t2;
            int s[3];
            s[0] = t1;
            for (int length = 1; length <= 3; ++length) {
                if (length > 1) s[length - 1] = direction ? prev(s[length - 2]) : next(s[length - 2]);
                int last = s[length - 1];
                int q = direction ? prev(last) : next(last);
                if (last == p || q == p) break;
                Cost remove_gain = d12 + cost(last, q) - cost(p, q);
                if (!(Cost{} < remove_gain)) continue;
                for (int i = 0; i < candidate_count; ++i) {
                    int c = candidate[i];
                    Cost dc = candidate_cost[i];
                    if (!(dc < remove_gain)) break;
                    if ((length >= 2 && c == s[1]) || (length == 3 && c == s[2])) continue;
                    for (int side = 0; side < 2; ++side) {
                        int e = side ? next(c) : prev(c);
                        if (e == s[0] || (length >= 2 && e == s[1]) || (length == 3 && e == s[2])) continue;
                        if (evaluated_moves_ >= evaluation_limit_) {
                            limit_reached_ = true;
                            return false;
                        }
                        ++evaluated_moves_;
                        Cost delta = dc + cost(last, e) - cost(c, e) - remove_gain;
                        if (!improves(delta)) continue;
                        // s[0] から last へ進む向きに c から 1 つ進んだ点が e かどうか
                        bool same_direction = (side == 1) == (direction == 0);
                        if (same_direction) {
                            // p S q ... c e の並びを p q ... c S e にする / S の向きは維持する
                            exchange(p, t1, last, q);
                            exchange(p, last, c, e);
                            exchange(p, c, q, t1);
                        } else {
                            // p S q ... e c の並びを p q ... e S' c にする / S' は S を反転した区間
                            exchange(p, t1, e, c);
                            exchange(p, e, q, last);
                        }
                        delta_sum_ += delta;
                        ++applied_moves_;
                        push(p);
                        push(q);
                        push(t1);
                        push(last);
                        push(c);
                        push(e);
                        return true;
                    }
                }
            }
        }
        return false;
    }
};

// state の巡回順を、先頭の点を維持したまま order の巡回路に置き換える
template<class EdgeCost>
void write_back(const TspProblem<EdgeCost>& problem, TspState<typename TspProblem<EdgeCost>::Cost>& state, vector<int> order) {
    int first = state.order()[0];
    rotate(order.begin(), find(order.begin(), order.end(), first), order.end());
    state.reset(problem, move(order));
}

// enumerate が列挙する 2-opt のうちコストの差分が最小の操作を適用することを、改善する操作がなくなるか評価回数の上限に達するまで繰り返す
template<class EdgeCost, class Enumerate>
TspLocalSearchResult run_best_improvement(
    const TspProblem<EdgeCost>& problem,
    TspState<typename TspProblem<EdgeCost>::Cost>& state,
    const TspLocalSearchOptions& options,
    Enumerate&& enumerate
) {
    using Cost = typename TspProblem<EdgeCost>::Cost;
    int64_t evaluated_moves = 0;
    int64_t applied_moves = 0;
    while (true) {
        bool stopped_by_limit = false;
        TspTwoOptMove<Cost> best_move;
        enumerate([&](int left, int right) {
            if (options.max_evaluated_moves >= 0 && evaluated_moves >= options.max_evaluated_moves) {
                stopped_by_limit = true;
                return false;
            }
            auto candidate_move = state.make_two_opt(problem, left, right);
            if (!candidate_move.valid()) return true;
            ++evaluated_moves;
            if (!(candidate_move.delta() < Cost{})) return true;
            if (!best_move.valid() || candidate_move.delta() < best_move.delta()) best_move = candidate_move;
            return true;
        });
        if (stopped_by_limit) return {evaluated_moves, applied_moves, false};
        if (!best_move.valid()) return {evaluated_moves, applied_moves, true};
        state.apply(problem, best_move);
        ++applied_moves;
    }
}
}

/// @brief 全ての 2-opt を評価する局所探索で state を改善する / 一巡の評価は O(n^2)
template<class EdgeCost>
TspLocalSearchResult tsp_two_opt_local_search(
    const TspProblem<EdgeCost>& problem,
    TspState<typename TspProblem<EdgeCost>::Cost>& state,
    TspLocalSearchOptions options = {}
) {
    using Cost = typename TspProblem<EdgeCost>::Cost;
    tsp_local_search_detail::check_options(options);
    if (!state.belongs_to(problem)) throw invalid_argument("tsp_two_opt_local_search: state and problem do not match");
    int n = state.size();
    if (n < 4) return {0, 0, true};
    if (!options.first_improvement) {
        auto enumerate = [&](auto&& visit) {
            for (int left = 1; left < n; ++left) {
                for (int right = left + 1; right < n; ++right) {
                    if (!visit(left, right)) return;
                }
            }
        };
        return tsp_local_search_detail::run_best_improvement(problem, state, options, enumerate);
    }
    // 改善する操作を適用した後も続きの組を評価し、改善する操作のない一巡で終了する
    int64_t evaluated_moves = 0;
    int64_t applied_moves = 0;
    while (true) {
        bool improved = false;
        for (int left = 1; left < n; ++left) {
            for (int right = left + 1; right < n; ++right) {
                if (left == 1 && right == n - 1) continue;
                if (options.max_evaluated_moves >= 0 && evaluated_moves >= options.max_evaluated_moves) return {evaluated_moves, applied_moves, false};
                auto candidate_move = state.make_two_opt(problem, left, right);
                ++evaluated_moves;
                if (!(candidate_move.delta() < Cost{})) continue;
                state.apply(problem, candidate_move);
                ++applied_moves;
                improved = true;
            }
        }
        if (!improved) return {evaluated_moves, applied_moves, true};
    }
}

/// @brief 候補表 candidates の点と候補点を隣接させる 2-opt だけを評価する局所探索で state を改善する
/// 巡回路の向きは変化する場合がある / order()[0] は変化しない
template<class EdgeCost>
TspLocalSearchResult tsp_two_opt_local_search(
    const TspProblem<EdgeCost>& problem,
    const TspCandidates& candidates,
    TspState<typename TspProblem<EdgeCost>::Cost>& state,
    TspLocalSearchOptions options = {}
) {
    tsp_local_search_detail::check_options(options);
    if (!candidates.belongs_to(problem)) throw invalid_argument("tsp_two_opt_local_search: candidates and problem do not match");
    if (!state.belongs_to(problem)) throw invalid_argument("tsp_two_opt_local_search: state and problem do not match");
    int n = state.size();
    if (n < 4 || candidates.candidate_count() == 0) return {0, 0, true};
    if (!options.first_improvement) {
        auto enumerate = [&](auto&& visit) {
            const auto& order = state.order();
            for (int index = 0; index < n; ++index) {
                int node = order[index];
                for (int candidate : candidates[node]) {
                    int other = state.position(candidate);
                    if (other < 0 || other == index) continue;
                    int edge_pairs[2][2] = {
                        {index, other},
                        {tsp_local_search_detail::previous_index(index, n), tsp_local_search_detail::previous_index(other, n)},
                    };
                    for (const auto& edge_pair : edge_pairs) {
                        int left, right;
                        if (!tsp_local_search_detail::make_two_opt_indices(edge_pair[0], edge_pair[1], n, left, right)) continue;
                        if (!visit(left, right)) return;
                    }
                }
            }
        };
        return tsp_local_search_detail::run_best_improvement(problem, state, options, enumerate);
    }
    tsp_local_search_detail::TourSearch<EdgeCost> search(problem, candidates, state.order(), state.total_cost(), false);
    search.set_use_or_opt(false);
    if (options.max_evaluated_moves >= 0) search.set_evaluation_limit(options.max_evaluated_moves);
    auto never_stop = [] { return false; };
    // まず、候補点への辺が削除する辺より短い操作だけを評価する
    // その後、全ての候補を評価する一巡を、操作を 1 つも適用しなくなるまで繰り返す
    search.push_all();
    bool finished = search.run(never_stop);
    search.set_exhaustive(true);
    while (finished) {
        int64_t applied_before = search.applied_moves();
        search.push_all();
        finished = search.run(never_stop);
        if (finished && search.applied_moves() == applied_before) break;
    }
    if (search.applied_moves() > 0) tsp_local_search_detail::write_back(problem, state, search.order());
    return {search.evaluated_moves(), search.applied_moves(), finished};
}
}
