#include <iostream>
#include <vector>
#include <cassert>
#include <algorithm>

#include <ext/pb_ds/assoc_container.hpp>
#include <ext/pb_ds/hash_policy.hpp>

#include "titan_cpplib/ahc/timer.cpp"
#include "titan_cpplib/ahc/beam_search/action.cpp"
#include "titan_cpplib/ahc/state_pool.cpp"
using namespace std;

// beam_search
namespace titan23 {

namespace beam_search {
  class State;
  struct ScoreState;
  using StatePtr = State*;
  StatePool<State> pool;
  StatePool<ScoreState> score_pool;

  // TODO:
  using ScoreType = double;  // スコアは小さいほどよい
  using HashType = unsigned long long;

  class State {
   public:
    HashType hash;
    Action first_action, last_action;
    ScoreType score;
    vector<Action> history;

   private:

    void _copy(StatePtr &new_state) const {
    }

    // TODO:
    void _apply_op(const Action &action) {
    }

    // TODO:
    ScoreType _eval_score() const {
    }

   public:
    State() {}

    // TODO:
    bool is_done() {
    }

    // TODO:
    ScoreType get_score() const {
    }

    // TODO:
    pair<ScoreType, HashType> try_op(const Action &action) const {
    }

    // TODO:
    void print() const {
    }

    // TODO:
    vector<Action>& get_actions(const int beam_turn, const vector<Action> &history) const {
      if (beam_turn == 0) {
        if (history.empty()) {
        } else {
        }
      }
    }

    // TODO:
    void init() {
      // TODO: field初期設定、スコア初期設定など
    }

    void apply_op(const Action &action) {
      last_action = action;
      _apply_op(action);
    }

    void copy(StatePtr &new_state) const {
      new_state->first_action = first_action;
      new_state->last_action = last_action;
      new_state->hash = hash;
      new_state->score = this->score;
      _copy(new_state);
    }

    bool operator>(const State &right) const { return score > right.score; }
    bool operator<(const State &right) const { return score < right.score; }
  };

  struct Result {
    vector<Action> history;
    int score;
    Result() {}

    void print() {
      for (Action action: history) cout << action;
      cout << endl;
    }
  };

  struct ScoreState {
    ScoreType score;
    int par;
    Action action;
    ScoreState() {}
  };

  class Param {
   private:
    Timer timer;
    double time_limit;
    int beam_depth_base, beam_width_base;
    int max_turn;
    vector<int> pred, pred_acc;
    int done_depth_total;
    bool adjace;

   public:
    Param() {}

    /**
     *
     @brief Construct a new Param object
     *
     * @param time_limit
     * @param max_turn
     * @param beam_depth_base
     * @param beam_width_base
     */
    Param(const double time_limit,
          const int max_turn,
          const int beam_depth_base,
          const int beam_width_base,
          const bool adjace=false) :
        time_limit(time_limit),
        beam_depth_base(beam_depth_base), beam_width_base(beam_width_base),
        max_turn(max_turn),
        pred(max_turn+1, 0), pred_acc(max_turn+2, 0),
        done_depth_total(0),
        adjace(adjace) {
      vector<int> t(max_turn+1);
      for (int turn = 0; turn < max_turn+1; ++turn) {
        pred[turn] = max(1, min(beam_depth_base, max_turn-turn));
        pred_acc[turn+1] = pred_acc[turn] + pred[turn];
      }
    }

    void init() { timer.reset(); }
    int get_max_turn() const { return max_turn; }
    int get_beam_depth() const { return beam_depth_base; }
    int get_beam_width() const { return beam_width_base; }

    int get_beam_depth(const int turn) const {
      return min(beam_depth_base, max_turn-turn);
    }

    void timestamp(const int turn, const int done_depth) {
      done_depth_total += done_depth;
    }

    int get_beam_width(const int turn) const {
      if ((!adjace) || turn == 0) return get_beam_width();
      double now_time = timer.elapsed();
      double rem_time = time_limit - now_time;
      int t = pred_acc.back() - pred_acc[turn];
      double pred_total_cnt = rem_time * (double)done_depth_total / now_time;
      int d = max(1, (int)(pred[turn] * pred_total_cnt / (double)t));
      return d;
    }
  };

