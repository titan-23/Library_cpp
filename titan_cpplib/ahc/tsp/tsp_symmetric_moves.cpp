/// https://github.com/titan-23/Library_cpp/blob/main/titan_cpplib/ahc/tsp/tsp_symmetric_moves.cpp
#pragma once
#include <algorithm>
#include <cstdint>
#include <limits>
#include <optional>
#include <stdexcept>
#include <type_traits>
#include "titan_cpplib/ahc/tsp/tsp.cpp"
using namespace std;
namespace titan23 {
template<class Cost>
template<class EdgeCost>
optional<TspTwoOptMove<Cost>> TspState<Cost>::make_two_opt(const TspProblem<EdgeCost>& problem, int left, int right) const {
    static_assert(is_same_v<Cost, typename TspProblem<EdgeCost>::Cost>);
    if (problem_id_ != static_cast<const void*>(&problem)) throw invalid_argument("TspState::make_two_opt: state and problem do not match");
    int n = size();
    if (n < 4 || left < 1 || left >= n || right < left || right >= n) return nullopt;
    if (left == right || (left == 1 && right == n - 1)) return nullopt;
    int before = order_[left - 1];
    int first = order_[left];
    int last = order_[right];
    int after = order_[(right + 1) % n];
    Cost delta{};
    delta += problem.edge_cost(before, last);
    delta += problem.edge_cost(first, after);
    delta -= problem.edge_cost(before, first);
    delta -= problem.edge_cost(last, after);
    return TspTwoOptMove<Cost>(problem_id_, this, revision_, left, right, move(delta));
}

template<class Cost>
template<class EdgeCost>
optional<TspOrOptMove<Cost>> TspState<Cost>::make_or_opt(const TspProblem<EdgeCost>& problem, int first, int length, int after, bool reversed) const {
    static_assert(is_same_v<Cost, typename TspProblem<EdgeCost>::Cost>);
    if (problem_id_ != static_cast<const void*>(&problem)) throw invalid_argument("TspState::make_or_opt: state and problem do not match");
    int n = size();
    if (n < 4 || first < 1 || first >= n || length <= 0 || length > n - first || after < 0 || after >= n) return nullopt;
    int past_last = first + length;
    if (first <= after && after < past_last) return nullopt;
    if (length == n - 1) return nullopt;
    if (after == first - 1) {
        if (!reversed || length == 1) return nullopt;
        int before = order_[first - 1];
        int block_first = order_[first];
        int block_last = order_[past_last - 1];
        int after_block = order_[past_last % n];
        Cost delta{};
        delta += problem.edge_cost(before, block_last);
        delta += problem.edge_cost(block_first, after_block);
        delta -= problem.edge_cost(before, block_first);
        delta -= problem.edge_cost(block_last, after_block);
        return TspOrOptMove<Cost>(problem_id_, this, revision_, first, length, after, true, move(delta));
    }
    if (reversed && length == n - 2) {
        if ((first == 1 && past_last == n - 1 && after == n - 1) || (first == 2 && past_last == n && after == 0)) return nullopt;
    }
    int before = order_[first - 1];
    int block_first = order_[first];
    int block_last = order_[past_last - 1];
    int after_block = order_[past_last % n];
    int destination = order_[after];
    int after_destination = order_[(after + 1) % n];
    Cost delta{};
    delta += problem.edge_cost(before, after_block);
    if (reversed) {
        delta += problem.edge_cost(destination, block_last);
        delta += problem.edge_cost(block_first, after_destination);
    } else {
        delta += problem.edge_cost(destination, block_first);
        delta += problem.edge_cost(block_last, after_destination);
    }
    delta -= problem.edge_cost(before, block_first);
    delta -= problem.edge_cost(block_last, after_block);
    delta -= problem.edge_cost(destination, after_destination);
    return TspOrOptMove<Cost>(problem_id_, this, revision_, first, length, after, reversed, move(delta));
}

template<class Cost>
template<class EdgeCost>
optional<TspDoubleBridgeMove<Cost>> TspState<Cost>::make_double_bridge(const TspProblem<EdgeCost>& problem, int cut1, int cut2, int cut3, int cut4) const {
    static_assert(is_same_v<Cost, typename TspProblem<EdgeCost>::Cost>);
    if (problem_id_ != static_cast<const void*>(&problem)) throw invalid_argument("TspState::make_double_bridge: state and problem do not match");
    int n = size();
    if (n < 5 || cut1 < 0 || cut1 >= cut2 || cut2 >= cut3 || cut3 >= cut4 || cut4 >= n) return nullopt;
    int end0 = order_[cut1];
    int begin1 = order_[(cut1 + 1) % n];
    int end1 = order_[cut2];
    int begin2 = order_[(cut2 + 1) % n];
    int end2 = order_[cut3];
    int begin3 = order_[(cut3 + 1) % n];
    int end3 = order_[cut4];
    int begin4 = order_[(cut4 + 1) % n];
    Cost delta{};
    delta += problem.edge_cost(end0, begin3);
    delta += problem.edge_cost(end3, begin2);
    delta += problem.edge_cost(end2, begin1);
    delta += problem.edge_cost(end1, begin4);
    delta -= problem.edge_cost(end0, begin1);
    delta -= problem.edge_cost(end1, begin2);
    delta -= problem.edge_cost(end2, begin3);
    delta -= problem.edge_cost(end3, begin4);
    return TspDoubleBridgeMove<Cost>(problem_id_, this, revision_, cut1, cut2, cut3, cut4, move(delta));
}

template<class Cost>
template<class EdgeCost, class Move>
void TspState<Cost>::apply(const TspProblem<EdgeCost>& problem, const Move& move) {
    static_assert(is_same_v<Cost, typename TspProblem<EdgeCost>::Cost>);
    using MoveType = remove_cvref_t<Move>;
    static_assert(is_same_v<MoveType, TspTwoOptMove<Cost>> || is_same_v<MoveType, TspOrOptMove<Cost>> || is_same_v<MoveType, TspDoubleBridgeMove<Cost>>);
    const void* problem_id = static_cast<const void*>(&problem);
    if (problem_id_ != problem_id || move.problem_id_ != problem_id) throw invalid_argument("TspState::apply: state, move, and problem do not match");
    if (move.state_ != this) throw logic_error("TspState::apply: move was created from another state");
    if (move.revision_ != revision_) throw logic_error("TspState::apply: move is stale");
    if (revision_ == numeric_limits<uint64_t>::max()) throw overflow_error("TspState::apply: revision overflow");
    if constexpr (is_same_v<MoveType, TspTwoOptMove<Cost>>) {
        reverse(order_.begin() + move.left_, order_.begin() + move.right_ + 1);
        for (int i = move.left_; i <= move.right_; ++i) position_[order_[i]] = i;
    } else if constexpr (is_same_v<MoveType, TspOrOptMove<Cost>>) {
        int past_last = move.first_ + move.length_;
        if (move.after_ == move.first_ - 1) {
            reverse(order_.begin() + move.first_, order_.begin() + past_last);
        } else {
            if (move.reversed_) reverse(order_.begin() + move.first_, order_.begin() + past_last);
            if (move.after_ < move.first_) {
                rotate(order_.begin() + move.after_ + 1, order_.begin() + move.first_, order_.begin() + past_last);
            } else {
                rotate(order_.begin() + move.first_, order_.begin() + past_last, order_.begin() + move.after_ + 1);
            }
        }
        for (int i = 0; i < size(); ++i) position_[order_[i]] = i;
    } else {
        if (work_buffer_.size() != order_.size()) throw logic_error("TspState::apply: invalid work buffer size");
        int out = 0;
        for (int i = 0; i <= move.cut1_; ++i) work_buffer_[out++] = order_[i];
        for (int i = move.cut3_ + 1; i <= move.cut4_; ++i) work_buffer_[out++] = order_[i];
        for (int i = move.cut2_ + 1; i <= move.cut3_; ++i) work_buffer_[out++] = order_[i];
        for (int i = move.cut1_ + 1; i <= move.cut2_; ++i) work_buffer_[out++] = order_[i];
        for (int i = move.cut4_ + 1; i < size(); ++i) work_buffer_[out++] = order_[i];
        order_.swap(work_buffer_);
        for (int i = 0; i < size(); ++i) position_[order_[i]] = i;
    }
    total_cost_ += move.delta_;
    ++revision_;
}
}
