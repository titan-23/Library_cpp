#include <iostream>
#include <vector>
#include <cassert>
#include <algorithm>

#include "titan_cpplib/others/print.cpp"
#include "titan_cpplib/algorithm/random.cpp"
#include "titan_cpplib/ahc/state_pool.cpp"
#include "titan_cpplib/ahc/timer.cpp"
#include "titan_cpplib/ds/hash_set.cpp"
#include "titan_cpplib/ahc/beam_search/beam_param.cpp"

using namespace std;

//! 木上のビームサーチライブラリ
namespace flying_squirrel_recursion {

using BeamParam = flying_squirrel::BeamParam;

template<typename ScoreType, typename HashType, typename Action, typename State>
class BeamSearchWithTree {
private:
    using TreeNodeID = int;
    using SubStateID = int;

    //! try_opした結果をメモしておく構造体
    struct SubState {
        TreeNodeID par;
        Action action;
        ScoreType score;

        SubState() : par(-1) {}
        SubState(TreeNodeID par, const Action &action, ScoreType score) : par(par), action(action), score(score) {}
    };

    //! ビームサーチの過程を表す木のノード
    struct TreeNode {
        TreeNodeID par;
        Action pre_action;
        ScoreType score;
        vector<TreeNodeID> child;

        TreeNode() : par(-1) {}

        bool is_leaf() const {
            return child.empty();
        }
    };

    titan23::StatePool<TreeNode> treenode_pool;
    titan23::StatePool<SubState> substate_pool;

    titan23::Random rnd;
    titan23::Timer beam_timer;
    ScoreType best_score;
    TreeNodeID best_id;
    titan23::HashSet seen;
    int now_turn;
    Action DAMMY_ACTION;

    void get_next_beam_recursion(State* state, TreeNodeID node, vector<SubStateID> &next_beam, const Action &last_action, int depth) {
        if (depth == 0) { // 葉
            vector<Action> actions = state->get_actions(now_turn, last_action);
            for (Action &action : actions) {
                auto [score, hash] = state->try_op(action);
                if (seen.contains_insert(hash)) continue;
                SubStateID substate = substate_pool.gen();
                substate_pool.get(substate)->par = node;
                substate_pool.get(substate)->action = action;
                substate_pool.get(substate)->score = score;
                next_beam.emplace_back(substate);
            }
            return;
        }
        for (const TreeNodeID nxt_node : treenode_pool.get(node)->child) {
            const Action &action = treenode_pool.get(nxt_node)->pre_action;
            state->apply_op(action);
            get_next_beam_recursion(state, nxt_node, next_beam, action, depth-1);
            state->rollback(action);
        }
    }

    tuple<int, TreeNodeID, vector<SubStateID>> get_next_beam(State* state, TreeNodeID node, int turn) {
        int cnt = 0;
        Action action = DAMMY_ACTION;
        while (true) { // 一本道は行くだけ
            if (treenode_pool.get(node)->child.size() != 1) break;
            ++cnt;
            node = treenode_pool.get(node)->child[0];
            action = treenode_pool.get(node)->pre_action;
            state->apply_op(treenode_pool.get(node)->pre_action);
        }
        vector<SubStateID> next_beam;
        seen.clear();
        get_next_beam_recursion(state, node, next_beam, action, turn-cnt);
        return make_tuple(cnt, node, next_beam);
    }

    //! 不要なNodeを削除し、木を更新する
    bool update_tree(const TreeNodeID node, const int depth) {
        if (treenode_pool.get(node)->is_leaf()) return depth == 0;
        int idx = 0;
        while (idx < treenode_pool.get(node)->child.size()) {
            TreeNodeID nxt_node = treenode_pool.get(node)->child[idx];
            if (!update_tree(nxt_node, depth-1)) {
                treenode_pool.del(nxt_node);
                treenode_pool.get(node)->child.erase(treenode_pool.get(node)->child.begin() + idx);
                continue;
            }
            ++idx;
        }
        return idx > 0;
    }

    //! node以上のパスを返す
    vector<Action> get_path(TreeNodeID node) {
        vector<Action> result;
        while (node != -1 && treenode_pool.get(node)->par != -1) {
            result.emplace_back(treenode_pool.get(node)->pre_action);
            node = treenode_pool.get(node)->par;
        }
        reverse(result.begin(), result.end());
        return result;
    }

