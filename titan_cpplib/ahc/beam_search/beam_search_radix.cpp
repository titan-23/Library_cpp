/// https://github.com/titan-23/Library_cpp/blob/main/titan_cpplib/ahc/beam_search/beam_search_radix.cpp
#pragma once
#include <bits/stdc++.h>
#include "titan_cpplib/ahc/timer.cpp"
#include "titan_cpplib/ahc/beam_search/beam_param.cpp"
#include "titan_cpplib/ahc/beam_search/beam_log.cpp"
#include "titan_cpplib/ahc/beam_search/candidates.cpp"

using namespace std;

namespace flying_squirrel {

// 圧縮辺木ビームサーチ
// 生存木を明示ノードで持ち、辺 = 合成済み Action
// - 単一子になった内部ノードは即座に縮約し、親子の辺を Action::compose で 1 本にまとめる
// - 根直下の単一子辺は合成せず確定列 result_prefix へ移す
// - compose が false を返した辺は単一子のまま残す
// - 手術は DFS 後の state が根にいる状態で行うため、
//   適用済み action が合成で書き換わる問題は起きない
template<typename ScoreType, typename HashType, class Action, class State, ScoreType INF>
class BeamSearchRadix {
private:
    static constexpr int NIL = -1;

    struct Node {
        Action edge;   // 親からこのノードへの合成済み Action
        int parent;
        int child_cnt;
        int first_child;
        int prev_sibling, next_sibling;
        ScoreType score;  // 葉: 自身のスコア / 内部: 部分木最小 (monotone_skip 時のみ維持)
    };

    titan23::Timer beam_timer;
    Action DAMMY_ACTION;

    vector<Node> pool;
    vector<int> free_ids;
    int root;
    int live_nodes;

    vector<int> leaves, next_leaves;
    vector<Action> result_prefix;   // 確定した根側の Action 列
    vector<Action> actions;         // 旧式 enumerate_actions 用
    vector<int> visited_leaves;     // 今ターン展開した葉。cand の parent_leaf はこの序数
    vector<int> bucket_cnt, bucket_items;
    vector<int> stk;                // DFS 用作業領域。(node << 1) | phase
    vector<int> porder;

    Candidates<ScoreType, HashType, Action, State, INF, false> candidates;

    bool found_finished;
    ScoreType best_finished_score;
    vector<Action> best_finished_path;

    bool monotone_skip = false;
    int explored_per_turn;

    int new_node() {
        int id;
        if (!free_ids.empty()) {
            id = free_ids.back();
            free_ids.pop_back();
        } else {
            id = (int)pool.size();
            pool.emplace_back();
        }
        Node &n = pool[id];
        n.parent = NIL;
        n.child_cnt = 0;
        n.first_child = NIL;
        n.prev_sibling = NIL;
        n.next_sibling = NIL;
        ++live_nodes;
        return id;
    }

    // edge は解放しない。再利用時の move 代入が古い実体を潰す
    void free_node(int id) {
        free_ids.push_back(id);
        --live_nodes;
    }

    void unlink_child(int p, int c) {
        Node &nc = pool[c];
        if (nc.prev_sibling != NIL) pool[nc.prev_sibling].next_sibling = nc.next_sibling;
        else pool[p].first_child = nc.next_sibling;
        if (nc.next_sibling != NIL) pool[nc.next_sibling].prev_sibling = nc.prev_sibling;
        pool[p].child_cnt--;
        nc.parent = NIL;
        nc.prev_sibling = NIL;
        nc.next_sibling = NIL;
    }

    // 辺の縮約: 単一子の内部ノード v (!= root) を消し、
    // v の辺の後ろに子の辺を合成して子の辺とする
    // compose が false なら何もしない
    void contract(int v) {
        int c = pool[v].first_child;
        if (!pool[v].edge.compose(pool[c].edge)) return;
        swap(pool[v].edge, pool[c].edge);
        Node &nv = pool[v];
        Node &nc = pool[c];
        int p = nv.parent;
        nc.parent = p;
        nc.prev_sibling = nv.prev_sibling;
        nc.next_sibling = nv.next_sibling;
        if (nv.prev_sibling != NIL) pool[nv.prev_sibling].next_sibling = c;
        else pool[p].first_child = c;
        if (nv.next_sibling != NIL) pool[nv.next_sibling].prev_sibling = c;
        free_node(v);
    }

    // 子を失った葉 v から枯れ枝を根方向へ刈り、単一子になった祖先は縮約を試す
    void remove_dead(int v) {
        while (v != root && pool[v].child_cnt == 0) {
            int p = pool[v].parent;
            unlink_child(p, v);
            free_node(v);
            v = p;
        }
        if (v != root && pool[v].child_cnt == 1) contract(v);
    }

