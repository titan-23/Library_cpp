/// https://github.com/titan-23/Library_cpp/blob/main/titan_cpplib/ahc/beam_search/beam_search_turn_optimized.cpp
#pragma once
#include <bits/stdc++.h>
#include "titan_cpplib/others/print.cpp"
#include "titan_cpplib/alg/random.cpp"
#include "titan_cpplib/ahc/timer.cpp"
#include "titan_cpplib/ds/hash_dict.cpp"
#include "titan_cpplib/ahc/beam_search/beam_param.cpp"
#include "titan_cpplib/ahc/beam_search/beam_result.cpp"
#include "titan_cpplib/ahc/beam_search/beam_log.cpp"
using namespace std;

namespace flying_squirrel {

/// @brief 候補全体を (par, score) でソートするか
constexpr const bool SORT_CANDIDATES_BY_SCORE = false;

/// @brief 複数ターン先へ遷移する Action に対応した定数倍最適化版木構造ビームサーチ
template<typename ScoreType, typename HashType, class Action, class State, ScoreType INF, bool record_history=false>
class BeamSearchWithTree {
private:
    using Result = BeamResult<ScoreType, Action, State>;

    // kind_end == 0: leaf, -1: POST, -2: 構築中 PRE, 正: 確定済み PRE の subtree_end
    static constexpr int LEAF_NODE = 0;
    static constexpr int POST_ORDER = -1;
    static constexpr int OPEN_PRE_ORDER = -2;
    titan23::Timer beam_timer;
    using ActionId = int;
    static constexpr ActionId BAD_ID = -1;
    vector<Action> result;
    Action DUMMY_ACTION;

    bool found_finished;
    ScoreType best_finished_score;
    ActionId best_finished_par_aid;
    Action best_finished_action;

    struct TreeNode {
        int kind_end;
        ActionId aid; // 同じ辺の PRE_ORDER、POST_ORDER、葉で共有する ID
        int target_turn; // 葉は遷移先ターン、PRE_ORDER は部分木内の最小値
        TreeNode(int kind, ActionId a, int target) : kind_end(kind), aid(a), target_turn(target) {}
    };

    /// @brief target_turn 別候補プール内に保持する最小限の情報
    struct PoolEntry {
        ScoreType score;
        ActionId aid;
        PoolEntry() : score(0), aid(BAD_ID) {}
        PoolEntry(ScoreType s, ActionId a) : score(s), aid(a) {}
    };

    struct CandidateParent {
        int par = 0;
        void set_parent(int value) { par = value; }
        int parent() const { return par; }
    };
    struct EmptyCandidateParent {
        void set_parent(int) {}
        int parent() const { return 0; }
    };

    /// @brief 現在のメタターンで一度採用された候補。通常は親番号を範囲境界へ分離する
    struct BeamCandidate : conditional_t<SORT_CANDIDATES_BY_SCORE, CandidateParent,
                                         EmptyCandidateParent> {
        using ParentBase = conditional_t<SORT_CANDIDATES_BY_SCORE, CandidateParent,
                                         EmptyCandidateParent>;
        ScoreType score;
        ActionId aid;
        int target_turn;
        BeamCandidate() : score(0), aid(BAD_ID), target_turn(0) {}
        BeamCandidate(int p, ScoreType s, ActionId a, int target)
            : score(s), aid(a), target_turn(target) { this->set_parent(p); }
        int parent() const { return ParentBase::parent(); }
    };

    vector<TreeNode> tree, nxt_tree;

    vector<Action> action_pool;
    vector<int> free_slots; // 再利用可能なスロット

    Action& act(ActionId id) {
        return action_pool[id];
    }
    ActionId arena_put_reserve() {
        ActionId slot;
        if (!free_slots.empty()) {
            slot = free_slots.back();
            free_slots.pop_back();
        } else {
            slot = action_pool.size();
            action_pool.emplace_back();
            is_survived_node.push_back(0);
        }
        return slot;
    }
    void arena_put_fill(ActionId slot, const Action& a) {
        act(slot) = a;
    }
    void arena_release(ActionId slot) {
        free_slots.push_back(slot);
    }

