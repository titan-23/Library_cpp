/// https://github.com/titan-23/Library_cpp/blob/main/titan_cpplib/ahc/chokudai_search/chokudai_search_state.cpp
#pragma once

#include <iostream>
#include <vector>
#include <cassert>
#include <algorithm>

#include "titan_cpplib/alg/random.cpp"
#include "titan_cpplib/others/print.cpp"
#include "titan_cpplib/ahc/chokudai_search/chokudai_search.cpp"

using namespace std;

// 木上の差分更新型 ChokudaiSearch ライブラリ
namespace chokudai_search {

using ScoreType = long long;
using HashType = uint64_t;
constexpr ScoreType INF = 1e18; // TODO 実際の評価値より大きい値にする
titan23::Random brnd;

// TODO Zobrist hash 用の乱数テーブルなど、探索開始前に1度だけ用意する初期化
void chokudai_init() {
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
        score = 0;
        hash = 0;
    }

    // TODO 現状態から遷移可能な `Action` を 1 つずつ `submit(action)` に渡す
    // turn は現在のターン、last_action は親の Action(turn == 0 では参照しない)
    // `submit.threshold()` で閾値を取得できる（常に INF）
    template<class Submit>
    void enumerate_actions(const int turn, const Action &last_action, Submit &&submit) const {
        // TODO: Action a = ...; submit(a); を必要なだけ繰り返す
    }

    // TODO 現在の状態に `action` を適用したときのスコアとハッシュ値を返す
    // ロールバックに必要な情報はすべてactionにメモしておく
    // score が threshold 以上だと確定したら計算を打ち切ってよい
    // INFを返すと無条件で採用しない
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
    // `action` をする
    void apply_op(const Action &action) {
        // TODO
        score = action.nxt_score;
        hash = action.nxt_hash;
    }

    // TODO `action` を戻す
    void rollback(const Action &action) {
        // TODO
        score = action.pre_score;
        hash = action.pre_hash;
    }

    // TODO デバッグ用に現在の状態を出力する(ライブラリからは呼ばれない)
    void print() const {
    }

    // record_history=true 時の可視化用 JSON 文字列。不要なら "{}" のまま
    string get_state_info() const {
        return "{}";
    }
};


/// @brief ChokudaiParamを返す
/// @param max_turn 最大探索ターン
/// @param beam_width 1 巡回・1 世代あたりの展開数
/// @return ChokudaiParam
flying_squirrel::ChokudaiParam gen_param(int max_turn, int beam_width) {
    return {max_turn, beam_width, -1};
}

/// @brief ChokudaiParamを返す
/// @param max_turn 最大探索ターン
/// @param beam_width 1 巡回・1 世代あたりの展開数
/// @param time_limit 打ち切り時間 [ms]。負なら無制限
/// @return ChokudaiParam
flying_squirrel::ChokudaiParam gen_param(int max_turn, int beam_width, double time_limit) {
    return {max_turn, beam_width, time_limit};
}

using Result = flying_squirrel::BeamResult<ScoreType, Action, State>;

template<bool materialize_final_state=true, class Param>
Result search(Param &param, const bool verbose=false, const string& history_file = "") {
    flying_squirrel::ChokudaiSearch<ScoreType, HashType, Action, State, INF, false> bs;
    return bs.search<materialize_final_state>(param, verbose, history_file);
}
} // namespace chokudai_search
