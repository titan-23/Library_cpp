/// https://github.com/titan-23/Library_cpp/blob/main/titan_cpplib/ahc/beam_search/beam_search.cpp
#pragma once
#include <bits/stdc++.h>
#include "titan_cpplib/others/print.cpp"
#include "titan_cpplib/alg/random.cpp"
#include "titan_cpplib/ahc/timer.cpp"
#include "titan_cpplib/ds/hash_dict.cpp"
#include "titan_cpplib/ahc/beam_search/beam_param.cpp"
#include "titan_cpplib/ahc/beam_search/beam_result.cpp"
#include "titan_cpplib/ahc/beam_search/beam_log.cpp"
#include "titan_cpplib/ahc/beam_search/beam_history.cpp"
#include "titan_cpplib/ahc/beam_search/candidates.cpp"

using namespace std;

namespace flying_squirrel {

/// @brief 木上の状態差分を利用して候補を展開するビームサーチ
template<typename ScoreType, typename HashType, class Action, class State, ScoreType INF, bool record_history=false>
class BeamSearchWithTree {
private:
    using Result = BeamResult<ScoreType, Action, State>;

    titan23::Random rnd;
    titan23::Timer beam_timer;
    Action DAMMY_ACTION;

    bool found_finished;
    ScoreType best_finished_score;
    vector<Action> best_finished_path;

    int node_id_counter;
    vector<HistoryNode<ScoreType, HashType>> history;
    vector<TurnSnapshot> snapshots;

    // 1ターン内の try_op 呼び出し回数
    int explored_per_turn;

    using ActionId = uint64_t;
    static constexpr int SLOT_BITS = 24;
    static constexpr ActionId SLOT_MASK = (ActionId(1) << SLOT_BITS) - 1;
    vector<vector<Action>> gblock;
    static inline ActionId make_id(int gen, int slot) {
        return (ActionId(gen) << SLOT_BITS) | ActionId(slot);
    }
    inline Action& act(ActionId id) {
        return gblock[(size_t)(id >> SLOT_BITS)][(size_t)(id & SLOT_MASK)];
    }

    struct CandIdx {
        int parent_leaf;
        ScoreType score;
        ActionId action_id;
        int node_id = -1;
    };

    Candidates<ScoreType, HashType, Action, State, INF, record_history> candidates;
    vector<ActionId> trace;
    vector<ActionId> tour;
    vector<int> leaf;
    vector<CandIdx> cand;

    vector<ActionId> next_tour;
    vector<int> next_leaf;
    vector<Action> actions;

    int freed_to;                     // 解放済みの最大深さ
                                      // result_prefix.size() と一致する
    vector<Action> result_prefix;     // 確定した深さ 1..freed_to の Action
    vector<vector<Action>> slab_pool; // 解放した世代ブロックの再利用プール

    /// @brief 確定した接頭辞を退避し、参照されなくなった世代ブロックを再利用プールへ移す
    /// @param L 次世代以降で参照される最小の深さ
    void confirm_and_free(int L) {
        while (freed_to + 1 < L) {
            int d = freed_to + 1;
            result_prefix.push_back(act(trace[d]));
            gblock[d].clear();
            slab_pool.push_back(move(gblock[d]));
            freed_to = d;
        }
    }

    /// @brief 生存候補の Action を世代ブロックへ移し、参照用の候補列を構築する
    void finalize_generation(int gen) {
        int sz = (int)candidates.size();
        if ((int)gblock.size() <= gen) gblock.resize(gen + 1);
        if (!slab_pool.empty()) {
            gblock[gen] = move(slab_pool.back());
            slab_pool.pop_back();
        }
        gblock[gen].resize(sz);
        cand.clear();
        cand.reserve(sz);
        for (int i = 0; i < sz; ++i) {
            gblock[gen][i] = move(candidates.next_beam[i].action);
            cand.push_back({candidates.next_beam[i].parent_leaf, candidates.next_beam[i].score, make_id(gen, i),
                            candidates.next_beam[i].node_id});
        }
    }

