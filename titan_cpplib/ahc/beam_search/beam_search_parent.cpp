#pragma once
#include <bits/stdc++.h>
#include "titan_cpplib/others/print.cpp"
#include "titan_cpplib/ahc/timer.cpp"
#include "titan_cpplib/ahc/beam_search/beam_param.cpp"
#include "titan_cpplib/ahc/beam_search/beam_result.cpp"
#include "titan_cpplib/ahc/beam_search/beam_log.cpp"
#include "titan_cpplib/ahc/beam_search/beam_history.cpp"
#include "titan_cpplib/ahc/beam_search/candidates.cpp"

using namespace std;

namespace flying_squirrel {

/// @brief direct parent と隣接葉 LCP で候補間の状態差分を復元するビームサーチ
template<typename ScoreType, typename HashType, class Action, class State, ScoreType INF, bool record_history=false>
class BeamSearchWithTree {
private:
    using Result = BeamResult<ScoreType, Action, State>;
    using Slot = uint32_t;

    static constexpr Slot BAD_SLOT = numeric_limits<Slot>::max();

    titan23::Timer beam_timer;
    Action DAMMY_ACTION;

    bool found_finished;
    ScoreType best_finished_score;
    vector<Action> best_finished_path;

    int node_id_counter;
    vector<HistoryNode<ScoreType, HashType>> history;
    vector<TurnSnapshot> snapshots;
    int explored_per_turn;

    vector<vector<Action>> action_block;
    vector<vector<Slot>> parent_block;
    vector<vector<Action>> action_slab_pool;
    vector<vector<Slot>> parent_slab_pool;

    struct CandIdx {
        int parent_leaf;
        ScoreType score;
        Slot action_slot;
        int node_id = -1;
    };

    Candidates<ScoreType, HashType, Action, State, INF, record_history> candidates;
    vector<CandIdx> cand;
    vector<Action> actions;

    vector<Slot> trace;
    vector<Slot> frontier_slot;
    vector<uint32_t> adjacent_lcp;
    uint32_t entry_lcp;

    vector<Slot> next_frontier_slot;
    vector<uint32_t> next_adjacent_lcp;
    uint32_t next_entry_lcp;
    size_t compat_tour_size;

    int freed_to;
    vector<Action> result_prefix;

    inline Action& act(int depth, Slot slot) {
        return action_block[(size_t)depth][(size_t)slot];
    }

    inline Slot parent(int depth, Slot slot) const {
        return parent_block[(size_t)depth][(size_t)slot];
    }

    /// @brief 確定した接頭辞を退避して不要な世代ブロックを再利用プールへ移す
    void confirm_and_free(int keep_from) {
        while (freed_to + 1 < keep_from) {
            int depth = freed_to + 1;
            result_prefix.push_back(move(act(depth, trace[depth])));

            action_block[depth].clear();
            action_slab_pool.push_back(move(action_block[depth]));
            parent_block[depth].clear();
            parent_slab_pool.push_back(move(parent_block[depth]));
            freed_to = depth;
        }
    }

    /// @brief 最終候補の Action と親 slot を世代別 SoA へ格納する
    void finalize_generation(int depth) {
        int sz = candidates.size();
        if ((int)action_block.size() <= depth) action_block.resize(depth + 1);
        if ((int)parent_block.size() <= depth) parent_block.resize(depth + 1);

        if (!action_slab_pool.empty()) {
            action_block[depth] = move(action_slab_pool.back());
            action_slab_pool.pop_back();
        }
        if (!parent_slab_pool.empty()) {
            parent_block[depth] = move(parent_slab_pool.back());
            parent_slab_pool.pop_back();
        }

        action_block[depth].clear();
        action_block[depth].reserve(sz);
        parent_block[depth].clear();
        parent_block[depth].reserve(sz);
        cand.clear();
        cand.reserve(sz);

        for (int i = 0; i < sz; ++i) {
            auto &src = candidates.next_beam[i];
            action_block[depth].emplace_back(move(src.action));

            Slot p = BAD_SLOT;
            if (depth > 1) {
                p = frontier_slot[src.parent_leaf];
            }
            parent_block[depth].push_back(p);
            cand.push_back({src.parent_leaf, src.score, (Slot)i, src.node_id});
        }
    }