    // result_prefix + (root, v] の辺列を out に組む。v 自身の辺を含む
    void build_path_to(int v, vector<Action> &out) const {
        out = result_prefix;
        size_t base = out.size();
        while (v != root) {
            out.push_back(pool[v].edge);
            v = pool[v].parent;
        }
        reverse(out.begin() + base, out.end());
    }

    // enumerate_actions が生成した action を try_op と候補登録へ流す受け口
    struct Submitter {
        BeamSearchRadix &bs;
        State &st;
        int parent_node;  // 葉のノード id (finished 経路復元用)
        int parent_ord;   // 葉の訪問序数 (cand の parent_leaf)
        int turn;

        inline ScoreType threshold() const { return bs.candidates.threshold(); }

        inline void operator()(Action &a) {
            ++bs.explored_per_turn;
            auto [score, hash, finished] = st.try_op(a, bs.candidates.threshold());
            if (score >= INF) return;
            if (finished) {
                if (!bs.found_finished || score < bs.best_finished_score) {
                    bs.found_finished = true;
                    bs.best_finished_score = score;
                    bs.build_path_to(parent_node, bs.best_finished_path);
                    bs.best_finished_path.push_back(a);
                }
                return;
            }
            // 採用確定時のみコピーする
            bs.candidates.push_lazy(score, hash, parent_ord, [&]() -> Action { return a; });
        }
    };

    void expand_leaf(State &state, int v, int turn) {
        int ord = (int)visited_leaves.size();
        visited_leaves.push_back(v);
        Submitter submit{*this, state, v, ord, turn};
        const Action &la = (turn == 0) ? DAMMY_ACTION : pool[v].edge;
        if constexpr (requires(Submitter &e) { state.enumerate_actions(0, DAMMY_ACTION, e); }) {
            state.enumerate_actions(turn, la, submit);
        } else {
            actions.clear();
            state.enumerate_actions(actions, turn, la, candidates.threshold());
            for (Action &action : actions) submit(action);
        }
    }

    // 根から DFS し全葉を展開する。下り辺で apply、上り辺で rollback
    // 終了時 state は根に戻っている
    void dfs_expand(State &state, int turn) {
        visited_leaves.clear();
        stk.clear();
        stk.push_back(root << 1);
        while (!stk.empty()) {
            int e = stk.back();
            stk.pop_back();
            int v = e >> 1;
            if (e & 1) {
                state.rollback(pool[v].edge);
                continue;
            }
            if (v != root) {
                // 部分木最小スコア >= threshold なら単調性の下で全滅するので打ち切る
                if (monotone_skip && pool[v].score >= candidates.threshold()) {
                    continue;
                }
                state.apply_op(pool[v].edge);
                stk.push_back((v << 1) | 1);
            }
            if (pool[v].child_cnt == 0) {
                expand_leaf(state, v, turn);
            } else {
                // 子リストは score 降順なので pop 順は良い方から。threshold が早く締まる
                for (int c = pool[v].first_child; c != NIL; c = pool[c].next_sibling) {
                    stk.push_back(c << 1);
                }
            }
        }
    }

    // 木の更新。DFS 後の state が根にいる状態で呼ぶ
    // 新しい葉の接続 -> 枯れ枝刈りと縮約 -> 根の確定、の順に行う
    void surgery(State &state) {
        // 候補を親序数でバケット分けする
        int sz = candidates.size();
        int L = (int)visited_leaves.size();
        bucket_cnt.assign(L + 1, 0);
        for (int i = 0; i < sz; ++i) {
            ++bucket_cnt[candidates.next_beam[i].parent_leaf + 1];
        }
        for (int p = 0; p < L; ++p) bucket_cnt[p + 1] += bucket_cnt[p];
        bucket_items.resize(sz);
        // bucket_cnt[p] を書き込みカーソルとして使い、後で 1 つ戻す
        for (int i = 0; i < sz; ++i) {
            bucket_items[bucket_cnt[candidates.next_beam[i].parent_leaf]++] = i;
        }
        for (int p = L; p > 0; --p) bucket_cnt[p] = bucket_cnt[p - 1];
        bucket_cnt[0] = 0;

        // 新しい葉の接続: score 昇順に先頭挿入するので子リストは score 降順になる
        next_leaves.clear();
        for (int p = 0; p < L; ++p) {
            int lo = bucket_cnt[p], hi = bucket_cnt[p + 1];
            if (lo == hi) continue;
            int d = hi - lo;
            if (d == 2) {
                if (candidates.next_beam[bucket_items[lo]].score >
                    candidates.next_beam[bucket_items[lo + 1]].score) {
                    swap(bucket_items[lo], bucket_items[lo + 1]);
                }
            } else if (d > 2) {
                sort(bucket_items.begin() + lo, bucket_items.begin() + hi, [&](int a, int b) {
                    return candidates.next_beam[a].score < candidates.next_beam[b].score;
                });
            }
            int pv = visited_leaves[p];
            for (int k = lo; k < hi; ++k) {
                auto &cnd = candidates.next_beam[bucket_items[k]];
                int u = new_node();
                Node &n = pool[u];
                n.edge = move(cnd.action);
                n.score = cnd.score;
                n.parent = pv;
                n.next_sibling = pool[pv].first_child;
                if (n.next_sibling != NIL) pool[n.next_sibling].prev_sibling = u;
                pool[pv].first_child = u;
                pool[pv].child_cnt++;
                next_leaves.push_back(u);
            }
        }

        // 枯れ枝刈りと縮約
        for (int v : leaves) {
            if (v == root) continue;
            if (pool[v].child_cnt == 0) remove_dead(v);
            else if (pool[v].child_cnt == 1) contract(v);
        }

        // 根の確定: 根直下が単一子の間、その辺を確定して根を進める
        // 二度と歩かない辺なので合成せず、state は確定辺を apply して追従する
        while (pool[root].child_cnt == 1) {
            int c = pool[root].first_child;
            result_prefix.push_back(pool[c].edge);
            state.apply_op(pool[c].edge);
            unlink_child(root, c);
            free_node(root);
            root = c;
        }

        swap(leaves, next_leaves);

        if (monotone_skip) recompute_subtree_best();
    }

