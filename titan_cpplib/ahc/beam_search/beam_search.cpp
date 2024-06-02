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
  struct SubState;
  using StatePtr = State*;
  StatePool<State> pool;
  StatePool<SubState> sub_pool;

  // TODO:
  using ScoreType = double;  // スコアは小さいほどよい
  using HashType = unsigned long long;

  class State {
   public:
    HashType hash;
    Action first_action, last_action;
    ScoreType score;
    long long substate_id;

   private:
    void inner_copy(StatePtr &new_state) const {
    }

    // TODO:
    void inner_apply_op(const Action &action) {
    }

   public:
    State() {}

    // TODO:
    bool is_done() const {
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
    vector<Action> get_actions(const int turn) const {
      if (turn == 0) {
      } else {
      }
    }

    // TODO:
    void init() {
      // TODO: field初期設定、スコア初期設定など
    }

    void apply_op(const Action &action) {
      last_action = action;
      inner_apply_op(action);
    }

    void copy(StatePtr &new_state) const {
      new_state->first_action = first_action;
      new_state->last_action = last_action;
      new_state->hash = hash;
      new_state->score = score;
      inner_copy(new_state);
    }

    bool operator>(const State &right) const { return score > right.score; }
    bool operator<(const State &right) const { return score < right.score; }
  };

  struct Result {
    vector<Action> history;
    ScoreType score;

    Result() {}

    ScoreType get_score() const {
      return score;
    }

    void print() const {
      for (const Action &action: history) {
        cout << action;
      }
      cout << endl;
    }
  };

  struct SubState {
    ScoreType score;
    long long state, par;
    Action action;
    SubState() {}
  };

  class Param {
   private:
    Timer timer;
    double time_limit;
    int beam_depth_base, beam_width_base;
    int decide_turn, max_turn;
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
          const int decide_turn,
          const int beam_depth_base,
          const int beam_width_base,
          const bool adjace=false) :
        time_limit(time_limit),
        beam_depth_base(beam_depth_base), beam_width_base(beam_width_base),
        decide_turn(decide_turn),
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

    int get_decide_turn() const {
      return decide_turn;
    }

    int get_beam_width(const int turn) {
      if ((!adjace) || turn == 0) return get_beam_width();
      double now_time = timer.elapsed();
      double rem_time = time_limit - now_time;
      int t = pred_acc.back() - pred_acc[turn];
      double pred_total_cnt = rem_time * (double)done_depth_total / now_time;
      int d = max(1, (int)(pred[turn] * pred_total_cnt / (double)t));
      return d;
    }
  };

  class BeamSearch {
   private:
    static inline void calc_next_beam(const vector<long long> &keep,
                                      const int turn,
                                      __gnu_pbds::gp_hash_table<HashType, uint8_t> &seen,
                                      vector<long long> &score_keep) {
      for (const long long now_state: keep) {
        const vector<Action> &actions = pool.get(now_state)->get_actions(turn);
        for (const Action &op : actions) {
          auto [new_score, new_hash] = pool.get(now_state)->try_op(op);
          if (seen.find(new_hash) != seen.end()) continue;
          seen[new_hash] = 0;
          const long long substate = sub_pool.gen();
          sub_pool.get(substate)->score = new_score;
          sub_pool.get(substate)->state = now_state;
          sub_pool.get(substate)->action = op;
          score_keep.emplace_back(substate);
        }
      }
    }

   public:
    static inline Result run_each_turn(Param &param, const bool verbose=false) {
      Result result;

      const long long best_state = pool.gen();
      pool.get(best_state)->init();
      param.init();

      for (int turn = 0; turn < param.get_max_turn(); ++turn) {
        if (verbose) cout << "# turn : " << turn << endl;
        const int beam_depth = param.get_beam_depth(turn);
        const int beam_width = param.get_beam_width(turn);
        int done_depth = 0;
        vector<long long> keep = { pool.copy(best_state) };
        __gnu_pbds::gp_hash_table<HashType, uint8_t> seen;
        for (int beam_turn = 0; beam_turn < beam_depth; ++beam_turn, ++done_depth) {
          vector<long long> score_keep;
          score_keep.reserve(keep.size());
          calc_next_beam(keep, turn, seen, score_keep);
          nth_element(score_keep.begin(), score_keep.begin() + min((long long)score_keep.size(), (long long)param.get_beam_width()), score_keep.end(), [&] (const long long &l, const long long &r) {
            return sub_pool.get(l)->score < sub_pool.get(r)->score;
          });
          const Action &op = sub_pool.get(score_keep.front())->action;
          bool is_all_same = true;
          vector<long long> new_keep(min((long long)beam_width, (long long)score_keep.size()));
          for (int i = 0; i < beam_width && i < score_keep.size(); ++i) {
            const long long state = pool.copy(sub_pool.get(score_keep[i])->state);
            if (beam_turn == 0) {
              pool.get(state)->first_action = sub_pool.get(score_keep[i])->action;
            }
            pool.get(state)->apply_op(sub_pool.get(score_keep[i])->action);
            if (is_all_same && pool.get(state)->first_action != op) is_all_same = false;
            new_keep[i] = state;
          }
          for (const long long state: score_keep) sub_pool.del(state);
          for (const long long state: keep) pool.del(state);
          swap(keep, new_keep);
          const long long d_best_state = *min_element(keep.begin(), keep.end(), [&] (const long long &l, const long long &r) {
            return (*pool.get(l)) < (*pool.get(r));
          });
          if (pool.get(d_best_state)->is_done() || is_all_same) break;
        }
        param.timestamp(turn, done_depth);
        const long long d_best_state = *min_element(keep.begin(), keep.end(), [&] (const long long &l, const long long &r) {
          return (*pool.get(l)) < (*pool.get(r));
        });
        const Action &op = pool.get(d_best_state)->first_action;
        pool.get(best_state)->apply_op(op);
        result.history.push_back(op);
        if (verbose) {
          pool.get(best_state)->print();
          cout << "Score = " << pool.get(best_state)->get_score() << endl << endl;
        }
        for (const long long node_id: keep) pool.del(node_id);
        if (pool.get(best_state)->is_done()) break;
      }
      result.score = pool.get(best_state)->get_score();
      pool.del(best_state);
      return result;
    }

    static inline Result run_normal(Param &param, const bool verbose=false) {
      param.init();
      Result result;

      __gnu_pbds::gp_hash_table<HashType, uint8_t> seen;
      vector<long long> keep;

      { // init state
        const long long state = pool.gen();
        pool.get(state)->init();
        seen[state] = 0;
        keep = { state };
        pool.get(state)->substate_id = -1;
      }

      for (int turn = 0; turn < param.get_max_turn(); ++turn) {
        if (verbose) cout << "# turn : " << turn << endl;
        const int beam_width = param.get_beam_width(turn);
        vector<long long> score_keep;
        calc_next_beam(keep, turn, seen, score_keep);
        nth_element(score_keep.begin(), score_keep.begin() + min((long long)score_keep.size(), (long long)param.get_beam_width()), score_keep.end(), [&] (const long long &l, const long long &r) {
          return sub_pool.get(l)->score < sub_pool.get(r)->score;
        });
        vector<long long> new_keep(min((long long)beam_width, (long long)score_keep.size()));
        for (int i = 0; i < beam_width && i < score_keep.size(); ++i) {
          long long substate = score_keep[i];
          long long new_state = pool.copy(sub_pool.get(substate)->state);
          pool.get(new_state)->apply_op(sub_pool.get(substate)->action);
          pool.get(new_state)->substate_id = substate;
          sub_pool.get(substate)->par = pool.get(sub_pool.get(substate)->state)->substate_id;
          new_keep[i] = new_state;
        }

        for (const long long state: keep) pool.del(state);
        for (int i = new_keep.size(); i < score_keep.size(); ++i) {
          sub_pool.del(score_keep[i]);
        }

        swap(keep, new_keep);
        const long long best_state = *min_element(keep.begin(), keep.end(), [&] (const long long &l, const long long &r) {
          return (*pool.get(l)) < (*pool.get(r));
        });
        if (verbose) {
          // pool.get(best_state)->print();
          cout << "Score = " << pool.get(best_state)->get_score() << endl << endl;
        }
        if (pool.get(best_state)->is_done()) break;
      }

      const long long best_state = *min_element(keep.begin(), keep.end(), [&] (const long long &l, const long long &r) {
        return (*pool.get(l)) < (*pool.get(r));
      });
      result.score = pool.get(best_state)->get_score();
      long long substate = pool.get(best_state)->substate_id;
      while (substate != -1) {
        result.history.emplace_back(sub_pool.get(substate)->action);
        substate = sub_pool.get(substate)->par;
      }
      reverse(result.history.begin(), result.history.end());
      return result;
    }

    static inline Result run_complex(Param &param, const bool verbose=false) {
      /*
      - 一度のビームサーチで、何ターン先を決めるか
        - 幅、深さ
        - 何ターン先まで読むか
      */

      Result result;
      param.init();

      const long long best_state = pool.gen();
      pool.get(best_state)->init();
      pool.get(best_state)->substate_id = -1;

      for (int turn = 0; turn < param.get_max_turn(); turn += param.get_decide_turn()) {
        if (verbose) cout << "# turn : " << turn << endl;

        vector<long long> keep;
        const int beam_depth = param.get_beam_depth(turn);
        const int beam_width = param.get_beam_width(turn);
        __gnu_pbds::gp_hash_table<HashType, uint8_t> seen;

        { // init
          const long long init_state = pool.copy(best_state);
          seen[init_state] = 0;
          pool.get(init_state)->substate_id = -1;
          keep = {init_state};
        }

        for (int beam_turn = 0; beam_turn < beam_depth; ++beam_turn) {
          vector<long long> score_keep;
          calc_next_beam(keep, turn, seen, score_keep);
          const int w = min((int)score_keep.size(), beam_width);
          nth_element(score_keep.begin(), score_keep.begin() + w, score_keep.end(), [&] (const long long &l, const long long &r) {
            return sub_pool.get(l)->score < sub_pool.get(r)->score;
          });
          vector<long long> new_keep;

          for (int i = 0; i < w; ++i) {
            long long substate = score_keep[i];
            long long nowstate = sub_pool.get(substate)->state;
            long long new_state = pool.copy(nowstate);
            pool.get(new_state)->apply_op(sub_pool.get(substate)->action);
            pool.get(new_state)->substate_id = substate;
            sub_pool.get(substate)->par = pool.get(nowstate)->substate_id;
            new_keep.emplace_back(new_state);
          }

          for (const long long state: keep) pool.del(state);
          for (int i = w; i < score_keep.size(); ++i) {
            sub_pool.del(score_keep[i]);
          }

          swap(keep, new_keep);
          const long long this_best_state = *min_element(keep.begin(), keep.end(), [&] (const long long &l, const long long &r) {
            return (*pool.get(l)) < (*pool.get(r));
          });
          if (pool.get(this_best_state)->is_done()) break;
        }

        const long long this_best_state = *min_element(keep.begin(), keep.end(), [&] (const long long &l, const long long &r) {
          return (*pool.get(l)) < (*pool.get(r));
        });
        long long substate = pool.get(this_best_state)->substate_id;
        vector<Action> history;
        while (substate != -1) {
          history.emplace_back(sub_pool.get(substate)->action);
          substate = sub_pool.get(substate)->par;
        }
        sub_pool.clear();
        reverse(history.begin(), history.end());
        for (int i = 0; i < min(param.get_decide_turn(), (int)(history.size())); ++i) {
          result.history.emplace_back(history[i]);
          pool.get(best_state)->apply_op(history[i]);
          if (verbose) {
            pool.get(best_state)->print();
            cout << "Score = " << pool.get(best_state)->get_score() << endl << endl;
          }
        }
        if (pool.get(best_state)->is_done()) break;
      }
      result.score = pool.get(best_state)->get_score();
      return result;
    }
  };
}
}  // namespace titan23
