#include <iostream>
#include <vector>
#include <cassert>
#include <algorithm>

#include "titan_cpplib/algorithm/random.cpp"
#include "titan_cpplib/others/print.cpp"
#include "titan_cpplib/ahc/beam_search.cpp"

using namespace std;

//! 木上のビームサーチライブラリ
namespace beam_serach { // flying squirrel over trees

using ScoreType = long long;
using HashType = unsigned long long;
const ScoreType INF = 1e18;

// Action
// メモリ量は少ない方がよく、score,hash のメモは無くしたい
struct Action {
    char d;
    ScoreType pre_score, nxt_score;
    HashType pre_hash, nxt_hash;

    Action() {}
    Action(const char d) : d(d), pre_score(INF), nxt_score(INF), pre_hash(0), nxt_hash(0) {}

    friend ostream& operator<<(ostream& os, const Action &action) {
        os << action.d;
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
        this->hash = 0;
    }

    // TODO
    //! `action` をしたときの評価値とハッシュ値を返す
    //! ロールバックに必要な情報はすべてactionにメモしておく
    pair<ScoreType, HashType> try_op(Action &action) const {
        action.pre_score = score;
        action.pre_hash = hash;
        ScoreType nxt_score = score;
        HashType nxt_hash = hash;

        // TODO

        action.nxt_score = nxt_score;
        action.nxt_hash = nxt_hash;
        return {nxt_score, nxt_hash};
    }

    // TODO
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
    //! 現状態から遷移可能な `Action` の `vector` を返す
    vector<Action> get_actions() const {
        vector<Action> actions;
        return actions;
    }

    void print() const {
    }
};

struct BeamParam {
    int MAX_TURN, BEAM_WIDTH;
};

vector<Action> search(const BeamParam &param, const bool verbose=false) {
    flysquirrel::BeamSearchWithTree<ScoreType, HashType, Action, State> bs;
    return bs.search(param, verbose);
}
} // namespace beam_search
