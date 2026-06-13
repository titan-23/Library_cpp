#pragma once

#include <ctime>
#ifdef _WIN32
#include <windows.h>
#endif

// CpuTimer
namespace titan23 {

/**
 * @brief プロセス CPU 時間で計る時間計測クラス (titan23::Timer と同一インターフェース)
 *
 * wall clock (Timer) との違い:
 * - 他プロセスにコアを取られて「待っていた時間」を含まない。
 *   ローカルでテストケースを並列実行するとき、wall 基準より挙動がジャッジに近づく。
 * - ジャッジ (1 プロセス占有) では CPU 時間 ≈ wall なので、時間予算をそのまま使える。
 *   ただし入出力待ちや起動オーバーヘッドは含まれないため、wall の制限時間に対しては
 *   従来どおりのマージンを取ること。
 *
 * 注意:
 * - キャッシュ・メモリ帯域の競合による劣化 (1 命令あたりの実効速度の低下) は
 *   CPU 時間にも乗る。並列数が物理コア以下でも遅くなる分はこれでは補正できない。
 * - マルチスレッドでは全スレッドの合計になる (このライブラリの用途では単一スレッド前提)。
 * - 取得は clock_gettime のシステムコールで wall clock よりやや重い (数十〜数百 ns)。
 *   世代ごとに数回呼ぶ程度なら無視できるが、候補ごとに呼ぶ用途には使わないこと。
 */
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
    CpuTimer() : start_ms(now_ms()) {}

    //! リセットする
    void reset() {
        start_ms = now_ms();
    }

    //! 経過 CPU 時間[ms](double)を返す
    double elapsed() const {
        return now_ms() - start_ms;
    }
};

}  // namespace titan23
