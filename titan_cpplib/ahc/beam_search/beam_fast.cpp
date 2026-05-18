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

    // ---- Action プール（世代ブロック arena ＋ 確定接頭辞バルク解放） ------------
    //! tour/trace/next_tour/cand は Action 実体でなく ActionId（8B ハンドル）を運ぶ。
    //! Action 実体は生成世代＝深さごとのブロックに1回だけ置き、以後コピー・移動しない。
    //! ハンドル＝(gen<<SLOT_BITS)|slot。gblock[gen] は確定時にだけ作る固定長 vector で
    //! 再確保されないため参照は安定。
    //!
    //! 解放基準: ある世代の処理後、その世代の全葉の global-LCA 深さ
    //! L = turn - max(lca_dist)。次世代以降の DFS が触れる最小深さは L で単調非減少。
    //! ゆえに深さ < L は二度と参照されない。L 未満のブロックをバルク解放し、
    //! 確定一本道の Action は解放直前に trace から result_prefix へ退避。
    //! メモリは生存窓×W に収まり、最終解 = result_prefix ＋ 未確定サフィックス再構築で
    //! 挙動完全不変。
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

    int freed_to;                 // 深さ <= freed_to は解放済み。result_prefix.size() と一致
    vector<Action> result_prefix; // 確定 Action 列（深さ 1..freed_to、順序通り）
    vector<vector<Action>> slab_pool; // 確定済みブロックの再利用バッファ。free せず使い回す

    //! 深さ < L のブロックは次世代以降の DFS が触れない（L は単調非減少）。
    //! 確定接頭辞 Action を trace から退避し、ブロックは free せず capacity を保ったまま
    //! slab_pool へ退避して使い回す。同時生存ブロック数＝生存窓深に収束し churn が消える。
    //! 深さ d (<= L-1) では全葉が同一ノードなので trace[d] が確定ノードそのもの。
    void confirm_and_free(int L) {
        while (freed_to + 1 < L) {
            int d = freed_to + 1;
            result_prefix.push_back(act(trace[d]));
            gblock[d].clear();
            slab_pool.push_back(move(gblock[d]));
            freed_to = d;
        }
    }

    //! candidates.next_beam[0..W) の生存 Action を世代ブロック gen へ1回 move し、
    //! cand を ActionId 参照で組み直す。これが採用ノードごと1回の実体コピー。
    //! ブロックは slab_pool から取り回し、capacity を再利用して再確保を避ける。
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
            cand.push_back({candidates.next_beam[i].parent_leaf,
                            candidates.next_beam[i].score,
                            make_id(gen, i),
                            candidates.next_beam[i].node_id});
        }
    }

    //! ActionId 区間を実体 Action 列へ展開する。最終解の組み立て用。
    template<class It>
    void materialize(vector<Action>& dst, It first, It last) {
        for (It it = first; it != last; ++it) dst.push_back(act(*it));
    }

    //! 確定接頭辞 ＋ trace[freed_to+1..upto] を best_finished_path に組む。
    //! result_prefix は確定一本道 ＝ 旧 act(trace[1..freed_to]) と同値なので挙動不変。
    void build_best_path(int upto) {
        best_finished_path = result_prefix;
        for (int k = freed_to + 1; k <= upto; ++k) best_finished_path.push_back(act(trace[k]));
    }

    //! get_actions / try_op 一体化用 sink。
    //! get_actions が action を生成し emit(a) を呼ぶと try_op + INF/finished 判定 + push + history を行う。
    //! 中間 actions バッファを挟まない。旧 get_actions(vector&, ...) は requires で従来パスに分岐。
    struct Emitter {
        BeamSearchWithTree &bs;
        State &st;
        int parent_leaf, parent_node_id, turn;

        //! ライブの最新 worst。push が進むたび縮むので get_actions 側の早期枝刈りに使える。
        inline ScoreType threshold() const { return bs.candidates.threshold(); }

        inline void operator()(Action &a) {
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
    //! ActionId（8B）コピーなので Action 実体は動かさない。
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

        if constexpr (requires(Emitter &e) { state.get_actions(0, DAMMY_ACTION, e); }) {
            Emitter emit{*this, state, 0, -1, 0};
            state.get_actions(0, DAMMY_ACTION, emit);
        } else {
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

        // 世代1（深さ1ノード）を確定し cand を ActionId 参照で構築。
        finalize_generation(1);
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
                if constexpr (requires(Emitter &e) { state.get_actions(turn, DAMMY_ACTION, e); }) {
                    Emitter emit{*this, state, now_leaf_idx, c.node_id, turn};
                    state.get_actions(turn, act(c.action_id), emit);
                } else {
                    actions.clear();
                    state.get_actions(actions, turn, act(c.action_id), candidates.threshold());
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

            // 確定接頭辞をバルク解放。L = turn - max_lca_dist は次世代以降の
            // DFS 最小到達深さ（単調非減少）。深さ < L は二度と参照されない。
            confirm_and_free(turn - max_lca_dist);

            swap(tour, next_tour);
            swap(leaf, next_leaf);

            // 世代 turn+1 を確定し cand を ActionId 参照で構築。
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
        // ridx[0..freed_to) は確定接頭辞（解放済み、deref しない）。result_prefix で置換。
        vector<Action> ret = result_prefix;
        ret.reserve(result_prefix.size() + (ridx.size() - freed_to) + 1);
        materialize(ret, ridx.begin() + freed_to, ridx.end());
        ret.push_back(act(cand[best_idx].action_id));

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
