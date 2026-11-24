/// https://github.com/titan-23/Library_cpp/blob/main/titan_cpplib/ahc/cpu_timer.cpp
#pragma once

#include <ctime>
#ifdef _WIN32
#include <windows.h>
#endif

// CpuTimer
namespace titan23 {

/// @brief プロセスの CPU 時間を計測するタイマー / 使用方法は titan23::Timer と同じ
/// マルチスレッドでは全スレッドの合計になる
/// 取得に数十から数百 ns かかるため、候補ごとには呼び出さないようにする
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
        // FILETIME の単位は 100 ns
        return (double)(k.QuadPart + u.QuadPart) * 1e-4;
#else
        timespec ts;
        clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &ts);
        return (double)ts.tv_sec * 1e3 + (double)ts.tv_nsec * 1e-6;
#endif
    }

public:
    /// @brief 計測を開始する
    CpuTimer() : start_ms(now_ms()) {}

    /// @brief 計測の起点を現時点に再設定する
    void reset() {
        start_ms = now_ms();
    }

    /// @brief 経過した CPU 時間を ms 単位で返す
    double elapsed() const {
        return now_ms() - start_ms;
    }
};

}  // namespace titan23
