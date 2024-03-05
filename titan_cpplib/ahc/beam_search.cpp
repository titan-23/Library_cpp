#include <vector>
#include <cassert>
#include <algorithm>
#include <memory>
using namespace std;

// beam_search
namespace titan23 {

namespace beam_search {
  struct Node;
  using NodePtr = shared_ptr<Node>;

  // TODO:
  enum class Action {};
  const vector<Action> Actions = {};

  struct Node {
    float score;   // スコアは小さいほどよい
    vector<Action> history;

    Node() {}

    bool operator>(const Node &right) const {
      return score > right.score;
    }
    bool operator<(const Node &right) const {
      return score < right.score;
    }

    void eval_score() {
      // TODO:
      score = 0;
    }

    NodePtr copy() {
      NodePtr new_node = make_shared<Node>();
      new_node->history = history;
      // TODO:
      return new_node;
    }

    void print() const {
    }

    void apply_op(const Action &action) {
      history.push_back(action);
      // TODO:
      eval_score();
    }

    void init() {
      // TODO:
      eval_score();
    }
  };

  struct Param {
    int MAX_TURN;
    int beam_depth, beam_width;
  };

  struct BeamSearch {
    Param param;

    BeamSearch() {}
    BeamSearch(Param &param) {
      this->param = param;
    };

    Node run() {

      NodePtr node = make_shared<Node>();
      node->init();

      for (int turn = 0; turn < param.MAX_TURN; ++turn) {
        vector<NodePtr> keep;

        NodePtr node0 = node->copy();
        keep.emplace_back(node0);

        int beam_depth = min(param.beam_depth, param.MAX_TURN - turn);
        for (int beam_turn = 0; beam_turn < beam_depth; ++beam_turn) {

          vector<NodePtr> keep_new;

          for (NodePtr &node: keep) {
            for (const Action &op : Actions) {
              NodePtr new_node = node->copy();
              new_node->apply_op(op);
              keep_new.emplace_back(new_node);
            }
          }
          keep.clear();

          nth_element(keep_new.begin(), keep_new.begin() + min((int)keep_new.size(), param.beam_width), keep_new.end(), [] (const NodePtr &l, const NodePtr &r) {
            return l->score < r->score;
          });

          Action op = keep_new.front()->history[turn];
          bool is_all_same = true;
          for (int i = 0; i < param.beam_width && i < keep_new.size(); ++i) {
            NodePtr node = keep_new[i];
            if (node->history[turn] != op) is_all_same = false;
            keep.emplace_back(node);
          }
          if (is_all_same) break;
        }

        NodePtr best_node = *min_element(keep.begin(), keep.end());
        Action op = best_node->history[turn];
        node->apply_op(op);
      }
      return *node;
    }
  };

}
}