    struct HistoryNode {
        int node_id;
        int parent_id;
        int turn;
        ScoreType score;
        HashType hash;
        string action_str;
        string state_info;
        int status;
    };
    struct TurnSnapshot {
        int turn;
        vector<int> active_node_ids;
    };
    vector<TurnSnapshot> snapshots;
    vector<HistoryNode> history;
    int node_id_counter;
    int max_turn_global;
    BeamParam* param_ptr;
    vector<uint8_t> is_survived_node;
    vector<int> aid_to_node_id; // 履歴上のノード ID

    void dump_history_json(const string& filename) const {
        ofstream ofs(filename);
        if(!ofs) return;
        ofs << "{\n  \"INF\": " << INF << ",\n  \"nodes\": [\n";
        for (int i = 0; i < history.size(); ++i) {
            const auto& node = history[i];
            ofs << "    {\n"
                << "      \"node_id\": " << node.node_id << ",\n"
                << "      \"parent_id\": " << node.parent_id << ",\n"
                << "      \"turn\": " << node.turn << ",\n"
                << "      \"score\": " << node.score << ",\n"
                << "      \"hash\": " << node.hash << ",\n"
                << "      \"action\": \"" << node.action_str << "\",\n"
                << "      \"state_info\": " << (node.state_info.empty() ? "{}" : node.state_info) << ",\n"
                << "      \"status\": " << node.status << "\n"
                << "    }";
            if (i + 1 < history.size()) ofs << ",";
            ofs << "\n";
        }
        ofs << "  ],\n  \"snapshots\": [\n";
        for (int i = 0; i < snapshots.size(); ++i) {
            ofs << "    {\n"
                << "      \"turn\": " << snapshots[i].turn << ",\n"
                << "      \"active_node_ids\": [";
            for (int j = 0; j < snapshots[i].active_node_ids.size(); ++j) {
                ofs << snapshots[i].active_node_ids[j];
                if (j + 1 < snapshots[i].active_node_ids.size()) ofs << ", ";
            }
            ofs << "]\n    }";
            if (i + 1 < snapshots.size()) ofs << ",";
            ofs << "\n";
        }
        ofs << "  ]\n}\n";
    }

    class Candidates {
    private:
        using T = pair<ScoreType, int>;
        vector<HashType> hashidx; // スロットごとのハッシュ
        titan23::HashDict<int, false> hash_to_idx;
        int beam_width = 0, entry = 0;
        int s = 1;
        vector<T> seg;
        bool is_built = false; // ビーム幅に達するまではセグメント木を参照しない

        void set(int k, T v) {
            k += s;
            seg[k] = v;
            while (k > 1) {
                k >>= 1;
                T nv = seg[k<<1].first > seg[k<<1|1].first ? seg[k<<1] : seg[k<<1|1];
                if (nv == seg[k]) break;
                seg[k] = nv;
            }
        }

        void build_segtree() {
            for (int i = 0; i < entry; ++i) {
                seg[i + s] = {next_beam[i].score, i};
            }
            fill(seg.begin() + (s + entry), seg.begin() + 2*s, make_pair(-INF, -1));
            for (int k = s - 1; k > 0; --k) {
                seg[k] = seg[k<<1].first > seg[k<<1|1].first ? seg[k<<1] : seg[k<<1|1];
            }
        }

    public:
        struct PushResult {
            int slot;
            ActionId evicted_aid;
        };

        vector<PoolEntry> next_beam;

        Candidates() {}
        explicit Candidates(int w) : hash_to_idx(w * 8) { reset(w); }

        int size() const { return entry; }
        int get_width() const { return beam_width; }

        ScoreType threshold() const { return entry < beam_width ? INF : seg[1].first; }

        /// @brief ActionId を確保する前に採否と格納位置を決める
        /// @return 棄却時は slot=-1。置換時は evicted_aid に旧 ActionId を返す
        PushResult push(ScoreType score, HashType hash) {
            auto pos = hash_to_idx.get_pos(hash);
            int idx = hash_to_idx.inner_get(pos, -1);
            // 置換済みのキーは辞書に残す。現在のスロットの hash と一致するときだけ
            // 生存中の同一候補なので、旧キーを -1 に更新するための再探索を省ける。
            if (idx != -1 && uint64_t(hashidx[idx]) != uint64_t(hash)) idx = -1;
            if (idx != -1) { // 同じハッシュはスコアが改善する場合だけ置き換える
                if (score < next_beam[idx].score) {
                    ActionId old_aid = next_beam[idx].aid;
                    next_beam[idx].score = score;
                    if (is_built) {
                        set(idx, {score, idx});
                    }
                    return {idx, old_aid};
                }
                return {-1, BAD_ID};
            }
            if (entry < beam_width) { // ビーム幅に達するまでは末尾に追加する
                int slot = entry;
                hash_to_idx.inner_set(pos, hash, slot);
                next_beam[slot].score = score;
                hashidx[slot] = hash;
                entry++;
                if (entry == beam_width) {
                    build_segtree();
                    is_built = true;
                }
                return {slot, BAD_ID};
            }
            // 最も悪い候補を置き換える
            auto [_, i] = seg[1];
            ActionId old_aid = next_beam[i].aid;
            next_beam[i].score = score;
            hash_to_idx.inner_set(pos, hash, i);
            hashidx[i] = hash;
            set(i, {score, i});
            return {i, old_aid};
        }

