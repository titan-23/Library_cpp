#pragma once
#include <bits/stdc++.h>
#include "titan_cpplib/others/print.cpp"
#include "titan_cpplib/algorithm/random.cpp"
#include "titan_cpplib/ahc/timer.cpp"
#include "titan_cpplib/ds/hash_dict.cpp"
#include "titan_cpplib/ahc/beam_search/beam_param.cpp"
#include "titan_cpplib/ahc/beam_search/beam_log.cpp"

using namespace std;

namespace flying_squirrel {

template<typename ScoreType, typename HashType, class Action, class State, ScoreType INF, bool record_history=true>
class BeamSearchWithTree {
private:
    static constexpr const int PRE_ORDER = -1;
    static constexpr const int POST_ORDER = -2;
    titan23::Random rnd;
    titan23::Timer beam_timer;
    // ---- Action フラット pool + free_slots + mark-and-sweep ------------------
    // Action 実体は action_pool[slot] に 1 回だけ置く。
    // TreeNode / BeamCandidate / current_new_candidates は実体でなく 4B の
    // slot index (= ActionId = aid) を運ぶ。aid は候補の一意キーとしても兼用し、
    // is_survived_node を action_pool 添字 (= aid) で引く。これにより
    // is_survived_node のサイズは action_pool 規模に bounded される
    // (旧設計の node_id_counter 添字は単調増加で 4M 級に膨らんでいた)。
    // 採用候補 1 つにつき: arena_put_reserve → cands.push(aid) → 採否で fill/release。
    // 各メタターン末 (update_tree の swap 後) に mark-and-sweep:
    //   step1: 新 tree の aid に current_alive_gen を立てる
    //   step2: 旧 tree (swap 後の nxt_tree) と current_new_candidates を走査し、
    //          alive_gen が現世代でない aid を free_slots に push (dedup あり)
    // 再利用は LIFO で、新規 arena_put_reserve 時に slot を pop。compaction は
    // 行わないので action_pool は穴あきになるが、再利用でメモリは tree + 候補プール
    // 規模に収まる。
    using ActionId = int;
    static constexpr ActionId BAD_ID = -1;
    vector<Action> result;
    Action DUMMY_ACTION;
    BeamParam* param_ptr;

    bool found_finished;
    ScoreType best_finished_score;
    ActionId best_finished_par_aid;
    Action best_finished_action;

    struct TreeNode {
        int dir_or_leaf_id;
        // PRE_ORDER 専用: 対応する POST_ORDER の次の index (排他)。skip 時のジャンプ先。
        int subtree_end;
        // Action 実体は action_pool に置き、ここでは 4B の slot index だけ持つ。
        // aid は候補一意キーも兼ねるので、PRE/POST/leaf trio は同 aid を共有する。
        ActionId aid;
        // leaf: action の target_turn / PRE_ORDER: 部分木 leaf の target_turn の最小値 / POST_ORDER: 未使用
        int target_turn;

        TreeNode(int d, ActionId a, int target)
            : dir_or_leaf_id(d), subtree_end(0), aid(a), target_turn(target) {}
    };

    struct BeamCandidate {
        // 8B-align メンバ (ScoreType / HashType) を先頭に並べる。sort/heap で 1
        // meta-turn あたり数十〜数百 K 回 swap されるので、ここの幅が hot。
        ScoreType score;
        HashType hash;
        int par;
        ActionId aid;
        int target_turn;

        BeamCandidate() : score(0), hash(0), par(0), aid(BAD_ID), target_turn(0) {}
        BeamCandidate(int p, ScoreType s, HashType h, ActionId a, int target)
            : score(s), hash(h), par(p), aid(a), target_turn(target) {}
    };

    vector<TreeNode> tree, nxt_tree;

    // ---- Action フラット pool 実体 -----------------------------------------
    vector<Action> action_pool;      // action_pool[slot] = Action 実体
    vector<int> free_slots;          // 解放済み slot の LIFO。arena_put が pop して再利用。
    vector<uint32_t> alive_gen;      // mark-and-sweep 用の slot 毎タグ。
    uint32_t current_alive_gen;      // mark phase で印を打つ世代カウンタ。

