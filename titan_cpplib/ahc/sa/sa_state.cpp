#pragma once

// OMP_NUM_THREADS=32 time ./a.out < in/0000.txt > out.txt
#include <bits/stdc++.h>
#include "titan_cpplib/ahc/timer.cpp"
#include "titan_cpplib/algorithm/random.cpp"
#include "titan_cpplib/ahc/sa/sa.cpp"
using namespace std;

// minimize SA
namespace sa {

void sa_init() {
    titan23::Random sarnd;
}

class State {
public:
    titan23::Random sarnd;
    using ScoreType = double; // TODO

    // TODO
    struct Param {
        double start_temp, end_temp;
        Param() : start_temp(1e3), end_temp(1e0) {}
    };
    inline static Param param;

    // TODO
    struct Changed {
        int TYPE_CNT = 0; // TODO
        int type;
        Changed() {}
    } changed;

    // TODO
    struct Result {
        ScoreType score, true_score;
        Result() {}
        Result(ScoreType s, ScoreType ts) :
            score(s), true_score(ts) {}
        void print(ostream &os = cout) const {}
    };

    bool is_valid;
    ScoreType score;
    State() {}

    // =========================
    // TODO

    // TODO
    void init(uint32_t seed=23) {
        sarnd.set_seed(seed);
        score = 0;
    }

    void reset_is_valid() { is_valid = true; }
    ScoreType get_score() const { return score; }
    ScoreType get_true_score() const { return score; }

    // TODO
    // thresholdを超えたら必ずreject(同じなら遷移する)
    // is_validをfalseにすると必ずrejectする / rollbackはする
    // progress: 焼きなまし進行度 0.0~1.0 まで
    void modify(const int64_t iter, const ScoreType threshold, const double progress) {}

    // TODO
    // scoreはライブラリ側で戻す
    void rollback() {}

    // TODO
    // scoreはライブラリ側で戻す
    void advance() {}

    // TODO
    Result get_result() const {
        return {get_score(), get_true_score()};
    }
};
}
