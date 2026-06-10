#pragma once
#include <bits/stdc++.h>
#include "titan_cpplib/ahc/timer.cpp"
using namespace std;

namespace titan23 {

class Profiler {
public:
    enum class SortBy {
        Name,
        TotalTime,
        AvgTime
    };

private:
    titan23::Timer timer;

    struct Record {
        int count = 0;
        double total_time_ms = 0.0;
    };

    struct TimerState {
        string name;
        double start_time_ms;
    };

    unordered_map<string, Record> records;
    vector<TimerState> timer_stack;

public:
    void start(const string& name) {
        timer_stack.push_back({name, timer.elapsed()});
    }

    void stop() {
        if (timer_stack.empty()) return;
        double current_time_ms = timer.elapsed();
        const auto& active_timer = timer_stack.back();
        double elapsed_ms = current_time_ms - active_timer.start_time_ms;
        records[active_timer.name].count++;
        records[active_timer.name].total_time_ms += elapsed_ms;
        timer_stack.pop_back();
    }

    void report(SortBy sort_by = SortBy::Name) const {
        vector<pair<string, Record>> sorted_records(records.begin(), records.end());

        sort(sorted_records.begin(), sorted_records.end(), [sort_by](const auto& a, const auto& b) {
            if (sort_by == SortBy::TotalTime) {
                if (a.second.total_time_ms != b.second.total_time_ms) {
                    return a.second.total_time_ms > b.second.total_time_ms;
                }
                return a.first < b.first;
            } else if (sort_by == SortBy::AvgTime) {
                double avg_a = a.second.count > 0 ? a.second.total_time_ms / a.second.count : 0.0;
                double avg_b = b.second.count > 0 ? b.second.total_time_ms / b.second.count : 0.0;
                if (avg_a != avg_b) {
                    return avg_a > avg_b;
                }
                return a.first < b.first;
            } else {
                return a.first < b.first;
            }
        });

        cerr << "---------------------------------------------------------\n";
        cerr << left << setw(25) << "Name"
                  << right << setw(10) << "Count"
                  << setw(15) << "Total(ms)"
                  << setw(10) << "Avg(ms)\n";
        cerr << "---------------------------------------------------------\n";

        for (const auto& [name, rec] : sorted_records) {
            double avg = rec.count > 0 ? rec.total_time_ms / rec.count : 0.0;
            cerr << left << setw(25) << name
                      << right << setw(10) << rec.count
                      << setw(15) << fixed << setprecision(3) << rec.total_time_ms
                      << setw(10) << fixed << setprecision(5) << avg << "\n";
        }
        cerr << "---------------------------------------------------------\n";
    }
} profiler;
} // namespace titan23
