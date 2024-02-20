#include <vector>
#include <set>
#include <cmath>
#include "../others/timer.cpp"
#include "../algorithm/random.cpp"

using namespace std;

// SA 最小化
namespace titan23 {

namespace sa {

  titan23::Random sa_random;

  using Changed = pair<int, pair<int, int>>;  // TODO: 差分の型

  struct State {
    double score;

    State () {}

    State() : score(0.0) {}

    double get_score() const { return score; }

    // TODO: 遷移
    Changed modify() {}

    // TODO: 遷移を戻す
    void rollback(Changed &changed) {}
  };

  // TIME_LIMIT: ms
  State run(const double TIME_LIMIT) {
    titan23::Timer sa_timer;

    double START_TEMP;  // TODO: 温度
    double END_TEMP;  // TODO: 温度
    double TEMP_VAL = (START_TEMP - END_TEMP) / TIME_LIMIT;

    // TODO: 初期解
    auto make_ans_init = [&] () -> State {
    };

    State ans = make_ans_init();
    State best_ans = ans;
    double score = ans.get_score();
    double best_score = score;

    while (true) {
      double now_time = sa_timer.elapsed();
      if (now_time > TIME_LIMIT) break;
      Changed changed = ans.modify();
      double new_score = ans.get_score();
      double arg = score - new_score;
      if (arg >= 0 || exp(arg/(START_TEMP-TEMP_VAL*now_time)) > sa_random.random()) {
        score = new_score;
        if (score < best_score) {
          best_score = score;
          best_ans = ans;
        }
      } else {
        ans.rollback(changed);
      }
    }
    return best_ans;
  }
}
}  // namespace titan23

