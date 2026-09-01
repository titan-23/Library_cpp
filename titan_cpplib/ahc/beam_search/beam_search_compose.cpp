/// https://github.com/titan-23/Library_cpp/blob/main/titan_cpplib/ahc/beam_search/beam_search_compose.cpp
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

/// @brief 子が1つだけの親子の Action を合成して探索するビームサーチ
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

    /// @brief Action 合成の効果を測るための探索カウンタ
    long long cnt_apply, cnt_rollback;             // 実行した apply_op と rollback の回数
    long long cnt_apply_ghost, cnt_rollback_ghost; // ghost のため省略した回数
    long long cnt_compose_align;                   // 状態調整に使った apply_op と rollback の合計
    long long cnt_tour_total, cnt_cand_total;      // tour の要素数と生存候補数の累計

    /// @brief Action 合成に関する計測値を出力する
    void print_counters() const {
        cerr << "[counters]" << endl;
        cerr << "  apply_op    real=" << cnt_apply << " ghost_skip=" << cnt_apply_ghost
             << " total_slots=" << (cnt_apply + cnt_apply_ghost) << endl;
        cerr << "  rollback    real=" << cnt_rollback << " ghost_skip=" << cnt_rollback_ghost
             << " total_slots=" << (cnt_rollback + cnt_rollback_ghost) << endl;
        cerr << "  compose_align apply+rollback=" << cnt_compose_align << endl;
        cerr << "  tour_total=" << cnt_tour_total << endl;
        cerr << "  cand_total (sum|cand|)=" << cnt_cand_total << endl;
    }

    /// @brief 世代番号とスロット番号を詰めた Action の識別子
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

    /// @brief 合成済みの中間 Action と直前世代の葉を管理する
    /// ghost のスロットにある Action は参照せず、apply_op と rollback の対象にも含めない
    vector<vector<uint8_t>> gblock_ghost;
    vector<vector<uint8_t>> ghost_slab_pool;
    vector<ActionId> prev_leaf_action_ids;

    inline bool is_ghost(ActionId id) const {
        return gblock_ghost[(size_t)(id >> SLOT_BITS)][(size_t)(id & SLOT_MASK)];
    }
    inline void set_ghost(ActionId id) {
        gblock_ghost[(size_t)(id >> SLOT_BITS)][(size_t)(id & SLOT_MASK)] = 1;
    }

    /// @brief 子が1つだけの親 Action を子へ合成し、親を ghost にする
    /// state に適用中の親は合成前に取り消し、合成に失敗した場合は再適用する
    /// @pre cand は parent_leaf 順に整列済みであること
    void compose_pass(int turn, State& s) {
        int n = (int)cand.size();
        int i = 0;
        ActionId last_applied = trace[turn];
        while (i < n) {
            int p = cand[i].parent_leaf;
            int j = i + 1;
            while (j < n && cand[j].parent_leaf == p) ++j;
            if (j - i == 1) {
                ActionId parent_aid = prev_leaf_action_ids[p];
                if (!is_ghost(parent_aid)) {
                    ActionId child_aid = cand[i].action_id;
                    Action& parent_a = act(parent_aid);
                    Action& child_a = act(child_aid);
                    bool is_last = (parent_aid == last_applied);
                    if (is_last) {
                        s.rollback(parent_a);
                        ++cnt_compose_align;
                    }
                    if (parent_a.compose(child_a)) {
                        using std::swap;
                        swap(parent_a, child_a);
                        set_ghost(parent_aid);
                    } else if (is_last) {
                        s.apply_op(parent_a);
                        ++cnt_compose_align;
                    }
                }
            }
            i = j;
        }
    }

    /// @brief 現世代の ActionId を次ターンの親葉順で保存する
    void snapshot_leaf_actions() {
        int n = (int)cand.size();
        prev_leaf_action_ids.resize(n);
        // parent_leaf は候補の逆順走査で採番されるため、ActionId も逆順に格納する
        for (int i = 0; i < n; ++i) {
            prev_leaf_action_ids[n - 1 - i] = cand[i].action_id;
        }
    }

    struct CandIdx {
        int parent_leaf;
        ScoreType score;
        ActionId action_id;
        int node_id = -1;
        int action_count = 0; // 候補が属する論理深さ
    };

    Candidates<ScoreType, HashType, Action, State, INF, record_history> candidates;
    vector<ActionId> trace;
    vector<ActionId> tour;
    vector<int> leaf;
    vector<int> eff_depth; // 各葉の論理深さ
    vector<CandIdx> cand;

    vector<ActionId> next_tour;
    vector<int> next_leaf;
    vector<int> next_eff_depth;
    vector<Action> actions;

    int freed_to;                     // 解放済みの最大深さ
    vector<Action> result_prefix;     // 確定区間にある非 ghost の Action
    vector<vector<Action>> slab_pool; // 解放した世代ブロックの再利用プール

    /// @brief 確定した接頭辞を退避し、参照されなくなった世代ブロックを再利用プールへ移す
    /// @param L 次世代以降で参照される最小の深さ
    void confirm_and_free(int L) {
        while (freed_to + 1 < L) {
            int d = freed_to + 1;
            // 合成後の Action は連鎖の終端にあるため、ghost は結果に含めない
            if (!is_ghost(trace[d])) {
                result_prefix.push_back(act(trace[d]));
            }
            gblock[d].clear();
            slab_pool.push_back(move(gblock[d]));
            gblock_ghost[d].clear();
            ghost_slab_pool.push_back(move(gblock_ghost[d]));
            freed_to = d;
        }
    }

    /// @brief 生存候補の Action を世代ブロックへ移し、参照用の候補列を構築する
    void finalize_generation(int gen) {
        int sz = (int)candidates.size();
        if ((int)gblock.size() <= gen) gblock.resize(gen + 1);
        if ((int)gblock_ghost.size() <= gen) gblock_ghost.resize(gen + 1);
        if (!slab_pool.empty()) {
            gblock[gen] = move(slab_pool.back());
            slab_pool.pop_back();
        }
        if (!ghost_slab_pool.empty()) {
            gblock_ghost[gen] = move(ghost_slab_pool.back());
            ghost_slab_pool.pop_back();
        }
        gblock[gen].resize(sz);
        gblock_ghost[gen].assign(sz, 0);
        cand.clear();
        cand.reserve(sz);
        for (int i = 0; i < sz; ++i) {
            gblock[gen][i] = move(candidates.next_beam[i].action);
            cand.push_back({candidates.next_beam[i].parent_leaf, candidates.next_beam[i].score, make_id(gen, i),
                            candidates.next_beam[i].node_id, gen});
        }
    }

    /// @brief ActionId の範囲にある非 ghost の Action を列へ展開する
    template<class It>
    void materialize(vector<Action>& dst, It first, It last) {
        for (It it = first; it != last; ++it) {
            if (!is_ghost(*it)) dst.push_back(act(*it));
        }
    }

    /// @brief 確定した接頭辞と未確定の trace から終了経路を構築する
    void build_best_path(int upto) {
        best_finished_path = result_prefix;
        for (int k = freed_to + 1; k <= upto; ++k) {
            if (!is_ghost(trace[k])) best_finished_path.push_back(act(trace[k]));
        }
    }

    /// @brief 探索状態をルートへ戻し、返却経路を適用した最終状態を構築する
    /// ghost は状態に適用されていないため rollback しない
    template<bool materialize_final_state>
    unique_ptr<State> build_final_state(State& state, const vector<Action>& result_actions, int current_depth) {
        if constexpr (materialize_final_state) {
            for (int d = current_depth; d > freed_to; --d) {
                if (!is_ghost(trace[d])) state.rollback(act(trace[d]));
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

    /// @brief 探索ごとの作業領域、記録、計測値を初期化する
    void init_bs() {
        beam_timer.reset();
        rnd = titan23::Random();
        found_finished = false;
        best_finished_score = INF;
        best_finished_path.clear();
        trace.clear();
        tour.clear();
        leaf.clear();
        eff_depth.clear();
        cand.clear();
        next_tour.clear();
        next_leaf.clear();
        next_eff_depth.clear();
        actions.clear();
        gblock.clear();
        slab_pool.clear();
        gblock_ghost.clear();
        ghost_slab_pool.clear();
        prev_leaf_action_ids.clear();
        freed_to = 0;
        result_prefix.clear();
        explored_per_turn = 0;
        cnt_apply = cnt_rollback = 0;
        cnt_apply_ghost = cnt_rollback_ghost = 0;
        cnt_compose_align = 0;
        cnt_tour_total = cnt_cand_total = 0;
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

    /// @brief 親葉までの差分経路を tour から深さ順に復元する
    /// 未復元の浅い区間だけをコピーし、共有接頭辞を保持する
    /// @param dst_base dst_base[d] を深さ d の書き込み先とする出力範囲
    template<class It>
    inline void copy_tour_path(int parent_leaf, int leaf_end, It dst_base) {
        int written_floor = INT_MAX; // 復元済み区間の最小深さ
        for (int k = parent_leaf; k < leaf_end; ++k) {
            int w0 = leaf[k];
            int w1 = leaf[k + 1];
            int seg_len = w1 - w0;
            int bot = eff_depth[k];      // 区間の最大深さ
            int top = bot - seg_len + 1; // 区間の最小深さ
            if (top < written_floor) {
                int hi = bot < written_floor - 1 ? bot : written_floor - 1;
                int copy_len = hi - top + 1;
                copy(tour.begin() + w0, tour.begin() + w0 + copy_len, dst_base + top);
                written_floor = top;
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
                print_counters();
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
        cnt_cand_total += cand.size();
        snapshot_leaf_actions();
        if constexpr (record_history) record_turn_survivors(1);
        leaf = {0};
        eff_depth = {0};
        turns_done = 1;

        for (int turn = 1; turn < param.max_turn; ++turn) {
            double now_time = beam_timer.elapsed();

            next_tour.clear();
            next_leaf.clear();
            next_eff_depth.clear();
            w = param.get_beam_width(param.max_turn - turn, cand.size(), param.time_limit - beam_timer.elapsed());
            candidates.reset(turn, w, param.clear_hash_every_turn, param.hash_window_turns);
            explored_per_turn = 0;

            int li = leaf.size() - 1;
            int gL = INT_MAX; // 全候補の LCA の深さ 世代ブロックの解放基準に使う
            int dP_state = eff_depth.back();

            if (!cand.empty()) {
                trace[cand.back().action_count] = cand.back().action_id;
            }

            for (int i = (int)cand.size() - 1; i >= 0; --i) {
                const auto &c = cand[i];
                int dC = c.action_count;

                // 区間内の各差分経路から、直前の候補との LCA の深さを求める
                int dL;
                if (c.parent_leaf >= li) {
                    dL = eff_depth[c.parent_leaf]; // 区間が空なら親自身が LCA となる
                } else {
                    dL = INT_MAX;
                    for (int k = c.parent_leaf; k < li; ++k) {
                        int seg_lca = eff_depth[k] - (leaf[k + 1] - leaf[k]);
                        if (seg_lca < dL) dL = seg_lca;
                    }
                }
                if (dL < gL) gL = dL;

                for (int d = dP_state; d > dL; --d) {
                    ActionId aid = trace[d];
                    if (!is_ghost(aid)) { state.rollback(act(aid)); ++cnt_rollback; }
                    else ++cnt_rollback_ghost;
                }

                next_tour.insert(next_tour.end(), trace.begin() + (dL + 1), trace.begin() + (dC + 1));

                trace[dC] = c.action_id;
                copy_tour_path(c.parent_leaf, li, trace.begin());

                for (int d = dL + 1; d <= dC; ++d) {
                    ActionId aid = trace[d];
                    if (!is_ghost(aid)) { state.apply_op(act(aid)); ++cnt_apply; }
                    else ++cnt_apply_ghost;
                }
                dP_state = dC;

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
                next_eff_depth.push_back(c.action_count);
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
                    print_counters();
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

            confirm_and_free(gL + 1);

            swap(tour, next_tour);
            swap(leaf, next_leaf);
            swap(eff_depth, next_eff_depth);
            cnt_tour_total += tour.size();

            finalize_generation(turn + 1);
            cnt_cand_total += cand.size();
            sort(cand.begin(), cand.end(), [](const CandIdx& a, const CandIdx& b) {
                if (a.parent_leaf != b.parent_leaf) return a.parent_leaf < b.parent_leaf;
                return a.score < b.score;
            });
            compose_pass(turn, state);
            snapshot_leaf_actions();

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

        // 最良候補の分岐部分を trace に復元する
        copy_tour_path(cand[best_idx].parent_leaf, (int)leaf.size() - 1, trace.begin());
        int dBest = cand[best_idx].action_count;
        vector<Action> ret = result_prefix;
        ret.reserve(result_prefix.size() + (dBest - freed_to) + 1);
        for (int d = freed_to + 1; d < dBest; ++d) {
            if (!is_ghost(trace[d])) ret.push_back(act(trace[d]));
        }
        ret.push_back(act(cand[best_idx].action_id));

        double elapsed_ms = beam_timer.elapsed();
        if constexpr (record_history) dump_history_json(history_file, INF, history, snapshots);
        if (verbose) {
            beam_log::on_max_turn(cerr);
            beam_log::width_trace(cerr, param.width_hist);
            beam_log::end_banner(cerr, "max_turn reached", turns_done, param.max_turn, elapsed_ms,
                                 param.ave_width(), best_score, true, (int)ret.size());
            print_counters();
        }
        unique_ptr<State> fs;
        if constexpr (materialize_final_state) fs = build_final_state<true>(state, ret, param.max_turn - 1);
        return {move(ret), best_score, turns_done, elapsed_ms, BeamStatus::MaxTurnReached, move(fs)};
    }
};
} // namespace flying_squirrel