    // 内部ノードの score を部分木最小に更新する
    // pre-order の逆順に畳めば子が親より先に確定する
    void recompute_subtree_best() {
        porder.clear();
        stk.clear();
        stk.push_back(root);
        while (!stk.empty()) {
            int v = stk.back();
            stk.pop_back();
            porder.push_back(v);
            if (pool[v].child_cnt > 0) pool[v].score = INF;
            for (int c = pool[v].first_child; c != NIL; c = pool[c].next_sibling) {
                stk.push_back(c);
            }
        }
        for (int i = (int)porder.size() - 1; i >= 0; --i) {
            int v = porder[i];
            if (v == root) continue;
            int p = pool[v].parent;
            if (pool[v].score < pool[p].score) pool[p].score = pool[v].score;
        }
    }

    void init_bs() {
        beam_timer.reset();
        found_finished = false;
        best_finished_score = INF;
        best_finished_path.clear();
        pool.clear();
        free_ids.clear();
        live_nodes = 0;
        leaves.clear();
        next_leaves.clear();
        result_prefix.clear();
        actions.clear();
        explored_per_turn = 0;
    }

public:
    /// @brief スコア単調 (子スコア >= 親スコア) な問題向けの部分木 skip を有効化する
    /// 単調性がない問題で有効にすると解が劣化しうる
    void set_monotone_skip(bool enable) { monotone_skip = enable; }

    /**
     * @brief ビームサーチをする
     *
     * @param param ターン数、ビーム幅を指定するパラメータ構造体
     * @param verbose ログ出力するかどうか
     * @return vector<Action> 合成済み Action を含む列
     */
    vector<Action> search(BeamParam &param, const bool verbose=false) {
        if (param.max_turn <= 0 || param.beam_width <= 0) return {};
        init_bs();
        if (verbose) {
            beam_log::start_banner(cerr, "BeamSearchRadix", param);
            if (param.is_adjusting) beam_log::warn(cerr, "dynamic beam width is experimental");
        }
        State state;
        state.init();

        root = new_node();
        pool[root].score = 0;
        leaves = {root};
        int turns_done = 0;

        for (int turn = 0; turn < param.max_turn; ++turn) {
            double now_time = beam_timer.elapsed();
            int w = param.get_beam_width(param.max_turn - turn, (int)leaves.size(), param.time_limit - now_time);
            candidates.reset(turn, w, param.clear_hash_every_turn, param.hash_window_turns);
            explored_per_turn = 0;

            dfs_expand(state, turn);

            if (found_finished) {
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
                return {};
            }

            if (verbose) {
                BeamCandidate<ScoreType, Action> bests = candidates.get_best();
                beam_log::turn_line(cerr, turn + 1, param.max_turn, now_time,
                                    w, live_nodes, (int)candidates.size(),
                                    explored_per_turn, bests.score);
            }

            surgery(state);

            param.timestamp(live_nodes, (int)leaves.size(), beam_timer.elapsed() - now_time);
            turns_done = turn + 1;
        }

        int best_leaf = leaves[0];
        for (int v : leaves) {
            if (pool[v].score < pool[best_leaf].score) best_leaf = v;
        }
        vector<Action> ret;
        build_path_to(best_leaf, ret);

        if (verbose) {
            beam_log::on_max_turn(cerr);
            beam_log::width_trace(cerr, param.width_hist);
            beam_log::end_banner(cerr, "max_turn reached", turns_done, param.max_turn,
                                 beam_timer.elapsed(), param.ave_width(),
                                 pool[best_leaf].score, true, (int)ret.size());
        }
        return ret;
    }
};
} // namespace flying_squirrel
