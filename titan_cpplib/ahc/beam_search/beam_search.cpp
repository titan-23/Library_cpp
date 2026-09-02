/// https://github.com/titan-23/Library_cpp/blob/main/titan_cpplib/ahc/beam_search/beam_search.cpp
#pragma once
#include <bits/stdc++.h>
#include "titan_cpplib/others/print.cpp"
#include "titan_cpplib/ahc/timer.cpp"
#include "titan_cpplib/ds/hash_dict.cpp"
#include "titan_cpplib/ahc/beam_search/beam_param.cpp"
#include "titan_cpplib/ahc/beam_search/beam_result.cpp"
#include "titan_cpplib/ahc/beam_search/beam_log.cpp"
#include "titan_cpplib/ahc/beam_search/beam_history.cpp"
#include "titan_cpplib/ahc/beam_search/candidates.cpp"

using namespace std;

namespace flying_squirrel {

/// @brief 木上の状態差分と深さ別スロットを利用して候補を展開するビームサーチ
template<typename ScoreType, typename HashType, class Action, class State, ScoreType INF, bool record_history=false>
class BeamSearchWithTree {
private:
    using Result = BeamResult<ScoreType, Action, State>;
    using Slot = uint32_t;

    titan23::Timer beam_timer;
    Action DAMMY_ACTION;

    bool found_finished;
    ScoreType best_finished_score;
    vector<Action> best_finished_path;

    int node_id_counter;
    vector<HistoryNode<ScoreType, HashType>> history;
    vector<TurnSnapshot> snapshots;
    int explored_per_turn;

    vector<vector<Action>> gblock;

    inline Action& act(int depth, Slot slot) {
        return gblock[(size_t)depth][(size_t)slot];
    }

    struct CandIdx {
        int parent_leaf;
        ScoreType score;
        Slot action_slot;
        int node_id = -1;
    };

    Candidates<ScoreType, HashType, Action, State, INF, record_history> candidates;
    vector<Slot> trace;
    vector<Slot> tour;
    vector<int> leaf;
    vector<CandIdx> cand;

    vector<Slot> next_tour;
    vector<int> next_leaf;
    vector<Action> actions;
    size_t logical_tour_size;

    int freed_to;
    vector<Action> result_prefix;
    vector<vector<Action>> slab_pool;

    /// @brief 確定した接頭辞を退避し、参照されなくなった世代ブロックを再利用プールへ移す
    /// @param limit 次世代以降で参照される最小の深さ
    void confirm_and_free(int limit) {
        while (freed_to + 1 < limit) {
            int depth = freed_to + 1;
            result_prefix.push_back(move(act(depth, trace[depth])));
            gblock[depth].clear();
            slab_pool.push_back(move(gblock[depth]));
            freed_to = depth;
        }
    }

    /// @brief 生存候補の Action を世代ブロックへ直接ムーブ構築し、参照用の候補列を構築する
    void finalize_generation(int gen) {
        int sz = (int)candidates.size();
        assert(sz >= 0 && (uint64_t)sz <= numeric_limits<Slot>::max());
        if ((int)gblock.size() <= gen) gblock.resize(gen + 1);
        if (!slab_pool.empty()) {
            gblock[gen] = move(slab_pool.back());
            slab_pool.pop_back();
        }
        vector<Action>& block = gblock[gen];
        block.clear();
        block.reserve(sz);
        cand.clear();
        cand.reserve(sz);
        for (int i = 0; i < sz; ++i) {
            block.emplace_back(move(candidates.next_beam[i].action));
            cand.push_back({candidates.next_beam[i].parent_leaf, candidates.next_beam[i].score,
                            (Slot)i, candidates.next_beam[i].node_id});
        }
    }

    /// @brief 深さが既知のスロット列を Action 列へ展開する
    template<class It>
    void materialize(vector<Action>& dst, int depth, It first, It last) {
        for (It it = first; it != last; ++it, ++depth) dst.push_back(act(depth, *it));
    }

    /// @brief 確定した接頭辞と未確定の trace から終了経路を構築する
    void build_best_path(int upto) {
        best_finished_path = result_prefix;
        for (int depth = freed_to + 1; depth <= upto; ++depth) {
            best_finished_path.push_back(act(depth, trace[depth]));
        }
    }

