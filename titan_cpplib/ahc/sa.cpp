#include <vector>
#include <cmath>
#include "titan_cpplib/ahc/timer.cpp"
#include "titan_cpplib/algorithm/random.cpp"

using namespace std;

// sa 最小化
namespace titan23 {

namespace sa {

  struct Param {
    double start_temp, end_temp;
  };

  Param param;
  titan23::Random sa_random;
  using ScoreType = double;

  struct Changed {
    ScoreType pre_score;
    Changed() {}
  };

  Changed changed;

  class State {
   public:
    ScoreType score;
    State() {}

    void init() {
    }

    ScoreType get_score() const { return score; }
    ScoreType get_true_score() const { return score; }

    // thresholdを超えたら、だめ
    void modify(const ScoreType threshold) {
    }

    void rollback() {
    }

    void advance() {
    }

    void print() const {
    }
  };

  // TIME_LIMIT: ms
  static inline State run(const double TIME_LIMIT) {
    titan23::Timer sa_timer;

    // const double START_TEMP = param.start_temp;
    // const double END_TEMP   = param.end_temp;
    const double START_TEMP = 1000;
    const double END_TEMP   = 1;
    const double TEMP_VAL = (START_TEMP - END_TEMP) / TIME_LIMIT;

    State ans;
    ans.init();
    State best_ans = ans;
    ScoreType score = ans.get_score();
    ScoreType best_score = score;
    double now_time;

    int cnt = 0;
    while (true) {
      // if ((cnt & 31) == 0) now_time = sa_timer.elapsed();
      now_time = sa_timer.elapsed();
      if (now_time > TIME_LIMIT) break;
      ++cnt;
      ScoreType threshold = score - (START_TEMP-TEMP_VAL*now_time) * log(sa_random.random());
      changed.pre_score = ans.score;
      ans.modify(threshold);
      ScoreType new_score = ans.get_score();
      if (new_score <= threshold) {
        ans.advance();
        score = new_score;
        if (score < best_score) {
          best_score = score;
          best_ans = ans;
          cerr << "score=" << best_score << endl;
        }
      } else {
        ans.score = changed.pre_score;
        ans.rollback();
      }
    }
    cerr << "cnt=" << cnt << endl;
    return best_ans;
  }
}
}  // namespace titan23
