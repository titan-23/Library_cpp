namespace titan23 {

/// @brief Doubling
template <class T, T (*op)(T, T), T (*e)()>
class Doubling {
private:
    int n, log;
    vector<vector<pair<int, T>>> db;

    int bit_length(long long LIM) {
        if (LIM == 0) return 0;
        return 64 - __builtin_clzll(LIM);
    }

public:
    /// @param LIM kthの最大値
    /// @param A A[i]:=iからの遷移先
    /// @param B B[i]:=iからの遷移で加算される値
    Doubling(long long LIM, const vector<int> &A, const vector<T> &B) : n(A.size()), log(bit_length(LIM)) {
        db.resize(log+1, vector<pair<int, T>>(n, {-1, e()}));
        for (int k = 0; k < n; ++k) {
            db[0][k] = {A[k], B[k]};
        }
        for (int k = 0; k < log; ++k) {
            for (int i = 0; i < n; ++i) {
                if (db[k][i].first == -1) {
                    db[k+1][i] = db[k][i];
                } else {
                    db[k+1][i].first = db[k][db[k][i].first].first;
                    db[k+1][i].second = op(db[k][i].second, db[k][db[k][i].first].second);
                }
            }
        }
    }

    /// @brief startからk回遷移したときの{位置、モノイド積}
    pair<int, T> kth(int start, long long k) const {
        assert(0 <= start && start < n);
        T res = e();
        for (int i = log-1; i >= 0; --i) {
            if (k >> i & 1) {
                res = op(res, db[i][start].second);
                start = db[i][start].first;
            }
            if (start == -1) break;
        }
        return {start, res};
    }
};
} // namespace titan23
