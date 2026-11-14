/// https://github.com/titan-23/Library_cpp/blob/main/titan_cpplib/alg/doubling.cpp
#pragma once

#include <vector>
#include <cassert>
#include <algorithm>
#include "titan_cpplib/others/bit.cpp"
using namespace std;

namespace titan23 {

/// @brief ダブリングで k 回遷移した先を求める
class Doubling {
private:
    int n, log;
    long long lim;
    vector<vector<int>> db;

public:
    /// @brief A[i] を i からの遷移先として構築する / 遷移先がなければ -1 / k <= LIM まで扱う / O(n log LIM)
    Doubling(long long LIM, const vector<int> &A) : n(A.size()), log(bit_length(LIM)), lim(LIM) {
        db.resize(max(log, 1), vector<int>(n, -1));
        for (int k = 0; k < n; ++k) {
            db[0][k] = A[k];
        }
        for (int k = 0; k+1 < log; ++k) {
            for (int i = 0; i < n; ++i) {
                if (db[k][i] == -1) {
                    db[k+1][i] = -1;
                } else {
                    db[k+1][i] = db[k][db[k][i]];
                }
            }
        }
    }

    /// @brief start から k 回遷移した位置を返す / 途中で遷移先がなければ -1 / O(log LIM)
    int kth(int start, long long k) const {
        assert(0 <= start && start < n);
        assert(0 <= k && k <= lim);
        for (int i = log-1; i >= 0; --i) {
            if (k >> i & 1) {
                start = db[i][start];
            }
            if (start == -1) break;
        }
        return start;
    }
};
} // namespace titan23
