/// https://github.com/titan-23/Library_cpp/blob/main/titan_cpplib/ahc/beam_search/beam_log.cpp
#pragma once

#include <bits/stdc++.h>
#include "titan_cpplib/others/print.cpp"
#include "titan_cpplib/ahc/beam_search/beam_param.cpp"
using namespace std;

namespace flying_squirrel {
namespace beam_log {

/// @brief ログ行の先頭に付ける [BS] の印を返す
inline const string& tag_bs() {
    static const string s = PRINT_BOLD + string("[BS]") + PRINT_NONE + " ";
    return s;
}
// 種類ごとの印。現在はすべて tag_bs() と同じ
inline const string& tag_info()  { return tag_bs(); }
inline const string& tag_ok()    { return tag_bs(); }
inline const string& tag_warn()  { return tag_bs(); }
inline const string& tag_error() { return tag_bs(); }
inline const string& tag_debug() { return tag_bs(); }
inline const string& tag_plain() { return tag_bs(); }
inline const string& tag_turn()  { return tag_bs(); }

// 文字列を種類ごとの色で囲む
inline string col_ok   (const string& m) { return PRINT_GREEN  + m + PRINT_NONE; }
inline string col_warn (const string& m) { return PRINT_YELLOW + m + PRINT_NONE; }
inline string col_error(const string& m) { return PRINT_RED    + m + PRINT_NONE; }
inline string col_debug(const string& m) { return PRINT_DIM    + m + PRINT_NONE; }

/// @brief msg に印を付け、種類に応じた色で 1 行出力する
inline void info (ostream& os, const string& msg) { os << tag_bs() << msg << "\n"; }
inline void ok   (ostream& os, const string& msg) { os << tag_bs() << col_ok(msg)    << "\n"; }
inline void warn (ostream& os, const string& msg) { os << tag_bs() << col_warn(msg)  << "\n"; }
inline void error(ostream& os, const string& msg) { os << tag_bs() << col_error(msg) << "\n"; }
inline void debug(ostream& os, const string& msg) { os << tag_bs() << col_debug(msg) << "\n"; }

#ifdef BS_DEBUG
/// @brief ok が false なら式と位置を出力して abort する / BS_DEBUG を定義したときだけ使える
inline void assert_check(bool ok, const char* expr, const char* file, int line, const string& msg = "") {
    if (ok) return;
    ostringstream ss;
    ss << file << ":" << line << " assertion failed: " << expr;
    if (!msg.empty()) ss << " | " << msg;
    error(cerr, ss.str());
    abort();
}
#endif

/// @brief 探索開始時に実装名と設定値を出力する
inline void start_banner(ostream& os, const char* impl_name, const BeamParam& param) {
    os << tag_plain() << "================================================\n";
    os << tag_info()  << "start " << to_bold(impl_name) << "\n";
    os << tag_info()  << "  max_turn   = " << param.max_turn << "\n";
    os << tag_info()  << "  beam_width = " << param.beam_width << "\n";
    os << tag_info()  << "  time_limit = " << fixed << setprecision(1) << param.time_limit << " [ms]\n";
    os << tag_info()  << "  adjusting  = " << (param.is_adjusting ? "true" : "false") << "\n";
    os << tag_info()  << "  clear_hash = " << (param.clear_hash_every_turn ? "true" : "false") << "\n";
    os << tag_plain() << "------------------------------------------------\n";
}

/// @brief 1 ターンの進捗を 1 行で出力する / explored < 0 なら展開数を省き、pool は使わない
template<class ScoreType>
inline void turn_line(ostream& os, int turn, int max_turn, double elapsed_ms, int width, int pool, int cand,
                      int explored, ScoreType best_score, bool has_best = true) {
    (void)pool;
    ostringstream ss;
    ss << setw(4) << setfill(' ') << turn << "/"
       << setw(4) << setfill(' ') << max_turn << setfill(' ')
       << " | t=" << fixed << setprecision(1) << setw(8) << elapsed_ms << "ms";
    if (explored >= 0) {
        ss << " | expl= " << setw(7) << explored;
    }
    ss << " | cand/w= " << setw(4) << cand << "/" << setw(4) << width;
    if (has_best) {
        ss << " | best= " << best_score;
    }
    os << tag_turn() << ss.str() << "\n";
}

/// @brief ターンの行の下に補足を 1 行出力する
inline void turn_line_extra(ostream& os, const string& extra) {
    os << tag_plain() << "  " << to_dim(extra) << "\n";
}

/// @brief 完成解を見つけたことを出力する
template<class ScoreType>
inline void on_solution_found(ostream& os, int turn, ScoreType score) {
    ostringstream ss;
    ss << "valid solution found at turn " << turn << " (score=" << score << ")";
    ok(os, ss.str());
}
/// @brief 候補がなくなったことを出力する
inline void on_no_candidates(ostream& os, int turn) {
    ostringstream ss;
    ss << "no candidates at turn " << turn;
    error(os, ss.str());
}
/// @brief max_turn に達したことを出力する
inline void on_max_turn(ostream& os) {
    ok(os, "reached max_turn");
}

/// @brief 探索終了時に終了理由と集計値を出力する
template<class ScoreType>
inline void end_banner(ostream& os, const char* reason, int turns_done, int max_turn, double total_ms,
                       double ave_width, ScoreType best_score, bool has_best, int actions_count) {
    os << tag_plain() << "------------------------------------------------\n";
    os << tag_bs()    << col_ok(string("finished: ") + reason) << "\n";
    os << tag_info()  << "  turns done    = " << turns_done << " / " << max_turn << "\n";
    os << tag_info()  << "  total time    = " << fixed << setprecision(1) << total_ms << " [ms]\n";
    os << tag_info()  << "  ave width     = " << fixed << setprecision(2) << ave_width << "\n";
    if (has_best) {
        os << tag_info() << "  best score    = " << best_score << "\n";
    }
    os << tag_info()  << "  actions count = " << actions_count << "\n";
    os << tag_plain() << "================================================\n";
}

/// @brief 終了時の集計に key = value の行を加える
inline void end_banner_extra(ostream& os, const string& key, long long value) {
    os << tag_info() << "  " << left << setw(14) << key << "= " << value << "\n";
}

/// @brief 実効ビーム幅の推移を ▁〜█ の文字列と統計値で出力する / 標本が cols 個を超えると区間ごとに平均する
inline void width_trace(ostream& os, const vector<int>& hist, int cols = 40) {
    if (hist.empty()) {
        os << tag_info() << "  wtrace = (no samples)\n";
        return;
    }
    static const char* blocks[8] = {"▁", "▂", "▃", "▄", "▅", "▆", "▇", "█"};
    const int n = (int)hist.size();

    // 標本数が cols を超える場合は区間ごとの平均で縮める
    vector<double> bucket;
    if (n <= cols) {
        bucket.assign(hist.begin(), hist.end());
    } else {
        bucket.resize(cols, 0.0);
        for (int b = 0; b < cols; ++b) {
            int lo = (int)((long long)b * n / cols);
            int hi = (int)((long long)(b + 1) * n / cols);
            if (hi <= lo) hi = lo + 1;
            double s = 0.0;
            for (int i = lo; i < hi && i < n; ++i) s += hist[i];
            bucket[b] = s / (hi - lo);
        }
    }

    int vmin = hist[0], vmax = hist[0];
    long long sum = 0;
    for (int v : hist) { vmin = min(vmin, v); vmax = max(vmax, v); sum += v; }
    double mean = (double)sum / n;

    vector<int> sorted = hist;
    sort(sorted.begin(), sorted.end());
    int p50 = sorted[n / 2];

    string spark;
    double span = (vmax > vmin) ? (double)(vmax - vmin) : 1.0;
    for (double v : bucket) {
        int lv = (int)((v - vmin) / span * 7.0 + 0.5);
        if (lv < 0) lv = 0;
        if (lv > 7) lv = 7;
        spark += blocks[lv];
    }

    os << tag_info() << "  wtrace = " << spark << " (n=" << n << ")\n";
    os << tag_info() << "  wstats = min=" << vmin
       << " p50=" << p50
       << " mean=" << fixed << setprecision(1) << mean
       << " max=" << vmax
       << " last=" << hist.back() << "\n";
}

} // namespace beam_log
} // namespace flying_squirrel
