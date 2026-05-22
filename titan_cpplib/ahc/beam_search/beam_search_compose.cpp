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

    int node_id_counter;
    vector<HistoryNode<ScoreType, HashType>> history;
    vector<TurnSnapshot> snapshots;

    // そのターンで実際に探索した頂点数 (= try_op 呼び出し回数)。
    // INF で弾かれたものも、finished で終端扱いされたものも含む。
    int explored_per_turn;

    // ---- 計測用カウンタ (compose 効果の比較指標) -------------------------
    // cnt_apply/cnt_rollback   : DFS が実際に state.apply_op/rollback を呼んだ回数
    // cnt_*_ghost              : ghost のため skip した回数 (ghost 版のみ非ゼロ)
    // cnt_compose_align        : compose_pass の state alignment 用 apply/rollback
    // cnt_tour_total           : 各ターンの tour.size() の合計
    // cnt_cand_total           : 各ターンの生存ノード数 Σ|cand|
    long long cnt_apply, cnt_rollback;
    long long cnt_apply_ghost, cnt_rollback_ghost;
    long long cnt_compose_align;
    long long cnt_tour_total, cnt_cand_total;

    void print_counters() const {
        cerr << "[counters]" << endl;
        cerr << "  apply_op    real=" << cnt_apply
             << " ghost_skip=" << cnt_apply_ghost
             << " total_slots=" << (cnt_apply + cnt_apply_ghost) << endl;
        cerr << "  rollback    real=" << cnt_rollback
             << " ghost_skip=" << cnt_rollback_ghost
             << " total_slots=" << (cnt_rollback + cnt_rollback_ghost) << endl;
        cerr << "  compose_align apply+rollback=" << cnt_compose_align << endl;
        cerr << "  tour_total=" << cnt_tour_total << endl;
        cerr << "  cand_total (sum|cand|)=" << cnt_cand_total << endl;
    }

    // ---- Action プール（世代ブロック arena ＋ 確定接頭辞バルク解放） ------------
    // tour/trace/next_tour/cand は Action 実体でなく ActionId（8B ハンドル）を運ぶ。
    // Action 実体は生成世代＝深さごとのブロックに1回だけ置き、以後コピー・移動しない。
    // ハンドル＝(gen<<SLOT_BITS)|slot。gblock[gen] は確定時にだけ作る固定長 vector で
    // 再確保されないため参照は安定。
    //
    // 解放基準: ある世代の処理後、その世代の全葉の global-LCA 深さ
    // L = turn - max(lca_dist)。次世代以降の DFS が触れる最小深さは L で単調非減少。
    // ゆえに深さ < L は二度と参照されない。L 未満のブロックをバルク解放し、
    // 確定一本道の Action は解放直前に trace から result_prefix へ退避。
    // メモリは生存窓×W に収まり、最終解 = result_prefix ＋ 未確定サフィックス再構築で
    // 挙動完全不変。
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

    // ---- Compose 関連 ----------------------------------------------------
    // gblock_ghost[gen][slot] が 1 なら、その slot の Action は forced-chain の
    // 中間ノードとして合成済み（実体は consumed・未定義）で apply/rollback は no-op。
    // prev_leaf_action_ids は直前の finalize 済み世代の leaf -> ActionId 表。
    // compose_pass は cand を parent_leaf 群に分け、count==1 の親を
    // 「親 then 子」で合成して親 slot を ghost 化する。
    vector<vector<uint8_t>> gblock_ghost;
    vector<vector<uint8_t>> ghost_slab_pool;
    vector<ActionId> prev_leaf_action_ids;

    inline bool is_ghost(ActionId id) const {
        return gblock_ghost[(size_t)(id >> SLOT_BITS)][(size_t)(id & SLOT_MASK)];
    }
    inline void set_ghost(ActionId id) {
        gblock_ghost[(size_t)(id >> SLOT_BITS)][(size_t)(id & SLOT_MASK)] = 1;
    }

    // finalize_generation 直後（cand が sort 済み）に呼ぶ。
    // parent_leaf が同じ cand が 1 個しかない親について parent.compose(child) を試み、
    // 成功したら composed を child slot へ swap、親 slot を ghost マーク。
    // compose が false を返した親はそのまま（合成不可、または合成を望まない）。
    //
    // 重要 (state alignment): 直前の inner loop の最後の iter (i=0) は
    // trace[turn] = cand[0]_{turn}.action_id を apply した直後に終わる。state には
    // この action の効果が乗っている。compose_pass がこの action を ghost 化すると
    // 次 turn 冒頭で trace[turn] が ghost (no-op) として skip され、代わりに
    // 合成済み child action (= parent + child の効果) が apply されるため、
    // parent の効果が二重に乗る。これを防ぐため、compose が parent_a を破壊する
    // 前に rollback で state から parent の効果を抜く。compose が false で
    // 終わった場合は元に戻すため re-apply する。
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

    // 現 cand の action_id 列を prev_leaf_action_ids に snapshot。
    // 次ターンの compose_pass がこれを「親世代の leaf 情報」として参照する。
    //
    // 重要: 子 cand の parent_leaf は親 DFS の reverse iter 順 (= now_leaf_idx) で
    // 振られる。cand_T[i] (sorted index) は size-1-i 番目の reverse iter で処理され、
    // emit した子の parent_leaf = size-1-i。よって parent_leaf=p で参照する親は
    // cand_T_sorted[size-1-p]。snapshot は reverse 順に詰める。
    void snapshot_leaf_actions() {
        int n = (int)cand.size();
        prev_leaf_action_ids.resize(n);
        for (int i = 0; i < n; ++i) {
            prev_leaf_action_ids[n - 1 - i] = cand[i].action_id;
        }
    }

    struct CandIdx {
        int parent_leaf;
        ScoreType score;
        ActionId action_id;
        int node_id = -1;
        int action_count = 0; // この cand の実深さ。非 composed=gen、composed 子=gen-1
    };

    Candidates<ScoreType, HashType, Action, State, INF, record_history> candidates;
    vector<ActionId> trace;
    vector<ActionId> tour;
    vector<int> leaf;
    vector<int> eff_depth;        // leaf[] と並走。leaf k の実深さ(action_count)
    vector<CandIdx> cand;

    vector<ActionId> next_tour;
    vector<int> next_leaf;
    vector<int> next_eff_depth;   // eff_depth[] の構築中バッファ
    vector<Action> actions;

    int freed_to;                 // 深さ <= freed_to は解放済み。result_prefix.size() と一致
    vector<Action> result_prefix; // 確定 Action 列（深さ 1..freed_to、順序通り）
    vector<vector<Action>> slab_pool; // 確定済みブロックの再利用バッファ。free せず使い回す

    // 深さ < L のブロックは次世代以降の DFS が触れない（L は単調非減少）。
    // 確定接頭辞 Action を trace から退避し、ブロックは free せず capacity を保ったまま
    // slab_pool へ退避して使い回す。同時生存ブロック数＝生存窓深に収束し churn が消える。
    // 深さ d (<= L-1) では全葉が同一ノードなので trace[d] が確定ノードそのもの。
    void confirm_and_free(int L) {
        while (freed_to + 1 < L) {
            int d = freed_to + 1;
            // ghost は最終 Action 列に含まない（composed 本体は chain の終端 slot にある）
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

    // candidates.next_beam[0..W) の生存 Action を世代ブロック gen へ1回 move し、
    // cand を ActionId 参照で組み直す。これが採用ノードごと1回の実体コピー。
    // ブロックは slab_pool から取り回し、capacity を再利用して再確保を避ける。
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
            cand.push_back({candidates.next_beam[i].parent_leaf,
                            candidates.next_beam[i].score,
                            make_id(gen, i),
                            candidates.next_beam[i].node_id,
                            gen});
        }
    }

    // ActionId 区間を実体 Action 列へ展開する。最終解の組み立て用。
    // ghost は composed 本体ではないので skip。
    template<class It>
    void materialize(vector<Action>& dst, It first, It last) {
        for (It it = first; it != last; ++it) {
            if (!is_ghost(*it)) dst.push_back(act(*it));
        }
    }

    // 確定接頭辞 ＋ trace[freed_to+1..upto] を best_finished_path に組む。
    // result_prefix は確定一本道 ＝ 旧 act(trace[1..freed_to]) と同値なので挙動不変。
    void build_best_path(int upto) {
        best_finished_path = result_prefix;
        for (int k = freed_to + 1; k <= upto; ++k) {
            if (!is_ghost(trace[k])) best_finished_path.push_back(act(trace[k]));
        }
    }

    // get_actions / try_op 一体化用 sink。
    // get_actions が action を生成し emit(a) を呼ぶと try_op + INF/finished 判定 + push + history を行う。
    // 中間 actions バッファを挟まない。旧 get_actions(vector&, ...) は requires で従来パスに分岐。
    struct Emitter {
        BeamSearchWithTree &bs;
        State &st;
        int parent_leaf, parent_node_id, turn;

        // ライブの最新 worst。push が進むたび縮むので get_actions 側の早期枝刈りに使える。
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

    // 当該ターンの生存 node_id を集め、生き残らなかった status==0 を 1 に直し snapshot を積む。
    // candidates.next_beam を読むだけで探索状態は変更しない。beam_search.cpp と同ロジック。
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

    // tour[leaf[k]..leaf[k+1]) を末尾 dst_end の手前にランク順に貼り込む共通ロジック。
    // dst_end は「親パス末尾の一つ後ろ」（葉のアクションを書く位置 = ancestor path one-past-end）。
    // 「経路復元: 親 leaf へ向かう祖先ぶんの action を tour から復元する」処理。
    // ActionId（8B）コピーなので Action 実体は動かさない。
    // tour 上の segment k = tour[leaf[k]..leaf[k+1]) を、その葉の実深さ eff_depth[k]
    // を底として dst_base[depth] へ深さ明示で貼り込む。深さキーの ratchet で
    // 各 segment は「まだ書かれていない、より浅い側」だけを書く。
    // 結果として分岐部 trace[dL+1..dC-1] のみが更新され、共有接頭辞は不変。
    template<class It>
    inline void copy_tour_path(int parent_leaf, int leaf_end, It dst_base) {
        int written_floor = INT_MAX; // 既に書かれた最も浅い深さ
        for (int k = parent_leaf; k < leaf_end; ++k) {
            int w0 = leaf[k];
            int w1 = leaf[k + 1];
            int seg_len = w1 - w0;
            int bot = eff_depth[k];           // segment の最深
            int top = bot - seg_len + 1;      // segment の最浅
            if (top < written_floor) {
                int hi = bot < written_floor - 1 ? bot : written_floor - 1;
                int copy_len = hi - top + 1;
                copy(tour.begin() + w0, tour.begin() + w0 + copy_len, dst_base + top);
                written_floor = top;
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
        candidates.reset(0, w, param.clear_hash_every_turn, param.hash_window_turns);

        if constexpr (requires(Emitter &e) { state.get_actions(0, DAMMY_ACTION, e); }) {
            Emitter emit{*this, state, 0, -1, 0};
            state.get_actions(0, DAMMY_ACTION, emit);
        } else {
            actions.clear();
            state.get_actions(actions, 0, DAMMY_ACTION, candidates.threshold());
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
                print_counters();
            }
            return best_finished_path;
        }

        // 世代1（深さ1ノード）を確定し cand を ActionId 参照で構築。
        finalize_generation(1);
        cnt_cand_total += cand.size();
        // 世代1 は parent_leaf = 0 のみで分岐なし、compose_pass の対象外。
        // 次世代の compose_pass が親として参照するので action_id を snapshot しておく。
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
            int gL = INT_MAX;   // 全 cand を通じた最浅の LCA 深さ（確定解放の基準）
            // dP_state: 現在 state がいる深さ。turn 開始時は前世代の最後の葉。
            int dP_state = eff_depth.back();

            if (!cand.empty()) {
                trace[cand.back().action_count] = cand.back().action_id;
            }

            for (int i = (int)cand.size() - 1; i >= 0; --i) {
                const auto &c = cand[i];
                int dC = c.action_count;          // この cand の深さ

                // dL = LCA(前cand, c) の深さ。区間 [parent_leaf, li) の各 segment の
                // 底 eff_depth[k] から長さ leaf[k+1]-leaf[k] を引いた値の最小。
                int dL;
                if (c.parent_leaf >= li) {
                    dL = eff_depth[c.parent_leaf];      // 区間空 = 親自身が LCA
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
                if constexpr (requires(Emitter &e) { state.get_actions(turn, DAMMY_ACTION, e); }) {
                    Emitter emit{*this, state, now_leaf_idx, c.node_id, turn};
                    state.get_actions(turn, act(c.action_id), emit);
                } else {
                    actions.clear();
                    state.get_actions(actions, turn, act(c.action_id), candidates.threshold());
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
                if constexpr (record_history) dump_history_json(history_file, INF, history, snapshots);
                if (verbose) {
                    beam_log::on_solution_found(cerr, turn + 1, best_finished_score);
                    beam_log::width_trace(cerr, param.width_hist);
                    beam_log::end_banner(cerr, "solution found", turn + 1, param.max_turn,
                                         beam_timer.elapsed(), param.ave_width(),
                                         best_finished_score, true, (int)best_finished_path.size());
                    print_counters();
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
                                    w, (int)tour.size(), (int)candidates.size(),
                                    explored_per_turn, bests.score);
            }

            if constexpr (record_history) record_turn_survivors(turn + 1);

            confirm_and_free(gL + 1);

            swap(tour, next_tour);
            swap(leaf, next_leaf);
            swap(eff_depth, next_eff_depth);
            cnt_tour_total += tour.size();

            // 世代 turn+1 を確定し cand を ActionId 参照で構築。
            finalize_generation(turn + 1);
            cnt_cand_total += cand.size();
            sort(cand.begin(), cand.end(), [](const CandIdx& a, const CandIdx& b) {
                if (a.parent_leaf != b.parent_leaf) return a.parent_leaf < b.parent_leaf;
                return a.score < b.score;
            });
            // 親世代 (turn) の単一子 leaf を検出して compose、続けて今世代の leaf を snapshot。
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

        // best cand の祖先パスを trace に復元（共有接頭辞は不変、分岐部のみ更新）。
        copy_tour_path(cand[best_idx].parent_leaf, (int)leaf.size() - 1, trace.begin());
        int dBest = cand[best_idx].action_count;
        vector<Action> ret = result_prefix;
        ret.reserve(result_prefix.size() + (dBest - freed_to) + 1);
        for (int d = freed_to + 1; d < dBest; ++d) {
            if (!is_ghost(trace[d])) ret.push_back(act(trace[d]));
        }
        ret.push_back(act(cand[best_idx].action_id));

        if constexpr (record_history) dump_history_json(history_file, INF, history, snapshots);
        if (verbose) {
            beam_log::on_max_turn(cerr);
            beam_log::width_trace(cerr, param.width_hist);
            beam_log::end_banner(cerr, "max_turn reached", turns_done, param.max_turn,
                                 beam_timer.elapsed(), param.ave_width(),
                                 best_score, true, (int)ret.size());
            print_counters();
        }
        return ret;
    }
};
} // namespace flying_squirrel
