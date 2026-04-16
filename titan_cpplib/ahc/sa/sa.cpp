#pragma once

// OMP_NUM_THREADS=8 time ./a.out < in/0000.txt > out.txt

#include <bits/stdc++.h>
#include <omp.h>
#include "titan_cpplib/ahc/timer.cpp"
#include "titan_cpplib/algorithm/random.cpp"
#include "titan_cpplib/others/print.cpp"
using namespace std;

// #ifdef __INTELLISENSE__
// #include "titan_cpplib/ahc/sa/sa_state.cpp"
// #endif

// minimize SA
namespace sa {

const int LOG_TABLE_SIZE = 4096;
double LOG_TABLE[LOG_TABLE_SIZE]; // 線形補間
static bool is_log_initialized = [] {
    for (int i = 0; i < LOG_TABLE_SIZE; ++i) {
        LOG_TABLE[i] = log((double)(i + 0.5) / LOG_TABLE_SIZE);
    }
    return true;
}();

// TIME_LIMIT: ms
Result sa_run(const double TIME_LIMIT, const bool verbose = false) {
    titan23::Timer sa_timer;

    const double START_TEMP = param.start_temp;
    const double END_TEMP   = param.end_temp;
    const double TEMP_VAL = (START_TEMP - END_TEMP) / TIME_LIMIT;

    State state;
    state.init();
    if (verbose) {
        cerr << "init-fin" << endl;
        cerr << sa_timer.elapsed() << endl;
        cerr << state.get_true_score() << endl;
    }
    Result best_result = state.get_result();
    ScoreType score = best_result.score;
    double now_time;

    long long cnt = 0, bst_cnt = 0, upd_cnt = 0;
    long long valid_cnt = 0;
    vector<long long> accept(changed.TYPE_CNT), modify(changed.TYPE_CNT);
    while (true) {
        // if ((cnt & 31) == 0) now_time = sa_timer.elapsed();
        now_time = sa_timer.elapsed();
        if (now_time > TIME_LIMIT) break;
        ++cnt;
        ScoreType threshold = score - (START_TEMP-TEMP_VAL*now_time) * LOG_TABLE[sarnd.randrange(LOG_TABLE_SIZE)];
        double progress = now_time / TIME_LIMIT;
        state.reset_is_valid();
        state.modify(threshold, progress);
        modify[changed.type]++;
        ScoreType new_score = state.get_score();
        if (state.is_valid) valid_cnt++;
        if (state.is_valid && new_score <= threshold) {
            ++upd_cnt;
            accept[changed.type]++;
            state.advance();
            score = new_score;
            if (score < best_result.score) {
                bst_cnt++;
                best_result = state.get_result();
                if (verbose) {
                    cerr << "Info: score=" << best_result.true_score << endl;
                }
            }
        } else {
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
        cerr << "Info: vaid_cnt=" << valid_cnt << ", " << (cnt > 0 ? (int)((double)valid_cnt/cnt*100) : 0) << "%" << endl;;
        cerr << "Info: accept rate=" << (cnt > 0 ? (int)((double)upd_cnt/cnt*100) : 0) << "%" << endl;
        cerr << "Info: update best rate=" << (cnt > 0 ? (int)((double)bst_cnt/cnt*100) : 0) << "%" << endl;
        cerr << "Info: best_score = " << best_result.score << endl;
        cerr << "Info: cnt=" << cnt << endl;
    }
    return best_result;
}

Result replica_run(
    const double TIME_LIMIT,
    const int NUM_REPLICAS=8,
    const int SWAP_ITER_INTERVAL=100,
    const bool verbose=false,
    const bool record=false
) {
    titan23::Timer sa_timer;

    vector<double> temps(NUM_REPLICAS);
    for (int i = 0; i < NUM_REPLICAS; ++i) {
        temps[i] = param.start_temp * pow(param.end_temp / param.start_temp, (double)i / max(1, NUM_REPLICAS - 1));
    }
    vector<State> states(NUM_REPLICAS);
    vector<int> rep_idx(NUM_REPLICAS);

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
            rep_idx[i] = i;
        }
    }

    Result best_result = states[0].get_result();
    for (int i = 1; i < NUM_REPLICAS; ++i) {
        if (states[i].get_score() < best_result.score) {
            best_result = states[i].get_result();
        }
    }

    long long swap_cnt = 0;
    long long swap_attempt_total = 0;
    long long swap_accept_total = 0;
    long long total_iter = 0;
    double now_time = sa_timer.elapsed();
    vector<Result> thread_best_results(max_threads, best_result);

