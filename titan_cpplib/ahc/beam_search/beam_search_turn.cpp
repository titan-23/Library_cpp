#pragma once
#include <bits/stdc++.h>
#include "titan_cpplib/others/print.cpp"
#include "titan_cpplib/algorithm/random.cpp"
#include "titan_cpplib/ahc/timer.cpp"
#include "titan_cpplib/ds/hash_dict.cpp"
#include "titan_cpplib/ahc/beam_search/beam_param.cpp"
#include "titan_cpplib/ahc/beam_search/beam_log.cpp"
#include "titan_cpplib/ahc/profiler.cpp"
using namespace std;

namespace flying_squirrel {

template<typename ScoreType, typename HashType, class Action, class State, ScoreType INF, bool record_history=false>
class BeamSearchWithTree {
private:
    static constexpr const int PRE_ORDER = -1;
    static constexpr const int POST_ORDER = -2;
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
        // leaf: 0 以上 (値は使わず種別判定のみ) / PRE_ORDER / POST_ORDER
        // 親子対応は「展開葉の通し番号」を get_next_beam / update_tree の両側で数えて取る
        int dir_or_leaf_id;
        // PRE_ORDER 専用: 対応する POST_ORDER の次の index (部分木の skip 用)
        int subtree_end;
        // 同じ action の PRE/POST/leaf ノードは同じ aid を持つ
        ActionId aid;
        // leaf: action の target_turn / PRE_ORDER: 部分木 leaf の target_turn の最小値
        int target_turn;
        TreeNode(int d, ActionId a, int target) : dir_or_leaf_id(d), subtree_end(0), aid(a), target_turn(target) {}
    };

    struct BeamCandidate {
        ScoreType score;
        int par; // 親 leaf の展開順 (この世代で何番目に展開した葉か)
        ActionId aid;
        int target_turn;
        // hash は持たない (プール内の重複置き換えは Candidates::hashidx が担う)
        BeamCandidate() : score(0), par(0), aid(BAD_ID), target_turn(0) {}
        BeamCandidate(int p, ScoreType s, ActionId a, int target) : score(s), par(p), aid(a), target_turn(target) {}
    };

    vector<TreeNode> tree, nxt_tree;

    // Action pool: aid の解放は update_tree 内の「死ぬ場所」でインラインに行う (各箇所のコメント参照)
    vector<Action> action_pool;
    vector<int> free_slots;     // 解放済み slot の LIFO

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
        action_pool[slot] = a;
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
    BeamParam* param_ptr; // search() 中は常に同一の param (init_bs で設定)
    vector<uint8_t> is_survived_node;  // aid 添字の生存フラグ (arena_put_reserve で action_pool と同時に伸ばす)
    vector<int> aid_to_node_id;  // record_history=true のときだけ使う aid → node_id (history JSON 用)

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
        vector<HashType> hashidx; // slot → hash の逆引き
        // key は Zobrist hash (一様) なので内部撹拌を省略する
        titan23::HashDict<int, false> hash_to_idx;
        int beam_width, entry;
        int s = 1;
        vector<T> seg;

        // entry < beam_width の間は segtree を作らず、entry == beam_width に達した時点で一括構築する
        // is_built==false の間は seg[1] / seg[idx+s] を参照しないこと
        bool is_built = false;

        void set(int k, T v) {
            PROF_START("cands_set");
            k += s;
            seg[k] = v;
            while (k > 1) {
                k >>= 1;
                T nv = seg[k<<1].first > seg[k<<1|1].first ? seg[k<<1] : seg[k<<1|1];
                if (nv == seg[k]) break;
                seg[k] = nv;
            }
            PROF_STOP();
        }

        // entry 個の next_beam[] から segtree を O(W) で一括構築する
        void build_segtree() {
            PROF_START("cands_build_segtree");
            for (int i = 0; i < entry; ++i) {
                seg[i + s] = {next_beam[i].score, i};
            }
            fill(seg.begin() + (s + entry), seg.begin() + 2*s, make_pair(-INF, -1));
            for (int k = s - 1; k > 0; --k) {
                seg[k] = seg[k<<1].first > seg[k<<1|1].first ? seg[k<<1] : seg[k<<1|1];
            }
            PROF_STOP();
        }