    /// @brief ActionId の範囲を Action の列へ展開する
    template<class It>
    void materialize(vector<Action>& dst, It first, It last) {
        for (It it = first; it != last; ++it) dst.push_back(act(*it));
    }

    /// @brief 確定した接頭辞と未確定の trace から終了経路を構築する
    void build_best_path(int upto) {
        best_finished_path = result_prefix;
        for (int k = freed_to + 1; k <= upto; ++k) best_finished_path.push_back(act(trace[k]));
    }

    /// @brief 探索状態をルートへ戻し、返却経路を適用した最終状態を構築する
    template<bool materialize_final_state>
    unique_ptr<State> build_final_state(State& state, const vector<Action>& result_actions, int current_depth) {
        if constexpr (materialize_final_state) {
            for (int d = current_depth; d > freed_to; --d) {
                state.rollback(act(trace[d]));
            }
            for (auto it = result_prefix.rbegin(); it != result_prefix.rend(); ++it) {
                state.rollback(*it);
            }
            return make_final_state<true>(state, result_actions);
        } else {
            (void)state;
            (void)result_actions;
            (void)current_depth;
            return nullptr;
        }
    }

    /// @brief 列挙された Action の評価、終了判定、候補登録を行う
    struct Submitter {
        BeamSearchWithTree &bs;
        State &st;
        int parent_leaf, parent_node_id, turn;

        /// @brief 現在の枝刈り閾値を返す
        inline ScoreType threshold() const { return bs.candidates.threshold(); }

        inline void operator()(Action &a) {
            ++bs.explored_per_turn;
            ScoreType th = bs.candidates.threshold();
            auto [score, hash, finished] = st.try_op(a, th);
            if (score >= INF) {
                if constexpr (record_history) {
                    bs.history.push_back({bs.node_id_counter++, parent_node_id, turn + 1,
                                          score, hash, a.to_string(), st.get_state_info(), 2});
                }
                return;
            }
            if (finished) {
                if (!bs.found_finished || score < bs.best_finished_score) {
                    bs.found_finished = true;
                    bs.best_finished_score = score;
                    bs.build_best_path(turn);
                    bs.best_finished_path.push_back(a);
                }
                if constexpr (record_history) {
                    bs.history.push_back({bs.node_id_counter++, parent_node_id, turn + 1,
                                          score, hash, a.to_string(), st.get_state_info(), 0});
                }
                return;
            }
            if constexpr (record_history) {
                int nidv = bs.node_id_counter++;
                string as = a.to_string();
                bool ok = bs.candidates.push(score, hash, parent_leaf, a, nidv);
                bs.history.push_back({nidv, parent_node_id, turn + 1,
                                      score, hash, move(as), st.get_state_info(), ok ? 0 : 1});
            } else {
                bs.candidates.push(score, hash, parent_leaf, a);
            }
        }
    };

    /// @brief 探索ごとの作業領域と記録を初期化する
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
        gblock.clear();
        slab_pool.clear();
        freed_to = 0;
        result_prefix.clear();
        explored_per_turn = 0;
        if constexpr (record_history) {
            node_id_counter = 0;
            history.clear();
            snapshots.clear();
        }
    }

    /// @brief 生存候補を記録し、脱落した履歴ノードの状態を更新する
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

