#pragma once
#include <bits/stdc++.h>
#include "titan_cpplib/others/print.cpp"
#include "titan_cpplib/algorithm/random.cpp"
#include "titan_cpplib/ahc/timer.cpp"
#include "titan_cpplib/ds/hash_dict.cpp"
#include "titan_cpplib/ahc/beam_search/beam_param.cpp"
#include "titan_cpplib/ahc/beam_search/beam_log.cpp"
#include "titan_cpplib/ahc/beam_search/beam_history.cpp"
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

    // record_history 用。false なら if constexpr で全て消えるためゼロコスト
    int node_id_counter;
    vector<HistoryNode<ScoreType, HashType>> history;
    vector<TurnSnapshot> snapshots;

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
        if constexpr (record_history) {
            node_id_counter = 0;
            history.clear();
            snapshots.clear();
        }
    }

    //! 当該ターンの生存 node_id を集め、生き残らなかった status==0 を 1 に直し snapshot を積む。
    //! candidates.next_beam を読むだけで探索状態は変更しない。beam_search.cpp と同ロジック。
    void record_turn_survivors(int turn_label) {
        unordered_set<int> survived;
        for (int i = 0; i < (int)candidates.size(); ++i) {
            survived.insert(candidates.next_beam[i].node_id);
        }
        for (int i = (int)history.size() - 1; i >= 0; --i) {
            if (history[i].turn != turn_label) break;
            if (history[i].status == 0 && survived.find(history[i].node_id) == survived.end()) {
                history[i].status = 1;
            }
        }
        snapshots.push_back({turn_label, vector<int>(survived.begin(), survived.end())});
    }

    //! tour[leaf[k]..leaf[k+1]) を末尾 dst_end の手前にランク順に貼り込む共通ロジック。
    //! dst_end は「親パス末尾の一つ後ろ」（葉のアクションを書く位置 = ancestor path one-past-end）。
    //! 「経路復元: 親 leaf へ向かう祖先ぶんの action を tour から復元する」処理。
    template<class It>
    inline void copy_tour_path(int parent_leaf, int leaf_end, It dst_end) {
        int prog = 0;
        for (int k = parent_leaf; k < leaf_end; ++k) {
            int w0 = leaf[k];
            int w1 = leaf[k + 1];
            int rank = w1 - w0;
            if (prog < rank) {
                int copy_len = rank - prog;
                copy(tour.begin() + w0, tour.begin() + w0 + copy_len, dst_end - rank);
                prog = rank;
            }
        }
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
        if (verbose) {
            beam_log::start_banner(cerr, "BeamSearchWithTree (tour)", param);
            if (param.is_adjusting) beam_log::warn(cerr, "dynamic beam width is experimental");
        }
        State state;
        state.init();
        trace.resize(param.max_turn + 1);
        int turns_done = 0;

        int w = param.get_beam_width(param.max_turn, 0, param.time_limit);
        candidates.reset(0, w, param.clear_hash_every_turn);

        actions.clear();
        state.get_actions(actions, 0, DAMMY_ACTION, candidates.threshold());
        for (Action &action : actions) {
            auto [score, hash, finished] = state.try_op(action, candidates.threshold());
            if (score >= INF) {
                if constexpr (record_history) {
                    history.push_back({node_id_counter++, -1, 1, score, hash,
                                       action.to_string(), state.get_state_info(), 2});
                }
                continue;
            }
            if (finished) {
                if (!found_finished || score < best_finished_score) {
                    found_finished = true;
                    best_finished_score = score;
                    if constexpr (record_history) {
                        history.push_back({node_id_counter++, -1, 1, score, hash,
                                           action.to_string(), state.get_state_info(), 0});
                    }
                    best_finished_path = {move(action)};
                } else if constexpr (record_history) {
                    history.push_back({node_id_counter++, -1, 1, score, hash,
                                       action.to_string(), state.get_state_info(), 0});
                }
            } else {
                if constexpr (record_history) {
                    int nidv = node_id_counter++;
                    string as = action.to_string();
                    bool ok = candidates.push(score, hash, 0, move(action), nidv);
                    history.push_back({nidv, -1, 1, score, hash,
                                       move(as), state.get_state_info(), ok ? 0 : 1});
                } else {
                    candidates.push(score, hash, 0, move(action));
                }
            }
        }

        if (found_finished) {
            if constexpr (record_history) dump_history_json(history_file, INF, history, snapshots);
            if (verbose) {
                beam_log::on_solution_found(cerr, 1, best_finished_score);
                beam_log::width_trace(cerr, param.width_hist);
                beam_log::end_banner(cerr, "solution found", 1, param.max_turn,
                                     beam_timer.elapsed(), param.ave_width(),
                                     best_finished_score, true, (int)best_finished_path.size());
            }
            return best_finished_path;
        }

        cand.clear();
        for (int i = 0; i < (int)candidates.size(); ++i) {
            cand.push_back(move(candidates.next_beam[i]));
        }
        if constexpr (record_history) record_turn_survivors(1);
        leaf = {0};
        turns_done = 1;

        for (int turn = 1; turn < param.max_turn; ++turn) {
            double now_time = beam_timer.elapsed();

            next_tour.clear();
            next_leaf.clear();
            w = param.get_beam_width(param.max_turn - turn, cand.size(), param.time_limit - beam_timer.elapsed());
            candidates.reset(turn, w, param.clear_hash_every_turn);

            int li = leaf.size() - 1;
            int f = 0;

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
                    state.rollback(trace[turn - 1 + f - k]);
                }

                next_tour.insert(next_tour.end(), trace.begin() + (turn - lca_dist), trace.begin() + (turn + 1));
                f = 1;

                trace[turn] = c.action;
                copy_tour_path(c.parent_leaf, li, trace.begin() + turn);

                for (int k = turn - lca_dist; k <= turn; ++k) {
                    state.apply_op(trace[k]);
                }

                actions.clear();
                state.get_actions(actions, turn, c.action, candidates.threshold());
                int now_leaf_idx = next_leaf.size();

                for (Action &action : actions) {
                    auto [score, hash, finished] = state.try_op(action, candidates.threshold());
                    if (score >= INF) {
                        if constexpr (record_history) {
                            history.push_back({node_id_counter++, c.node_id, turn + 1, score, hash,
                                               action.to_string(), state.get_state_info(), 2});
                        }
                        continue;
                    }
                    if (finished) {
                        if (!found_finished || score < best_finished_score) {
                            found_finished = true;
                            best_finished_score = score;
                            best_finished_path.assign(trace.begin() + 1, trace.begin() + turn + 1);
                            best_finished_path.push_back(action);
                        }
                        if constexpr (record_history) {
                            history.push_back({node_id_counter++, c.node_id, turn + 1, score, hash,
                                               action.to_string(), state.get_state_info(), 0});
                        }
                    } else {
                        if constexpr (record_history) {
                            int nidv = node_id_counter++;
                            string as = action.to_string();
                            bool ok = candidates.push(score, hash, now_leaf_idx, action, nidv);
                            history.push_back({nidv, c.node_id, turn + 1, score, hash,
                                               move(as), state.get_state_info(), ok ? 0 : 1});
                        } else {
                            candidates.push(score, hash, now_leaf_idx, action);
                        }
                    }
                }
                next_leaf.push_back(next_tour.size());
                li = c.parent_leaf;
            }

            if (found_finished) {
                if constexpr (record_history) dump_history_json(history_file, INF, history, snapshots);
                if (verbose) {
                    beam_log::on_solution_found(cerr, turn + 1, best_finished_score);
                    beam_log::width_trace(cerr, param.width_hist);
                    beam_log::end_banner(cerr, "solution found", turn + 1, param.max_turn,
                                         beam_timer.elapsed(), param.ave_width(),
                                         best_finished_score, true, (int)best_finished_path.size());
                }
                return best_finished_path;
            }

            if (candidates.size() == 0) {
                beam_log::on_no_candidates(cerr, turn);
                assert(candidates.size() > 0);
            }

            if (verbose) {
                BeamCandidate<ScoreType, Action> bests = candidates.get_best();
                beam_log::turn_line(cerr, turn + 1, param.max_turn, now_time,
                                    w, (int)tour.size(), (int)candidates.size(), bests.score);
            }

            if constexpr (record_history) record_turn_survivors(turn + 1);

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
            turns_done = turn + 1;
        }

        int best_idx = -1;
        ScoreType best_score = INF;
        for (int i = 0; i < (int)cand.size(); ++i) {
            if (best_idx == -1 || cand[i].score < best_score) {
                best_score = cand[i].score;
                best_idx = i;
            }
        }

        vector<Action> ret(trace.begin() + 1, trace.begin() + param.max_turn);
        int len = ret.size();
        copy_tour_path(cand[best_idx].parent_leaf, (int)leaf.size() - 1, ret.begin() + len);
        ret.push_back(cand[best_idx].action);

        if constexpr (record_history) dump_history_json(history_file, INF, history, snapshots);
        if (verbose) {
            beam_log::on_max_turn(cerr);
            beam_log::width_trace(cerr, param.width_hist);
            beam_log::end_banner(cerr, "max_turn reached", turns_done, param.max_turn,
                                 beam_timer.elapsed(), param.ave_width(),
                                 best_score, true, (int)ret.size());
        }
        return ret;
    }
};
} // namespace flying_squirrel
