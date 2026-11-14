/// https://github.com/titan-23/Library_cpp/blob/main/titan_cpplib/ahc/beam_search/beam_search_state.cpp
#pragma once

#include <iostream>
#include <vector>
#include <cassert>
#include <algorithm>

#include "titan_cpplib/alg/random.cpp"
#include "titan_cpplib/others/print.cpp"
#include "titan_cpplib/ahc/beam_search/naive_beam_search.cpp"
#include "titan_cpplib/ahc/beam_search/beam_search.cpp"

using namespace std;

// 木上のビームサーチライブラリ
namespace beam_search {

using ScoreType = long long;
using HashType = uint64_t;
const ScoreType INF = 1e18; // TODO -INF も扱えるようにする
titan23::Random brnd;

// TODO Zobrist hash 用の乱数表など、探索開始前に 1 度だけ行う初期化を書く
void beam_init() {
}

// TODO Action を定義する
// 多数複製されるので小さく保つ。score と hash のメモはできれば省く
/// @brief 1 回の遷移と、それを戻すための情報
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

/// @brief 探索の状態 / TODO の箇所を問題に合わせて書く
class State {
private:
    ScoreType score;
    HashType hash;

public:
    // TODO State を初期化する
    void init() {
        score = 0;
        hash = 0;
    }

    // TODO 現状態から遷移可能な `Action` を 1 つずつ `submit(action)` に渡す
    // turn は現在のターン、last_action は親の Action である。turn == 0 では last_action を参照しない
    // `submit.threshold()` で現在の枝刈り閾値を取得できる
    template<class Submit>
    void enumerate_actions(const int turn, const Action &last_action, Submit &&submit) const {
        // TODO: Action a = ...; submit(a); を必要なだけ繰り返す
    }

    // TODO 現在の状態に `action` を適用したときのスコアとハッシュ値を返す
    // ロールバックに必要な情報はすべて action に記録しておく
    // score が threshold 以上だと確定したら計算を打ち切ってよい
    // INF を返した遷移は必ず採用されない
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

    // TODO デバッグ用に現在の状態を出力する。ライブラリからは呼ばれない
    void print() const {
    }

    // record_history が true のときに使う可視化用の JSON 文字列。不要なら "{}" のままでよい
    string get_state_info() const {
        return "{}";
    }
};


/// @brief 最大探索ターン max_turn とビーム幅 beam_width を指定した BeamParam を返す
flying_squirrel::BeamParam gen_param(int max_turn, int beam_width) {
    return {max_turn, beam_width, -1};
}

/// @brief 制限時間 time_limit [ms] も指定した BeamParam を返す
/// is_adjusting は幅を制限時間に合わせるか、clear_hash_every_turn はハッシュ辞書を毎ターン消すかを表す
flying_squirrel::BeamParam gen_param(int max_turn, int beam_width, double time_limit, bool is_adjusting=false, bool clear_hash_every_turn=true) {
    return {max_turn, beam_width, time_limit, is_adjusting, clear_hash_every_turn};
}

using Result = flying_squirrel::BeamResult<ScoreType, Action, State>;

/// @brief BeamSearchWithTree で探索し、結果を返す
template<bool materialize_final_state=true>
Result search(flying_squirrel::BeamParam &param, const bool verbose=false, const string& history_file = "") {
    flying_squirrel::BeamSearchWithTree<ScoreType, HashType, Action, State, INF, false> bs;
    return bs.search<materialize_final_state>(param, verbose, history_file);
}
} // namespace beam_search