    Action& act(ActionId id) {
        return action_pool[(size_t)id];
    }
    // 2 段 arena_put: cands.push の前に reserve で slot を取り (Action コピーなし)、
    // 採用なら fill で実体を書き込み、棄却なら release で slot を戻す。
    // is_survived_node も aid 添字なので、新規 slot 生成時に同時に伸長する。
    ActionId arena_put_reserve() {
        ActionId slot;
        if (!free_slots.empty()) {
            slot = free_slots.back();
            free_slots.pop_back();
        } else {
            slot = (ActionId)action_pool.size();
            action_pool.emplace_back();
            alive_gen.push_back(0);
            is_survived_node.push_back(0);
        }
        return slot;
    }
    void arena_put_fill(ActionId slot, const Action& a) {
        action_pool[(size_t)slot] = a;
    }
    void arena_release(ActionId slot) {
        free_slots.push_back(slot);
    }
    // tree に残らなかった aid を free_slots に戻す。update_tree 末 (swap 後) に呼ぶ。
    //   step1: 新 tree (= 現 tree) の aid に current_alive_gen を立てる。
    //   step2: 旧 tree (= swap 後の nxt_tree) と current_new_candidates を走査し、
    //          alive_gen が現世代でない aid を free_slots に push。
    //          push 後に alive_gen を current_alive_gen へ更新して dedup。
    void mark_and_sweep() {
        ++current_alive_gen;
        for (const auto& nd : tree) {
            alive_gen[(size_t)nd.aid] = current_alive_gen;
        }
        for (const auto& nd : nxt_tree) {
            const size_t s = (size_t)nd.aid;
            if (alive_gen[s] != current_alive_gen) {
                free_slots.push_back(nd.aid);
                alive_gen[s] = current_alive_gen;
            }
        }
        for (const auto& nc : current_new_candidates) {
            const size_t s = (size_t)nc.aid;
            if (alive_gen[s] != current_alive_gen) {
                free_slots.push_back(nc.aid);
                alive_gen[s] = current_alive_gen;
            }
        }
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

    // is_survived_node は aid 添字。 arena_put_reserve で action_pool 拡張と
    // 同時に伸ばすので、check_survived_capacity は不要。
    vector<uint8_t> is_survived_node;
    // record_history=true のときだけ aid → node_id 写像を持つ (history JSON 用)。
    vector<int> aid_to_node_id;

    void dump_history_json(const string& filename) const {
        ofstream ofs(filename);
        if(!ofs) return;
        ofs << "{\n  \"INF\": " << INF << ",\n  \"nodes\": [\n";
        for (size_t i = 0; i < history.size(); ++i) {
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
        for (size_t i = 0; i < snapshots.size(); ++i) {
            ofs << "    {\n"
                << "      \"turn\": " << snapshots[i].turn << ",\n"
                << "      \"active_node_ids\": [";
            for (size_t j = 0; j < snapshots[i].active_node_ids.size(); ++j) {
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
        vector<HashType> hashidx;
        titan23::HashDict<int> func;
        int beam_width, entry;
        int s = 1;
        vector<T> seg;

        // fill phase 中は segtree を触らず、entry == beam_width 到達で build_segtree()。
        // is_built==false の間 seg[1] / seg[idx+s] は参照禁止。
        bool is_built = false;

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

        // entry 個の next_beam[] から segtree を O(W) で一括構築。
        // [entry, s) のリーフは reset で {-INF, -1} 済みなので触らない。
        void build_segtree() {
            for (int i = 0; i < entry; ++i) {
                seg[i + s] = {next_beam[i].score, i};
            }
            for (int k = s - 1; k > 0; --k) {
                seg[k] = seg[k<<1].first > seg[k<<1|1].first ? seg[k<<1] : seg[k<<1|1];
            }
        }

    public:
        vector<BeamCandidate> next_beam;

        Candidates() {}

        int size() const { return entry; }
        int get_width() const { return beam_width; }

        ScoreType threshold() const { return entry < beam_width ? INF : seg[1].first; }

        // 受理した場合 next_beam のインデックスを返し、棄却なら -1。
        // aid は呼び出し側が arena_put_reserve で取って渡す (棄却時は arena_release)。
        // is_survived は aid 添字で更新する: 採用 aid に 1、追い出した aid に 0。
        int push(ScoreType score, HashType hash, int par, ActionId aid, vector<uint8_t>& is_survived, int target_turn) {
            // 早期 reject は segtree 構築済みのときのみ。
            if (is_built && score >= seg[1].first) return -1;
            auto dat = func.get_pos(hash);
            int idx = func.inner_get(dat, -1);
            if (idx != -1) {
                // 既存 slot のスコアは next_beam[idx].score から読む。fill phase で seg 無効でも一貫。
                if (score < next_beam[idx].score) {
                    is_survived[next_beam[idx].aid] = 0;
                    next_beam[idx] = {par, score, hash, aid, target_turn};
                    is_survived[aid] = 1;
                    if (is_built) {
                        set(idx, {score, idx});
                    }
                    return idx;
                }
                return -1;
            }
            if (entry < beam_width) {
                // fill phase: segtree は触らず append のみ。
                int slot = entry;
                func.inner_set(dat, hash, slot);
                next_beam[slot] = {par, score, hash, aid, target_turn};
                is_survived[aid] = 1;
                hashidx[slot] = hash;
                entry++;
                if (entry == beam_width) {
                    // 満杯到達で一括構築。
                    build_segtree();
                    is_built = true;
                }
                return slot;
            }
            // entry == beam_width (is_built == true): worst を置換。
            auto [_, i] = seg[1];
            is_survived[next_beam[i].aid] = 0;
            next_beam[i] = {par, score, hash, aid, target_turn};
            is_survived[aid] = 1;
            func.set(hashidx[i], -1);
            func.inner_set(dat, hash, i);
            hashidx[i] = hash;
            set(i, {score, i});
            return i;
        }

        void reset(int w) {
            beam_width = w;
            while (s < w) {
                s <<= 1;
            }
            if (seg.size() < 2*s) {
                seg.resize(2*s);
            }
            fill(seg.begin(), seg.begin()+(2*s), make_pair(-INF, -1));
            if (hashidx.size() < w) {
                hashidx.resize(w);
                next_beam.resize(w);
            }
            // 同 target_turn の pool 内では毎回 clear。
            // 「全ターン通して一度見た hash」の dedup は BeamSearchWithTree::seen_hash が担う。
            func.clear();
            if (func.inner_len() == 1) {
                func = titan23::HashDict<int>(beam_width*8);
            }
            entry = 0;
            is_built = false;
        }
    };

    vector<Candidates> cands_pool;
    vector<int> turn_to_pool_idx;
    vector<int> free_pool_indices;
    vector<ScoreType> thresholds;
    // update_tree 用の (PRE_ORDER index, dirty) スタック。dirty-bit lazy update に使う。
    vector<pair<int, bool>> pre_stack;

    // clear_hash_every_turn=false のとき、全ターン通して一度見た hash を dedup する。
    //   value = (best_score_at_smallest_t, smallest_t)
    // - 未登録                       → 通す
    // - 既登録 (s0, t0) で t < t0    → 通す (より小さい target_turn 側で再挑戦)
    // - 既登録 (s0, t0) で t==t0 && score < s0 → 通す (同じ最小 t で score 改善)
    // - その他                       → drop
    // 既に大きい target_turn の pool に居る同 hash entry は触らない。
    titan23::HashDict<pair<ScoreType, int>> seen_hash;
    bool use_global_seen;

    // update_tree 末尾で集計する「tree 内 leaf の target_turn の最小値」。
    // get_cands で remain_meta の計算に使う。
    int current_min_target_in_tree;

    // 1 メタターン中に「target_turn == 現 turn」の leaf を展開した回数。
    // BeamParam::timestamp_meta の applied_w としてフィードする。
    int expanded_leaf_count;

    // 新規 pool 確保時にだけビーム幅を推定する。
    // 残時間と「1 メタターン dt の EMA」から W を線形スケーリング
    // (= フィードバック制御) で求める。詳細は BeamParam::recommend_width 参照。
    int compute_req_w(int /*target_turn*/) {
        if (!param_ptr->is_adjusting) return param_ptr->beam_width;

        // calibration: EMA が成熟するまで固定幅
        if (param_ptr->meta_sample_count < param_ptr->calibration_meta_count
            || !param_ptr->cost_model_ready()) {
            return param_ptr->beam_width;
        }

        // 残メタターン数の推定。ema_step を優先し、未初期化なら累積平均、それでも無ければ 1。
        double ave_step;
        if (param_ptr->ema_step > 0.0) {
            ave_step = max(0.5, param_ptr->ema_step);
        } else if (param_ptr->target_step_count > 0) {
            ave_step = max(0.5,
                (double)param_ptr->target_step_sum / (double)param_ptr->target_step_count);
        } else {
            ave_step = 1.0;
        }
        int base = max_turn_global - current_min_target_in_tree;
        if (base < 0) base = 0;
        int remain_meta = max(1, (int)ceil((double)base / ave_step));

        // 残時間 [ms]
        double remain_time_ms = param_ptr->time_limit - beam_timer.elapsed();
        if (remain_time_ms <= 0.0) return 1;

        int rec = param_ptr->recommend_width(remain_time_ms, remain_meta);
        if (rec < 0) return param_ptr->beam_width;
        return rec;
    }

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
            int req_w = compute_req_w(target_turn);
            cands_pool[idx].reset(req_w);
            thresholds[target_turn] = INF;
        }
        return cands_pool[idx];
    }

    vector<BeamCandidate> current_new_candidates;
    vector<Action> actions;

    void get_next_beam(State& state, const int turn) {
        current_new_candidates.clear();
        expanded_leaf_count = 0;

        if (turn == 0) {
            // 初期状態を 1 つの leaf として展開する扱い
            expanded_leaf_count = 1;
            actions.clear();
            state.get_actions(actions, (result.empty() ? DUMMY_ACTION : result.back()), thresholds);
            int parent_id = -1;
            for (Action &action : actions) {
                auto [score, hash, finished] = state.try_op(action, thresholds);
                if (score >= INF) continue;

                int target_turn = action.target_turn;
                if (target_turn > max_turn_global) continue;

                // global seen_hash チェック
                pair<int, bool> seen_pos;
                bool seen_exists = false;
                ScoreType seen_s0 = 0;
                int seen_t0 = 0;
                if (use_global_seen) {
                    seen_pos = seen_hash.get_pos(hash);
                    if (seen_pos.second) {
                        seen_exists = true;
                        auto sv = seen_hash.inner_get(seen_pos);
                        seen_s0 = sv.first;
                        seen_t0 = sv.second;
                        bool pass = (target_turn < seen_t0) || (target_turn == seen_t0 && score < seen_s0);
                        if (!pass) continue;
                    }
                }

                int status = 0;
                int current_node_id = -1;
                if (finished) {
                    if (!found_finished || score < best_finished_score) {
                        found_finished = true;
                        best_finished_score = score;
                        best_finished_par_aid = BAD_ID;  // 仮想 root の子: 親 aid なし
                        best_finished_action = action;
                    }
                    if constexpr (record_history) current_node_id = node_id_counter++;
                } else {
                    ActionId aid = arena_put_reserve();
                    Candidates& cands = get_cands(target_turn);
                    int slot = cands.push(score, hash, 0, aid, is_survived_node, target_turn);
                    if (slot >= 0) {
                        arena_put_fill(aid, action);
                        thresholds[target_turn] = cands.threshold();
                        current_new_candidates.push_back({0, score, hash, aid, target_turn});
                        if (use_global_seen) {
                            seen_hash.inner_set(seen_pos, hash, {score, target_turn});
                        }
                        if constexpr (record_history) {
                            if ((size_t)aid >= aid_to_node_id.size()) aid_to_node_id.resize((size_t)aid + 1, -1);
                            current_node_id = node_id_counter++;
                            aid_to_node_id[aid] = current_node_id;
                        }
                    } else {
                        arena_release(aid);
                        status = 1;
                        if constexpr (record_history) current_node_id = node_id_counter++;
                    }
                }
                if constexpr (record_history) {
                    history.push_back({current_node_id, parent_id, target_turn, score, hash, action.to_string(), state.get_state_info(), status});
                }
            }
            return;
        }

        int leaf_id = 0;
        const int tree_size = (int)tree.size();
        for (int i = 0; i < tree_size; ) {
            TreeNode& node = tree[i];
            const int dir_or_leaf_id = node.dir_or_leaf_id;
            if (dir_or_leaf_id >= 0) {
                if (node.target_turn == turn) {
                    ++expanded_leaf_count;
                    // ループ内の arena_put_reserve/fill が action_pool を reallocate
                    // しうるので、参照のまま保持できない。ローカルに 1 度 copy しておく。
                    Action action = act(node.aid);
                    state.apply_op(action);
                    actions.clear();
                    ActionId parent_aid = node.aid;
                    state.get_actions(actions, action, thresholds);

                    for (Action &child_action : actions) {
                        auto [score, hash, finished] = state.try_op(child_action, thresholds);
                        if (score >= INF) continue;

                        int t_turn = child_action.target_turn;
                        if (t_turn > max_turn_global) continue;

                        // global seen_hash チェック
                        pair<int, bool> seen_pos;
                        bool seen_exists = false;
                        ScoreType seen_s0 = 0;
                        int seen_t0 = 0;
                        if (use_global_seen) {
                            seen_pos = seen_hash.get_pos(hash);
                            if (seen_pos.second) {
                                seen_exists = true;
                                auto sv = seen_hash.inner_get(seen_pos);
                                seen_s0 = sv.first;
                                seen_t0 = sv.second;
                                bool pass = (t_turn < seen_t0) || (t_turn == seen_t0 && score < seen_s0);
                                if (!pass) continue;
                            }
                        }

                        int status = 0;
                        int current_node_id = -1;
                        if (finished) {
                            if (!found_finished || score < best_finished_score) {
                                found_finished = true;
                                best_finished_score = score;
                                best_finished_par_aid = parent_aid;
                                best_finished_action = child_action;
                            }
                            if constexpr (record_history) current_node_id = node_id_counter++;
                        } else {
                            ActionId aid = arena_put_reserve();
                            Candidates& cands = get_cands(t_turn);
                            int slot = cands.push(score, hash, leaf_id, aid, is_survived_node, t_turn);
                            if (slot >= 0) {
                                arena_put_fill(aid, child_action);
                                thresholds[t_turn] = cands.threshold();
                                current_new_candidates.push_back({leaf_id, score, hash, aid, t_turn});
                                if (use_global_seen) {
                                    seen_hash.inner_set(seen_pos, hash, {score, t_turn});
                                }
                                if constexpr (record_history) {
                                    if ((size_t)aid >= aid_to_node_id.size()) aid_to_node_id.resize((size_t)aid + 1, -1);
                                    current_node_id = node_id_counter++;
                                    aid_to_node_id[aid] = current_node_id;
                                }
                            } else {
                                arena_release(aid);
                                status = 1;
                                if constexpr (record_history) current_node_id = node_id_counter++;
                            }
                        }
                        if constexpr (record_history) {
                            int parent_node_id = (parent_aid == BAD_ID || (size_t)parent_aid >= aid_to_node_id.size()) ? -1 : aid_to_node_id[parent_aid];
                            history.push_back({current_node_id, parent_node_id, t_turn, score, hash, child_action.to_string(), state.get_state_info(), status});
                        }
                    }
                    node.dir_or_leaf_id = leaf_id;
                    ++leaf_id;
                    state.rollback(action);
                } else {
                    node.dir_or_leaf_id = leaf_id;
                    ++leaf_id;
                }
                ++i;
            } else if (dir_or_leaf_id == PRE_ORDER) {
                // 部分木中の leaf がすべて future-turn なら state を進めずに POST_ORDER の次までジャンプ。
                // PRE_ORDER では target_turn を「部分木 leaf の target_turn 最小値」として再利用している。
                if (node.target_turn > turn) {
                    i = node.subtree_end;
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

    // tree の root レベルを走査して leaf の target_turn 最小値を求める。
    // PRE_ORDER の subtree_end を使って subtree 中身を一気に skip するので、
    // root レベルのノード数に比例した軽い走査で済む。
    void update_current_min_target_in_tree() {
        int min_t = INT_MAX;
        const int n = (int)tree.size();
        int k = 0;
        while (k < n) {
            const TreeNode& nd = tree[k];
            int d = nd.dir_or_leaf_id;
            if (d >= 0) {
                if (nd.target_turn < min_t) min_t = nd.target_turn;
                ++k;
            } else if (d == PRE_ORDER) {
                if (nd.target_turn < min_t) min_t = nd.target_turn;
                k = nd.subtree_end;
            } else {
                ++k;
            }
        }
        current_min_target_in_tree = (min_t == INT_MAX) ? max_turn_global : min_t;
    }

    void update_tree(State& state, const int turn) {
        // メタターン開始時の min target_turn。終了時の min との差分を 1 メタターン進行量として集計する。
        const int prev_min_target = current_min_target_in_tree;
        nxt_tree.clear();
        if (turn == 0) {
            for (int i = 0; i < (int)current_new_candidates.size(); ++i) {
                const auto &[score, hash, par, aid, t_turn] = current_new_candidates[i];
                if (is_survived_node[aid]) {
                    nxt_tree.emplace_back(0, aid, t_turn);
                }
            }
            swap(tree, nxt_tree);
            update_current_min_target_in_tree();
            mark_and_sweep();
            int delta = current_min_target_in_tree - prev_min_target;
            if (delta > 0) param_ptr->note_target_step(delta);
            return;
        }

        int i = 0;
        while (i < tree.size()) {
            int dir_or_leaf_id = tree[i].dir_or_leaf_id;
            if (dir_or_leaf_id == PRE_ORDER && i + 1 < tree.size() && tree[i].aid == tree.back().aid) {
                result.emplace_back(act(tree[i].aid));
                state.apply_op(act(tree[i].aid));
                tree.pop_back();
                ++i;
            } else {
                break;
            }
        }

        // dirty-bit lazy update:
        //   PRE_ORDER を src から copy emit する時、target_turn には src の旧 subtree_min を
        //   そのまま入れる ( stable ならこれで正しい)。dirty=false で push。
        //   dirty 化は「inherited subtree_min への寄与を変えうる変更」に限定する:
        //     - target_turn == 親 inherited の leaf が evict / 展開 / 空 subtree pop
        //     - もしくは新 subtree_min が親 inherited を下回る (decrease)
        //   POST_ORDER close 時、dirty=true の subtree だけ直接の子を線形 scan して subtree_min
        //   を再計算する。recompute 結果が inherited と同値なら親への伝播もスキップ。
        pre_stack.clear();
        nxt_tree.reserve((size_t)tree.size() + (size_t)current_new_candidates.size());

        int next_beam_idx = 0;
        const int num_candidates = (int)current_new_candidates.size();
        const int tree_size = (int)tree.size();
        for (; i < tree_size; ++i) {
            TreeNode& src = tree[i];
            const int dir_or_leaf_id = src.dir_or_leaf_id;
            if (dir_or_leaf_id >= 0) {
                if (src.target_turn == turn) {
                    // 1 パス展開: 仮に PRE_ORDER を先に emit し、生存子を直接 emit する。
                    // 生存子 0 件なら巻き戻す。is_survived_node の二度読みを回避する。
                    int pre_idx = (int)nxt_tree.size();
                    nxt_tree.emplace_back(PRE_ORDER, src.aid, INT_MAX);
                    int subtree_min = INT_MAX;
                    int emit_cnt = 0;
                    while (next_beam_idx < num_candidates
                           && current_new_candidates[next_beam_idx].par == dir_or_leaf_id) {
                        const auto& nc = current_new_candidates[next_beam_idx];
                        if (is_survived_node[nc.aid]) {
                            nxt_tree.emplace_back(dir_or_leaf_id, nc.aid, nc.target_turn);
                            if (nc.target_turn < subtree_min) subtree_min = nc.target_turn;
                            ++emit_cnt;
                        }
                        ++next_beam_idx;
                    }
                    if (emit_cnt > 0) {
                        nxt_tree.emplace_back(POST_ORDER, src.aid, 0);
                        nxt_tree[pre_idx].target_turn = subtree_min;
                        nxt_tree[pre_idx].subtree_end = (int)nxt_tree.size();
                    } else {
                        nxt_tree.pop_back();
                    }
                    // 旧 leaf が親 inherited に寄与していた / 新 subtree_min が親 inherited を下回る
                    // 場合のみ親を dirty 化する。
                    if (!pre_stack.empty()) {
                        auto& top = pre_stack.back();
                        int gp_min = nxt_tree[top.first].target_turn;
                        if (src.target_turn == gp_min || (emit_cnt > 0 && subtree_min < gp_min)) {
                            top.second = true;
                        }
                    }
                } else {
                    if (is_survived_node[src.aid]) {
                        // 通常 emit: propagation なし。
                        nxt_tree.emplace_back(dir_or_leaf_id, src.aid, src.target_turn);
                    } else {
                        // evict → inherited min に寄与していたときだけ親を dirty 化。
                        if (!pre_stack.empty()) {
                            auto& top = pre_stack.back();
                            if (src.target_turn == nxt_tree[top.first].target_turn) {
                                top.second = true;
                            }
                        }
                    }
                }
            } else if (dir_or_leaf_id == PRE_ORDER) {
                // src.target_turn は前ターンの subtree_min。stable ならこの値がそのまま使える。
                int pre_idx = (int)nxt_tree.size();
                nxt_tree.emplace_back(PRE_ORDER, src.aid, src.target_turn);
                pre_stack.push_back({pre_idx, false});
            } else {
                if (!nxt_tree.empty()
                    && nxt_tree.back().dir_or_leaf_id == PRE_ORDER
                    && nxt_tree.back().aid == src.aid) {
                    // 空サブツリー: PRE_ORDER を取り消し、POST_ORDER も emit しない。
                    int popped_min = nxt_tree.back().target_turn;
                    nxt_tree.pop_back();
                    pre_stack.pop_back();
                    // 消えた subtree が親 inherited に寄与していたときだけ親を dirty 化。
                    if (!pre_stack.empty()) {
                        auto& top = pre_stack.back();
                        if (popped_min == nxt_tree[top.first].target_turn) {
                            top.second = true;
                        }
                    }
                } else {
                    int pre_idx = pre_stack.back().first;
                    bool dirty = pre_stack.back().second;
                    int old_min = nxt_tree[pre_idx].target_turn;
                    nxt_tree.emplace_back(POST_ORDER, src.aid, 0);
                    nxt_tree[pre_idx].subtree_end = (int)nxt_tree.size();
                    pre_stack.pop_back();
                    if (dirty) {
                        // 直接の子だけ線形 scan して subtree_min を再計算。
                        // PRE_ORDER の子は subtree_end で nest 中身を skip できるので O(direct children)。
                        int min_t = INT_MAX;
                        int k = pre_idx + 1;
                        const int end_excl = (int)nxt_tree.size() - 1; // POST_ORDER の直前まで
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
                        // recompute 結果が inherited と異なり、かつ親 inherited に影響しうるときだけ伝播。
                        if (min_t != old_min && !pre_stack.empty()) {
                            auto& top = pre_stack.back();
                            int gp_min = nxt_tree[top.first].target_turn;
                            if (old_min == gp_min || min_t < gp_min) {
                                top.second = true;
                            }
                        }
                    }
                    // else: emit 時の src.target_turn (= 旧 subtree_min) がそのまま正しい
                }
            }
        }

        swap(tree, nxt_tree);
        update_current_min_target_in_tree();
        mark_and_sweep();
        int delta = current_min_target_in_tree - prev_min_target;
        if (delta > 0) param_ptr->note_target_step(delta);
    }

    void get_result() {
        ActionId target_aid = BAD_ID;
        bool is_pre_order = false;
        Action best_action;

        if (found_finished) {
            target_aid = best_finished_par_aid;
            best_action = best_finished_action;
        } else {
            ScoreType best_score = INF;
            for (int t = 0; t <= max_turn_global; ++t) {
                if (turn_to_pool_idx[t] == -1) continue;
                Candidates& cands = cands_pool[turn_to_pool_idx[t]];
                for (int i = 0; i < cands.size(); ++i) {
                    const auto &[score, hash, par, aid, t_turn] = cands.next_beam[i];
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
        beam_timer.reset();
        rnd = titan23::Random();
        node_id_counter = 0;
        history.clear();
        snapshots.clear();
        result.clear();
        tree.clear();
        nxt_tree.clear();
        current_new_candidates.clear();
        found_finished = false;
        best_finished_score = INF;
        best_finished_par_aid = BAD_ID;
        max_turn_global = param.max_turn;
        param_ptr = &param;
        DUMMY_ACTION.target_turn = -1;

        // Action フラット pool の初期化。
        action_pool.clear();
        free_slots.clear();
        alive_gen.clear();
        current_alive_gen = 0;
        if constexpr (record_history) aid_to_node_id.clear();

        if (turn_to_pool_idx.size() < max_turn_global + 1) {
            turn_to_pool_idx.resize(max_turn_global + 1);
        }
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

        // 開始時の「tree 内 leaf の最小 target_turn」は 0 起点
        current_min_target_in_tree = 0;
        expanded_leaf_count = 0;

        // global seen_hash の初期化。clear_hash_every_turn==false のときだけ有効化
        use_global_seen = !param.clear_hash_every_turn;
        if (use_global_seen) {
            int cap = param.seen_hash_capacity_hint;
            if (cap <= 0) {
                cap = max(1 << 14, param.beam_width * max(1, max_turn_global) * 2);
            }
            seen_hash = titan23::HashDict<pair<ScoreType, int>>(cap);
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
            double now_time = beam_timer.elapsed();

            get_next_beam(state, turn);
            // dt_expand: get_next_beam の時間 (W に比例する成分の計測)。
            // verbose / record_history のオーバーヘッドを含めないためここで一度時刻を取る。
            double dt_expand_ms = beam_timer.elapsed() - now_time;

            if (found_finished) {
                turns_done = turn + 1;
                get_result();
                if constexpr (record_history) dump_history_json(history_file);
                if (verbose) {
                    beam_log::on_solution_found(cerr, turns_done, best_finished_score);
                    beam_log::end_banner(cerr, "solution found", turns_done, param.max_turn,
                                         beam_timer.elapsed(), param.ave_width(),
                                         best_finished_score, true, (int)result.size());
                }
                return result;
            }

            if (verbose) {
                // ターン行: best はこの turn の primary pool (target_turn==turn+1) 相当を出す。
                // w も同じ pool の動的 beam_width を表示する。
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
                beam_log::turn_line(cerr, turn + 1, param.max_turn, now_time,
                                    w, (int)tree.size(), (int)current_new_candidates.size(),
                                    -1, best_for_log);
                if (!has_best) {
                    // best が無いケースだけ補助行で示す
                    beam_log::turn_line_extra(cerr, "(no candidates at this turn yet)");
                }
            }

            if constexpr (record_history) {
                vector<int> active_ids;
                for (int t = turn; t <= max_turn_global; ++t) {
                    if (turn_to_pool_idx[t] == -1) continue;
                    Candidates& cands = cands_pool[turn_to_pool_idx[t]];
                    for(int i = 0; i < cands.size(); ++i) {
                        ActionId aid = cands.next_beam[i].aid;
                        if (is_survived_node[aid]) {
                            int node_id = ((size_t)aid < aid_to_node_id.size()) ? aid_to_node_id[aid] : -1;
                            if (node_id >= 0) active_ids.push_back(node_id);
                        }
                    }
                }
                snapshots.push_back({turn + 1, active_ids});
            }

            // 探索木の更新
            // dt_update: sort + update_tree の時間 ((tree+exp) に比例する成分の計測)。
            // verbose / record_history のオーバーヘッドを除いてここから時刻を取り直す。
            double t_update_start = beam_timer.elapsed();
            if (turn != 0) {
                sort(current_new_candidates.begin(), current_new_candidates.end(),
                    [] (const auto& a, const auto& b) {
                        if (a.par != b.par) return a.par < b.par;
                        return a.score < b.score;
                    }
                );
            } else {
                sort(current_new_candidates.begin(), current_new_candidates.end(),
                    [] (const auto& a, const auto& b) {
                        return a.score < b.score;
                    }
                );
            }
            int prev_min_target = current_min_target_in_tree;
            update_tree(state, turn);
            int delta_target = current_min_target_in_tree - prev_min_target;
            if (delta_target < 0) delta_target = 0;
            double dt_update_ms = beam_timer.elapsed() - t_update_start;

            if (turn_to_pool_idx[turn] != -1) {
                free_pool_indices.push_back(turn_to_pool_idx[turn]);
                turn_to_pool_idx[turn] = -1;
                thresholds[turn] = INF;
            }

            param.timestamp_meta(dt_expand_ms, dt_update_ms,
                                 (int)tree.size(), (int)current_new_candidates.size(),
                                 expanded_leaf_count, delta_target);
            turns_done = turn + 1;
        }

        get_result();
        if constexpr (record_history) dump_history_json(history_file);
        if (verbose) {
            beam_log::on_max_turn(cerr);
            beam_log::end_banner(cerr, "max_turn reached", turns_done, param.max_turn,
                                 beam_timer.elapsed(), param.ave_width(),
                                 (ScoreType)0, false, (int)result.size());
        }
        return result;
    }
};
} // namespace flying_squirrel
