/// https://github.com/titan-23/Library_cpp/blob/main/titan_cpplib/ahc/cpu_timer.cpp
#pragma once

#include <ctime>
#ifdef _WIN32
#include <windows.h>
#endif

// CpuTimer
namespace titan23 {

/// @brief プロセスの CPU 時間で計るタイマー / titan23::Timer と同じ使い方
/// マルチスレッドでは全スレッドの合計になる。取得が数十から数百 ns かかるので、候補ごとには呼ばない
class CpuTimer {
private:
    double start_ms;

    static double now_ms() {
#ifdef _WIN32
        FILETIME creation, exit_, kernel, user;
        GetProcessTimes(GetCurrentProcess(), &creation, &exit_, &kernel, &user);
        ULARGE_INTEGER k, u;
        k.LowPart = kernel.dwLowDateTime; k.HighPart = kernel.dwHighDateTime;
        u.LowPart = user.dwLowDateTime;   u.HighPart = user.dwHighDateTime;
        // FILETIME は 100ns 単位
        return (double)(k.QuadPart + u.QuadPart) * 1e-4;
#else
        timespec ts;
        clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &ts);
        return (double)ts.tv_sec * 1e3 + (double)ts.tv_nsec * 1e-6;
#endif
    }

public:
    /// @brief 計測を始める
    CpuTimer() : start_ms(now_ms()) {}

    /// @brief 計測を始め直す
    void reset() {
        start_ms = now_ms();
    }

    /// @brief 経過した CPU 時間を ms で返す
    double elapsed() const {
        return now_ms() - start_ms;
    }
};

}  // namespace titan23