    /// @brief root 直下の候補から最初の実展開順と LCP を作る
    void build_root_frontier() {
        int sz = (int)cand.size();
        frontier_slot.resize(sz);
        for (int j = 0; j < sz; ++j) frontier_slot[j] = cand[sz - 1 - j].action_slot;
        adjacent_lcp.assign(max(0, sz - 1), 0);
        entry_lcp = 0;
    }

    /// @brief sort 後の候補から次世代の実展開順と隣接葉 LCP を構築する
    void build_next_frontier(int depth) {
        int old_size = (int)frontier_slot.size();
        int next_size = (int)cand.size();

        next_frontier_slot.clear();
        next_frontier_slot.reserve(next_size);
        next_adjacent_lcp.clear();
        next_adjacent_lcp.reserve(max(0, next_size - 1));

        int cursor = old_size - 1;
        auto descend_to = [&](int parent_ordinal) {
            uint32_t h = (uint32_t)depth;
            while (cursor > parent_ordinal) {
                --cursor;
                h = min(h, adjacent_lcp[cursor]);
            }
            return h;
        };

        int previous_parent = -1;
        for (int j = 0; j < next_size; ++j) {
            const CandIdx &c = cand[next_size - 1 - j];
            next_frontier_slot.push_back(c.action_slot);

            if (j == 0) {
                next_entry_lcp = descend_to(c.parent_leaf);
            } else if (c.parent_leaf == previous_parent) {
                next_adjacent_lcp.push_back((uint32_t)depth);
            } else {
                next_adjacent_lcp.push_back(descend_to(c.parent_leaf));
            }
            previous_parent = c.parent_leaf;
        }
    }

    /// @brief 確定済み接頭辞と現在の trace から終了経路を作る
    void build_best_path(int upto) {
        best_finished_path = result_prefix;
        for (int depth = freed_to + 1; depth <= upto; ++depth) {
            best_finished_path.push_back(act(depth, trace[depth]));
        }
    }

    /// @brief 現在の State を root へ戻して返却経路を適用する
    template<bool materialize_final_state>
    unique_ptr<State> build_final_state(State& state, const vector<Action>& result_actions, int current_depth) {
        if constexpr (materialize_final_state) {
            for (int depth = current_depth; depth > freed_to; --depth) {
                state.rollback(act(depth, trace[depth]));
            }
            for (auto it = result_prefix.rbegin(); it != result_prefix.rend(); ++it) state.rollback(*it);
            return make_final_state<true>(state, result_actions);
        } else {
            (void)state;
            (void)result_actions;
            (void)current_depth;
            return nullptr;
        }
    }