        void set_aid(int slot, ActionId aid) {
            next_beam[slot].aid = aid;
        }

        void reset(int w) {
            beam_width = w;
            s = 1;
            while (s < w) s <<= 1;
            if ((int)seg.size() < 2 * s) seg.resize(2 * s);
            if ((int)hashidx.size() < w) {
                hashidx.resize(w);
                next_beam.resize(w);
            }
            if (hash_to_idx.inner_len() < beam_width * 8) {
                hash_to_idx = titan23::HashDict<int, false>(beam_width * 8);
            } else {
                hash_to_idx.clear();
            }
            entry = 0;
            is_built = false;
        }
    };

    vector<Candidates> cands_pool;
    vector<int> turn_to_pool_idx; // target_turn から cands_pool への添字 -1 は未確保
    vector<int> free_pool_indices;
    vector<ScoreType> thresholds; // 各ターンで受理できるスコアの上限
    struct PreFrame {
        int pre_idx;
        int min_target;
    };
    vector<PreFrame> pre_stack; // PRE_ORDER の添字と生存する直下要素の最小ターン

    /// @brief clear_hash_every_turn が false のとき、ハッシュごとの最良候補を全ターンで共有する
    /// より早いターンへの候補か、同じターンでスコアを改善する候補だけを通す
    titan23::HashDict<pair<ScoreType, int>, false> seen_hash;
    bool use_global_seen;

    int min_target_in_tree; // 木にある葉の最小 target_turn
    int expanded_leaf_count; // この世代で展開した葉数

    /// @brief 動的調整時のビーム幅を残り時間と実測値から求める
    int compute_req_w() {
        BeamParam& param = *param_ptr;
        if (!param.is_adjusting) return param.beam_width;
        if (param.meta_sample_count < param.calibration_meta_count || !param.cost_model_ready()) {
            return param.beam_width;
        }
        // 1 世代あたりの進行ターン数は EMA、累積平均の順で見積もり、未計測なら 1 とする
        double ave_step;
        if (param.ema_step > 0.0) {
            ave_step = max(0.5, param.ema_step);
        } else if (param.target_step_count > 0) {
            ave_step = max(0.5, (double)param.target_step_sum / param.target_step_count);
        } else {
            ave_step = 1.0;
        }
        int base = max_turn_global - min_target_in_tree;
        if (base < 0) base = 0;
        int remain_meta = max(1, (int)ceil(base / ave_step));
        double remain_time_ms = param.time_limit - beam_timer.elapsed();
        if (remain_time_ms <= 0.0) return 1;
        int rec = param.recommend_width(remain_time_ms, remain_meta);
        if (rec < 0) return param.beam_width;
        return rec;
    }

    /// @brief target_turn の候補プールを必要に応じて確保する
    Candidates& get_cands(int target_turn) {
        int idx = turn_to_pool_idx[target_turn];
        if (idx == -1) {
            int requested_width = compute_req_w();
            if (!free_pool_indices.empty()) {
                idx = free_pool_indices.back();
                free_pool_indices.pop_back();
                cands_pool[idx].reset(requested_width);
            } else {
                idx = cands_pool.size();
                cands_pool.emplace_back(requested_width);
            }
            turn_to_pool_idx[target_turn] = idx;
            thresholds[target_turn] = INF;
        }
        return cands_pool[idx];
    }

    vector<BeamCandidate> new_candidates;
    vector<int> candidate_group_ends; // 展開した親ごとの new_candidates 終端

