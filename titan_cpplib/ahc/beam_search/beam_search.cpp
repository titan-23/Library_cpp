#pragma once

#include <iostream>
#include <vector>
#include <cassert>
#include <algorithm>
#include <tuple>

#include "titan_cpplib/others/print.cpp"
#include "titan_cpplib/algorithm/random.cpp"
#include "titan_cpplib/ahc/state_pool.cpp"
#include "titan_cpplib/ahc/timer.cpp"
#include "titan_cpplib/ds/hash_dict.cpp"
#include "titan_cpplib/ahc/beam_search/beam_param.cpp"

using namespace std;

//! 木上のビームサーチライブラリ
namespace flying_squirrel {

template<typename ScoreType, typename HashType, class Action, class State>
class BeamSearchWithTree {
    // ref: https://eijirou-kyopro.hatenablog.com/entry/2024/02/01/115639

private:
    static constexpr const int PRE_ORDER = -1;
    static constexpr const int POST_ORDER = -2;
    titan23::Random rnd;
    titan23::Timer beam_timer;
    using ActionIDType = int;
    ActionIDType ActionID;
    vector<Action> result;
    Action DAMMY_ACTION;

    struct TreeNode {
        int dir_or_leaf_id;
        Action action;
        ActionIDType action_id;

        TreeNode(int d, Action a, ActionIDType id) : dir_or_leaf_id(d), action(move(a)), action_id(id) {}
    };

    struct BeamCandidate {
        int par;
        ScoreType score;
        Action action;
        ActionIDType action_id;

        BeamCandidate() : par(0), score(0), action(), action_id(0) {}
        BeamCandidate(int p, ScoreType s, Action a, ActionIDType id) : par(p), score(s), action(move(a)), action_id(id) {}
    };

    // ビームサーチの過程を表す木
    // <dir or id, action, action_id>
    // dir or id := 葉のとき、leaf_id
    //              そうでないとき、行きがけなら-1、帰りがけなら-2
    vector<TreeNode> tree, nxt_tree;

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
        // 次のビーム候補を保持する配列
        vector<BeamCandidate> next_beam;

        Candidates() {}

        int size() const {
            return entry;
        }

        /// @brief 現在のworstを返す / これ以上なら意味ない
        ScoreType threshold() const {
            return entry < beam_width ? numeric_limits<ScoreType>::max() : seg[1].first;
        }

        /// 追加できたらtrueを返す
        bool push(
            ScoreType score, HashType hash,
            int par, const Action &action, ActionIDType action_id
        ) {
            if (entry == beam_width && score >= seg[1].first) {
                return false;
            }
            auto dat = func.get_pos(hash);
            int idx = func.inner_get(dat, -1);
            if (idx != -1) {
                if (score < seg[idx+s].first) {
                    next_beam[idx] = {par, score, move(action), action_id};
                    set(idx, {score, idx});
                    return true;
                }
                return false;
            }
            if (entry < beam_width) {
                func.inner_set(dat, hash, entry);
                next_beam[entry] = {par, score, move(action), action_id};
                hashidx[entry] = hash;
                set(entry, {score, entry});
                entry++;
                return true;
            }
            auto [_, i] = seg[1];
            next_beam[i] = {par, score, move(action), action_id};
            func.set(hashidx[i], -1);
            func.inner_set(dat, hash, i);
            hashidx[i] = hash;
            set(i, {score, i});
            return true;
        }

        void reset(int w) {
            beam_width = w;
            while (s < w) {
                s <<= 1;
            }
            if (seg.size() < 2*s) {
                seg.resize(2*s);
            }
            fill(seg.begin(), seg.begin()+(2*w), make_pair(numeric_limits<ScoreType>::lowest(), -1));
            if (hashidx.size() < w) {
                hashidx.resize(w);
                next_beam.resize(w);
            }
            func.clear();
            if (func.inner_len() == 1) {
                func = titan23::HashDict<int>(beam_width*8);
            }
            entry = 0;
        }