    public:
        vector<BeamCandidate> next_beam;

        Candidates() {}

        int size() const { return entry; }
        int get_width() const { return beam_width; }

        ScoreType threshold() const { return entry < beam_width ? INF : seg[1].first; }

        // 受理した場合 next_beam のインデックスを返し、棄却なら -1
        // aid は呼び出し側が arena_put_reserve で取って渡す (棄却時は arena_release)
        // is_survived は採用した aid に 1、追い出した aid に 0 をセットする
        int push(ScoreType score, HashType hash, int par, ActionId aid, vector<uint8_t>& is_survived, int target_turn) {
            PROF_START("cands_push");
            if (is_built && score >= seg[1].first) {
                PROF_STOP();
                return -1;
            }
            auto pos = hash_to_idx.get_pos(hash);
            int idx = hash_to_idx.inner_get(pos, -1);
            if (idx != -1) {
                // 既に同じ hash がある場合、score が改善するときだけ置き換える
                if (score < next_beam[idx].score) {
                    is_survived[next_beam[idx].aid] = 0;
                    next_beam[idx] = {par, score, aid, target_turn};
                    is_survived[aid] = 1;
                    if (is_built) {
                        set(idx, {score, idx});
                    }
                    PROF_STOP();
                    return idx;
                }
                PROF_STOP();
                return -1;
            }
            if (entry < beam_width) {
                // entry < beam_width の間は segtree を更新せず、末尾に追加するだけ
                int slot = entry;
                hash_to_idx.inner_set(pos, hash, slot);
                next_beam[slot] = {par, score, aid, target_turn};
                is_survived[aid] = 1;
                hashidx[slot] = hash;
                entry++;
                if (entry == beam_width) {
                    build_segtree();
                    is_built = true;
                }
                PROF_STOP();
                return slot;
            }
            // entry == beam_width なので worst と置き換える
            auto [_, i] = seg[1];
            is_survived[next_beam[i].aid] = 0;
            next_beam[i] = {par, score, aid, target_turn};
            is_survived[aid] = 1;
            hash_to_idx.set(hashidx[i], -1);
            hash_to_idx.inner_set(pos, hash, i);
            hashidx[i] = hash;
            set(i, {score, i});
            PROF_STOP();
            return i;
        }

        void reset(int w) {
            PROF_START("cands_reset");
            beam_width = w;
            while (s < w) {
                s <<= 1;
            }
            if (seg.size() < 2*s) {
                seg.resize(2*s);
            }
            if (hashidx.size() < w) {
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
            PROF_STOP();
        }
    };

    vector<Candidates> cands_pool;
    vector<int> turn_to_pool_idx; // target_turn → cands_pool の index (-1 は未確保)
    vector<int> free_pool_indices;
    vector<ScoreType> thresholds; // thresholds[t] = pool t の現在の worst score
    // update_tree 用の (PRE_ORDER の index, 要再計算フラグ) スタック
    vector<pair<int, bool>> pre_stack;

    // clear_hash_every_turn=false のとき、全ターンを通して一度見た hash を dedup する
    //   value = (best_score_at_smallest_t, smallest_t)
    // - 未登録                            → 通す
    // - 既登録 (s0, t0) で t < t0         → 通す (より小さい target_turn 側で再挑戦)
    // - 既登録 (s0, t0) で t==t0 かつ score < s0 → 通す (同じ最小 t で score 改善)
    // - その他                            → 捨てる
    // 既に大きい target_turn の pool に入っている同 hash の entry はそのまま残す
    titan23::HashDict<pair<ScoreType, int>, false> seen_hash; // key は Zobrist hash なので撹拌省略
    bool use_global_seen;

    // tree 内 leaf の target_turn の最小値
    // update_tree 末尾で更新し、compute_req_w の残り世代数の推定に使う
    int min_target_in_tree;

    // この世代で展開した leaf 数 (BeamParam::timestamp_meta の applied_w)
    int expanded_leaf_count;