    /// @brief 候補を評価し、完成解を更新するか、未完了の候補を遷移先ターンのプールに登録する
    // State::try_op と候補登録を呼出箇所ごとに複製すると命令キャッシュを圧迫する。
    // 候補列挙が支配的なケースでは、hot section に一つだけ置く方が速い。
#ifdef __clang__
    [[gnu::hot, gnu::noinline]]
#else
    [[gnu::hot, gnu::noinline, gnu::noclone]]
#endif
    void process_candidate(State& state, Action& action, int parent_leaf,
                           ActionId parent_aid, int turn) {
        auto [score, hash, finished] = state.try_op(action, thresholds);
        if (score >= INF) return;
        const int target_turn = action.target_turn;
#ifdef BS_DEBUG
        beam_log::assert_check(target_turn > turn, "target_turn > turn", __FILE__, __LINE__,
                               "target_turn=" + to_string(target_turn) + ", turn=" + to_string(turn));
#endif
        if (target_turn <= turn || target_turn > max_turn_global) return;
        if (!finished && score >= thresholds[target_turn]) return;
        pair<int, bool> seen_pos;
        if (use_global_seen) {
            seen_pos = seen_hash.get_pos(hash);
            if (seen_pos.second) {
                auto sv = seen_hash.inner_get(seen_pos);
                ScoreType seen_s0 = sv.first;
                int seen_t0 = sv.second;
                bool pass = (target_turn < seen_t0) || (target_turn == seen_t0 && score < seen_s0);
                if (!pass) {
                    return;
                }
            }
        }

        int status = 0;
        int node_id = -1;
        if (finished) {
            if (!found_finished || score < best_finished_score) {
                found_finished = true;
                best_finished_score = score;
                best_finished_par_aid = parent_aid;
                best_finished_action = action;
            }
            if constexpr (record_history) node_id = node_id_counter++;
        } else {
            Candidates& cands = get_cands(target_turn);
            auto pushed = cands.push(score, hash);
            if (pushed.slot >= 0) {
                ActionId aid = arena_put_reserve();
                arena_put_fill(aid, action);
                if (pushed.evicted_aid != BAD_ID) {
                    is_survived_node[pushed.evicted_aid] = 0;
                }
                cands.set_aid(pushed.slot, aid);
                is_survived_node[aid] = 1;
                if (cands.size() == cands.get_width()) {
                    thresholds[target_turn] = cands.threshold();
                }
                new_candidates.push_back({parent_leaf, score, aid, target_turn});
                if (use_global_seen) {
                    seen_hash.inner_set(seen_pos, hash, {score, target_turn});
                }
                if constexpr (record_history) {
                    if ((size_t)aid >= aid_to_node_id.size()) aid_to_node_id.resize((size_t)aid + 1, -1);
                    node_id = node_id_counter++;
                    aid_to_node_id[aid] = node_id;
                }
            } else {
                status = 1;
                if constexpr (record_history) node_id = node_id_counter++;
            }
        }
        if constexpr (record_history) {
            int parent_node_id = (parent_aid == BAD_ID || (size_t)parent_aid >= aid_to_node_id.size())
                                     ? -1 : aid_to_node_id[parent_aid];
            history.push_back({node_id, parent_node_id, target_turn, score, hash, action.to_string(),
                               state.get_state_info(), status});
        }
    }

    /// @brief enumerate_actions が候補登録と枝刈り閾値の取得に使う受け口
    struct Submitter {
        BeamSearchWithTree& bs;
        State& st;
        int parent_leaf;
        ActionId parent_aid;
        int turn;

        inline ScoreType threshold(int target_turn) const {
#ifdef BS_DEBUG
            bs.beam_log_threshold_check(target_turn);
#endif
            if (target_turn < 0 || target_turn > bs.max_turn_global) return INF;
            return bs.thresholds[target_turn];
        }
        inline void operator()(Action& a) { bs.process_candidate(st, a, parent_leaf, parent_aid, turn); }
    };

#ifdef BS_DEBUG
    void beam_log_threshold_check(int target_turn) const {
        beam_log::assert_check(0 <= target_turn && target_turn <= max_turn_global,
                               "0 <= target_turn && target_turn <= max_turn_global", __FILE__, __LINE__,
                               "target_turn=" + to_string(target_turn) + ", max_turn=" + to_string(max_turn_global));
    }
#endif

