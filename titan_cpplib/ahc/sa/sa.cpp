#pragma once

// OMP_NUM_THREADS=32 time ./a.out < in/0000.txt > out.txt
#include <bits/stdc++.h>
#include <omp.h>
#include "titan_cpplib/ahc/timer.cpp"
#include "titan_cpplib/algorithm/random.cpp"
using namespace std;

// simulated annealing (minimize)
namespace sa {

const int LOG_TABLE_SIZE = 4096;
double LOG_TABLE[LOG_TABLE_SIZE];
static bool is_log_initialized = [] {
    for (int i = 0; i < LOG_TABLE_SIZE; ++i) {
        LOG_TABLE[i] = log((double)(i + 0.5) / LOG_TABLE_SIZE);
    }
    return true;
}();

/// @brief 焼きなましを実行する
/// @tparam State
/// @param TIME_LIMIT 実行時間[ms]
/// @param seed 乱数のシード値
/// @param verbose ログ出力をするかどうか
/// @return State::Result 最良解
template<class State>
typename State::Result sa_run(const double TIME_LIMIT, const uint32_t seed=23, const bool verbose=true) {
    // 準備
    titan23::Timer sa_timer;
    titan23::Random sa_rnd;

    const double START_TEMP = State::param.start_temp;
    const double END_TEMP   = max(State::param.end_temp, 1e-9);
    const double TEMP_VAL = (START_TEMP - END_TEMP) / TIME_LIMIT;
    const int TEMP_TABLE_SIZE = 4096;
    vector<double> TEMP_TABLE(TEMP_TABLE_SIZE);
    for (int i = 0; i < TEMP_TABLE_SIZE; ++i) {
        TEMP_TABLE[i] = START_TEMP * pow(END_TEMP / START_TEMP, (double)i / (TEMP_TABLE_SIZE - 1));
    }

    State state;
    int64_t iter = 0, bst_cnt = 0, upd_cnt = 0;
    int64_t valid_cnt = 0;
    vector<int64_t> accept(state.changed.TYPE_CNT), accept_worse(state.changed.TYPE_CNT), modify(state.changed.TYPE_CNT);
    double now_time = 0;

    // 初期化
    state.init(seed);
    typename State::Result best_result = state.get_result();
    typename State::ScoreType score = best_result.score;
    if (verbose) {
        cerr << "init-fin" << endl;
        cerr << "init-time  = " << sa_timer.elapsed() << endl;
        cerr << "init-score = " << best_result.true_score << " (" << best_result.score << ")" << endl;
        cerr << "--------" << endl;
    }

    while (true) {
        // if ((iter & 31) == 0) now_time = sa_timer.elapsed();
        now_time = sa_timer.elapsed();
        if (now_time > TIME_LIMIT) break;
        ++iter;
        double progress = now_time / TIME_LIMIT;

        // 線形冷却
        // typename State::ScoreType threshold = score - (START_TEMP-TEMP_VAL*now_time) * LOG_TABLE[sa_rnd.randrange(LOG_TABLE_SIZE)];

        // 指数冷却
        int temp_idx = (int)(progress * (TEMP_TABLE_SIZE - 1));
        double now_temp = TEMP_TABLE[temp_idx];
        typename State::ScoreType threshold = score - now_temp * LOG_TABLE[sa_rnd.randrange(LOG_TABLE_SIZE)];

        state.reset_is_valid();
        state.modify(iter, threshold, progress);
        modify[state.changed.type]++;
        typename State::ScoreType new_score = state.get_score();
        if (state.is_valid) valid_cnt++;
        if (state.is_valid && new_score <= threshold) {
            ++upd_cnt;
            accept[state.changed.type]++;
            if (new_score > score) accept_worse[state.changed.type]++;
            state.advance();
            score = new_score;
            if (score < best_result.score) {
                bst_cnt++;
                best_result = state.get_result();
                if (verbose) {
                    cerr << "Info: score=" << best_result.true_score << " (" << best_result.score << ")"
                         << " | time=" << now_time << " | prog=" << (progress * 100.0) << "%"
                         << " | temp=" << now_temp << endl;
                }
            }
        } else {
            state.rollback();
            state.score = score;
        }
    }
    if (verbose) {
        double total_time = sa_timer.elapsed();
        cerr << "=============" << endl;
        for (int i = 0; i < (int)modify.size(); ++i) {
            double accept_rate = (modify[i] > 0) ? ((double)accept[i] / modify[i] * 100.0) : 0.0;
            double worse_rate = (accept[i] > 0) ? ((double)accept_worse[i] / accept[i] * 100.0) : 0.0;
            cerr << "Info: Type=" << i << " | " << accept[i] << " / " << modify[i]
                 << " (" << accept_rate << "%)"
                 << " | worse=" << accept_worse[i] << " / " << accept[i]
                 << " (" << worse_rate << "%)" << endl;
        }
        cerr << "Info: bst=" << bst_cnt << endl;
        cerr << "Info: ac=" << upd_cnt << endl;
        cerr << "Info: loop=" << iter << endl;
        cerr << "Info: valid_cnt=" << valid_cnt << ", " << (iter > 0 ? (int)((double)valid_cnt/iter*100) : 0) << "%" << endl;
        cerr << "Info: accept rate=" << (iter > 0 ? (int)((double)upd_cnt/iter*100) : 0) << "%" << endl;
        cerr << "Info: update best rate=" << (iter > 0 ? (int)((double)bst_cnt/iter*100) : 0) << "%" << endl;
        cerr << "Info: best_score = " << best_result.true_score << " (" << best_result.score << ")" << endl;
        cerr << "Info: total_time = " << total_time << " ms" << endl;
        cerr << "Info: ips = " << (total_time > 0 ? iter * 1000.0 / total_time : 0.0) << endl;
    }
    return best_result;
}

/// @brief 2段階の多始点焼きなましを実行する
/// @tparam State
/// @param TIME_LIMIT 実行時間[ms]
/// @param FIRST_PHASE_RATIO 第1フェーズに割り当てる時間の割合 (例: 0.2 なら 20% を第1フェーズに使用)
/// @param NUM_STARTS 第1フェーズで実行する初期状態の数
/// @param seed 乱数のベースシード値
/// @param verbose ログ出力をするかどうか
/// @return State::Result 最良解
template<class State>
typename State::Result sa_multi_run(
    const double TIME_LIMIT,
    const double FIRST_PHASE_RATIO,
    const int NUM_STARTS,
    const uint32_t seed = 23,
    const bool verbose = true)
{
    titan23::Timer sa_timer;
    titan23::Random sa_rnd;

    const double phase1_time_limit = TIME_LIMIT * FIRST_PHASE_RATIO;
    const double time_per_start = phase1_time_limit / NUM_STARTS;
    const double START_TEMP = State::param.start_temp;
    const double END_TEMP = max(State::param.end_temp, 1e-9);

    const int TEMP_TABLE_SIZE = 4096;
    vector<double> TEMP_TABLE(TEMP_TABLE_SIZE);
    for (int i = 0; i < TEMP_TABLE_SIZE; ++i) {
        TEMP_TABLE[i] = START_TEMP * pow(END_TEMP / START_TEMP, (double)i / (TEMP_TABLE_SIZE - 1));
    }

    State best_overall_state;
    typename State::Result best_overall_result;
    bool is_first = true;

    for (int i = 0; i < NUM_STARTS; ++i) {
        State state;
        state.init(seed + i);

        State best_local_state = state;
        typename State::Result best_local_result = state.get_result();
        typename State::ScoreType score = best_local_result.score;

        int64_t iter = 0;
        double start_time = sa_timer.elapsed();
        double now_time = start_time;

        while (true) {
            now_time = sa_timer.elapsed();
            if (now_time - start_time > time_per_start) break;
            ++iter;
            double progress = (now_time - start_time) / time_per_start;
            if (progress > 1.0) progress = 1.0;

            int temp_idx = (int)(progress * (TEMP_TABLE_SIZE - 1));
            double now_temp = TEMP_TABLE[temp_idx];
            typename State::ScoreType threshold = score - now_temp * LOG_TABLE[sa_rnd.randrange(LOG_TABLE_SIZE)];

            state.reset_is_valid();
            state.modify(iter, threshold, progress);
            typename State::ScoreType new_score = state.get_score();

            if (state.is_valid && new_score <= threshold) {
                state.advance();
                score = new_score;
                if (score < best_local_result.score) {
                    best_local_result = state.get_result();
                    best_local_state = state;
                }
            } else {
                state.rollback();
                state.score = score;
            }
        }

        if (is_first || best_local_result.score < best_overall_result.score) {
            best_overall_state = best_local_state;
            best_overall_result = best_local_result;
            is_first = false;
        }
    }

    if (verbose) {
        cerr << "--- Phase 1 finish ---" << endl;
        cerr << "Best score in Phase 1: " << best_overall_result.true_score << " (" << best_overall_result.score << ")" << endl;
        cerr << "Elapsed time: " << sa_timer.elapsed() << " ms" << endl;
    }

    State state = best_overall_state;
    typename State::Result best_result = best_overall_result;
    typename State::ScoreType score = state.get_score();

    int64_t iter = 0, bst_cnt = 0, upd_cnt = 0, valid_cnt = 0;
    vector<int64_t> accept(state.changed.TYPE_CNT), accept_worse(state.changed.TYPE_CNT), modify(state.changed.TYPE_CNT);

    double start_time = sa_timer.elapsed();
    double now_time = start_time;
    double phase2_time_limit = TIME_LIMIT - start_time;
    if (phase2_time_limit <= 0) return best_result;

    const double phase2_start_temp = START_TEMP * pow(END_TEMP / START_TEMP, FIRST_PHASE_RATIO);
    vector<double> PHASE2_TEMP_TABLE(TEMP_TABLE_SIZE);
    for (int i = 0; i < TEMP_TABLE_SIZE; ++i) {
        PHASE2_TEMP_TABLE[i] = phase2_start_temp * pow(END_TEMP / phase2_start_temp, (double)i / (TEMP_TABLE_SIZE - 1));
    }

    while (true) {
        now_time = sa_timer.elapsed();
        if (now_time > TIME_LIMIT) break;
        ++iter;
        double progress = (now_time - start_time) / phase2_time_limit;
        if (progress > 1.0) progress = 1.0;

        int temp_idx = (int)(progress * (TEMP_TABLE_SIZE - 1));
        double now_temp = PHASE2_TEMP_TABLE[temp_idx];
        typename State::ScoreType threshold = score - now_temp * LOG_TABLE[sa_rnd.randrange(LOG_TABLE_SIZE)];

        state.reset_is_valid();
        state.modify(iter, threshold, progress);
        modify[state.changed.type]++;
        typename State::ScoreType new_score = state.get_score();

        if (state.is_valid) valid_cnt++;
        if (state.is_valid && new_score <= threshold) {
            ++upd_cnt;
            accept[state.changed.type]++;
            if (new_score > score) accept_worse[state.changed.type]++;
            state.advance();
            score = new_score;
            if (score < best_result.score) {
                bst_cnt++;
                best_result = state.get_result();
                if (verbose) {
                    cerr << "Info: score=" << best_result.true_score << " (" << best_result.score << ")"
                         << " | time=" << now_time << " | prog=" << (progress * 100.0) << "%"
                         << " | temp=" << now_temp << endl;
                }
            }
        } else {
            state.rollback();
            state.score = score;
        }
    }

    if (verbose) {
        double total_time = sa_timer.elapsed();
        cerr << "=============" << endl;
        cerr << "--- Phase 2 finish ---" << endl;
        for (int i = 0; i < (int)modify.size(); ++i) {
            double accept_rate = (modify[i] > 0) ? ((double)accept[i] / modify[i] * 100.0) : 0.0;
            double worse_rate = (accept[i] > 0) ? ((double)accept_worse[i] / accept[i] * 100.0) : 0.0;
            cerr << "Info: Type=" << i << " | " << accept[i] << " / " << modify[i]
                 << " (" << accept_rate << "%)"
                 << " | worse=" << accept_worse[i] << " / " << accept[i]
                 << " (" << worse_rate << "%)" << endl;
        }
        cerr << "Info: bst=" << bst_cnt << endl;
        cerr << "Info: ac=" << upd_cnt << endl;
        cerr << "Info: loop=" << iter << endl;
        cerr << "Info: valid_cnt=" << valid_cnt << ", " << (iter > 0 ? (int)((double)valid_cnt/iter*100) : 0) << "%" << endl;
        cerr << "Info: accept rate=" << (iter > 0 ? (int)((double)upd_cnt/iter*100) : 0) << "%" << endl;
        cerr << "Info: update best rate=" << (iter > 0 ? (int)((double)bst_cnt/iter*100) : 0) << "%" << endl;
        cerr << "Info: best_score = " << best_result.true_score << " (" << best_result.score << ")" << endl;
        cerr << "Info: total_time = " << total_time << " ms" << endl;
        cerr << "Info: ips = " << (total_time > 0 ? iter * 1000.0 / total_time : 0.0) << endl;
    }
    return best_result;
}

/// @brief
/// @tparam State
/// @param TIME_LIMIT 実行時間[ms]
/// @param NUM_REPLICAS レプリカの個数
/// @param SWAP_ITER_INTERVAL 熱浴交換のイテレーション回数
/// @param verbose ログ出力するかどうか
/// @param record 可視化用に記録するかどうか
/// @return State::Result 最良解
template<class State>
typename State::Result replica_run(
    const double TIME_LIMIT,
    const int NUM_REPLICAS=32,
    const int SWAP_ITER_INTERVAL=100,
    const bool verbose=true,
    const bool record=false
) {
    titan23::Timer sa_timer;
    thread_local titan23::Random sa_rnd;

    vector<double> temps(NUM_REPLICAS);
    for (int i = 0; i < NUM_REPLICAS; ++i) {
        temps[i] = State::param.start_temp * pow(State::param.end_temp / State::param.start_temp, (double)i / max(1, NUM_REPLICAS-1));
    }
    vector<State> states(NUM_REPLICAS);
    vector<int> rep_idx(NUM_REPLICAS);

    #pragma omp parallel
    {
        #pragma omp for schedule(static)
        for (int i = 0; i < NUM_REPLICAS; ++i) {
            states[i].init(1000 + i);
            rep_idx[i] = i;
        }
    }

    int max_threads = omp_get_max_threads();
    int type_cnt = max(1, states[0].changed.TYPE_CNT);
    vector<vector<int64_t>> accept(max_threads, vector<int64_t>(type_cnt, 0));
    vector<vector<int64_t>> accept_worse(max_threads, vector<int64_t>(type_cnt, 0));
    vector<vector<int64_t>> modify(max_threads, vector<int64_t>(type_cnt, 0));
    vector<int64_t> iter(max_threads, 0);

    typename State::Result best_result = states[0].get_result();
    for (int i = 1; i < NUM_REPLICAS; ++i) {
        if (states[i].get_score() < best_result.score) {
            best_result = states[i].get_result();
        }
    }

    if (verbose) {
        cerr << "init-fin" << endl;
        cerr << "init-time  = " << sa_timer.elapsed() << endl;
        cerr << "init-score = " << best_result.true_score << " (" << best_result.score << ")" << endl;
        cerr << "--------" << endl;
    }

    int64_t swap_cnt = 0;
    int64_t swap_attempt_total = 0;
    int64_t swap_accept_total = 0;
    int64_t total_iter = 0;
    double now_time = sa_timer.elapsed();
    vector<typename State::Result> thread_best_results(max_threads, best_result);

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
                    typename State::ScoreType cur_score = states[r].get_score();
                    typename State::ScoreType threshold = cur_score - now_temp * LOG_TABLE[sa_rnd.randrange(LOG_TABLE_SIZE)];
                    states[r].reset_is_valid();
                    states[r].modify(iter[r], threshold, progress);
                    ++iter[r];
                    modify[tid][states[r].changed.type]++;
                    typename State::ScoreType new_score = states[r].get_score();

                    if (states[r].is_valid && new_score <= threshold) {
                        states[r].advance();
                        accept[tid][states[r].changed.type]++;
                        if (new_score > cur_score) accept_worse[tid][states[r].changed.type]++;
                        if (new_score < thread_best_results[tid].score) {
                            thread_best_results[tid] = states[r].get_result();
                        }
                    } else {
                        states[r].rollback();
                        states[r].score = cur_score;
                    }
                }
            }
        }
        total_iter += (int64_t)NUM_REPLICAS * SWAP_ITER_INTERVAL;

        now_time = sa_timer.elapsed();
        double block_time = now_time - block_start_time;

        for (int tid = 0; tid < max_threads; ++tid) {
            if (thread_best_results[tid].score < best_result.score) {
                best_result = thread_best_results[tid];
            }
        }

        if (now_time > TIME_LIMIT) break;

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
                     << " | best_score=" << best_result.true_score << " (" << best_result.score << ")" << endl;
            }
        }

        int offset = swap_cnt % 2;
        for (int i = offset; i+1 < NUM_REPLICAS; i += 2) {
            int r1 = rep_idx[i], r2 = rep_idx[i+1];
            double T1 = temps[i];
            double T2 = temps[i+1];
            typename State::ScoreType E1 = states[r1].get_score();
            typename State::ScoreType E2 = states[r2].get_score();
            double delta = (E1 - E2) * (1.0 / T1 - 1.0 / T2);
            bool do_swap = delta >= 0.0 || LOG_TABLE[sa_rnd.randrange(LOG_TABLE_SIZE)] < delta;

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
        double total_time = sa_timer.elapsed();
        cerr << "=== Replica Exchange Log ===" << endl;
        cerr << "Total iterations : " << total_iter << endl;
        cerr << "Total time       : " << total_time << " ms" << endl;
        cerr << "ips              : " << (total_time > 0 ? total_iter * 1000.0 / total_time : 0.0) << endl;
        cerr << "--- Accept Rates by Type ---" << endl;
        for (int t = 0; t < type_cnt; ++t) {
            int64_t total_acc = 0, total_mod = 0, total_worse = 0;
            for (int tid = 0; tid < max_threads; ++tid) {
                total_acc += accept[tid][t];
                total_mod += modify[tid][t];
                total_worse += accept_worse[tid][t];
            }
            if (total_mod > 0) {
                double worse_rate = (total_acc > 0) ? ((double)total_worse / total_acc * 100.0) : 0.0;
                cerr << "  Type[" << t << "] : " << total_acc << " / " << total_mod
                     << " (" << (double)total_acc / total_mod * 100.0 << "%)"
                     << " | worse=" << total_worse << " / " << total_acc
                     << " (" << worse_rate << "%)" << endl;
            }
        }
        cerr << "Swap attempts    : " << swap_attempt_total << endl;
        cerr << "Swap accepts     : " << swap_accept_total << " ("
             << (swap_attempt_total > 0 ? (double)swap_accept_total / swap_attempt_total * 100.0 : 0.0) << "%)" << endl;
        cerr << "Avg time/swap    : " << (swap_cnt > 0 ? now_time / swap_cnt : 0.0) << " ms" << endl;
        cerr << "Best Score       : " << best_result.true_score << " (" << best_result.score << ")" << endl;
        cerr << "--- Final Scores by Temp ---" << endl;
        for (int i = 0; i < NUM_REPLICAS; ++i) {
            cerr << "  Temp[" << i << "] (" << temps[i] << ") : " << states[rep_idx[i]].get_true_score() << endl;
        }
    }
    return best_result;
}
} // namespace sa
