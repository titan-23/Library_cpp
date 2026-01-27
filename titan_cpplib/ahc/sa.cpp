#include <vector>
#include <cmath>

#include "titan_cpplib/ahc/timer.cpp"
#include "titan_cpplib/algorithm/random.cpp"
#include "titan_cpplib/others/print.cpp"

using namespace std;

// sa 最小化
namespace sa {

titan23::Random sarnd;
const int LOG_TABLE_SIZE = 4096;
double LOG_TABLE[LOG_TABLE_SIZE]; // 線形補間
static bool is_log_initialized = [] {
    for (int i = 0; i < LOG_TABLE_SIZE; ++i) {
        LOG_TABLE[i] = log((double)(i + 0.5) / LOG_TABLE_SIZE);
    }
    return true;
}();

// TODO
using ScoreType = double;

struct Param {
    double start_temp = 1000, end_temp = 1;
} param;

// TODO
struct Changed {
    int TYPE_CNT = 0; // TODO
    int type;
    ScoreType pre_score;
    Changed() {}
} changed;

// TODO
void sa_init() {}

class State {
public:
    bool is_valid;
    ScoreType score;
    State() {}

    // TODO
    void init() {
        score = 0;
    }

    void reset_is_valid() { is_valid = true; }
    ScoreType get_score() const { return score; } // TODO 最大化なら `-score` などにする
    ScoreType get_true_score() const { return score; }

    // TODO
    // thresholdを超えたらダメ(同じなら遷移する)
    // is_validをfalseにすると必ずrejectする、rollbackはする
    // progress:焼きなまし進行度 0.0~1.0 まで
    void modify(const ScoreType threshold, const double progress) {}

    // TODO
    // scoreはもう戻してある
    void rollback() {}

    // TODO
    void advance() {}

    // TODO
    void print() const {}
};

// TIME_LIMIT: ms
State sa_run(const double TIME_LIMIT, const bool verbose = false) {
    titan23::Timer sa_timer;

    const double START_TEMP = param.start_temp;
    const double END_TEMP   = param.end_temp;
    const double TEMP_VAL = (START_TEMP - END_TEMP) / TIME_LIMIT;

    State state;
    state.init();
    State best_state = state;
    ScoreType score = state.get_score();
    ScoreType best_score = score;
    double now_time;

    long long cnt = 0, bst_cnt = 0, upd_cn = 0;
    vector<long long> accept(changed.TYPE_CNT), modify(changed.TYPE_CNT);
    while (true) {
        // if ((cnt & 31) == 0) now_time = sa_timer.elapsed();
        now_time = sa_timer.elapsed();
        if (now_time > TIME_LIMIT) break;
        ++cnt;
        ScoreType threshold = score - (START_TEMP-TEMP_VAL*now_time) * LOG_TABLE[sarnd.randrange(LOG_TABLE_SIZE)];
        changed.pre_score = state.score;
        double progress = now_time / TIME_LIMIT;
        state.reset_is_valid();
        state.modify(threshold, progress);
        modify[changed.type]++;
        ScoreType new_score = state.get_score();
        if (state.is_valid && new_score <= threshold) {
            ++upd_cnt;
            accept[changed.type]++;
            state.advance();
            score = new_score;
            if (score < best_score) {
                bst_cnt++;
                best_score = score;
                best_state = state;
                if (verbose) {
                    cerr << "Info: score=" << best_score << endl;
                }
            }
        } else {
            state.score = changed.pre_score;
            state.rollback();
        }
    }
    if (verbose) {
        cerr << "=============" << endl;
        for (int i = 0; i < modify.size(); ++i) {
            cerr << "Info: Type=" << i << " | " << accept[i] << " / " << modify[i] << endl;
        }
        cerr << "Info: bst=" << bst_cnt << endl;
        cerr << "Info: ac=" << upd_cnt << endl;
        cerr << "Info: loop=" << cnt << endl;
        cerr << "Info: accept rate=" << (cnt > 0 ? (int)((double)upd_cnt/cnt*100) : 0) << "%" << endl;
        cerr << "Info: update best rate=" << (cnt > 0 ? (int)((double)bst_cnt/cnt*100) : 0) << "%" << endl;
        cerr << "Info: best_score = " << best_score << endl;
        cerr << "Info: cnt=" << cnt << endl;
    }
    return best_state;
}
} // namespace sa
