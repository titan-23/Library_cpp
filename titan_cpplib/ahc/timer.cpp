#include <iostream>
#include <chrono>
#include <thread>

using namespace std;

// Timer
namespace titan23 {

  /**
   * @brief 時間計測クラス
   */
  class Timer {
   private:
    chrono::time_point<chrono::high_resolution_clock> start_timepoint;

   public:
    Timer() : start_timepoint(chrono::high_resolution_clock::now()) {}

    /**
     * @brief リセットする
     */
    void reset() {
      start_timepoint = chrono::high_resolution_clock::now();
    }

    /**
     * @brief 経過時間[ms]を返す
     */
    double elapsed() const {
      auto end_timepoint = chrono::high_resolution_clock::now();
      auto start = chrono::time_point_cast<chrono::microseconds>(start_timepoint).time_since_epoch().count();
      auto end = chrono::time_point_cast<chrono::microseconds>(end_timepoint).time_since_epoch().count();
      return (end - start) * 0.001;
    }
  };
}
