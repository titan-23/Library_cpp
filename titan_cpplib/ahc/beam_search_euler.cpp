#include <iostream>
#include <vector>
#include <cassert>
#include <algorithm>

#include "titan_cpplib/algorithm/random.cpp"
#include "titan_cpplib/ahc/state_pool.cpp"
#include "titan_cpplib/ahc/timer.cpp"
#include "titan_cpplib/data_structures/hash_set.cpp"
#include "titan_cpplib/others/print.cpp"

using namespace std;

#define rep(i, n) for (int i = 0; i < (n); ++i)

//! 木上のビームサーチライブラリ
namespace beam_search_with_tree {

using ScoreType = long long;
using HashType = unsigned long long;
const ScoreType INF = 1e9;

// Action
struct Action {
    char d;
    ScoreType pre_score, nxt_score;
    HashType pre_hash, nxt_hash;

    Action() {}
    Action(const char d) : d(d), pre_score(INF), nxt_score(INF), pre_hash(0), nxt_hash(0) {}
};
ostream& operator<<(ostream& os, const Action &action) {
    os << action.d;
    return os;
}

class State {
  private:

  public:
    ScoreType score;
    HashType hash;

    State() {}

    // TODO Stateを初期化する
    void init() {}

    // TODO
    //! `action` をしたときの評価値とハッシュ値を返す
    //! ロールバックに必要な情報はすべてactionにメモしておく
    pair<ScoreType, HashType> try_op(Action &action) const {}

    bool is_done() const {}

    // TODO
    //! `action` をする
    void apply_op(const Action &action) {}

    // TODO
    //! `action` を戻す
    void rollback(const Action &action) {}

    // TODO
    //! 現状態から遷移可能な `Action` の `vector` を返す
    vector<Action> get_actions() const {}

    void print() const {}
};

struct BeamParam {
    int MAX_TURN;
    int BEAM_WIDTH;
};

class BeamSearchWithTree {
    // ref: https://eijirou-kyopro.hatenablog.com/entry/2024/02/01/115639

  private:
    static constexpr const int PRE_ORDER = -1;
    static constexpr const int POST_ORDER = -2;

    titan23::HashSet seen;
    using ActionIDType = int;
    ActionIDType ActionID;
    vector<Action> result;

    // ビームサーチの過程を表す木
    // <dir or id, action, action_id>
    // dir or id := 葉のとき、leaf_id
    //              そうでないとき、行きがけなら-1、帰りがけなら-2
    vector<tuple<int, Action, ActionIDType>> tree;

    // 次のビーム候補を保持する配列
    vector<tuple<int, ScoreType, Action, ActionIDType>> next_beam; // <par, score, action, action_id>

    vector<vector<int>> next_beam_data;

    void get_next_beam(State* state, const int turn) {
        next_beam.clear();
        next_beam.reserve(tree.size() * 4); // TODO
        seen.clear();

        if (turn == 0) {
            vector<Action> actions = state->get_actions();
            for (Action &action : actions) {
                auto [score, hash] = state->try_op(action);
                if (seen.contains_insert(hash)) continue;
                next_beam.emplace_back(PRE_ORDER, score, action, ActionID);
                ActionID++;
            }
            return;
        }

        int leaf_id = 0;
        for (int i = 0; i < tree.size(); ++i) {
            auto [dir_or_leaf_id, action, _] = tree[i];
            if (dir_or_leaf_id >= 0) {
                state->apply_op(action);
                vector<Action> actions = state->get_actions();
                std::get<0>(tree[i]) = leaf_id;
                for (Action &action : actions) {
                    auto [score, hash] = state->try_op(action);
                    if (seen.contains_insert(hash)) continue;
                    next_beam.emplace_back(leaf_id, score, action, ActionID);
                    ActionID++;
                }
                ++leaf_id;
                state->rollback(action);
            } else if (dir_or_leaf_id == PRE_ORDER) {
                state->apply_op(action);
            } else {
                state->rollback(action);
            }
        }
    }

