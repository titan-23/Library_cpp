#pragma once

// OMP_NUM_THREADS=8 time ./a.out < in/0000.txt > out.txt

#include <bits/stdc++.h>
#include "titan_cpplib/ahc/timer.cpp"
#include "titan_cpplib/algorithm/random.cpp"
#include "titan_cpplib/others/print.cpp"
#include "titan_cpplib/ahc/sa/sa.cpp"
using namespace std;

// minimize SA
namespace sa {

thread_local titan23::Random sarnd;

void sa_init() {}

class State {
public:
    using ScoreType = double; // TODO

    // TODO
    struct Param {
        double start_temp = 1e3;
        double end_temp = 1e0;
    };

    // TODO
    struct Changed {
        int TYPE_CNT = 0; // TODO
        int type;
        Changed() {}
    };

    // TODO
    struct Result {
        ScoreType score, true_score;
        Result() {}
        Result(ScoreType s, ScoreType ts) : score(s), true_score(ts) {}
        void print(ostream &os = cout) const {}
    };

    Param param;
    Changed changed;

    bool is_valid;
    ScoreType score;

    State() {}

    // TODO
    void init() {
        score = 0;
    }

    void reset_is_valid() { is_valid = true; }
    ScoreType get_score() const { return score; } // TODO 最大化なら `-score` などにする
    ScoreType get_true_score() const { return score; }

    // TODO
    // thresholdを超えたら必ずreject(同じなら遷移する)
    // is_validをfalseにすると必ずrejectする
    // progress:焼きなまし進行度 0.0~1.0 まで
    void modify(const ScoreType threshold, const double progress) {}

    // TODO
    void rollback() {}

    // TODO
    void advance() {}

    Result get_result() const {
        return {get_score(), get_true_score()};
    }
};
}
