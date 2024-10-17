#include <vector>
#include <algorithm>

#include "titan_cpplib/others/print.cpp"
#include "titan_cpplib/algorithm/random.cpp"
#include "titan_cpplib/ahc/state_pool.cpp"
#include "titan_cpplib/ahc/timer.cpp"
#include "titan_cpplib/data_structures/hash_set.cpp"
using namespace std;

//! 木上のビームサーチライブラリ
namespace beam_search_with_tree {

using ScoreType = long long;
using HashType = unsigned long long;

struct Action {
    Action() {}

    friend ostream& operator<<(ostream& os, const Action &action) {
        return os;
    }
};

class State {
  private:
    titan23::Random srand;
    ScoreType score;
    HashType hash;

  public:
    // TODO Stateを初期化する
    void init() {
        this->score = 0;
    }

    // TODO
    //! `action` をしたときの評価値とハッシュ値を返す
    //! ロールバックに必要な情報はすべてactionにメモしておく
    pair<ScoreType, HashType> try_op(Action &action) const {
    }

    // TODO
    //! `action` をする
    void apply_op(const Action &action) {
    }

    // TODO
    //! `action` を戻す
    void rollback(const Action &action) {
    }

    // TODO
    //! 現状態から遷移可能な `Action` の `vector` を返す
    vector<Action> get_actions() const {
    }

    ScoreType get_score() {
        return this->score;
    }

    void print() const {
    }
};

struct BeamParam {
    int MAX_TURN;
    int BEAM_WIDTH;
};

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


class BeamSearchWithTree {
  private:
    ScoreType best_score;
    TreeNodeID best_id;
    titan23::HashSet seen;

    void get_next_beam_recursion(State* state, TreeNodeID node, vector<SubStateID> &next_beam, int depth, const int beam_width) {
        if (depth == 0) { // 葉
            vector<Action> actions = state->get_actions();
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
            state->apply_op(treenode_pool.get(nxt_node)->pre_action);
            get_next_beam_recursion(state, nxt_node, next_beam, depth-1, beam_width);
            state->rollback(treenode_pool.get(nxt_node)->pre_action);
        }
    }

    tuple<int, TreeNodeID, vector<SubStateID>> get_next_beam(State* state, TreeNodeID node, int turn, const int beam_width) {
        int cnt = 0;
        while (true) { // 一本道は行くだけ
            if (treenode_pool.get(node)->child.size() != 1) break;
            ++cnt;
            node = treenode_pool.get(node)->child[0];
            state->apply_op(treenode_pool.get(node)->pre_action);
        }
        vector<SubStateID> next_beam;
        seen.clear();
        get_next_beam_recursion(state, node, next_beam, turn-cnt, beam_width);
        return make_tuple(cnt, node, next_beam);
    }

    //! 親を返す / 無ければ自分を返す(!?)
    TreeNodeID get_par(SubStateID s_node, int cnt=1) {
        assert(0 <= s_node && s_node < substate_pool.get_size());
        TreeNodeID node = substate_pool.get(s_node)->par;
        assert(node != -1);
        for (int i = 0; i < cnt; ++i) {
            assert(node < treenode_pool.get_size());
            if (treenode_pool.get(node)->par == -1) {
                assert(node != -1);
                return node;
            }
            assert(node != -1);
            node = treenode_pool.get(node)->par;
        }
        assert(node != -1);
        assert(node < treenode_pool.get_size());
        return node;
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

  public:
    /**
     * @brief ビームサーチをする
     *
     * @param param ターン数、ビーム幅を指定するパラメータ構造体
     * @param verbose 途中結果のスコアを標準エラー出力するかどうか
     * @return vector<Action>
     */
    vector<Action> search(const BeamParam &param, const bool verbose = false) {
        TreeNodeID root = treenode_pool.gen();
        treenode_pool.get(root)->child.clear();
        treenode_pool.get(root)->par = -1;

        State* state = new State;
        state->init();

        this->seen = titan23::HashSet(param.BEAM_WIDTH * 4); // TODO

        int now_turn = 0;

        for (int turn = 0; turn < param.MAX_TURN; ++turn) {
            if (verbose) cerr << "# turn : " << turn << endl;

            // 次のビーム候補を求める
            auto [apply_only_turn, next_root, next_beam] = get_next_beam(state, root, turn-now_turn, param.BEAM_WIDTH);
            root = next_root;
            now_turn += apply_only_turn;
            assert(!next_beam.empty());
            if (verbose) {
                cerr << "min_score=" << substate_pool.get((*min_element(next_beam.begin(), next_beam.end(), [] (const SubStateID &left, const SubStateID &right) {
                    return substate_pool.get(left)->score < substate_pool.get(right)->score;
                })))->score << endl;
            }

            // ビームを絞る // TODO 評価値が一致した場合、親の評価値も参考にするなど
            int beam_width = min(param.BEAM_WIDTH, (int)next_beam.size());
            nth_element(next_beam.begin(), next_beam.begin() + beam_width, next_beam.end(), [&] (const SubStateID &left, const SubStateID &right) {
                // if (substate_pool.get(left)->score != substate_pool.get(right)->score) {
                    return substate_pool.get(left)->score < substate_pool.get(right)->score;
                // }
                // return treenode_pool.get(get_par(left))->score < treenode_pool.get(get_par(right))->score;
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
        }

        // 答えを復元する
        vector<Action> result = get_result(root);
        return result;
    }
};
} // namespace beam_search

// int main() {
//     beam_search_with_tree::BeamParam param;
//     param.MAX_TURN = 2500;
//     param.BEAM_WIDTH = 1000;
//     beam_search_with_tree::BeamSearchWithTree bs;
//     vector<beam_search_with_tree::Action> ans = bs.search(param, true);
// }