    void get_next_beam(State& state, const int turn) {
        new_candidates.clear();
        candidate_group_ends.clear();
        expanded_leaf_count = 0;

        if (turn == 0) {
            expanded_leaf_count = 1;
            const Action& last_action = (result.empty() ? DUMMY_ACTION : result.back());
            Submitter submit{*this, state, 0, BAD_ID, turn};
            state.enumerate_actions(last_action, submit);
            candidate_group_ends.push_back((int)new_candidates.size());
            return;
        }

        const int tree_size = tree.size();
        for (int i = 0; i < tree_size; ) {
            TreeNode& node = tree[i];
            const int kind_end = node.kind_end;
            if (kind_end == LEAF_NODE) {
                if (node.target_turn == turn) {
                    const int par = expanded_leaf_count;
                    ++expanded_leaf_count;
                    // enumerate_actions 中に action_pool が再確保され得るため、親 Action は退避する。
                    Action action = act(node.aid);
                    state.apply_op(action);
                    Submitter submit{*this, state, par, node.aid, turn};
                    state.enumerate_actions(action, submit);
                    candidate_group_ends.push_back((int)new_candidates.size());
                    state.rollback(action);
                }
                ++i;
            } else if (kind_end > 0) {
                if (node.target_turn > turn) {
                    i = kind_end;
                    continue;
                }
                state.apply_op(act(node.aid));
                ++i;
            } else {
                state.rollback(act(node.aid));
                ++i;
            }
        }
    }

    void update_tree(State& state, const int turn) {
        BeamParam& param = *param_ptr;
        const int prev_min_target = min_target_in_tree;
        int root_min = INT_MAX;
        nxt_tree.clear();
        if (turn == 0) {
            nxt_tree.reserve(new_candidates.size());
            for (int i = 0; i < new_candidates.size(); ++i) {
                const auto& candidate = new_candidates[i];
                ActionId aid = candidate.aid;
                int t_turn = candidate.target_turn;
                if (!is_survived_node[aid]) {
                    arena_release(aid);
                    continue;
                }
#ifdef BS_DEBUG
                beam_log::assert_check(t_turn > turn, "t_turn > turn", __FILE__, __LINE__,
                                       "target_turn=" + to_string(t_turn) + ", turn=" + to_string(turn));
#endif
                nxt_tree.emplace_back(LEAF_NODE, aid, t_turn);
                if (t_turn < root_min) root_min = t_turn;
            }
            swap(tree, nxt_tree);
            min_target_in_tree = (root_min == INT_MAX) ? max_turn_global : root_min;
            int delta = min_target_in_tree - prev_min_target;
            if (delta > 0) param.note_target_step(delta);
            return;
        }

        int i = 0;
        while (i < tree.size()) {
            int kind_end = tree[i].kind_end;
            if (kind_end > 0 && i + 1 < tree.size() && tree[i].aid == tree.back().aid) {
                ActionId aid = tree[i].aid;
                state.apply_op(act(aid));
                if constexpr (is_move_constructible_v<Action>) {
                    result.emplace_back(move(act(aid)));
                } else {
                    result.emplace_back(act(aid));
                }
                arena_release(aid);
                tree.pop_back();
                ++i;
            } else {
                break;
            }
        }

        pre_stack.clear();
        nxt_tree.reserve(tree.size() + new_candidates.size() + (size_t)expanded_leaf_count);

        auto add_child_min = [&](const int target_turn) {
            if (pre_stack.empty()) {
                if (target_turn < root_min) root_min = target_turn;
            } else if (target_turn < pre_stack.back().min_target) {
                pre_stack.back().min_target = target_turn;
            }
        };

        int next_beam_idx = 0;
        int expanded_ordinal = 0;
        const int num_candidates = new_candidates.size();
        const int tree_size = tree.size();
        for (; i < tree_size; ++i) {
            TreeNode& src = tree[i];
            const int kind_end = src.kind_end;
            if (kind_end == LEAF_NODE) {
                if (src.target_turn == turn) {
                    const int par = expanded_ordinal;
                    ++expanded_ordinal;
                    int pre_idx = nxt_tree.size();
                    nxt_tree.emplace_back(OPEN_PRE_ORDER, src.aid, INT_MAX);
                    int subtree_min = INT_MAX;
                    int emit_cnt = 0;
                    const int group_end = SORT_CANDIDATES_BY_SCORE
                                              ? num_candidates
                                              : candidate_group_ends[par];
                    while (next_beam_idx < num_candidates
                           && (SORT_CANDIDATES_BY_SCORE
                                   ? new_candidates[next_beam_idx].parent() == par
                                   : next_beam_idx < group_end)) {
                        const auto& nc = new_candidates[next_beam_idx];
                        if (!is_survived_node[nc.aid]) {
                            arena_release(nc.aid);
                            ++next_beam_idx;
                            continue;
                        }
#ifdef BS_DEBUG
                        beam_log::assert_check(nc.target_turn > turn, "nc.target_turn > turn", __FILE__,
                                               __LINE__, "target_turn=" + to_string(nc.target_turn) +
                                                             ", turn=" + to_string(turn));
#endif
                        nxt_tree.emplace_back(LEAF_NODE, nc.aid, nc.target_turn);
                        if (nc.target_turn < subtree_min) subtree_min = nc.target_turn;
                        ++emit_cnt;
                        ++next_beam_idx;
                    }
                    if (emit_cnt > 0) {
                        nxt_tree.emplace_back(POST_ORDER, src.aid, 0);
                        nxt_tree[pre_idx].target_turn = subtree_min;
                        nxt_tree[pre_idx].kind_end = nxt_tree.size();
                        add_child_min(subtree_min);
                    } else {
                        nxt_tree.pop_back();
                        arena_release(src.aid);
                    }
                } else {
                    if (is_survived_node[src.aid]) {
                        nxt_tree.emplace_back(LEAF_NODE, src.aid, src.target_turn);
                        add_child_min(src.target_turn);
                    } else {
                        arena_release(src.aid);
                    }
                }
            } else if (kind_end > 0) {
                int pre_idx = nxt_tree.size();
                nxt_tree.emplace_back(OPEN_PRE_ORDER, src.aid, INT_MAX);
                pre_stack.push_back({pre_idx, INT_MAX});
            } else {
                if (!nxt_tree.empty()
                    && nxt_tree.back().kind_end == OPEN_PRE_ORDER
                    && nxt_tree.back().aid == src.aid) {
                    arena_release(src.aid);
                    nxt_tree.pop_back();
                    pre_stack.pop_back();
                } else {
                    const int pre_idx = pre_stack.back().pre_idx;
                    const int min_t = pre_stack.back().min_target;
                    nxt_tree.emplace_back(POST_ORDER, src.aid, 0);
                    nxt_tree[pre_idx].kind_end = nxt_tree.size();
                    nxt_tree[pre_idx].target_turn = min_t;
                    pre_stack.pop_back();
                    add_child_min(min_t);
                }
            }
        }

        swap(tree, nxt_tree);
        min_target_in_tree = (root_min == INT_MAX) ? max_turn_global : root_min;
        int delta = min_target_in_tree - prev_min_target;
        if (delta > 0) param.note_target_step(delta);
    }

