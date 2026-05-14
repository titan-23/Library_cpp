#pragma once

#include <bits/stdc++.h>
#include "titan_cpplib/others/print.cpp"
#include "titan_cpplib/ahc/beam_search/beam_param.cpp"
using namespace std;

namespace flying_squirrel {
namespace beam_log {

inline const string& tag_info() {
    static const string s = PRINT_BOLD + string("[BeamSearch]") + PRINT_NONE + "[INFO ] ";
    return s;
}
inline const string& tag_ok() {
    static const string s = PRINT_BOLD + string("[BeamSearch]") + PRINT_NONE
                               + PRINT_GREEN + "[OK   ]" + PRINT_NONE + " ";
    return s;
}
inline const string& tag_warn() {
    static const string s = PRINT_BOLD + string("[BeamSearch]") + PRINT_NONE
                               + PRINT_YELLOW + "[WARN ]" + PRINT_NONE + " ";
    return s;
}
inline const string& tag_error() {
    static const string s = PRINT_BOLD + string("[BeamSearch]") + PRINT_NONE
                               + PRINT_RED + "[ERROR]" + PRINT_NONE + " ";
    return s;
}
inline const string& tag_debug() {
    static const string s = PRINT_BOLD + string("[BeamSearch]") + PRINT_NONE
                               + PRINT_DIM + "[DEBUG]" + PRINT_NONE + " ";
    return s;
}
inline const string& tag_plain() {
    static const string s = PRINT_BOLD + string("[BeamSearch]") + PRINT_NONE + "        ";
    return s;
}

inline void info (ostream& os, const string& msg) { os << tag_info()  << msg << "\n"; }
inline void ok   (ostream& os, const string& msg) { os << tag_ok()    << msg << "\n"; }
inline void warn (ostream& os, const string& msg) { os << tag_warn()  << msg << "\n"; }
inline void error(ostream& os, const string& msg) { os << tag_error() << msg << "\n"; }
inline void debug(ostream& os, const string& msg) { os << tag_debug() << msg << "\n"; }

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

template<class ScoreType>
inline void turn_line(ostream& os,
                      int turn, int max_turn,
                      double elapsed_ms,
                      int width, int pool, int cand,
                      ScoreType best_score, bool has_best = true) {
    ostringstream ss;
    ss << "turn "
       << setw(4) << setfill(' ') << turn << "/"
       << setw(4) << setfill(' ') << max_turn << setfill(' ')
       << " | t=" << fixed << setprecision(1) << setw(8) << elapsed_ms << "ms"
       << " | w=" << setw(6) << width
       << " pool=" << setw(6) << pool
       << " cand=" << setw(6) << cand;
    if (has_best) {
        ss << " best=" << best_score;
    }
    os << tag_info() << ss.str() << "\n";
}

inline void turn_line_extra(ostream& os, const string& extra) {
    os << tag_plain() << "  " << to_dim(extra) << "\n";
}

template<class ScoreType>
inline void on_solution_found(ostream& os, int turn, ScoreType score) {
    ostringstream ss;
    ss << "valid solution found at turn " << turn << " (score=" << score << ")";
    ok(os, ss.str());
}
inline void on_no_candidates(ostream& os, int turn) {
    ostringstream ss;
    ss << "no candidates at turn " << turn;
    error(os, ss.str());
}
inline void on_max_turn(ostream& os) {
    ok(os, "reached max_turn");
}

template<class ScoreType>
inline void end_banner(ostream& os,
                       const char* reason,
                       int turns_done, int max_turn,
                       double total_ms,
                       double ave_width,
                       ScoreType best_score, bool has_best,
                       int actions_count) {
    os << tag_plain() << "------------------------------------------------\n";
    os << tag_ok()    << "finished: " << reason << "\n";
    os << tag_info()  << "  turns done    = " << turns_done << " / " << max_turn << "\n";
    os << tag_info()  << "  total time    = " << fixed << setprecision(1) << total_ms << " [ms]\n";
    os << tag_info()  << "  ave width     = " << fixed << setprecision(2) << ave_width << "\n";
    if (has_best) {
        os << tag_info() << "  best score    = " << best_score << "\n";
    }
    os << tag_info()  << "  actions count = " << actions_count << "\n";
    os << tag_plain() << "================================================\n";
}

inline void end_banner_extra(ostream& os, const string& key, long long value) {
    os << tag_info() << "  " << left << setw(14) << key << "= " << value << "\n";
}

} // namespace beam_log
} // namespace flying_squirrel
