// #include <omp.h>
// -fopenmp
// OMP_NUM_THREADS=8 ./main

#include <vector>
#include <cmath>

#include "titan_cpplib/ahc/timer.cpp"
#include "titan_cpplib/algorithm/random.cpp"
#include "titan_cpplib/others/print.cpp"

using namespace std;

// sa 最小化
namespace sa {

const int LOG_TABLE_SIZE = 4096;
double LOG_TABLE[LOG_TABLE_SIZE]; // 線形補間
static bool is_log_initialized = [] {
    for (int i = 0; i < LOG_TABLE_SIZE; ++i) {
        LOG_TABLE[i] = log((double)(i + 0.5) / LOG_TABLE_SIZE);
    }
    return true;
}();

using ScoreType = double; // TODO

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

thread_local titan23::Random sarnd;
thread_local Changed changed;

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

    long long cnt = 0, bst_cnt = 0, upd_cnt = 0;
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

State replica_run(
    const double TIME_LIMIT,
    const int NUM_REPLICAS=8,
    const int SWAP_ITER_INTERVAL=100,
    const bool verbose = false
) {
    titan23::Timer sa_timer;

    vector<double> temps(NUM_REPLICAS);
    for (int i = 0; i < NUM_REPLICAS; ++i) {
        temps[i] = param.start_temp * pow(param.end_temp / param.start_temp, (double)i / max(1, NUM_REPLICAS - 1));
    }
    vector<State> states(NUM_REPLICAS);

    int max_threads = omp_get_max_threads();
    int type_cnt = max(1, changed.TYPE_CNT);
    vector<vector<long long>> accept(max_threads, vector<long long>(type_cnt, 0));
    vector<vector<long long>> modify(max_threads, vector<long long>(type_cnt, 0));

    #pragma omp parallel
    {
        sarnd.set_seed(1000 + omp_get_thread_num());
        #pragma omp for schedule(static)
        for (int i = 0; i < NUM_REPLICAS; ++i) {
            states[i].init();
        }
    }

    State best_state = states[0];
    ScoreType best_score = states[0].get_score();
    for (int i = 1; i < NUM_REPLICAS; ++i) {
        if (states[i].get_score() < best_score) {
            best_score = states[i].get_score();
            best_state = states[i];
        }
    }

    long long swap_cnt = 0;
    long long swap_attempt_total = 0;
    long long swap_accept_total = 0;
    long long total_iter = 0;
    double now_time = sa_timer.elapsed();

    while (now_time < TIME_LIMIT) {
        double block_start_time = now_time;

        #pragma omp parallel
        {
            int tid = omp_get_thread_num();
            #pragma omp for schedule(static)
            for (int i = 0; i < NUM_REPLICAS; ++i) {
                double now_temp = temps[i];
                double progress = now_time / TIME_LIMIT;
                for (int step = 0; step < SWAP_ITER_INTERVAL; ++step) {
                    ScoreType threshold = states[i].score - now_temp * LOG_TABLE[sarnd.randrange(LOG_TABLE_SIZE)];
                    states[i].reset_is_valid();
                    states[i].modify(threshold, progress);
                    modify[tid][changed.type]++;
                    ScoreType new_score = states[i].get_score();
                    if (states[i].is_valid && new_score <= threshold) {
                        states[i].advance();
                        accept[tid][changed.type]++;
                    } else {
                        states[i].rollback();
                    }
                }
            }
        }
        total_iter += (long long)NUM_REPLICAS * SWAP_ITER_INTERVAL;

        now_time = sa_timer.elapsed();
        double block_time = now_time - block_start_time;

        if (now_time > TIME_LIMIT) break;

        bool updated = false;
        for (int i = 0; i < NUM_REPLICAS; ++i) {
            if (states[i].score < best_score) {
                best_score = states[i].score;
                best_state = states[i];
                updated = true;
            }
        }

        if (verbose) {
            if (updated) {
                cerr << "Info: time=" << (int)now_time << "ms | score=" << best_score
                     << " | block_time=" << block_time << "ms" << endl;
            } else if (swap_cnt % 100 == 0) {
                cerr << "Info: swap=" << swap_cnt << " | time=" << (int)now_time << "ms"
                     << " | block_time=" << block_time << "ms" << endl;
            }
        }

        int offset = swap_cnt % 2;
        for (int i = offset; i + 1 < NUM_REPLICAS; i += 2) {
            double T1 = temps[i];
            double T2 = temps[i + 1];
            ScoreType E1 = states[i].get_score();
            ScoreType E2 = states[i + 1].get_score();
            double delta = (E1 - E2) * (1.0 / T1 - 1.0 / T2);
            bool do_swap = delta >= 0.0 || LOG_TABLE[sarnd.randrange(LOG_TABLE_SIZE)] < delta;

            swap_attempt_total++;
            if (do_swap) {
                swap(states[i], states[i+1]);
                swap_accept_total++;
            }
        }
        swap_cnt++;
    }

    if (verbose) {
        cerr << "=== Replica Exchange Log ===" << endl;
        cerr << "Total iterations : " << total_iter << endl;

        cerr << "--- Accept Rates by Type ---" << endl;
        for (int t = 0; t < type_cnt; ++t) {
            long long total_acc = 0, total_mod = 0;
            // 全スレッドのカウントを集計
            for (int tid = 0; tid < max_threads; ++tid) {
                total_acc += accept[tid][t];
                total_mod += modify[tid][t];
            }
            if (total_mod > 0) {
                cerr << "  Type[" << t << "] : " << total_acc << " / " << total_mod
                     << " (" << (double)total_acc / total_mod * 100.0 << "%)" << endl;
            }
        }

        cerr << "Swap attempts    : " << swap_attempt_total << endl;
        cerr << "Swap accepts     : " << swap_accept_total << " ("
             << (swap_attempt_total > 0 ? (double)swap_accept_total / swap_attempt_total * 100.0 : 0.0) << "%)" << endl;
        cerr << "Avg time/swap    : " << (swap_cnt > 0 ? now_time / swap_cnt : 0.0) << " ms" << endl;
        cerr << "Best Score       : " << best_score << endl;
        cerr << "--- Final Scores by Temp ---" << endl;
        for (int i = 0; i < NUM_REPLICAS; ++i) {
            cerr << "  Temp[" << i << "] (" << temps[i] << ") : " << states[i].get_score() << endl;
        }
    }
    return best_state;
}
} // namespace sa