    ScoreType get_result() {
        ActionId target_aid = BAD_ID;
        Action best_action;
        ScoreType selected_score = INF;

        if (found_finished) {
            target_aid = best_finished_par_aid;
            best_action = best_finished_action;
            selected_score = best_finished_score;
        } else {
            ScoreType best_score = INF;
            for (int t = 0; t <= max_turn_global; ++t) {
                if (turn_to_pool_idx[t] == -1) continue;
                Candidates& cands = cands_pool[turn_to_pool_idx[t]];
                for (int i = 0; i < cands.size(); ++i) {
                    const auto &[score, aid] = cands.next_beam[i];
                    if (is_survived_node[aid]) {
                        if (target_aid == BAD_ID || score < best_score) {
                            best_score = score;
                            target_aid = aid;
                        }
                    }
                }
                if (target_aid != BAD_ID) break;
            }
            selected_score = best_score;
        }

        if (target_aid == BAD_ID) {
            if (found_finished) result.emplace_back(best_action);
            return selected_score;
        }

        vector<ActionId> path;
        for (const auto& node : tree) {
            int kind_end = node.kind_end;
            if (kind_end == LEAF_NODE) {
                if (node.aid == target_aid) {
                    result.reserve(result.size() + path.size() + 1 + (found_finished ? 1 : 0));
                    for (ActionId aid : path) result.emplace_back(act(aid));
                    result.emplace_back(act(node.aid));
                    if (found_finished) result.emplace_back(best_action);
                    return selected_score;
                }
            } else if (kind_end > 0) {
                path.push_back(node.aid);
            } else {
                assert(kind_end == POST_ORDER && !path.empty());
                path.pop_back();
            }
        }
        return selected_score;
    }

