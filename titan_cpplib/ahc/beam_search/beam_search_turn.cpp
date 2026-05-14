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
    using ActionIDType = int;
    vector<Action> result;
    Action DUMMY_ACTION;
    BeamParam* param_ptr;

    bool found_finished;
    ScoreType best_finished_score;
    int best_finished_par_action_id;
    Action best_finished_action;

    struct TreeNode {
        // フィールド順は sizeof(TreeNode) を最小化するために決めている:
        // dir_or_leaf_id + subtree_end の 8 byte が、もともと Action(8-byte align)前の padding
        // だった枠にちょうど収まり、subtree_end 追加のサイズ増を 0 にできる。
        int dir_or_leaf_id;
        // PRE_ORDER 専用: 対応する POST_ORDER の次の index (排他)。skip 時のジャンプ先。
        int subtree_end;
        Action action;
        ActionIDType action_id;
        // leaf: action の target_turn / PRE_ORDER: 部分木 leaf の target_turn の最小値 / POST_ORDER: 未使用
        int target_turn;

        TreeNode(int d, Action a, ActionIDType id, int target)
            : dir_or_leaf_id(d), subtree_end(0), action(move(a)), action_id(id), target_turn(target) {}
    };

    struct BeamCandidate {
        int par;
        ScoreType score;
        HashType hash;
        Action action;
        ActionIDType action_id;
        int target_turn;

        BeamCandidate() : par(0), score(0), hash(0), action(), action_id(0), target_turn(0) {}
        BeamCandidate(int p, ScoreType s, HashType h, Action a, ActionIDType id, int target)
            : par(p), score(s), hash(h), action(move(a)), action_id(id), target_turn(target) {}
    };

    vector<TreeNode> tree, nxt_tree;

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

    vector<uint8_t> is_survived_node;

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

        void set(int k, T v) {
            k += s;
            seg[k] = v;
            while (k > 1) {
                k >>= 1;
                seg[k] = seg[k<<1].first > seg[k<<1|1].first ? seg[k<<1] : seg[k<<1|1];
            }
        }

    public:
        vector<BeamCandidate> next_beam;

        Candidates() {}

        int size() const { return entry; }
        int get_width() const { return beam_width; }

        ScoreType threshold() const { return entry < beam_width ? INF : seg[1].first; }

        bool push(ScoreType score, HashType hash, int par, const Action &action, ActionIDType action_id, vector<uint8_t>& is_survived, int target_turn) {
            if (entry == beam_width && score >= seg[1].first) return false;
            auto dat = func.get_pos(hash);
            int idx = func.inner_get(dat, -1);
            if (idx == -2) {
                // 過去ターンで採用済みのハッシュ → 即 drop
                return false;
            }
            if (idx != -1) {
                if (score < seg[idx+s].first) {
                    is_survived[next_beam[idx].action_id] = 0;
                    next_beam[idx] = {par, score, hash, action, action_id, target_turn};
                    is_survived[action_id] = 1;
                    set(idx, {score, idx});
                    return true;
                }
                return false;
            }
            if (entry < beam_width) {
                func.inner_set(dat, hash, entry);
                next_beam[entry] = {par, score, hash, action, action_id, target_turn};
                is_survived[action_id] = 1;
                hashidx[entry] = hash;
                set(entry, {score, entry});
                entry++;
                return true;
            }
            auto [_, i] = seg[1];
            is_survived[next_beam[i].action_id] = 0;
            next_beam[i] = {par, score, hash, action, action_id, target_turn};
            is_survived[action_id] = 1;
            func.set(hashidx[i], -1);
            func.inner_set(dat, hash, i);
            hashidx[i] = hash;
            set(i, {score, i});
            return true;
        }

        void reset(int w, bool clear_hash) {
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
            if (clear_hash) {
                func.clear();
            } else {
                // 現ビームに居る hash を archived(-2) にマークしてターン跨ぎで dedup する
                for (int i = 0; i < entry; ++i) {
                    func.set(hashidx[i], -2);
                }
            }
            if (func.inner_len() == 1) {
                func = titan23::HashDict<int>(beam_width*8);
            }
            entry = 0;
        }
    };

    vector<Candidates> cands_pool;
    vector<int> turn_to_pool_idx;
    vector<int> free_pool_indices;
    vector<ScoreType> thresholds;
    // update_tree 用の (PRE_ORDER index, dirty) スタック。dirty-bit lazy update に使う。
    vector<pair<int, bool>> pre_stack;

    Candidates& get_cands(int target_turn, int req_w) {
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
            cands_pool[idx].reset(req_w, param_ptr->clear_hash_every_turn);
            thresholds[target_turn] = INF;
        }
        return cands_pool[idx];
    }

    vector<BeamCandidate> current_new_candidates;
    vector<Action> actions;

    void check_survived_capacity(int current_node_id) {
        if (current_node_id >= (int)is_survived_node.size()) {
            int new_size = max((int)is_survived_node.size() * 2 + 1, current_node_id + 1);
            is_survived_node.resize(new_size, 0);
        }
    }

    void get_next_beam(State& state, const int turn) {
        current_new_candidates.clear();

        if (turn == 0) {
            actions.clear();
            state.get_actions(actions, (result.empty() ? DUMMY_ACTION : result.back()), thresholds);
            int parent_id = -1;
            for (Action &action : actions) {
                auto [score, hash, finished] = state.try_op(action, thresholds);
                if (score >= INF) continue;

                int target_turn = action.target_turn;
                if (target_turn > max_turn_global) continue;

                int current_node_id = node_id_counter++;
                check_survived_capacity(current_node_id);

                int status = 0;
                if (finished) {
                    if (!found_finished || score < best_finished_score) {
                        found_finished = true;
                        best_finished_score = score;
                        best_finished_par_action_id = -1;
                        best_finished_action = action;
                    }
                } else {
                    int req_w = param_ptr->get_beam_width(max_turn_global - target_turn, (int)tree.size(), param_ptr->time_limit - beam_timer.elapsed());
                    Candidates& cands = get_cands(target_turn, req_w);
                    if (cands.push(score, hash, 0, action, current_node_id, is_survived_node, target_turn)) {
                        thresholds[target_turn] = cands.threshold();
                        current_new_candidates.push_back({0, score, hash, action, current_node_id, target_turn});
                    } else {
                        status = 1;
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
                    const Action& action = node.action;
                    state.apply_op(action);
                    actions.clear();
                    int parent_id = node.action_id;
                    state.get_actions(actions, action, thresholds);

                    for (Action &child_action : actions) {
                        auto [score, hash, finished] = state.try_op(child_action, thresholds);
                        if (score >= INF) continue;

                        int t_turn = child_action.target_turn;
                        if (t_turn > max_turn_global) continue;

                        int current_node_id = node_id_counter++;
                        check_survived_capacity(current_node_id);

                        int status = 0;
                        if (finished) {
                            if (!found_finished || score < best_finished_score) {
                                found_finished = true;
                                best_finished_score = score;
                                best_finished_par_action_id = parent_id;
                                best_finished_action = child_action;
                            }
                        } else {
                            int req_w = param_ptr->get_beam_width(max_turn_global - t_turn, (int)tree.size(), param_ptr->time_limit - beam_timer.elapsed());
                            Candidates& cands = get_cands(t_turn, req_w);
                            if (cands.push(score, hash, leaf_id, child_action, current_node_id, is_survived_node, t_turn)) {
                                thresholds[t_turn] = cands.threshold();
                                current_new_candidates.push_back({leaf_id, score, hash, child_action, current_node_id, t_turn});
                            } else {
                                status = 1;
                            }
                        }
                        if constexpr (record_history) {
                            history.push_back({current_node_id, parent_id, t_turn, score, hash, child_action.to_string(), state.get_state_info(), status});
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
                state.apply_op(node.action);
                ++i;
            } else {
                state.rollback(node.action);
                ++i;
            }
        }
    }

    void update_tree(State& state, const int turn) {
        nxt_tree.clear();
        if (turn == 0) {
            for (int i = 0; i < (int)current_new_candidates.size(); ++i) {
                const auto &[par, score, hash, new_action, action_id, t_turn] = current_new_candidates[i];
                if (is_survived_node[action_id]) {
                    nxt_tree.emplace_back(0, new_action, action_id, t_turn);
                }
            }
            swap(tree, nxt_tree);
            return;
        }

        int i = 0;
        while (i < tree.size()) {
            int dir_or_leaf_id = tree[i].dir_or_leaf_id;
            if (dir_or_leaf_id == PRE_ORDER && i + 1 < tree.size() && tree[i].action_id == tree.back().action_id) {
                result.emplace_back(tree[i].action);
                state.apply_op(tree[i].action);
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
                    nxt_tree.emplace_back(PRE_ORDER, src.action, src.action_id, INT_MAX);
                    int subtree_min = INT_MAX;
                    int emit_cnt = 0;
                    while (next_beam_idx < num_candidates
                           && current_new_candidates[next_beam_idx].par == dir_or_leaf_id) {
                        const auto& nc = current_new_candidates[next_beam_idx];
                        if (is_survived_node[nc.action_id]) {
                            nxt_tree.emplace_back(dir_or_leaf_id, nc.action, nc.action_id, nc.target_turn);
                            if (nc.target_turn < subtree_min) subtree_min = nc.target_turn;
                            ++emit_cnt;
                        }
                        ++next_beam_idx;
                    }
                    if (emit_cnt > 0) {
                        nxt_tree.emplace_back(POST_ORDER, src.action, src.action_id, 0);
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
                    if (is_survived_node[src.action_id]) {
                        // 通常 emit: propagation なし。
                        nxt_tree.emplace_back(dir_or_leaf_id, src.action, src.action_id, src.target_turn);
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
                nxt_tree.emplace_back(PRE_ORDER, src.action, src.action_id, src.target_turn);
                pre_stack.push_back({pre_idx, false});
            } else {
                if (!nxt_tree.empty()
                    && nxt_tree.back().dir_or_leaf_id == PRE_ORDER
                    && nxt_tree.back().action_id == src.action_id) {
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
                    nxt_tree.emplace_back(POST_ORDER, src.action, src.action_id, 0);
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
    }

    void get_result() {
        int target_action_id = -1;
        bool is_pre_order = false;
        Action best_action;

        if (found_finished) {
            target_action_id = best_finished_par_action_id;
            best_action = best_finished_action;
        } else {
            ScoreType best_score = INF;
            for (int t = 0; t <= max_turn_global; ++t) {
                if (turn_to_pool_idx[t] == -1) continue;
                Candidates& cands = cands_pool[turn_to_pool_idx[t]];
                for (int i = 0; i < cands.size(); ++i) {
                    const auto &[par, score, hash, action, action_id, t_turn] = cands.next_beam[i];
                    if (is_survived_node[action_id]) {
                        if (target_action_id == -1 || score < best_score) {
                            best_score = score;
                            target_action_id = action_id;
                            best_action = action;
                        }
                    }
                }
                if (target_action_id != -1) break;
            }
        }

        if (target_action_id == -1) {
             if (found_finished) {
                 result.emplace_back(best_action);
             }
             return;
        }

        for (const auto& node : tree) {
            int dir_or_leaf_id = node.dir_or_leaf_id;
            const Action& action = node.action;
            int action_id = node.action_id;
            if (dir_or_leaf_id >= 0) {
                if (action_id == target_action_id) {
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
        best_finished_par_action_id = -1;
        max_turn_global = param.max_turn;
        param_ptr = &param;
        DUMMY_ACTION.target_turn = -1;

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
                // ターン行: best はこの turn の primary pool (target_turn==turn+1) 相当を出す
                ScoreType best_for_log = 0;
                bool has_best = false;
                for (int t = turn + 1; t <= max_turn_global; ++t) {
                    if (turn_to_pool_idx[t] == -1) continue;
                    Candidates& cands = cands_pool[turn_to_pool_idx[t]];
                    for (int i = 0; i < cands.size(); ++i) {
                        ScoreType s = cands.next_beam[i].score;
                        if (!has_best || s < best_for_log) { best_for_log = s; has_best = true; }
                    }
                    if (has_best) break;
                }
                int w = param.beam_width;
                beam_log::turn_line(cerr, turn + 1, param.max_turn, now_time,
                                    w, (int)tree.size(), (int)current_new_candidates.size(),
                                    best_for_log);
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
                        int id = cands.next_beam[i].action_id;
                        if(is_survived_node[id]) active_ids.push_back(id);
                    }
                }
                snapshots.push_back({turn + 1, active_ids});
            }

            // 探索木の更新
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
            update_tree(state, turn);

            if (turn_to_pool_idx[turn] != -1) {
                free_pool_indices.push_back(turn_to_pool_idx[turn]);
                turn_to_pool_idx[turn] = -1;
                thresholds[turn] = INF;
            }

            param.timestamp(tree.size(), current_new_candidates.size(), beam_timer.elapsed()-now_time);
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
