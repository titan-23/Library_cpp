#include <iostream>
#include <vector>
#include <cassert>
#include <algorithm>

#include "titan_cpplib/algorithm/random.cpp"
#include "titan_cpplib/others/print.cpp"
#include "titan_cpplib/ahc/beam_search/beam_search.cpp"
#include "titan_cpplib/ahc/beam_search/beam_fast.cpp"

using namespace std;

//! 木上のビームサーチライブラリ
namespace beam_search {

using ScoreType = long long;
using HashType = unsigned long long;
const ScoreType INF = 1e18; // TODO -INFもできるように
titan23::Random brnd;

void beam_init() {
}

// TODO Action
// メモリ量は少ない方がよく、score,hash のメモは無くしたい
struct Action {
    ScoreType pre_score, nxt_score;
    HashType pre_hash, nxt_hash;

    Action() : pre_score(INF), nxt_score(INF), pre_hash(0), nxt_hash(0) {}
    friend ostream& operator<<(ostream& os, const Action &action) {
        return os;
    }

    string to_string() const {
        return "";
    }
};

class State {
private:
    ScoreType score;
    HashType hash;

public:
    // TODO Stateを初期化する
    void init() {
        this->score = 0;
        this->hash = 0;
    }

    // TODO 現在の状態に `action` を適用したときのスコアとハッシュ値を返す
    //! ロールバックに必要な情報はすべてactionにメモしておく
    //! threshold以上であれば計算しなくてよい
    //! INFを返すと無条件で採用しない
    tuple<ScoreType, HashType, bool> try_op(Action &action, const ScoreType threshold) const {
        action.pre_score = score;
        action.pre_hash = hash;
        ScoreType nxt_score = score;
        HashType nxt_hash = hash;
        bool finished = false;

        // TODO

        action.nxt_score = nxt_score;
        action.nxt_hash = nxt_hash;
        return {nxt_score, nxt_hash, finished};
    }

    // TODO 現在の状態に `action` を適用する
    //! `action` をする
    void apply_op(const Action &action) {
        // TODO
        score = action.nxt_score;
        hash = action.nxt_hash;
    }

    // TODO
    //! `action` を戻す
    void rollback(const Action &action) {
        // TODO
        score = action.pre_score;
        hash = action.pre_hash;
    }

    // TODO
    //! 現状態から遷移可能な `Action` の `vector` を `actions` に入れる
    void get_actions(vector<Action> &actions, const int turn, const Action &last_action, const ScoreType threshold) const {
    }

    // TODO
    void print() const {
    }

    string get_state_info() const {
        return "{}";
    }
};


flying_squirrel::BeamParam gen_param(int max_turn, int beam_width) {
    return {max_turn, beam_width, -1};
}

flying_squirrel::BeamParam gen_param(int max_turn, int beam_width, double time_limit, bool is_adjusting) {
    return {max_turn, beam_width, time_limit, is_adjusting};
}

vector<Action> search(flying_squirrel::BeamParam &param, const bool verbose=false, const string& history_file = "") {
    flying_squirrel::BeamSearchWithTree<ScoreType, HashType, Action, State, INF> bs;
    return bs.search(param, verbose, history_file);
}
} // namespace beam_search