        void shuffle(titan23::Random &rnd) {
            for (int i = 0; i < entry-1; ++i) {
                int j = rnd.randrange(i, entry);
                swap(next_beam[i], next_beam[j]);
            }
        }

        BeamCandidate get_best() {
            return *min_element(next_beam.begin(), next_beam.begin() + entry, [&] (const BeamCandidate &left, const BeamCandidate &right) {
                return left.score < right.score;
            });
        }
    } candidates;

    vector<Action> actions;

    /**
     * @brief 次のビームを求める
     *
     * @param state スタートのState
     * @param t_turn 次のビームのターン数
     * @param turn 謎
     */
    void get_next_beam(State* state, const int t_turn, const int turn) {
        if (turn == 0) {
            actions.clear();
            state->get_actions(actions, t_turn, (result.empty() ? DAMMY_ACTION : result.back()));
            for (Action &action : actions) {
                auto [score, hash] = state->try_op(action, candidates.threshold());
                if (candidates.push(score, hash, PRE_ORDER, move(action), ActionID)) {
                    ActionID++;
                }
            }
            return;
        }

        int leaf_id = 0;
        for (int i = 0; i < tree.size(); ++i) {
            auto &[dir_or_leaf_id, action, _] = tree[i];
            if (dir_or_leaf_id >= 0) {
                state->apply_op(move(action));
                actions.clear();
                state->get_actions(actions, t_turn, action);
                tree[i].dir_or_leaf_id = leaf_id;
                for (Action &action : actions) {
                    auto [score, hash] = state->try_op(action, candidates.threshold());
                    if (candidates.push(score, hash, leaf_id, move(action), ActionID)) {
                        ActionID++;
                    }
                }
                ++leaf_id;
                state->rollback(move(action));
            } else if (dir_or_leaf_id == PRE_ORDER) {
                state->apply_op(move(action));
            } else {
                state->rollback(move(action));
            }
        }
    }

    //! 不要なNodeを削除し、木を更新する
    int update_tree(State* state, const int turn) {
        nxt_tree.clear();
        if (turn == 0) {
            for (int i = 0; i < candidates.size(); ++i) {
                const auto &[par, _, new_action, action_id] = candidates.next_beam[i];
                assert(par == -1);
                nxt_tree.emplace_back(0, move(new_action), action_id);
            }
            swap(tree, nxt_tree);
            return 0;
        }

        int i = 0;
        int apply_only_turn = 0;
        while (true) {
            const auto &[dir_or_leaf_id, action, action_id] = tree[i];
            // 行きがけかつ帰りがけのaction_idが一致しているなら、一本道なので行くだけ
            if (dir_or_leaf_id == PRE_ORDER && action_id == tree.back().action_id) {
                ++i;
                result.emplace_back(action);
                state->apply_op(action);
                tree.pop_back();
                apply_only_turn++;
            } else {
                break;
            }
        }

        int next_beam_idx = 0;
        const int num_candidates = candidates.size();
        for (; i < tree.size(); ++i) {
            auto &[dir_or_leaf_id, action, action_id] = tree[i];
            if (dir_or_leaf_id >= 0) {
                bool has_child = false;
                int start_idx = next_beam_idx;
                while(next_beam_idx < num_candidates) {
                    if (candidates.next_beam[next_beam_idx].par != dir_or_leaf_id) {
                        break;
                    }
                    has_child = true;
                    next_beam_idx++;
                }
                if (!has_child) continue;
                nxt_tree.emplace_back(PRE_ORDER, move(action), action_id);
                for (int k = start_idx; k < next_beam_idx; ++k) {
                    auto &[_, __, new_action, new_action_id] = candidates.next_beam[k];
                    nxt_tree.emplace_back(dir_or_leaf_id, move(new_action), new_action_id);
                }
                nxt_tree.emplace_back(POST_ORDER, move(action), action_id);
            } else if (dir_or_leaf_id == PRE_ORDER) {
                nxt_tree.emplace_back(PRE_ORDER, move(action), action_id);
            } else {
                int pre_dir = nxt_tree.back().dir_or_leaf_id;
                if (pre_dir == PRE_ORDER) {
                    nxt_tree.pop_back(); // 一つ前が行きがけなら、削除して追加しない
                } else {
                    nxt_tree.emplace_back(POST_ORDER, move(action), action_id);
                }
            }
        }
        swap(tree, nxt_tree);
        return apply_only_turn;
    }