    void init_bs(BeamParam &param) {
        param_ptr = &param;
        beam_timer.reset();
        node_id_counter = 0;
        history.clear();
        snapshots.clear();
        result.clear();
        tree.clear();
        nxt_tree.clear();
        new_candidates.clear();
        const size_t initial_width = (size_t)param.beam_width;
        tree.reserve(initial_width);
        nxt_tree.reserve(initial_width);
        new_candidates.reserve(initial_width);
        candidate_group_ends.reserve(initial_width);
        found_finished = false;
        best_finished_score = INF;
        best_finished_par_aid = BAD_ID;
        max_turn_global = param.max_turn;
        DUMMY_ACTION.target_turn = -1;

        action_pool.clear();
        action_pool.reserve(initial_width);
        free_slots.clear();
        free_slots.reserve(initial_width);
        if constexpr (record_history) aid_to_node_id.clear();

        turn_to_pool_idx.assign(max_turn_global + 1, -1);

        free_pool_indices.clear();
        for (int i = 0; i < cands_pool.size(); ++i) {
            free_pool_indices.push_back(i);
        }

        thresholds.assign(max_turn_global + 1, INF);
        is_survived_node.clear();
        is_survived_node.reserve(initial_width);

        param.target_step_sum = 0;
        param.target_step_count = 0;

        min_target_in_tree = 0;
        expanded_leaf_count = 0;

        use_global_seen = !param.clear_hash_every_turn;
        if (use_global_seen) {
            int cap = param.seen_hash_capacity_hint;
            if (cap <= 0) {
                long long auto_cap = 2LL * param.beam_width * max(1, max_turn_global);
                auto_cap = max(1LL << 14, auto_cap);
                cap = (int)min<long long>(auto_cap, INT_MAX / 4);
            }
            seen_hash = titan23::HashDict<pair<ScoreType, int>, false>(cap);
        }
    }

public:
    /// @brief 複数ターン先への遷移を含むビームサーチを実行する
    /// @tparam materialize_final_state 最終状態を構築するか
    /// @param param ターン数やビーム幅などの設定
    /// @param verbose ログを出力するか
    /// @param history_file record_history が true のときに履歴を JSON で出力するファイル名
    /// @return 探索結果
    template<bool materialize_final_state=true>
    Result search(BeamParam &param, const bool verbose=false, const string& history_file = "") {
        if (param.max_turn <= 0 || param.beam_width <= 0) {
            return {{}, INF, 0, 0.0, BeamStatus::InvalidParameter, nullptr};
        }
        init_bs(param);
        if (verbose) {
            beam_log::start_banner(cerr, "BeamSearchWithTree (multi-turn optimized)", param);
            if (param.is_adjusting) beam_log::warn(cerr, "dynamic beam width is experimental");
        }
        State state;
        state.init();

        int turns_done = 0;
        for (int turn = 0; turn < param.max_turn; ++turn) {
            if (turn != 0 && min_target_in_tree > turn) {
                if (turn_to_pool_idx[turn] != -1) {
                    free_pool_indices.push_back(turn_to_pool_idx[turn]);
                    turn_to_pool_idx[turn] = -1;
                    thresholds[turn] = INF;
                }
                turns_done = turn + 1;
                continue;
            }

            double now_time = beam_timer.elapsed();

            get_next_beam(state, turn);
            double dt_expand_ms = beam_timer.elapsed() - now_time;

            if (found_finished) {
                turns_done = turn + 1;
                int applied_prefix = (int)result.size();
                ScoreType result_score = get_result();
                double elapsed_ms = beam_timer.elapsed();
                if constexpr (record_history) dump_history_json(history_file);
                if (verbose) {
                    beam_log::on_solution_found(cerr, turns_done, best_finished_score);
                    beam_log::end_banner(cerr, "solution found", turns_done, param.max_turn,
                                         elapsed_ms, param.ave_width(), best_finished_score, true, result.size());
                }
                unique_ptr<State> fs;
                if constexpr (materialize_final_state) fs = make_final_state<true>(state, result, applied_prefix);
                return {move(result), result_score, turns_done, elapsed_ms, BeamStatus::Finished, move(fs)};
            }

            if (verbose) {
                ScoreType best_for_log = 0;
                bool has_best = false;
                int w = param.beam_width;
                for (int t = turn + 1; t <= max_turn_global; ++t) {
                    if (turn_to_pool_idx[t] == -1) continue;
                    Candidates& cands = cands_pool[turn_to_pool_idx[t]];
                    if (!has_best) w = cands.get_width();
                    for (int i = 0; i < cands.size(); ++i) {
                        ScoreType s = cands.next_beam[i].score;
                        if (!has_best || s < best_for_log) { best_for_log = s; has_best = true; }
                    }
                    if (has_best) break;
                }
                beam_log::turn_line(cerr, turn + 1, param.max_turn, now_time, w, tree.size(),
                                    new_candidates.size(), -1, best_for_log);
                if (!has_best) {
                    beam_log::turn_line_extra(cerr, "(no candidates at this turn yet)");
                }
            }

            if constexpr (record_history) {
                vector<int> active_ids;
                for (int t = turn; t <= max_turn_global; ++t) {
                    if (turn_to_pool_idx[t] == -1) continue;
                    Candidates& cands = cands_pool[turn_to_pool_idx[t]];
                    for (int i = 0; i < cands.size(); ++i) {
                        ActionId aid = cands.next_beam[i].aid;
                        if (is_survived_node[aid]) {
                            int node_id = ((size_t)aid < aid_to_node_id.size()) ? aid_to_node_id[aid] : -1;
                            if (node_id >= 0) active_ids.push_back(node_id);
                        }
                    }
                }
                snapshots.push_back({turn + 1, active_ids});
            }

            double t_update_start = beam_timer.elapsed();
            if constexpr (SORT_CANDIDATES_BY_SCORE) {
                if (turn != 0) {
                    sort(new_candidates.begin(), new_candidates.end(), [] (const auto& a, const auto& b) {
                        if (a.parent() != b.parent()) return a.parent() < b.parent();
                        return a.score < b.score;
                    });
                } else {
                    if (new_candidates.size() > 1) {
                        sort(new_candidates.begin(), new_candidates.end(),
                             [] (const auto& a, const auto& b) { return a.score < b.score; });
                    }
                }
            } else {
                if (new_candidates.size() > 1) {
                    int l = 0;
                    for (int r : candidate_group_ends) {
                        if (r - l > 1) {
                            sort(new_candidates.begin() + l, new_candidates.begin() + r,
                                 [] (const auto& a, const auto& b) { return a.score < b.score; });
                        }
                        l = r;
                    }
                }
            }

            int prev_min_target = min_target_in_tree;
            update_tree(state, turn);
            int delta_target = min_target_in_tree - prev_min_target;
            if (delta_target < 0) delta_target = 0;
            double dt_update_ms = beam_timer.elapsed() - t_update_start;

            if (turn_to_pool_idx[turn] != -1) {
                free_pool_indices.push_back(turn_to_pool_idx[turn]);
                turn_to_pool_idx[turn] = -1;
                thresholds[turn] = INF;
            }

            param.timestamp_meta(dt_expand_ms, dt_update_ms, tree.size(), new_candidates.size(),
                                 expanded_leaf_count, delta_target);
            turns_done = turn + 1;
        }

        int applied_prefix = (int)result.size();
        ScoreType result_score = get_result();
        double elapsed_ms = beam_timer.elapsed();
        if constexpr (record_history) dump_history_json(history_file);
        BeamStatus status = result_score < INF ? BeamStatus::MaxTurnReached : BeamStatus::NoCandidates;
        if (verbose) {
            if (status == BeamStatus::MaxTurnReached) beam_log::on_max_turn(cerr);
            else beam_log::on_no_candidates(cerr, turns_done);
            const char* reason = status == BeamStatus::MaxTurnReached ? "max_turn reached" : "no candidates";
            beam_log::end_banner(cerr, reason, turns_done, param.max_turn,
                                 elapsed_ms, param.ave_width(), result_score, result_score < INF, result.size());
        }
        unique_ptr<State> fs;
        if constexpr (materialize_final_state) {
            if (!result.empty()) fs = make_final_state<true>(state, result, applied_prefix);
        }
        return {move(result), result_score, turns_done, elapsed_ms, status, move(fs)};
    }
};
} // namespace flying_squirrel