  /**
   * @fn
   * @brief ビームサーチを実行する
   * @param (verbose: bool) ログの出力
   * @details コピーが少ない
   */
  static inline Result run(Param &param, const bool verbose=false) {
    Result result;

    const int best_state = pool.gen();
    pool.get(best_state)->init();
    param.init();

    for (int turn = 0; turn < param.get_max_turn(); ++turn) {
      if (verbose) cout << "# turn : " << turn << endl;
      const int beam_depth = param.get_beam_depth(turn);
      const int beam_width = param.get_beam_width(turn);
      int done_depth = 0;
      vector<int> keep;
      int state0 = pool.copy(best_state);
      keep.emplace_back(state0);
      for (int beam_turn = 0; beam_turn < beam_depth; ++beam_turn, ++done_depth) {
        __gnu_pbds::gp_hash_table<HashType, uint8_t> seen;
        vector<int> score_keep;
        score_keep.reserve(keep.size());
        for (const int now_state: keep) {
          const vector<Action> &actions = pool.get(now_state)->get_actions(beam_turn, result.history);
          for (const Action &op : actions) {
            auto [new_score, new_hash] = pool.get(now_state)->try_op(op);
            if (seen.find(new_hash) != seen.end()) continue;
            seen[new_hash] = 0;
            int score_state = score_pool.gen();
            score_pool.get(score_state)->score = new_score;
            score_pool.get(score_state)->par = now_state;
            score_pool.get(score_state)->action = op;
            score_keep.emplace_back(score_state);
          }
        }
        nth_element(score_keep.begin(), score_keep.begin() + min((int)score_keep.size(), param.get_beam_width()), score_keep.end(), [&] (const int &l, const int &r) {
          return score_pool.get(l)->score < score_pool.get(r)->score;
        });
        Action op = score_pool.get(score_keep.front())->action;
        bool is_all_same = true;
        vector<int> new_keep(min(beam_width, (int)score_keep.size()));
        for (int i = 0; i < beam_width && i < score_keep.size(); ++i) {
          int state = pool.copy(score_pool.get(score_keep[i])->par);
          if (beam_turn == 0) {
            pool.get(state)->first_action = score_pool.get(score_keep[i])->action;
          }
          pool.get(state)->apply_op(score_pool.get(score_keep[i])->action);
          if (is_all_same && pool.get(state)->first_action != op) is_all_same = false;
          new_keep[i] = state;
        }
        for (const int state: score_keep) score_pool.del(state);
        for (const int state: keep) pool.del(state);
        swap(keep, new_keep);
        int d_best_state = *min_element(keep.begin(), keep.end(), [&] (const int &l, const int &r) {
          return (*pool.get(l)) < (*pool.get(r));
        });
        if (pool.get(d_best_state)->is_done() || is_all_same) break;
      }
      param.timestamp(turn, done_depth);
      int d_best_state = *min_element(keep.begin(), keep.end(), [&] (const int &l, const int &r) {
        return (*pool.get(l)) < (*pool.get(r));
      });
      Action op = pool.get(d_best_state)->first_action;
      pool.get(best_state)->apply_op(op);
      result.history.push_back(op);
      if (verbose) {
        pool.get(best_state)->print();
        cout << "Score = " << pool.get(best_state)->get_score() << endl << endl;
      }
      for (const int node_id: keep) pool.del(node_id);
      if (pool.get(best_state)->is_done()) break;
    }
    result.score = pool.get(best_state)->get_score();
    pool.del(best_state);
    return result;
  }

  /**
   * @fn
   * @brief ビームサーチを実行する
   * @param (verbose: bool) ログの出力
   * @details コピーをする。try_opの計算量が十分大きいなら、copyする方が速め。
   */
  static inline Result run_copy(Param &param, const bool verbose=false) {
    Result result;

    const int best_state = pool.gen();
    pool.get(best_state)->init();
    param.init();

    for (int turn = 0; turn < param.get_max_turn(); ++turn) {
      if (verbose) cout << "# turn : " << turn << endl;
      const int beam_depth = param.get_beam_depth(turn);
      const int beam_width = param.get_beam_width(turn);
      int done_depth = 0;
      vector<int> keep;
      int state0 = pool.copy(best_state);
      keep.emplace_back(state0);
      for (int beam_turn = 0; beam_turn < beam_depth; ++beam_turn, ++done_depth) {
        __gnu_pbds::gp_hash_table<HashType, uint8_t> seen;
        vector<int> keep_new;
        for (const int now_state: keep) {
          const vector<Action> &actions = pool.get(now_state)->get_actions(beam_turn, result.history);
          for (const Action &op : actions) {
            int new_state = pool.copy(now_state);
            pool.get(new_state)->apply_op(op);
            if (seen.find(pool.get(new_state)->hash) != seen.end()) continue;
            seen[pool.get(new_state)->hash] = 0;
            keep_new.emplace_back(new_state);
          }
          pool.del(now_state);
        }
        keep.clear();
        nth_element(keep_new.begin(), keep_new.begin() + min((int)keep_new.size(), param.get_beam_width()), keep_new.end(), [&] (const int &l, const int &r) {
          return (*pool.get(l)) < (*pool.get(r));
        });
        Action &op = pool.get(keep_new.front())->history[turn];
        bool is_all_same = true;
        for (int i = 0; i < beam_width && i < keep_new.size(); ++i) {
          int state = keep_new[i];
          if (is_all_same && pool.get(state)->history[turn] != op) is_all_same = false;
          keep.emplace_back(state);
        }
        for (int i = beam_width; i < keep_new.size(); ++i) pool.del(keep_new[i]);
        int d_best_state = *min_element(keep.begin(), keep.end(), [&] (const int &l, const int &r) {
          return (*pool.get(l)) < (*pool.get(r));
        });
        if (pool.get(d_best_state)->is_done() || is_all_same) break;
      }
      param.timestamp(turn, done_depth);
      int d_best_state = *min_element(keep.begin(), keep.end(), [&] (const int &l, const int &r) {
        return (*pool.get(l)) < (*pool.get(r));
      });
      Action op = pool.get(d_best_state)->first_action;
      pool.get(best_state)->apply_op(op);
      result.history.push_back(op);
      if (verbose) {
        pool.get(best_state)->print();
        cout << "Score = " << pool.get(best_state)->get_score() << endl << endl;
      }
      for (const int node_id: keep) pool.del(node_id);
      if (pool.get(best_state)->is_done()) break;
    }
    result.score = pool.get(best_state)->get_score();
    pool.del(best_state);
    return result;
  }
}
}