    void get_result() {
        int best_id = -1;
        ScoreType best_score = 0;
        for (int i = 0; i < candidates.size(); ++i) {
            const auto &[par, score, _, __] = candidates.next_beam[i];
            if (best_id == -1 || score < best_score) {
                best_score = score;
                best_id = par;
            }
        }
        if (best_id == -1) {
            int best_ch_idx = -1;
            Action best_action;
            ScoreType best_score = 0;
            for (int i = 0; i < candidates.size(); ++i) {
                const auto &[_, score, action, __] = candidates.next_beam[i];
                if (best_ch_idx == -1 || score < best_score) {
                    best_score = score;
                    best_ch_idx = 1;
                    best_action = action;
                }
            }
            assert(best_ch_idx != -1);
            result.emplace_back(best_action);
            return;
        }
        for (const auto &[dir_or_leaf_id, action, _] : tree) {
            if (dir_or_leaf_id >= 0) {
                if (best_id == dir_or_leaf_id) {
                    result.emplace_back(action);
                    return;
                }
            } else if (dir_or_leaf_id == PRE_ORDER) {
                result.emplace_back(action);
            } else {
                result.pop_back();
            }
        }
        cerr << PRINT_RED << "Error: 解が見つかりませんでした" << PRINT_NONE << endl;
        assert(false);
    }

    void init_bs(const BeamParam &param) {
        beam_timer.reset();
        rnd = titan23::Random();
        ActionID = 0;
        result.clear();
    }

public:
    /**
     * @brief ビームサーチをする
     *
     * @param param ターン数、ビーム幅を指定するパラメータ構造体
     * @param verbose ログ出力するかどうか
     * @return vector<Action>
     */
    vector<Action> search(BeamParam &param, const bool verbose=false) {
        init_bs(param);
        if (verbose) cerr << PRINT_GREEN << "Info: start search()" << PRINT_NONE << endl;
        State* state = new State;
        state->init();

        int now_turn = 0;
        for (int turn = 0; turn < param.max_turn; ++turn) {
            double now_time = beam_timer.elapsed();
            if (verbose) cerr << "\nInfo: # turn : " << turn+1 << " | " << now_time << " ms" << endl;

            // 次のビーム候補を求める
            int w = param.get_beam_width(param.max_turn-turn, tree.size(), param.time_limit-beam_timer.elapsed());
            candidates.reset(w);
            get_next_beam(state, turn, turn-now_turn);
            candidates.shuffle(rnd);

            if (candidates.size() == 0) {
                cerr << to_red("Error: \t次の候補が見つかりませんでした") << endl;
                assert(candidates.size() > 0);
            }

            BeamCandidate bests = candidates.get_best();

            if (verbose) cerr << "Info: \tbest_score = " << bests.score << endl;
            if (bests.score == 0) { // TODO 終了条件
                if (verbose) cerr << to_green("Info: find valid solution.") << endl;
                candidates.next_beam.clear();
                candidates.next_beam.push_back(bests);
                get_result();
                result.emplace_back(candidates.next_beam[0].action);
                if (verbose) param.report();
                return result;
            }

            // 探索木の更新
            if (turn != 0) {
                sort(candidates.next_beam.begin(), candidates.next_beam.begin() + candidates.size(),
                    [] (const auto& a, const auto& b) {
                        return a.par < b.par;
                    }
                );
            }
            int apply_only_turn = update_tree(state, turn);
            now_turn += apply_only_turn;

            param.timestamp(tree.size(), candidates.size(), beam_timer.elapsed()-now_time);
        }

        // 答えを復元する
        if (verbose) cerr << to_green("Info: max_turn finished.") << endl;
        if (verbose) param.report();
        get_result();
        return result;
    }
};
} // namespace flying_squirrel
