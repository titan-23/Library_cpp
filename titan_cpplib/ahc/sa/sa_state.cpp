/// https://github.com/titan-23/Library_cpp/blob/main/titan_cpplib/ahc/sa/sa_state.cpp
#pragma once

// OMP_NUM_THREADS=32 time ./a.out < in/0000.txt > out.txt
#include <bits/stdc++.h>
#include "titan_cpplib/ahc/timer.cpp"
#include "titan_cpplib/alg/random.cpp"
#include "titan_cpplib/ahc/sa/sa.cpp"
using namespace std;

// スコアを最小化する焼きなまし
namespace sa {

void sa_init() {
    titan23::Random sarnd;
}

/// @brief sa_run に渡す状態のテンプレート
class State {
public:
    titan23::Random sarnd;
    using ScoreType = double; // TODO

    // TODO
    /// @brief 開始温度と終了温度
    struct Param {
        double start_temp, end_temp;
        Param() : start_temp(1e3), end_temp(1e0) {}
    };
    inline static Param param;

    // TODO
    /// @brief 直前の遷移の情報 / type は遷移の種類で、[0, TYPE_CNT) の値を取る
    struct Changed {
        int TYPE_CNT = 1; // TODO
        int type = 0;
        Changed() {}
    } changed;

    // TODO
    /// @brief 最良解として保存する情報
    struct Result {
        ScoreType score, true_score;
        Result() {}
        Result(ScoreType s, ScoreType ts) :
            score(s), true_score(ts) {}
        void print(ostream &os = cout) const {
        }
    };

    bool is_valid;
    ScoreType score;
    State() {}

    // =========================
    // TODO

    // TODO
    /// @brief seed で乱数と状態を初期化する
    void init(uint32_t seed) {
        sarnd.set_seed(seed);
        score = 0;
    }

    void reset_is_valid() { is_valid = true; }
    ScoreType get_score() const { return score; }
    ScoreType get_true_score() const { return score; }

    // TODO
    /// @brief 近傍へ遷移し、score を更新する \
    /// @brief score が threshold を超えたら必ず棄却され、等しければ遷移する \
    /// @brief is_valid を false にすると必ず棄却され、rollback が呼び出される \
    /// @brief progress は進行度で、0.0 から 1.0 の値を取る
    void modify(const int64_t iter, const ScoreType threshold, const double progress) {
    }

    // TODO
    /// @brief modify の前の状態に戻す \
    /// @brief score はライブラリが元の値に戻すため、復元は不要 \
    /// @brief is_valid = false の場合も呼び出されることに注意する
    void rollback() {
        if (!is_valid) { return; }
    }

    // TODO
    /// @brief 遷移を確定する \
    /// @brief 遅延評価の場合は盤面の変更をここで確定させる
    void advance() {
    }

    // TODO
    /// @brief 現在の解を Result で返す
    Result get_result() const {
        return {get_score(), get_true_score()};
    }
};
} // namespace sa