    //! for debug
    void print_tree(State* state, const TreeNodeID node, int depth) {
    }

    //! node以下で、葉かつ最も評価値の良いノードを見るける / 葉はターン数からは判断していないので注意
    void get_best_node(TreeNodeID node) {
        if (treenode_pool.get(node)->is_leaf()) {
            if (best_id == -1 || treenode_pool.get(node)->score < best_score) {
                best_score = treenode_pool.get(node)->score;
                best_id = node;
            }
            return;
        }
        for (TreeNodeID nxt_node : treenode_pool.get(node)->child) {
            get_best_node(nxt_node);
        }
    }

    vector<Action> get_result(TreeNodeID root) {
        best_id = -1; // 更新
        get_best_node(root);
        TreeNodeID node = best_id;
        vector<Action> result = get_path(node);
        cerr << treenode_pool.get_size() << endl;
        return result;
    }

    void init_bs(const BeamParam &param) {
        now_turn = 0;
        beam_timer.reset();
        rnd = titan23::Random();
        this->seen = titan23::HashSet(param.beam_width); // TODO
        treenode_pool.clear();
        substate_pool.clear();
    }

public:
    /**
     * @brief ビームサーチをする
     *
     * @param param ターン数、ビーム幅を指定するパラメータ構造体
     * @param verbose 途中結果のスコアを標準エラー出力するかどうか
     * @return vector<Action>
     */
    vector<Action> search(BeamParam &param, const bool verbose=false) {
        if (verbose) cerr << PRINT_GREEN << "Info: start search()" << PRINT_NONE << endl;
        init_bs(param);
        TreeNodeID root = treenode_pool.gen();
        treenode_pool.get(root)->child.clear();
        treenode_pool.get(root)->par = -1;

        State* state = new State;
        state->init();

        int now_turn = 0;

        for (int turn = 0; turn < param.max_turn; ++turn) {
            now_turn = turn;
            double now_time = beam_timer.elapsed();
            if (verbose) cerr << "\nInfo: # turn : " << turn+1 << " | " << now_time << " ms" << endl;

            // 次のビーム候補を求める
            auto [apply_only_turn, next_root, next_beam] = get_next_beam(state, root, turn-now_turn);
            rnd.shuffle(next_beam); // シャッフルして多様性を確保(できているのか？)
            root = next_root;
            now_turn += apply_only_turn;
            assert(!next_beam.empty());
            if (verbose) {
                cerr << "\tmin_score=" << substate_pool.get((*min_element(next_beam.begin(), next_beam.end(), [this] (const SubStateID &left, const SubStateID &right) {
                    return substate_pool.get(left)->score < substate_pool.get(right)->score;
                })))->score << endl;
            }

            // ビームを絞る
            // int beam_width = min(param.beam_width, (int)next_beam.size());
            int beam_width = min(param.get_beam_width(param.max_turn-turn, treenode_pool.used_size(), param.time_limit-beam_timer.elapsed()), (int)next_beam.size());
            nth_element(next_beam.begin(), next_beam.begin() + beam_width, next_beam.end(), [&] (const SubStateID &left, const SubStateID &right) {
                return substate_pool.get(left)->score < substate_pool.get(right)->score;
            });

            // 探索木の更新
            for (int i = 0; i < beam_width; ++i) {
                SubStateID s = next_beam[i];
                TreeNodeID new_node = treenode_pool.gen();
                treenode_pool.get(substate_pool.get(s)->par)->child.emplace_back(new_node);
                treenode_pool.get(new_node)->par = substate_pool.get(s)->par;
                treenode_pool.get(new_node)->pre_action = substate_pool.get(s)->action;
                treenode_pool.get(new_node)->score = substate_pool.get(s)->score;
            }
            substate_pool.clear();
            update_tree(root, turn-now_turn+1);

            param.timestamp(treenode_pool.used_size(), beam_width, beam_timer.elapsed()-now_time);
        }

        // 答えを復元する
        if (verbose) cerr << PRINT_GREEN << "Info: MAX_TURN finished." << PRINT_NONE << endl;
        vector<Action> result = get_result(root);
        return result;
    }
};
} // namespace flying_squirrel_recursion
