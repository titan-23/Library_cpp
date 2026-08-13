#pragma once

#include <vector>
#include <cassert>
#include <algorithm>
#include "titan_cpplib/others/bit.cpp"
using namespace std;

namespace titan23 {

/// @brief Doubling
class Doubling {
private:
    int n, log;
    long long lim;
    vector<vector<int>> db;

public:
    /// @param LIM kth の最大値
    /// @param A A[i]:= i からの遷移先
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

    /// @brief start から k 回遷移したときの位置
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
