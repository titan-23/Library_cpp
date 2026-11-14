/// https://github.com/titan-23/Library_cpp/blob/main/titan_cpplib/ahc/timer.cpp
#pragma once

#include <iostream>
#include <chrono>

using namespace std;

// Timer
namespace titan23 {

/// @brief 経過時間を計測する
class Timer {
private:
    chrono::time_point<chrono::high_resolution_clock> start_timepoint;

public:
    /// @brief 構築した時刻から計測を始める
    Timer() : start_timepoint(chrono::high_resolution_clock::now()) {}

    /// @brief 計測の起点を現在の時刻に戻す
    void reset() {
        start_timepoint = chrono::high_resolution_clock::now();
    }

    /// @brief 起点からの経過時間を ms 単位の double で返す
    double elapsed() const {
        // auto end_timepoint = chrono::high_resolution_clock::now();
        // auto start = chrono::time_point_cast<chrono::microseconds>(start_timepoint).time_since_epoch().count();
        // auto end = chrono::time_point_cast<chrono::microseconds>(end_timepoint).time_since_epoch().count();
        // return (end - start) * 0.001;
        auto end_timepoint = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double, std::milli> duration = end_timepoint - start_timepoint;
        return duration.count();
    }
};
}  // namespace titan23
