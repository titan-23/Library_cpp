#pragma once

#include <vector>
#include <cassert>
using namespace std;

namespace titan23 {

/// @brief Doubling
class Doubling {
private:
    int n, log;
    vector<vector<int>> db;

    int bit_length(long long LIM) {
        if (LIM == 0) return 0;
        return 64 - __builtin_clzll(LIM);
    }

public:
    /// @param LIM kthの最大値
    /// @param A A[i]:=iからの遷移先
    Doubling(long long LIM, const vector<int> &A) : n(A.size()), log(bit_length(LIM)) {
        db.resize(log+1, vector<int>(n, -1));
        for (int k = 0; k < n; ++k) {
            db[0][k] = A[k];
        }
        for (int k = 0; k < log; ++k) {
            for (int i = 0; i < n; ++i) {
                if (db[k][i] == -1) {
                    db[k+1][i] = -1;
                } else {
                    db[k+1][i] = db[k][db[k][i]];
                }
            }
        }
    }

    /// @brief startからk回遷移したときの位置
    int kth(int start, long long k) const {
        assert(0 <= start && start < n);
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
