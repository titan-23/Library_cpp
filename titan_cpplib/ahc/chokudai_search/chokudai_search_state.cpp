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

/// @brief 探索開始前に 1 度だけ呼んで初期化する
void chokudai_init() {
    // TODO Zobrist hash 用の乱数テーブルなどを用意する
}

// TODO Action を問題に合わせて書く。score と hash のメモは無くしたい
/// @brief 遷移と rollback に使う情報 / メモリ量は少ないほどよい
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

/// @brief 問題の状態 / apply_op と rollback で差分更新する
class State {
private:
    ScoreType score;
    HashType hash;

public:
    /// @brief 状態を初期化する
    void init() {
        // TODO
        score = 0;
        hash = 0;
    }

    /// @brief 現状態から遷移可能な Action を 1 つずつ submit(action) に渡す
    /// last_action は親の Action で、turn == 0 では参照しない。submit.threshold() は常に INF を返す
    template<class Submit>
    void enumerate_actions(const int turn, const Action &last_action, Submit &&submit) const {
        // TODO: Action a = ...; submit(a); を必要なだけ繰り返す
    }

    /// @brief action を適用したときの評価値、ハッシュ値、終了したかを返す / INF を返すと採用されない
    /// rollback に必要な情報はすべて action に書く
    /// 評価値が threshold 以上と確定した時点で計算を打ち切ってよい
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

    /// @brief 現在の状態に action を適用する
    void apply_op(const Action &action) {
        // TODO
        score = action.nxt_score;
        hash = action.nxt_hash;
    }

    /// @brief action を適用する前の状態に戻す
    void rollback(const Action &action) {
        // TODO
        score = action.pre_score;
        hash = action.pre_hash;
    }

    /// @brief デバッグ用に現在の状態を出力する / ライブラリからは呼ばれない
    void print() const {
        // TODO
    }

    /// @brief record_history=true のときに使う可視化用の JSON 文字列を返す / 不要なら "{}" のままでよい
    string get_state_info() const {
        return "{}";
    }
};


/// @brief 最大探索ターン max_turn と、各巡回で各世代から展開する数 beam_width を指定した ChokudaiParam を返す / 時間制限なし
flying_squirrel::ChokudaiParam gen_param(int max_turn, int beam_width) {
    return {max_turn, beam_width, -1};
}

/// @brief 打ち切り時間 time_limit [ms] も指定した ChokudaiParam を返す / time_limit が負なら無制限
flying_squirrel::ChokudaiParam gen_param(int max_turn, int beam_width, double time_limit) {
    return {max_turn, beam_width, time_limit};
}

using Result = flying_squirrel::BeamResult<ScoreType, Action, State>;

/// @brief param に従って ChokudaiSearch を実行し、結果を返す
template<bool materialize_final_state=true, class Param>
Result search(Param &param, const bool verbose=false, const string& history_file = "") {
    flying_squirrel::ChokudaiSearch<ScoreType, HashType, Action, State, INF, false> bs;
    return bs.search<materialize_final_state>(param, verbose, history_file);
}
} // namespace chokudai_search