    /// @brief 親葉までの差分経路を tour から復元する
    /// @param dst_end 復元先となる親経路の終端
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
    /// @brief ビームサーチを実行する
    /// @tparam materialize_final_state 最終状態を構築する場合は true
    /// @param param 探索ターン数とビーム幅の設定
    /// @param verbose ログを出力する場合は true
    /// @param history_file record_history が true のときに履歴を JSON で出力するファイル名
    /// @return 探索結果
    template<bool materialize_final_state=true>
    Result search(BeamParam &param, const bool verbose=false, const string& history_file = "") {
        if (param.max_turn <= 0 || param.beam_width <= 0) {
            return {{}, INF, 0, 0.0, BeamStatus::InvalidParameter, nullptr};
        }
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
        candidates.reset(0, w, param.clear_hash_every_turn, param.hash_window_turns);

        if constexpr (requires(Submitter &e) { state.enumerate_actions(0, DAMMY_ACTION, e); }) {
            Submitter submit{*this, state, 0, -1, 0};
            state.enumerate_actions(0, DAMMY_ACTION, submit);
        } else {
            actions.clear();
            state.enumerate_actions(actions, 0, DAMMY_ACTION, candidates.threshold());
            explored_per_turn += (int)actions.size();
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
                        history.push_back({nidv, -1, 1, score, hash, move(as), state.get_state_info(), ok ? 0 : 1});
                    } else {
                        candidates.push(score, hash, 0, move(action));
                    }
                }
            }
        }

        if (found_finished) {
            double elapsed_ms = beam_timer.elapsed();
            if constexpr (record_history) dump_history_json(history_file, INF, history, snapshots);
            if (verbose) {
                beam_log::on_solution_found(cerr, 1, best_finished_score);
                beam_log::width_trace(cerr, param.width_hist);
                beam_log::end_banner(cerr, "solution found", 1, param.max_turn, elapsed_ms, param.ave_width(),
                                     best_finished_score, true, (int)best_finished_path.size());
            }
            unique_ptr<State> fs;
            if constexpr (materialize_final_state) fs = build_final_state<true>(state, best_finished_path, 0);
            return {move(best_finished_path), best_finished_score, 1, elapsed_ms, BeamStatus::Finished, move(fs)};
        }

        if (candidates.size() == 0) {
            beam_log::on_no_candidates(cerr, 0);
            double elapsed_ms = beam_timer.elapsed();
            return {{}, INF, 0, elapsed_ms, BeamStatus::NoCandidates, nullptr};
        }

        finalize_generation(1);
        if constexpr (record_history) record_turn_survivors(1);
        leaf = {0};
        turns_done = 1;

        for (int turn = 1; turn < param.max_turn; ++turn) {
            double now_time = beam_timer.elapsed();

            next_tour.clear();
            next_leaf.clear();
            w = param.get_beam_width(param.max_turn - turn, cand.size(), param.time_limit - beam_timer.elapsed());
            candidates.reset(turn, w, param.clear_hash_every_turn, param.hash_window_turns);
            explored_per_turn = 0;

            int li = leaf.size() - 1;
            int f = 0;
            int max_lca_dist = 0;

            if (!cand.empty()) {
                trace[turn] = cand.back().action_id;
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
                if (lca_dist > max_lca_dist) max_lca_dist = lca_dist;

                for (int k = 0; k < lca_dist + f; ++k) {
                    state.rollback(act(trace[turn - 1 + f - k]));
                }

                next_tour.insert(next_tour.end(), trace.begin() + (turn - lca_dist), trace.begin() + (turn + 1));
                f = 1;

                trace[turn] = c.action_id;
                copy_tour_path(c.parent_leaf, li, trace.begin() + turn);

                for (int k = turn - lca_dist; k <= turn; ++k) {
                    state.apply_op(act(trace[k]));
                }

                int now_leaf_idx = next_leaf.size();
                if constexpr (requires(Submitter &e) { state.enumerate_actions(turn, DAMMY_ACTION, e); }) {
                    Submitter submit{*this, state, now_leaf_idx, c.node_id, turn};
                    state.enumerate_actions(turn, act(c.action_id), submit);
                } else {
                    actions.clear();
                    state.enumerate_actions(actions, turn, act(c.action_id), candidates.threshold());
                    explored_per_turn += (int)actions.size();
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
                                build_best_path(turn);
                                if constexpr (record_history) {
                                    history.push_back({node_id_counter++, c.node_id, turn + 1, score, hash,
                                                       action.to_string(), state.get_state_info(), 0});
                                }
                                best_finished_path.push_back(move(action));
                            } else if constexpr (record_history) {
                                history.push_back({node_id_counter++, c.node_id, turn + 1, score, hash,
                                                   action.to_string(), state.get_state_info(), 0});
                            }
                        } else {
                            if constexpr (record_history) {
                                int nidv = node_id_counter++;
                                string as = action.to_string();
                                bool ok = candidates.push(score, hash, now_leaf_idx, move(action), nidv);
                                history.push_back({nidv, c.node_id, turn + 1, score, hash,
                                                   move(as), state.get_state_info(), ok ? 0 : 1});
                            } else {
                                candidates.push(score, hash, now_leaf_idx, move(action));
                            }
                        }
                    }
                }
                next_leaf.push_back(next_tour.size());
                li = c.parent_leaf;
            }

            if (found_finished) {
                double elapsed_ms = beam_timer.elapsed();
                if constexpr (record_history) dump_history_json(history_file, INF, history, snapshots);
                if (verbose) {
                    beam_log::on_solution_found(cerr, turn + 1, best_finished_score);
                    beam_log::width_trace(cerr, param.width_hist);
                    beam_log::end_banner(cerr, "solution found", turn + 1, param.max_turn, elapsed_ms,
                                         param.ave_width(), best_finished_score, true, (int)best_finished_path.size());
                }
                unique_ptr<State> fs;
                if constexpr (materialize_final_state) fs = build_final_state<true>(state, best_finished_path, turn);
                return {move(best_finished_path), best_finished_score, turn + 1, elapsed_ms,
                        BeamStatus::Finished, move(fs)};
            }

            if (candidates.size() == 0) {
                beam_log::on_no_candidates(cerr, turn);
                double elapsed_ms = beam_timer.elapsed();
                return {{}, INF, turn, elapsed_ms, BeamStatus::NoCandidates, nullptr};
            }

            if (verbose) {
                BeamCandidate<ScoreType, Action> bests = candidates.get_best();
                beam_log::turn_line(cerr, turn + 1, param.max_turn, now_time, w, (int)tour.size(),
                                    (int)candidates.size(), explored_per_turn, bests.score);
            }

            if constexpr (record_history) record_turn_survivors(turn + 1);

            confirm_and_free(turn - max_lca_dist);

            swap(tour, next_tour);
            swap(leaf, next_leaf);

            finalize_generation(turn + 1);
            sort(cand.begin(), cand.end(), [](const CandIdx& a, const CandIdx& b) {
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

        vector<ActionId> ridx(trace.begin() + 1, trace.begin() + param.max_turn);
        copy_tour_path(cand[best_idx].parent_leaf, (int)leaf.size() - 1, ridx.end());
        vector<Action> ret = result_prefix;
        ret.reserve(result_prefix.size() + (ridx.size() - freed_to) + 1);
        materialize(ret, ridx.begin() + freed_to, ridx.end());
        ret.push_back(act(cand[best_idx].action_id));

        double elapsed_ms = beam_timer.elapsed();
        if constexpr (record_history) dump_history_json(history_file, INF, history, snapshots);
        if (verbose) {
            beam_log::on_max_turn(cerr);
            beam_log::width_trace(cerr, param.width_hist);
            beam_log::end_banner(cerr, "max_turn reached", turns_done, param.max_turn, elapsed_ms,
                                 param.ave_width(), best_score, true, (int)ret.size());
        }
        unique_ptr<State> fs;
        if constexpr (materialize_final_state) fs = build_final_state<true>(state, ret, param.max_turn - 1);
        return {move(ret), best_score, turns_done, elapsed_ms, BeamStatus::MaxTurnReached, move(fs)};
    }
};
} // namespace flying_squirrel
