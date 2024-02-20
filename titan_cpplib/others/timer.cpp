#include <iostream>
#include <chrono>
#include <thread>

// Timer
namespace titan23 {
  class Timer {
  public:
    Timer() : start_timepoint(std::chrono::high_resolution_clock::now()) {}

    void reset() {
      start_timepoint = std::chrono::high_resolution_clock::now();
    }

    double elapsed() {
      auto end_timepoint = std::chrono::high_resolution_clock::now();
      auto start = std::chrono::time_point_cast<std::chrono::microseconds>(start_timepoint).time_since_epoch().count();
      auto end = std::chrono::time_point_cast<std::chrono::microseconds>(end_timepoint).time_since_epoch().count();
      return (end - start) * 0.001; // ミリ秒単位で経過時間を返す
    }
    
  private:
    std::chrono::time_point<std::chrono::high_resolution_clock> start_timepoint;
  };
}

