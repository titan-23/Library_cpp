#include <iostream>
#include <vector>
#include <stack>
#include <cassert>
#include <algorithm>
#include "titan_cpplib/ahc/beam_search/action.cpp"
#include "titan_cpplib/ahc/state_pool.cpp"
using namespace std;

// beam_search
namespace titan23 {

namespace beam_search {
  const int MAX_TURN;

  struct State;
  using StatePtr = State*;

  using ScoreType = float;

  struct ScoreState {
    ScoreType score;
    int par;
    Action action;
    ScoreState() {}
  };

  class State {
   public:
    ScoreType score;  // スコアは小さいほどよい
    vector<Action> history;

   private:
    // TODO: 
  
    // TODO:
    void _copy(StatePtr &new_state) const {
    }

    // TODO:
    void _apply_op(const Action &action) {
    }

    // TODO:
    ScoreType _eval_score() const {
    }

    void eval_score() {
      this->score = _eval_score();
    }

   public:
    State() {}

    // TODO:
    ScoreType try_op(const Action &action) const {
    }

    // TODO:
    long long get_score_true() {
    }

    // TODO:
    void print() const {
    }

    // TODO:
    vector<Action> get_actions() {
    }

    // TODO:
    void init() {
      eval_score();
    }

    void apply_op(const Action &action) {
      history.push_back(action);
      _apply_op(action);
      eval_score();
    }

    void copy(StatePtr &new_state) const {
      new_state->score = this->score;
      new_state->history = this->history;
      _copy(new_state);
    }

    bool operator>(const State &right) const { return score > right.score; }
    bool operator<(const State &right) const { return score < right.score; }
  };

  struct Param {
    int beam_depth, beam_width;
  };

  /**
   * @fn
   * @brief ビームサーチを実行する
   * @param (verbose: bool) ログの出力
   * @return 探索した中での最適な状態
   * @details コピーが少ない
   */
  static State run(const Param &param, const bool verbose=false) {
    StatePool<State> pool;
    StatePool<ScoreState> score_pool;
    pool.init(param.beam_width*2+1);
    score_pool.init(param.beam_width);
    const int best_state = pool.gen();
    pool.get(best_state)->init();

    for (int turn = 0; turn < MAX_TURN; ++turn) {
      if (verbose) cerr << "# turn : " << turn << endl;
      vector<int> keep;
      int state0 = pool.copy(best_state);
      keep.emplace_back(state0);
      int beam_depth = min(param.beam_depth, MAX_TURN - turn);
      for (int beam_turn = 0; beam_turn < beam_depth; ++beam_turn) {
        if (verbose) cerr << "  beam_turn : " << beam_turn << " / " << beam_depth << endl;
        vector<int> score_keep;
        for (const int now_state: keep) {
          for (const Action &op : pool.get(now_state)->get_actions()) {
            ScoreType new_score = pool.get(now_state)->try_op(op);
            // if (verbose) cerr << "    action : " << op << ' ' << "score : " << new_score << endl;
            int score_state = score_pool.gen();
            score_pool.get(score_state)->score = new_score;
            score_pool.get(score_state)->par = now_state;
            score_pool.get(score_state)->action = op;
            score_keep.emplace_back(score_state);
          }
          // if (verbose) cerr << endl;
        }
        nth_element(score_keep.begin(), score_keep.begin() + min((int)score_keep.size(), param.beam_width), score_keep.end(), [&] (const int &l, const int &r) {
          return score_pool.get(l)->score < score_pool.get(r)->score;
        });
        Action &op = score_pool.get(score_keep.front())->action;
        if (verbose) cerr << "  best_action : " << op << endl << endl;;
        bool is_all_same = true;
        vector<int> new_keep(min(param.beam_width, (int)score_keep.size()));
        for (int i = 0; i < param.beam_width && i < score_keep.size(); ++i) {
          int state = pool.copy(score_pool.get(score_keep[i])->par);
          pool.get(state)->apply_op(score_pool.get(score_keep[i])->action);
          if (is_all_same && pool.get(state)->history[turn] != op) is_all_same = false;
          new_keep[i] = state;
        }
        for (const int state: score_keep) score_pool.del(state);
        for (const int state: keep) pool.del(state);
        swap(keep, new_keep);
        if (is_all_same) break;
      }
      int d_best_state = *min_element(keep.begin(), keep.end(), [&] (const int &l, const int &r) {
        return (*pool.get(l)) < (*pool.get(r));
      });
      Action op = pool.get(d_best_state)->history[turn];
      pool.get(best_state)->apply_op(op);
      for (const int node_id: keep) pool.del(node_id);
    }
    return *pool.get(best_state);
  }

  /**
   * @fn
   * @brief ビームサーチを実行する
   * @param (verbose: bool) ログの出力
   * @return 探索した中での最適な状態
   * @details コピーをする。try_opの計算量が十分大きいなら、copyする方が速め。
   */
  static State run_copy(const Param &param, const bool verbose=false) {
    StatePool<State> pool;
    pool.init(param.beam_width+1);
    const int best_state = pool.gen();
    pool.get(best_state)->init();
    for (int turn = 0; turn < MAX_TURN; ++turn) {
      vector<int> keep;
      int state0 = pool.copy(best_state);
      keep.emplace_back(state0);
      int beam_depth = min(param.beam_depth, MAX_TURN - turn);
      for (int beam_turn = 0; beam_turn < beam_depth; ++beam_turn) {
        vector<int> keep_new;
        for (const int now_state: keep) {
          for (const Action &op : pool.get(now_state)->get_actions()) {
            int new_state = pool.copy(now_state);
            pool.get(new_state)->apply_op(op);
            keep_new.emplace_back(new_state);
          }
          pool.del(now_state);
        }
        keep.clear();
        nth_element(keep_new.begin(), keep_new.begin() + min((int)keep_new.size(), param.beam_width), keep_new.end(), [&] (const int &l, const int &r) {
          return (*pool.get(l)) < (*pool.get(r));
        });
        Action &op = pool.get(keep_new.front())->history[turn];
        bool is_all_same = true;
        for (int i = 0; i < param.beam_width && i < keep_new.size(); ++i) {
          int state = keep_new[i];
          if (is_all_same && pool.get(state)->history[turn] != op) is_all_same = false;
          keep.emplace_back(state);
        }
        for (int i = param.beam_width; i < keep_new.size(); ++i) {
          pool.del(keep_new[i]);
        }
        if (is_all_same) {
          break;
        }
      }
      int d_best_state = *min_element(keep.begin(), keep.end(), [&] (const int &l, const int &r) {
        return (*pool.get(l)) < (*pool.get(r));
      });
      Action op = pool.get(d_best_state)->history[turn];
      pool.get(best_state)->apply_op(op);
      for (const int node_id: keep) {
        pool.del(node_id);
      }
    }
    return *pool.get(best_state);
  }
}
}