    /// @brief 列挙された Action の評価と候補登録を行う
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
                bool ok = bs.candidates.push_lazy(score, hash, parent_leaf, [&a] { return a; }, nidv);
                bs.history.push_back({nidv, parent_node_id, turn + 1,
                                      score, hash, move(as), st.get_state_info(), ok ? 0 : 1});
            } else {
                bs.candidates.push_lazy(score, hash, parent_leaf, [&a] { return a; });
            }
        }
    };

    /// @brief 探索用の作業領域を初期化する
    void init_bs() {
        beam_timer.reset();
        found_finished = false;
        best_finished_score = INF;
        best_finished_path.clear();
        node_id_counter = 0;
        history.clear();
        snapshots.clear();
        explored_per_turn = 0;

        action_block.clear();
        parent_block.clear();
        action_slab_pool.clear();
        parent_slab_pool.clear();
        cand.clear();
        actions.clear();
        trace.clear();
        frontier_slot.clear();
        adjacent_lcp.clear();
        entry_lcp = 0;
        next_frontier_slot.clear();
        next_adjacent_lcp.clear();
        next_entry_lcp = 0;
        compat_tour_size = 0;
        freed_to = 0;
        result_prefix.clear();
    }

    /// @brief 生存候補を記録して脱落した履歴ノードの状態を更新する
    void record_turn_survivors(int turn_label) {
        unordered_set<int> survived;
        for (int i = 0; i < candidates.size(); ++i) survived.insert(candidates.next_beam[i].node_id);
        for (int i = (int)history.size() - 1; i >= 0; --i) {
            if (history[i].turn != turn_label) break;
            if (history[i].status == 0 && survived.find(history[i].node_id) == survived.end()) {
                history[i].status = 1;
            }
        }
        snapshots.push_back({turn_label, vector<int>(survived.begin(), survived.end())});
    }

    /// @brief vector 列挙で得た Action を評価して候補へ登録する
    void submit_vector_action(State& state, Action& action, int parent_leaf, int parent_node_id, int turn) {
        auto [score, hash, finished] = state.try_op(action, candidates.threshold());
        if (score >= INF) {
            if constexpr (record_history) {
                history.push_back({node_id_counter++, parent_node_id, turn + 1,
                                   score, hash, action.to_string(), state.get_state_info(), 2});
            }
            return;
        }
        if (finished) {
            if (!found_finished || score < best_finished_score) {
                found_finished = true;
                best_finished_score = score;
                build_best_path(turn);
                if constexpr (record_history) {
                    history.push_back({node_id_counter++, parent_node_id, turn + 1,
                                       score, hash, action.to_string(), state.get_state_info(), 0});
                }
                best_finished_path.push_back(move(action));
            } else if constexpr (record_history) {
                history.push_back({node_id_counter++, parent_node_id, turn + 1,
                                   score, hash, action.to_string(), state.get_state_info(), 0});
            }
            return;
        }
        if constexpr (record_history) {
            int nidv = node_id_counter++;
            string as = action.to_string();
            bool ok = candidates.push_lazy(score, hash, parent_leaf, [&action] { return move(action); }, nidv);
            history.push_back({nidv, parent_node_id, turn + 1,
                               score, hash, move(as), state.get_state_info(), ok ? 0 : 1});
        } else {
            candidates.push_lazy(score, hash, parent_leaf, [&action] { return move(action); });
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
            beam_log::start_banner(cerr, "BeamSearchWithTree (parent)", param);
            if (param.is_adjusting) beam_log::warn(cerr, "dynamic beam width is experimental");
        }

        State state;
        state.init();
        trace.assign(param.max_turn + 1, BAD_SLOT);
        int turns_done = 0;

        int w = param.get_beam_width(param.max_turn, 0, param.time_limit);
        candidates.reset(0, w, true, param.hash_window_turns);

        if constexpr (requires(Submitter &e) { state.enumerate_actions(0, DAMMY_ACTION, e); }) {
            Submitter submit{*this, state, 0, -1, 0};
            state.enumerate_actions(0, DAMMY_ACTION, submit);
        } else {
            actions.clear();
            state.enumerate_actions(actions, 0, DAMMY_ACTION, candidates.threshold());
            explored_per_turn += (int)actions.size();
            for (Action &action : actions) submit_vector_action(state, action, 0, -1, 0);
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
        build_root_frontier();
        turns_done = 1;

        for (int turn = 1; turn < param.max_turn; ++turn) {
            double now_time = beam_timer.elapsed();
            w = param.get_beam_width(param.max_turn - turn, (int)cand.size(), param.time_limit - now_time);
            candidates.reset(turn, w, param.clear_hash_every_turn, param.hash_window_turns);
            explored_per_turn = 0;

            size_t next_compat_tour_size = 0;

            for (int j = 0; j < (int)frontier_slot.size(); ++j) {
                int i = (int)cand.size() - 1 - j;
                const CandIdx &c = cand[i];

                uint32_t h = j == 0 ? entry_lcp : adjacent_lcp[j - 1];
                int source_depth = turn - 1 + (j != 0);
                next_compat_tour_size += (size_t)(turn - (int)h);

                for (int depth = source_depth; depth > (int)h; --depth) {
                    state.rollback(act(depth, trace[depth]));
                }

                Slot target_slot = frontier_slot[j];
                Slot slot = target_slot;
                for (int depth = turn; depth > (int)h; --depth) {
                    trace[depth] = slot;
                    if (depth > (int)h + 1) slot = parent(depth, slot);
                }

                for (int depth = (int)h + 1; depth <= turn; ++depth) {
                    state.apply_op(act(depth, trace[depth]));
                }

                if constexpr (requires(Submitter &e) { state.enumerate_actions(turn, DAMMY_ACTION, e); }) {
                    Submitter submit{*this, state, j, c.node_id, turn};
                    state.enumerate_actions(turn, act(turn, target_slot), submit);
                } else {
                    actions.clear();
                    state.enumerate_actions(actions, turn, act(turn, target_slot), candidates.threshold());
                    explored_per_turn += (int)actions.size();
                    for (Action &action : actions) submit_vector_action(state, action, j, c.node_id, turn);
                }
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
                unique_ptr<State> fs;
                if constexpr (materialize_final_state) {
                    fs = build_final_state<true>(state, best_finished_path, turn);
                }
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
                beam_log::turn_line(cerr, turn + 1, param.max_turn, now_time, w, (int)compat_tour_size,
                                    candidates.size(), explored_per_turn, bests.score);
            }

            if constexpr (record_history) record_turn_survivors(turn + 1);

            int keep_from = turn + 1;
            if (frontier_slot.size() > 1) {
                uint32_t common_depth = *min_element(adjacent_lcp.begin(), adjacent_lcp.end());
                keep_from = (int)common_depth + 1;
            }
            confirm_and_free(keep_from);
            finalize_generation(turn + 1);
            sort(cand.begin(), cand.end(), [](const CandIdx& a, const CandIdx& b) {
                if (a.parent_leaf != b.parent_leaf) return a.parent_leaf < b.parent_leaf;
                return a.score < b.score;
            });

            if (turn + 1 < param.max_turn) {
                build_next_frontier(turn);
                frontier_slot.swap(next_frontier_slot);
                adjacent_lcp.swap(next_adjacent_lcp);
                entry_lcp = next_entry_lcp;
            }
            compat_tour_size = next_compat_tour_size;

            param.timestamp((int)compat_tour_size, candidates.size(), beam_timer.elapsed() - now_time);
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

        vector<Slot> result_slot(param.max_turn + 1, BAD_SLOT);
        Slot slot = cand[best_idx].action_slot;
        for (int depth = param.max_turn; depth > freed_to; --depth) {
            result_slot[depth] = slot;
            if (depth > freed_to + 1) slot = parent(depth, slot);
        }

        vector<Action> ret = result_prefix;
        ret.reserve(param.max_turn);
        for (int depth = freed_to + 1; depth <= param.max_turn; ++depth) {
            ret.push_back(act(depth, result_slot[depth]));
        }

        double elapsed_ms = beam_timer.elapsed();
        if constexpr (record_history) dump_history_json(history_file, INF, history, snapshots);
        if (verbose) {
            beam_log::on_max_turn(cerr);
            beam_log::width_trace(cerr, param.width_hist);
            beam_log::end_banner(cerr, "max_turn reached", turns_done, param.max_turn, elapsed_ms,
                                 param.ave_width(), best_score, true, (int)ret.size());
        }
        unique_ptr<State> fs;
        if constexpr (materialize_final_state) {
            fs = build_final_state<true>(state, ret, param.max_turn - 1);
        }
        return {move(ret), best_score, turns_done, elapsed_ms, BeamStatus::MaxTurnReached, move(fs)};
    }
};

}