    //! 不要なNodeを削除し、木を更新する
    int update_tree(State* state, const int turn) {
        vector<tuple<int, Action, ActionIDType>> new_tree;
        new_tree.reserve(tree.size());
        if (turn == 0) {
            for (auto &[par, _, new_action, action_id] : next_beam) {
                assert(par == -1);
                new_tree.emplace_back(0, new_action, action_id);
            }
            swap(tree, new_tree);
            return 0;
        }

        int i = 0;
        int apply_only_turn = 0;
        while (true) {
            const auto &[dir_or_leaf_id, action, action_id] = tree[i];
            // 行きがけかつ帰りがけのaction_idが一致しているなら、一本道なので行くだけ
            if (dir_or_leaf_id == PRE_ORDER && action_id == std::get<2>(tree.back())) {
                ++i;
                result.emplace_back(action);
                state->apply_op(action);
                tree.pop_back();
                apply_only_turn++;
            } else {
                break;
            }
        }

        for (; i < tree.size(); ++i) {
            const auto &[dir_or_leaf_id, action, action_id] = tree[i];
            if (dir_or_leaf_id >= 0) {
                if (next_beam_data[dir_or_leaf_id].empty()) continue;
                new_tree.emplace_back(PRE_ORDER, action, action_id);
                for (const int beam_idx : next_beam_data[dir_or_leaf_id]) {
                    auto &[_, __, new_action, new_action_id] = next_beam[beam_idx];
                    new_tree.emplace_back(dir_or_leaf_id, new_action, new_action_id);
                }
                new_tree.emplace_back(POST_ORDER, action, action_id);
                next_beam_data[dir_or_leaf_id].clear();
            } else if (dir_or_leaf_id == PRE_ORDER) {
                new_tree.emplace_back(PRE_ORDER, action, action_id);
            } else {
                int pre_dir = std::get<0>(new_tree.back());
                if (pre_dir == PRE_ORDER) {
                    new_tree.pop_back(); // 一つ前が行きがけなら、削除して追加しない
                } else {
                    new_tree.emplace_back(POST_ORDER, action, action_id);
                }
            }
        }
        swap(tree, new_tree);
        return apply_only_turn;
    }

    void get_result() {
        int best_id = -1;
        ScoreType best_score = 0;
        for (auto [par, score, _, __] : next_beam) {
            if (best_id == -1 || score < best_score) {
                best_score = score;
                best_id = par;
            }
        }
        assert(best_id != -1);
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

  public:

    /**
     * @brief ビームサーチをする
     *
     * @param param ターン数、ビーム幅を指定するパラメータ構造体
     * @param verbose ログ出力するかどうか
     * @return vector<Action>
     */
    vector<Action> search(const BeamParam &param, const bool verbose = false) {
        if (verbose) cerr << PRINT_GREEN << "Info: start search()" << PRINT_NONE << endl;

        ActionID = 0;
        State* state = new State;
        state->init();

        this->seen = titan23::HashSet(param.BEAM_WIDTH * 4); // TODO

        int now_turn = 0;
        for (int turn = 0; turn < param.MAX_TURN; ++turn) {
            if (verbose) cerr << "Info: # turn : " << turn+1 << endl;

            // 次のビーム候補を求める
            get_next_beam(state, turn-now_turn);

            if (next_beam.empty()) {
                cerr << PRINT_RED << "Error: \t次の候補が見つかりませんでした" << PRINT_NONE << endl;
                assert(!next_beam.empty());
            }

            // ビームを絞る
            int beam_width = min(param.BEAM_WIDTH, (int)next_beam.size());
            assert(beam_width <= param.BEAM_WIDTH);

            nth_element(next_beam.begin(), next_beam.begin() + beam_width, next_beam.end(), [&] (const tuple<int, ScoreType, Action, ActionIDType> &left, const tuple<int, ScoreType, Action, ActionIDType> &right) {
                return std::get<1>(left) < std::get<1>(right);
            });

            tuple<int, ScoreType, Action, ActionIDType> bests = *min_element(next_beam.begin(), next_beam.begin() + beam_width, [&] (const tuple<int, ScoreType, Action, ActionIDType> &left, const tuple<int, ScoreType, Action, ActionIDType> &right) {
                return std::get<1>(left) < std::get<1>(right);
            });
            if (verbose) cerr << "Info: \tbest_score = " << std::get<1>(bests) << endl;
            if (std::get<1>(bests) == 0) { // TODO 終了条件
                cerr << PRINT_GREEN << "Info: find valid solution." << PRINT_NONE << endl;
                get_result();
                result.emplace_back(std::get<2>(bests));
                return result;
            }

            // 探索木の更新
            if (turn != 0) {
                if (next_beam_data.size() < next_beam.size()) {
                    next_beam_data.resize(next_beam.size());
                }
                for (int i = 0; i < beam_width; ++i) {
                    auto &[par, _, new_action, new_action_id] = next_beam[i];
                    next_beam_data[par].emplace_back(i);
                }
            }
            int apply_only_turn = update_tree(state, turn);
            now_turn += apply_only_turn;
        }

        // 答えを復元する
        if (verbose) cerr << PRINT_GREEN << "Info: MAX_TURN finished." << PRINT_NONE << endl;
        get_result();
        return result;
    }
};
} // namespace beam_search

// int main() {
//     beam_search_with_tree::BeamParam param;
//     param.MAX_TURN = 1000;
//     param.BEAM_WIDTH = 100;
//     beam_search_with_tree::init_zhs();
//     beam_search_with_tree::BeamSearchWithTree bs;
//     vector<beam_search_with_tree::Action> ans = bs.search(param, true);
//     for (const beam_search_with_tree::Action &action : ans) {
//         cout << action;
//     }
//     cout << endl;
//     cerr << "Score = " << ans.size() << endl;
//     return 0;
// }