    /// @brief 探索状態をルートへ戻し、返却経路を適用した最終状態を構築する
    template<bool materialize_final_state>
    unique_ptr<State> build_final_state(State& state, const vector<Action>& result_actions, int current_depth) {
        if constexpr (materialize_final_state) {
            for (int depth = current_depth; depth > freed_to; --depth) {
                state.rollback(act(depth, trace[depth]));
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

        inline void operator()(Action &action) {
            ++bs.explored_per_turn;
            ScoreType threshold = bs.candidates.threshold();
            auto [score, hash, finished] = st.try_op(action, threshold);
            if (score >= INF) {
                if constexpr (record_history) {
                    bs.history.push_back({bs.node_id_counter++, parent_node_id, turn + 1, score, hash,
                                          action.to_string(), st.get_state_info(), 2});
                }
                return;
            }
            if (finished) {
                if (!bs.found_finished || score < bs.best_finished_score) {
                    bs.found_finished = true;
                    bs.best_finished_score = score;
                    bs.build_best_path(turn);
                    bs.best_finished_path.push_back(action);
                }
                if constexpr (record_history) {
                    bs.history.push_back({bs.node_id_counter++, parent_node_id, turn + 1, score, hash,
                                          action.to_string(), st.get_state_info(), 0});
                }
                return;
            }
            if constexpr (record_history) {
                int node_id = bs.node_id_counter++;
                string action_string = action.to_string();
                bool accepted = bs.candidates.push_lazy(score, hash, parent_leaf, [&] { return action; }, node_id);
                bs.history.push_back({node_id, parent_node_id, turn + 1, score, hash, move(action_string),
                                      st.get_state_info(), accepted ? 0 : 1});
            } else {
                bs.candidates.push_lazy(score, hash, parent_leaf, [&] { return action; });
            }
        }
    };

    /// @brief 探索ごとの作業領域と記録を初期化する
    void init_bs() {
        beam_timer.reset();
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
        logical_tour_size = 0;
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
        int progress = 0;
        for (int k = parent_leaf; k < leaf_end; ++k) {
            int begin = leaf[k];
            int end = leaf[k + 1];
            int rank = end - begin;
            if (progress < rank) {
                int copy_len = rank - progress;
                copy(tour.begin() + begin, tour.begin() + begin + copy_len, dst_end - rank);
                progress = rank;
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

        int width = param.get_beam_width(param.max_turn, 0, param.time_limit);
        candidates.reset(0, width, true, param.hash_window_turns);

        if constexpr (requires(Submitter &submitter) { state.enumerate_actions(0, DAMMY_ACTION, submitter); }) {
            Submitter submitter{*this, state, 0, -1, 0};
            state.enumerate_actions(0, DAMMY_ACTION, submitter);
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
                        int node_id = node_id_counter++;
                        string action_string = action.to_string();
                        bool accepted = candidates.push_lazy(score, hash, 0, [&] { return move(action); }, node_id);
                        history.push_back({node_id, -1, 1, score, hash, move(action_string), state.get_state_info(),
                                           accepted ? 0 : 1});
                    } else {
                        candidates.push_lazy(score, hash, 0, [&] { return move(action); });
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
            unique_ptr<State> final_state;
            if constexpr (materialize_final_state) {
                final_state = build_final_state<true>(state, best_finished_path, 0);
            }
            return {move(best_finished_path), best_finished_score, 1, elapsed_ms, BeamStatus::Finished,
                    move(final_state)};
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
            size_t omitted_prefix_size = 0;
            width = param.get_beam_width(param.max_turn - turn, cand.size(), param.time_limit - now_time);
            candidates.reset(turn, width, param.clear_hash_every_turn, param.hash_window_turns);
            explored_per_turn = 0;

            int leaf_index = (int)leaf.size() - 1;
            int after_first = 0;
            int max_internal_lca_dist = 0;

            if (!cand.empty()) trace[turn] = cand.back().action_slot;

            for (int i = (int)cand.size() - 1; i >= 0; --i) {
                const CandIdx &current = cand[i];

                int lca_dist = 0;
                int current_lca = leaf[leaf_index];
                for (int k = leaf_index - 1; k >= current.parent_leaf; --k) {
                    if (current_lca - leaf[k] > lca_dist) lca_dist = current_lca - leaf[k];
                    current_lca = leaf[k];
                }
                if (after_first && lca_dist > max_internal_lca_dist) {
                    max_internal_lca_dist = lca_dist;
                }

                for (int k = 0; k < lca_dist + after_first; ++k) {
                    int depth = turn - 1 + after_first - k;
                    state.rollback(act(depth, trace[depth]));
                }

                if (!after_first) omitted_prefix_size = (size_t)lca_dist + 1;
                if (after_first) {
                    next_tour.insert(next_tour.end(), trace.begin() + (turn - lca_dist),
                                     trace.begin() + (turn + 1));
                }
                after_first = 1;

                trace[turn] = current.action_slot;
                copy_tour_path(current.parent_leaf, leaf_index, trace.begin() + turn);

                for (int depth = turn - lca_dist; depth <= turn; ++depth) {
                    state.apply_op(act(depth, trace[depth]));
                }

                int current_leaf_index = (int)next_leaf.size();
                if constexpr (requires(Submitter &submitter) {
                                  state.enumerate_actions(turn, DAMMY_ACTION, submitter);
                              }) {
                    Submitter submitter{*this, state, current_leaf_index, current.node_id, turn};
                    state.enumerate_actions(turn, act(turn, current.action_slot), submitter);
                } else {
                    actions.clear();
                    state.enumerate_actions(actions, turn, act(turn, current.action_slot), candidates.threshold());
                    explored_per_turn += (int)actions.size();
                    for (Action &action : actions) {
                        auto [score, hash, finished] = state.try_op(action, candidates.threshold());
                        if (score >= INF) {
                            if constexpr (record_history) {
                                history.push_back({node_id_counter++, current.node_id, turn + 1, score, hash,
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
                                    history.push_back({node_id_counter++, current.node_id, turn + 1, score, hash,
                                                       action.to_string(), state.get_state_info(), 0});
                                }
                                best_finished_path.push_back(move(action));
                            } else if constexpr (record_history) {
                                history.push_back({node_id_counter++, current.node_id, turn + 1, score, hash,
                                                   action.to_string(), state.get_state_info(), 0});
                            }
                        } else {
                            if constexpr (record_history) {
                                int node_id = node_id_counter++;
                                string action_string = action.to_string();
                                bool accepted = candidates.push_lazy(score, hash, current_leaf_index,
                                                                     [&] { return move(action); }, node_id);
                                history.push_back({node_id, current.node_id, turn + 1, score, hash,
                                                   move(action_string), state.get_state_info(), accepted ? 0 : 1});
                            } else {
                                candidates.push_lazy(score, hash, current_leaf_index,
                                                     [&] { return move(action); });
                            }
                        }
                    }
                }
                next_leaf.push_back((int)next_tour.size());
                leaf_index = current.parent_leaf;
            }

            if (found_finished) {
                double elapsed_ms = beam_timer.elapsed();
                if constexpr (record_history) dump_history_json(history_file, INF, history, snapshots);
                if (verbose) {
                    beam_log::on_solution_found(cerr, turn + 1, best_finished_score);
                    beam_log::width_trace(cerr, param.width_hist);
                    beam_log::end_banner(cerr, "solution found", turn + 1, param.max_turn, elapsed_ms,
                                         param.ave_width(), best_finished_score, true,
                                         (int)best_finished_path.size());
                }
                unique_ptr<State> final_state;
                if constexpr (materialize_final_state) {
                    final_state = build_final_state<true>(state, best_finished_path, turn);
                }
                return {move(best_finished_path), best_finished_score, turn + 1, elapsed_ms,
                        BeamStatus::Finished, move(final_state)};
            }

            if (candidates.size() == 0) {
                beam_log::on_no_candidates(cerr, turn);
                double elapsed_ms = beam_timer.elapsed();
                return {{}, INF, turn, elapsed_ms, BeamStatus::NoCandidates, nullptr};
            }

            if (verbose) {
                BeamCandidate<ScoreType, Action> best = candidates.get_best();
                beam_log::turn_line(cerr, turn + 1, param.max_turn, now_time, width,
                                    (int)logical_tour_size, (int)candidates.size(), explored_per_turn, best.score);
            }

            if constexpr (record_history) record_turn_survivors(turn + 1);

            if (cand.size() == 1) {
                confirm_and_free(turn + 1);
            } else {
                confirm_and_free(turn - max_internal_lca_dist);
            }

            swap(tour, next_tour);
            swap(leaf, next_leaf);
            logical_tour_size = tour.size() + omitted_prefix_size;

            finalize_generation(turn + 1);
            sort(cand.begin(), cand.end(), [](const CandIdx& left, const CandIdx& right) {
                if (left.parent_leaf != right.parent_leaf) return left.parent_leaf < right.parent_leaf;
                return left.score < right.score;
            });

            param.timestamp((int)logical_tour_size, candidates.size(), beam_timer.elapsed() - now_time);
            turns_done = turn + 1;
        }

        int best_index = -1;
        ScoreType best_score = INF;
        for (int i = 0; i < (int)cand.size(); ++i) {
            if (best_index == -1 || cand[i].score < best_score) {
                best_score = cand[i].score;
                best_index = i;
            }
        }

        vector<Slot> result_slots(trace.begin() + 1, trace.begin() + param.max_turn);
        copy_tour_path(cand[best_index].parent_leaf, (int)leaf.size() - 1, result_slots.end());
        vector<Action> result = result_prefix;
        result.reserve(result_prefix.size() + (result_slots.size() - freed_to) + 1);
        materialize(result, freed_to + 1, result_slots.begin() + freed_to, result_slots.end());
        result.push_back(act(param.max_turn, cand[best_index].action_slot));

        double elapsed_ms = beam_timer.elapsed();
        if constexpr (record_history) dump_history_json(history_file, INF, history, snapshots);
        if (verbose) {
            beam_log::on_max_turn(cerr);
            beam_log::width_trace(cerr, param.width_hist);
            beam_log::end_banner(cerr, "max_turn reached", turns_done, param.max_turn, elapsed_ms,
                                 param.ave_width(), best_score, true, (int)result.size());
        }
        unique_ptr<State> final_state;
        if constexpr (materialize_final_state) {
            final_state = build_final_state<true>(state, result, param.max_turn - 1);
        }
        return {move(result), best_score, turns_done, elapsed_ms, BeamStatus::MaxTurnReached, move(final_state)};
    }
};
}
