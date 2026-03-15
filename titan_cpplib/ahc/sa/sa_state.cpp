#pragma once

// OMP_NUM_THREADS=8 time ./a.out < in/0000.txt > out.txt

#include <bits/stdc++.h>
#include "titan_cpplib/ahc/timer.cpp"
#include "titan_cpplib/algorithm/random.cpp"
#include "titan_cpplib/others/print.cpp"
using namespace std;

// minimize SA
namespace sa {

using ScoreType = double; // TODO

struct Param {
    double start_temp = 1e3;
    double end_temp = 1e0;
} param;

// TODO
struct Changed {
    int TYPE_CNT = 0; // TODO
    int type;
    ScoreType pre_score;
    Changed() {}
};

thread_local Changed changed;
thread_local titan23::Random sarnd;

// TODO
void sa_init() {}

// TODO
struct Result {
    ScoreType score, true_score;
    Result() {}
    Result(ScoreType s, ScoreType ts) : score(s), true_score(ts) {}
    void print(ostream &os = cout) const {}
};

class State {
public:
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
    // thresholdを超えたらダメ(同じなら遷移する)
    // is_validをfalseにすると必ずrejectする、rollbackはする
    // progress:焼きなまし進行度 0.0~1.0 まで
    void modify(const ScoreType threshold, const double progress) {}

    // TODO
    // scoreはもう戻してある
    void rollback() {}

    // TODO
    void advance() {}

    Result get_result() const {
        return {get_score(), get_true_score()};
    }
};
}

// includeのタイミングはここ
#include "titan_cpplib/ahc/sa/sa.cpp"
// sa::Result ans = sa::sa_run(1900, true);
