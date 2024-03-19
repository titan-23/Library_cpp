#include <vector>
#include <cmath>
#include "titan_cpplib/ahc/timer.cpp"
#include "titan_cpplib/algorithm/random.cpp"

using namespace std;

// SA 最小化
namespace titan23 {

namespace sa {

  titan23::Random sa_random;

  using Score = double;
  struct Changed {
    Score pre_score;
    Changed() {}
    Changed(Score score) : pre_score(score) {}
  };

  struct State {
    Score score;
    State() : score(0.0) {}

    void init() {}

    Score get_score() const { return score; }

    // TODO: 遷移
    // thresholdを超えたら、だめ
    Changed modify(const Score threshold) {
      Changed changed(score);
      return changed;
    }

    // TODO: 遷移を戻す
    void rollback(Changed &changed) {
      score = changed.pre_score;
    }

    // TODO: 表示する
    void print() const {
    }
  };

  // TIME_LIMIT: ms
  State run(const double TIME_LIMIT) {
    titan23::Timer sa_timer;

    const double START_TEMP = 100;  // TODO: 温度
    const double END_TEMP = 1;  // TODO: 温度
    const double TEMP_VAL = (START_TEMP - END_TEMP) / TIME_LIMIT;

    State ans;
    ans.init();
    State best_ans = ans;
    Score score = ans.get_score();
    Score best_score = score;
    double now_time;

    int cnt = 0;
    while (true) {
      // if ((cnt & 31) == 0) now_time = sa_timer.elapsed();
      now_time = sa_timer.elapsed();
      if (now_time > TIME_LIMIT) break;
      ++cnt;
      Score threshold = score - (START_TEMP-TEMP_VAL*now_time) * log(sa_random.random());
      Changed changed = ans.modify(threshold);
      Score new_score = ans.get_score();
      if (new_score <= threshold) {
        score = new_score;
        if (score < best_score) {
          best_score = score;
          best_ans = ans;
          // cerr << "score=" << best_score << endl;
        }
      } else {
        ans.rollback(changed);
      }
    }
    cerr << "cnt=" << cnt << endl;
    return best_ans;
  }
}
}  // namespace titan23