    // is_adjusting のときは残時間と 1 世代の所要時間の実測から
    // BeamParam::recommend_width で見積もる
    int compute_req_w() {
        BeamParam& param = *param_ptr;
        if (!param.is_adjusting) return param.beam_width;
        if (param.meta_sample_count < param.calibration_meta_count || !param.cost_model_ready()) {
            return param.beam_width;
        }
        // 1 世代で進む target_turn 量
        // ema_step を優先し、未初期化なら累積平均、それも無ければ 1
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

    // target_turn の pool を返す (未確保なら遅延確保する)
    Candidates& get_cands(int target_turn) {
        int idx = turn_to_pool_idx[target_turn];
        if (idx == -1) {
            if (!free_pool_indices.empty()) {
                idx = free_pool_indices.back();
                free_pool_indices.pop_back();
            } else {
                idx = cands_pool.size();
                cands_pool.emplace_back();
            }
            turn_to_pool_idx[target_turn] = idx;
            cands_pool[idx].reset(compute_req_w());
            thresholds[target_turn] = INF;
        }
        return cands_pool[idx];
    }

    vector<BeamCandidate> new_candidates;

    // 候補 1 件を処理する: try_op で評価し、枝刈り・dedup を経て target_turn の pool へ push する
    //   (finished は best_finished を更新するだけで pool には入れない)
    // always_inline は user の try_op をインライン化させるため (関数境界越しだと数 % 遅くなる)
    // parent_leaf: 親 leaf の展開順 (turn==0 では 0) / parent_aid: 親 leaf の aid (turn==0 では BAD_ID)
    [[gnu::always_inline]] inline void process_candidate(State& state, Action& action, int parent_leaf, ActionId parent_aid) {
        PROF_START("try_op");
        auto [score, hash, finished] = state.try_op(action, thresholds);
        PROF_STOP();
        if (score >= INF) return;
        const int target_turn = action.target_turn;
        if (target_turn > max_turn_global) return;
        if (!finished && score >= thresholds[target_turn]) return;
        pair<int, bool> seen_pos;
        if (use_global_seen) {
            PROF_START("seen_probe");
            seen_pos = seen_hash.get_pos(hash);
            if (seen_pos.second) {
                auto sv = seen_hash.inner_get(seen_pos);
                ScoreType seen_s0 = sv.first;
                int seen_t0 = sv.second;
                bool pass = (target_turn < seen_t0) || (target_turn == seen_t0 && score < seen_s0);
                if (!pass) {
                    PROF_STOP();
                    return;
                }
            }
            PROF_STOP();
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
            ActionId aid = arena_put_reserve();
            Candidates& cands = get_cands(target_turn);
            int slot = cands.push(score, hash, parent_leaf, aid, is_survived_node, target_turn);
            if (slot >= 0) {
                PROF_START("arena_fill");
                arena_put_fill(aid, action);
                PROF_STOP();
                thresholds[target_turn] = cands.threshold();
                new_candidates.push_back({parent_leaf, score, aid, target_turn});
                if (use_global_seen) {
                    PROF_START("seen_set");
                    seen_hash.inner_set(seen_pos, hash, {score, target_turn});
                    PROF_STOP();
                }
                if constexpr (record_history) {
                    if (aid >= aid_to_node_id.size()) aid_to_node_id.resize(aid + 1, -1);
                    node_id = node_id_counter++;
                    aid_to_node_id[aid] = node_id;
                }
            } else {
                arena_release(aid);
                status = 1;
                if constexpr (record_history) node_id = node_id_counter++;
            }
        }
        if constexpr (record_history) {
            int parent_node_id = (parent_aid == BAD_ID || parent_aid >= aid_to_node_id.size()) ? -1 : aid_to_node_id[parent_aid];
            history.push_back({node_id, parent_node_id, target_turn, score, hash, action.to_string(), state.get_state_info(), status});
        }
    }

    // enumerate_actions に渡す submit オブジェクト
    // submit(action) で候補を登録し、submit.threshold(target_turn) で枝刈り閾値を取得する
    struct Submitter {
        BeamSearchWithTree& bs;
        State& st;
        int parent_leaf;
        ActionId parent_aid;

        inline ScoreType threshold(int target_turn) const { return bs.thresholds[target_turn]; }
        inline void operator()(Action& a) { bs.process_candidate(st, a, parent_leaf, parent_aid); }
    };

    void get_next_beam(State& state, const int turn) {
        new_candidates.clear();
        expanded_leaf_count = 0;

        if (turn == 0) {
            expanded_leaf_count = 1;
            const Action& last_action = (result.empty() ? DUMMY_ACTION : result.back());
            Submitter submit{*this, state, 0, BAD_ID};
            PROF_START("enumerate_actions");
            state.enumerate_actions(last_action, submit);
            PROF_STOP();
            return;
        }

        const int tree_size = tree.size();
        for (int i = 0; i < tree_size; ) {
            TreeNode& node = tree[i];
            const int dir_or_leaf_id = node.dir_or_leaf_id;
            if (dir_or_leaf_id >= 0) {
                if (node.target_turn == turn) {
                    const int par = expanded_leaf_count;
                    ++expanded_leaf_count;
                    PROF_START("expand_leaf");
                    PROF_START("leaf_action_copy");
                    Action action = act(node.aid);
                    PROF_STOP();
                    state.apply_op(action);
                    Submitter submit{*this, state, par, node.aid};
                    PROF_START("enumerate_actions");
                    state.enumerate_actions(action, submit);
                    PROF_STOP();
                    state.rollback(action);
                    PROF_STOP();
                }
                ++i;
            } else if (dir_or_leaf_id == PRE_ORDER) {
                if (node.target_turn > turn) {
                    i = node.subtree_end;
                    continue;
                }
                PROF_START("walk_apply");
                state.apply_op(act(node.aid));
                PROF_STOP();
                ++i;
            } else {
                PROF_START("walk_rollback");
                state.rollback(act(node.aid));
                PROF_STOP();
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
            for (int i = 0; i < new_candidates.size(); ++i) {
                const auto &[score, par, aid, t_turn] = new_candidates[i];
                if (is_survived_node[aid]) {
                    nxt_tree.emplace_back(0, aid, t_turn);
                    if (t_turn < root_min) root_min = t_turn;
                } else {
                    arena_release(aid);
                }
            }
            swap(tree, nxt_tree);
            min_target_in_tree = (root_min == INT_MAX) ? max_turn_global : root_min;
            int delta = min_target_in_tree - prev_min_target;
            if (delta > 0) param.note_target_step(delta);
            return;
        }

        int i = 0;
        while (i < tree.size()) {
            int dir_or_leaf_id = tree[i].dir_or_leaf_id;
            if (dir_or_leaf_id == PRE_ORDER && i + 1 < tree.size() && tree[i].aid == tree.back().aid) {
                result.emplace_back(act(tree[i].aid));
                state.apply_op(act(tree[i].aid));
                arena_release(tree[i].aid);
                tree.pop_back();
                ++i;
            } else {
                break;
            }
        }

        // subtree_min (PRE_ORDER の target_turn) は遅延更新する:
        //   PRE_ORDER をそのままコピーするときは旧 subtree_min を入れ、フラグを立てずに push する
        //   再計算フラグを立てるのは、親の subtree_min に影響しうる次の変更があったときだけ:
        //     - subtree_min に寄与していた leaf の追い出し / 展開 / 空になった部分木の除去
        //     - 新しい subtree_min が親の値を下回る
        //   POST_ORDER を処理するとき、フラグの立った部分木だけ直接の子を線形 scan して再計算する
        //   再計算結果が旧値と同じなら親への伝播もスキップする
        pre_stack.clear();
        nxt_tree.reserve(tree.size() + new_candidates.size());

        int next_beam_idx = 0;
        int expanded_ordinal = 0; // 展開葉の通し番号 (get_next_beam と同じ数え方)
        const int num_candidates = new_candidates.size();
        const int tree_size = tree.size();
        for (; i < tree_size; ++i) {
            TreeNode& src = tree[i];
            const int dir_or_leaf_id = src.dir_or_leaf_id;
            if (dir_or_leaf_id >= 0) {
                if (src.target_turn == turn) {
                    // 展開済み leaf: 先に PRE_ORDER を submit しておき、生存子を直接 submit する
                    // (生存子が 0 件なら取り消す)
                    const int par = expanded_ordinal;
                    ++expanded_ordinal;
                    int pre_idx = nxt_tree.size();
                    nxt_tree.emplace_back(PRE_ORDER, src.aid, INT_MAX);
                    int subtree_min = INT_MAX;
                    int emit_cnt = 0;
                    while (next_beam_idx < num_candidates
                           && new_candidates[next_beam_idx].par == par) {
                        const auto& nc = new_candidates[next_beam_idx];
                        if (is_survived_node[nc.aid]) {
                            nxt_tree.emplace_back(0, nc.aid, nc.target_turn);
                            if (nc.target_turn < subtree_min) subtree_min = nc.target_turn;
                            ++emit_cnt;
                        } else {
                            arena_release(nc.aid); // 受理後にビームから追い出された候補
                        }
                        ++next_beam_idx;
                    }
                    if (emit_cnt > 0) {
                        nxt_tree.emplace_back(POST_ORDER, src.aid, 0);
                        nxt_tree[pre_idx].target_turn = subtree_min;
                        nxt_tree[pre_idx].subtree_end = nxt_tree.size();
                        if (pre_stack.empty() && subtree_min < root_min) root_min = subtree_min;
                    } else {
                        nxt_tree.pop_back();
                        arena_release(src.aid); // 展開したが生き残った子が 0 の葉
                    }
                    // 旧 leaf が親の subtree_min に寄与していた、
                    // または新 subtree_min が親の値を下回るときだけ親の再計算フラグを立てる
                    if (!pre_stack.empty()) {
                        auto& top = pre_stack.back();
                        int gp_min = nxt_tree[top.first].target_turn;
                        if (src.target_turn == gp_min || (emit_cnt > 0 && subtree_min < gp_min)) {
                            top.second = true;
                        }
                    }
                } else {
                    if (is_survived_node[src.aid]) {
                        nxt_tree.emplace_back(dir_or_leaf_id, src.aid, src.target_turn);
                        if (pre_stack.empty() && src.target_turn < root_min) root_min = src.target_turn;
                    } else {
                        // ビームから追い出された leaf は捨て、親の subtree_min に寄与していたときだけ再計算フラグを立てる
                        arena_release(src.aid);
                        if (!pre_stack.empty()) {
                            auto& top = pre_stack.back();
                            if (src.target_turn == nxt_tree[top.first].target_turn) {
                                top.second = true;
                            }
                        }
                    }
                }
            } else if (dir_or_leaf_id == PRE_ORDER) {
                // src.target_turn は前世代の subtree_min で、変化が無ければそのまま使える
                int pre_idx = nxt_tree.size();
                nxt_tree.emplace_back(PRE_ORDER, src.aid, src.target_turn);
                pre_stack.push_back({pre_idx, false});
            } else {
                if (!nxt_tree.empty()
                    && nxt_tree.back().dir_or_leaf_id == PRE_ORDER
                    && nxt_tree.back().aid == src.aid) {
                    // 空になった部分木: PRE_ORDER を取り消し、POST_ORDER も submit しない
                    // (PRE と POST は同じ aid なので、ここで 1 回だけ解放する)
                    arena_release(src.aid);
                    int popped_min = nxt_tree.back().target_turn;
                    nxt_tree.pop_back();
                    pre_stack.pop_back();
                    // 消えた部分木が親の subtree_min に寄与していたときだけ再計算フラグを立てる
                    if (!pre_stack.empty()) {
                        auto& top = pre_stack.back();
                        if (popped_min == nxt_tree[top.first].target_turn) {
                            top.second = true;
                        }
                    }
                } else {
                    int pre_idx = pre_stack.back().first;
                    bool need_recalc = pre_stack.back().second;
                    int old_min = nxt_tree[pre_idx].target_turn;
                    nxt_tree.emplace_back(POST_ORDER, src.aid, 0);
                    nxt_tree[pre_idx].subtree_end = nxt_tree.size();
                    pre_stack.pop_back();
                    if (need_recalc) {
                        // 直接の子だけ線形 scan して subtree_min を再計算する
                        // (孫以下は subtree_end で skip するので O(直接の子数))
                        int min_t = INT_MAX;
                        int k = pre_idx + 1;
                        const int end_excl = nxt_tree.size() - 1; // POST_ORDER の直前まで
                        while (k < end_excl) {
                            const TreeNode& child = nxt_tree[k];
                            int t = child.target_turn;
                            if (t < min_t) min_t = t;
                            if (child.dir_or_leaf_id == PRE_ORDER) {
                                k = child.subtree_end;
                            } else {
                                ++k;
                            }
                        }
                        nxt_tree[pre_idx].target_turn = min_t;
                        // 旧値から変わり、かつ親の subtree_min に影響しうるときだけ伝播する
                        if (min_t != old_min && !pre_stack.empty()) {
                            auto& top = pre_stack.back();
                            int gp_min = nxt_tree[top.first].target_turn;
                            if (old_min == gp_min || min_t < gp_min) {
                                top.second = true;
                            }
                        }
                    }
                    // root レベルの部分木なら、確定した subtree_min を root_min に反映する
                    if (pre_stack.empty() && nxt_tree[pre_idx].target_turn < root_min) {
                        root_min = nxt_tree[pre_idx].target_turn;
                    }
                }
            }
        }

        swap(tree, nxt_tree);
        min_target_in_tree = (root_min == INT_MAX) ? max_turn_global : root_min;
        int delta = min_target_in_tree - prev_min_target;
        if (delta > 0) param.note_target_step(delta);
    }

    void get_result() {
        ActionId target_aid = BAD_ID;
        Action best_action;

        if (found_finished) {
            target_aid = best_finished_par_aid;
            best_action = best_finished_action;
        } else {
            // target_turn の小さい pool から順に、生存中の最良候補を探す
            ScoreType best_score = INF;
            for (int t = 0; t <= max_turn_global; ++t) {
                if (turn_to_pool_idx[t] == -1) continue;
                Candidates& cands = cands_pool[turn_to_pool_idx[t]];
                for (int i = 0; i < cands.size(); ++i) {
                    const auto &[score, par, aid, t_turn] = cands.next_beam[i];
                    if (is_survived_node[aid]) {
                        if (target_aid == BAD_ID || score < best_score) {
                            best_score = score;
                            target_aid = aid;
                            best_action = act(aid);
                        }
                    }
                }
                if (target_aid != BAD_ID) break;
            }
        }

        if (target_aid == BAD_ID) {
             if (found_finished) {
                 result.emplace_back(best_action);
             }
             return;
        }

        // Euler tour を歩き、PRE で push / POST で pop しながら target までの経路を復元する
        for (const auto& node : tree) {
            int dir_or_leaf_id = node.dir_or_leaf_id;
            const Action& action = act(node.aid);
            if (dir_or_leaf_id >= 0) {
                if (node.aid == target_aid) {
                    result.emplace_back(action);
                    if (found_finished) {
                        result.emplace_back(best_action);
                    }
                    return;
                }
            } else if (dir_or_leaf_id == PRE_ORDER) {
                result.emplace_back(action);
            } else if (dir_or_leaf_id == POST_ORDER) {
                result.pop_back();
            }
        }
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
        found_finished = false;
        best_finished_score = INF;
        best_finished_par_aid = BAD_ID;
        max_turn_global = param.max_turn;
        DUMMY_ACTION.target_turn = -1;

        action_pool.clear();
        free_slots.clear();
        if constexpr (record_history) aid_to_node_id.clear();

        turn_to_pool_idx.assign(max_turn_global + 1, -1);

        free_pool_indices.clear();
        for (int i = 0; i < cands_pool.size(); ++i) {
            free_pool_indices.push_back(i);
        }

        thresholds.assign(max_turn_global + 1, INF);
        is_survived_node.clear();

        // target_step 統計は毎 search でリセット
        param.target_step_sum = 0;
        param.target_step_count = 0;

        min_target_in_tree = 0;
        expanded_leaf_count = 0;

        // global seen_hash は clear_hash_every_turn==false のときだけ有効化
        use_global_seen = !param.clear_hash_every_turn;
        if (use_global_seen) {
            int cap = param.seen_hash_capacity_hint;
            if (cap <= 0) {
                cap = max(1 << 14, param.beam_width * max(1, max_turn_global) * 2);
            }
            PROF_START("init_seen_hash");
            seen_hash = titan23::HashDict<pair<ScoreType, int>, false>(cap);
            PROF_STOP();
        }
    }

public:
    vector<Action> search(BeamParam &param, const bool verbose=false, const string& history_file = "") {
        init_bs(param);
        if (verbose) {
            beam_log::start_banner(cerr, "BeamSearchWithTree (multi-turn)", param);
            if (param.is_adjusting) beam_log::warn(cerr, "dynamic beam width is experimental");
        }
        State state;
        state.init();

        int turns_done = 0;
        for (int turn = 0; turn < param.max_turn; ++turn) {
            // この turn に展開する葉が無ければ木も pool も変化しないので walk / update_tree を丸ごとスキップする
            // (このとき pool[turn] は必ず空: pool が非空なら対応する葉が tree にある)
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
            // dt_expand: get_next_beam の所要時間 (W に比例する成分)
            // verbose / record_history のオーバーヘッドを含めないようここで時刻を取る
            double dt_expand_ms = beam_timer.elapsed() - now_time;

            if (found_finished) {
                turns_done = turn + 1;
                PROF_START("get_result");
                get_result();
                PROF_STOP();
                if constexpr (record_history) dump_history_json(history_file);
                if (verbose) {
                    beam_log::on_solution_found(cerr, turns_done, best_finished_score);
                    beam_log::end_banner(cerr, "solution found", turns_done, param.max_turn, beam_timer.elapsed(), param.ave_width(), best_finished_score, true, result.size());
                }
                return result;
            }

            if (verbose) {
                // best と w は、最初に候補が見つかった pool (target_turn が最小の非空 pool) のものを出す
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
                beam_log::turn_line(cerr, turn + 1, param.max_turn, now_time, w, tree.size(), new_candidates.size(), -1, best_for_log);
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
                            int node_id = (aid < aid_to_node_id.size()) ? aid_to_node_id[aid] : -1;
                            if (node_id >= 0) active_ids.push_back(node_id);
                        }
                    }
                }
                snapshots.push_back({turn + 1, active_ids});
            }

            // dt_update: sort + update_tree の所要時間 (tree サイズに比例する成分)
            // verbose / record_history のオーバーヘッドを除くためここから時刻を取り直す
            double t_update_start = beam_timer.elapsed();
            PROF_START("sort_candidates");
            if (turn != 0) {
                // update_tree が展開順 (par) に消費するので par を第一キーにする
                sort(new_candidates.begin(), new_candidates.end(), [] (const auto& a, const auto& b) { if (a.par != b.par) return a.par < b.par; return a.score < b.score; });
            } else {
                sort(new_candidates.begin(), new_candidates.end(), [] (const auto& a, const auto& b) { return a.score < b.score; });
            }
            PROF_STOP();
            int prev_min_target = min_target_in_tree;
            PROF_START("update_tree");
            update_tree(state, turn);
            PROF_STOP();
            int delta_target = min_target_in_tree - prev_min_target;
            if (delta_target < 0) delta_target = 0;
            double dt_update_ms = beam_timer.elapsed() - t_update_start;

            // このターンの pool は以降使わないので解放する
            if (turn_to_pool_idx[turn] != -1) {
                free_pool_indices.push_back(turn_to_pool_idx[turn]);
                turn_to_pool_idx[turn] = -1;
                thresholds[turn] = INF;
            }

            param.timestamp_meta(dt_expand_ms, dt_update_ms, tree.size(), new_candidates.size(), expanded_leaf_count, delta_target);
            turns_done = turn + 1;
        }

        PROF_START("get_result");
        get_result();
        PROF_STOP();
        if constexpr (record_history) dump_history_json(history_file);
        if (verbose) {
            beam_log::on_max_turn(cerr);
            beam_log::end_banner(cerr, "max_turn reached", turns_done, param.max_turn, beam_timer.elapsed(), param.ave_width(), (ScoreType)0, false, result.size());
        }
        return result;
    }
};
} // namespace flying_squirrel
