#pragma once
#include <bits/stdc++.h>
#include "titan_cpplib/others/print.cpp"
#include "titan_cpplib/algorithm/random.cpp"
#include "titan_cpplib/ahc/timer.cpp"
#include "titan_cpplib/ds/hash_dict.cpp"
#include "titan_cpplib/ahc/beam_search/beam_param.cpp"
#include "titan_cpplib/ahc/beam_search/candidates.cpp"

using namespace std;

namespace flying_squirrel {

template<typename ScoreType, typename HashType, class Action, class State, ScoreType INF, bool record_history=false>
class BeamSearchWithTree {
private:
    titan23::Random rnd;
    titan23::Timer beam_timer;
    Action DAMMY_ACTION;

    bool found_finished;
    ScoreType best_finished_score;
    vector<Action> best_finished_path;

    Candidates<ScoreType, HashType, Action, State, INF, record_history> candidates;
    vector<Action> trace;
    vector<Action> tour;
    vector<int> leaf;
    vector<BeamCandidate<ScoreType, Action>> cand;

    vector<Action> next_tour;
    vector<int> next_leaf;
    vector<Action> actions;

    void init_bs() {
        beam_timer.reset();
        rnd = titan23::Random();
        found_finished = false;
        best_finished_score = INF;
        best_finished_path.clear();
        trace.clear();
        tour.clear();
        leaf.clear();
        cand.clear();
        next_tour.clear();
        next_leaf.clear();
        actions.clear();
    }

public:
    /**
     * @brief ビームサーチをする
     *
     * @param param ターン数、ビーム幅を指定するパラメータ構造体
     * @param verbose ログ出力するかどうか
     * @return vector<Action>
     */
    vector<Action> search(BeamParam &param, const bool verbose=false, const string& history_file = "") {
        init_bs();
        if (verbose) cerr << "[BeamSearch] Info: start search()" << endl;
        State* state = new State;
        state->init();

        int w = param.get_beam_width(param.max_turn, 0, param.time_limit);
        candidates.reset(0, w);

        actions.clear();
        state->get_actions(actions, 0, DAMMY_ACTION, candidates.threshold());
        for (Action &action : actions) {
            auto [score, hash, finished] = state->try_op(action, candidates.threshold());
            if (score >= INF) continue;
            if (finished) {
                if (!found_finished || score < best_finished_score) {
                    found_finished = true;
                    best_finished_score = score;
                    best_finished_path = {move(action)};
                }
            } else {
                candidates.push(score, hash, 0, move(action));
            }
        }

        if (found_finished) {
            return best_finished_path;
        }

        cand.clear();
        for (int i = 0; i < (int)candidates.size(); ++i) {
            cand.push_back(move(candidates.next_beam[i]));
        }
        leaf = {0};

        for (int turn = 1; turn < param.max_turn; ++turn) {
            double now_time = beam_timer.elapsed();
            if (verbose) cerr << "\n[BeamSearch] Info: # turn : " << turn << " | " << now_time << " [ms]" << endl;

            next_tour.clear();
            next_leaf.clear();
            w = param.get_beam_width(param.max_turn - turn, cand.size(), param.time_limit - beam_timer.elapsed());
            candidates.reset(turn, w);

            int li = leaf.size() - 1;
            int f = 0;
            trace.resize(turn + 1);

            if (!cand.empty()) {
                trace[turn] = cand.back().action;
            }

            for (int i = (int)cand.size() - 1; i >= 0; --i) {
                const auto &c = cand[i];

                int lca_dist = 0;
                int now_lca = leaf[li];
                for (int k = li - 1; k >= c.parent_leaf; --k) {
                    if (now_lca - leaf[k] > lca_dist) {
                        lca_dist = now_lca - leaf[k];
                    }
                    now_lca = leaf[k];
                }

                for (int k = 0; k < lca_dist + f; ++k) {
                    state->rollback(trace[turn - 1 + f - k]);
                }

                next_tour.insert(next_tour.end(), trace.begin() + (turn - lca_dist), trace.begin() + (turn + 1));
                f = 1;

                trace[turn] = c.action;
                int prog = 0;
                for (int k = c.parent_leaf; k < li; ++k) {
                    int w0 = leaf[k];
                    int w1 = leaf[k + 1];
                    int rank = w1 - w0;
                    if (prog < rank) {
                        int copy_len = rank - prog;
                        copy(tour.begin() + w0, tour.begin() + w0 + copy_len, trace.begin() + (turn - rank));
                        prog = rank;
                    }
                }

                for (int k = turn - lca_dist; k <= turn; ++k) {
                    state->apply_op(trace[k]);
                }

                actions.clear();
                state->get_actions(actions, turn, c.action, candidates.threshold());
                int now_leaf_idx = next_leaf.size();

                for (Action &action : actions) {
                    auto [score, hash, finished] = state->try_op(action, candidates.threshold());
                    if (score >= INF) continue;
                    if (finished) {
                        if (!found_finished || score < best_finished_score) {
                            found_finished = true;
                            best_finished_score = score;
                            best_finished_path.assign(trace.begin() + 1, trace.begin() + turn + 1);
                            best_finished_path.push_back(action);
                        }
                    } else {
                        candidates.push(score, hash, now_leaf_idx, action);
                    }
                }
                next_leaf.push_back(next_tour.size());
                li = c.parent_leaf;
            }

            if (found_finished) {
                if (verbose) cerr << "[BeamSearch] Info: find valid solution." << endl;
                return best_finished_path;
            }

            if (candidates.size() == 0) {
                cerr << "[BeamSearch] " << to_red("Error: \t次の候補が見つかりませんでした") << endl;
                assert(candidates.size() > 0);
            }

            if (verbose) {
                BeamCandidate<ScoreType, Action> bests = candidates.get_best();
                cerr << "[BeamSearch] " << PRINT_GREEN << "Info: \tbest_score = " << bests.score << PRINT_NONE << endl;
            }

            swap(tour, next_tour);
            swap(leaf, next_leaf);

            cand.clear();
            for (int i = 0; i < (int)candidates.size(); ++i) {
                cand.push_back(move(candidates.next_beam[i]));
            }
            sort(cand.begin(), cand.end(), [](const BeamCandidate<ScoreType, Action>& a, const BeamCandidate<ScoreType, Action>& b) {
                if (a.parent_leaf != b.parent_leaf) return a.parent_leaf < b.parent_leaf;
                return a.score < b.score;
            });

            param.timestamp(tour.size(), candidates.size(), beam_timer.elapsed() - now_time);
        }

        if (verbose) {
            cerr << "[BeamSearch] Info: max_turn finished." << endl;
            param.report();
        }

        int best_idx = -1;
        ScoreType best_score = INF;
        for (int i = 0; i < (int)cand.size(); ++i) {
            if (best_idx == -1 || cand[i].score < best_score) {
                best_score = cand[i].score;
                best_idx = i;
            }
        }

        vector<Action> ret(trace.begin() + 1, trace.end());
        int len = ret.size();
        int prog = 0;
        for (int k = cand[best_idx].parent_leaf; k < (int)leaf.size() - 1; ++k) {
            int w0 = leaf[k];
            int w1 = leaf[k + 1];
            int rank = w1 - w0;
            if (prog < rank) {
                int copy_len = rank - prog;
                copy(tour.begin() + w0, tour.begin() + w0 + copy_len, ret.begin() + (len - rank));
                prog = rank;
            }
        }
        ret.push_back(cand[best_idx].action);

        return ret;
    }
};
} // namespace flying_squirrel