    ofstream score_log;
    string timestamp_str = "";
    if (record) {
        string log_filename = "replica_scores.csv";
        score_log.open(log_filename);
        if (score_log.is_open()) {
            score_log << "time_ms";
            for (int i = 0; i < NUM_REPLICAS; ++i) {
                score_log << ",temp_" << i << "_score";
            }
            score_log << "\n";
        }
    }
    int record_num = 0;
    double last_print_time = 0.0;
    const double PRINT_INTERVAL = 10000.0; // 10sec

    while (now_time < TIME_LIMIT) {
        double block_start_time = now_time;

        #pragma omp parallel
        {
            int tid = omp_get_thread_num();
            #pragma omp for schedule(static)
            for (int i = 0; i < NUM_REPLICAS; ++i) {
                int r = rep_idx[i];
                double now_temp = temps[i];
                double progress = now_time / TIME_LIMIT;

                for (int step = 0; step < SWAP_ITER_INTERVAL; ++step) {
                    ScoreType threshold = states[r].get_score() - now_temp * LOG_TABLE[sarnd.randrange(LOG_TABLE_SIZE)];
                    states[r].reset_is_valid();
                    states[r].modify(threshold, progress);
                    modify[tid][changed.type]++;
                    ScoreType new_score = states[r].get_score();

                    if (states[r].is_valid && new_score <= threshold) {
                        states[r].advance();
                        accept[tid][changed.type]++;
                        if (new_score < thread_best_results[tid].score) {
                            thread_best_results[tid] = states[r].get_result();
                        }
                    } else {
                        states[r].rollback();
                    }
                }
            }
        }
        total_iter += (long long)NUM_REPLICAS * SWAP_ITER_INTERVAL;

        now_time = sa_timer.elapsed();
        double block_time = now_time - block_start_time;

        if (now_time > TIME_LIMIT) break;

        bool updated = false;
        for (int tid = 0; tid < max_threads; ++tid) {
            if (thread_best_results[tid].score < best_result.score) {
                best_result = thread_best_results[tid];
                updated = true;
            }
        }

        if (record) {
            if (score_log.is_open()) {
                score_log << now_time;
                for (int i = 0; i < NUM_REPLICAS; ++i) {
                    score_log << "," << states[rep_idx[i]].get_true_score();
                }
                score_log << "\n";
            }
            if (now_time - last_print_time >= PRINT_INTERVAL) {
                last_print_time = now_time;
                ostringstream oss_file;
                oss_file << "best_state_" << record_num << ".txt";
                record_num++;
                ofstream out_state(oss_file.str());
                if (out_state.is_open()) {
                    best_result.print(out_state);
                    out_state.close();
                }
            }
        }
        if (verbose) {
            if (swap_cnt % 100 == 0) {
                cerr << "Info: swap=" << swap_cnt << " | time=" << (int)now_time << "ms"
                     << " | block_time=" << block_time << "ms"
                     << " | best_score=" << best_result.true_score << endl;
            }
        }

        int offset = swap_cnt % 2;
        for (int i = offset; i+1 < NUM_REPLICAS; i += 2) {
            int r1 = rep_idx[i], r2 = rep_idx[i+1];
            double T1 = temps[i];
            double T2 = temps[i+1];
            ScoreType E1 = states[r1].get_score();
            ScoreType E2 = states[r2].get_score();
            double delta = (E1 - E2) * (1.0 / T1 - 1.0 / T2);
            bool do_swap = delta >= 0.0 || LOG_TABLE[sarnd.randrange(LOG_TABLE_SIZE)] < delta;

            swap_attempt_total++;
            if (do_swap) {
                swap(rep_idx[i], rep_idx[i+1]);
                swap_accept_total++;
            }
        }
        swap_cnt++;
    }

    if (record && score_log.is_open()) {
        score_log.close();
    }
    if (verbose) {
        cerr << "=== Replica Exchange Log ===" << endl;
        cerr << "Total iterations : " << total_iter << endl;
        cerr << "--- Accept Rates by Type ---" << endl;
        for (int t = 0; t < type_cnt; ++t) {
            long long total_acc = 0, total_mod = 0;
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
        cerr << "Best Score       : " << best_result.score << endl;
        cerr << "--- Final Scores by Temp ---" << endl;
        for (int i = 0; i < NUM_REPLICAS; ++i) {
            cerr << "  Temp[" << i << "] (" << temps[i] << ") : " << states[rep_idx[i]].get_true_score() << endl;
        }
    }
    return best_result;
}
} // namespace sa
