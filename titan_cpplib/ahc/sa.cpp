#include <vector>
#include <cmath>

#include "titan_cpplib/ahc/timer.cpp"
#include "titan_cpplib/algorithm/random.cpp"
#include "titan_cpplib/others/print.cpp"

using namespace std;

// sa 最小化
namespace sa {

struct Param {
    double start_temp, end_temp;
};

Param param;
titan23::Random sa_random;
using ScoreType = double;

struct Changed {
    int type;
    ScoreType pre_score;
    Changed() {}
};

Changed changed;

class State {
public:
    bool is_valid;
    ScoreType score;
    State() {}

    void init() {
        score = 0;
    }

    void reset_is_valid() { is_valid = true; }
    ScoreType get_score() const { return score; }
    ScoreType get_true_score() const { return score; }

    // thresholdを超えたらダメ(同じなら遷移する)
    void modify(const ScoreType threshold) {}

    void rollback() {}

    void advance() {}

    void print() const {}
};

// TIME_LIMIT: ms
State sa_run(const double TIME_LIMIT, const bool verbose = false) {
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
    int bst_cnt = 0;
    int upd_cnt = 0;
    while (true) {
        // if ((cnt & 31) == 0) now_time = sa_timer.elapsed();
        now_time = sa_timer.elapsed();
        if (now_time > TIME_LIMIT) break;
        ++cnt;
        ScoreType threshold = score - (START_TEMP-TEMP_VAL*now_time) * log(sa_random.random());
        changed.pre_score = ans.score;
        ans.reset_is_valid();
        ans.modify(threshold);
        ScoreType new_score = ans.get_score();
        if (ans.is_valid && new_score <= threshold) {
            ++upd_cnt;
            ans.advance();
            score = new_score;
            if (score < best_score) {
                bst_cnt++;
                best_score = score;
                best_ans = ans;
                if (verbose) {
                    cerr << "Info: score=" << best_score << endl;
                }
            }
        } else {
            ans.score = changed.pre_score;
            ans.rollback();
        }
    }
    if (verbose) {
        cerr << "Info: best_score = " << best_score << endl;
        cerr << "Info: bst=" << bst_cnt << endl;
        cerr << "Info: upd=" << upd_cnt << endl;
        cerr << "Info: cnt=" << cnt << endl;
    }
    return best_ans;
}
} // namespace sa
