#include <iostream>
#include <vector>
#include <tuple>
using namespace std;

namespace titan23 {

// https://maspypy.com/分割統治による静的列の区間積クエリ
template<class T, T (*op)(T, T), T (*e)()>
class RangeProductDC {
private:
    vector<T> a;
    vector<tuple<int, int, int>> query;

public:
    RangeProductDC() {}
    RangeProductDC(vector<T> a) : a(a) {}

    void add_query(int l, int r) {
        query.emplace_back(query.size(), l, r);
    }

    vector<T> solve() {
        vector<T> dp(a.size()+1, e());
        vector<T> ans(query.size(), e());

        auto calc = [&] (int L, int mid, int R, vector<tuple<int, int, int>> &Q) -> void {
            dp[mid] = e();
            for (int i = mid; i > L; --i) {
                dp[i-1] = op(a[i-1], dp[i]);
            }
            for (int i = mid; i < R; ++i) {
                dp[i+1] = op(dp[i], a[i]);
            }
            for (auto &[qdx, l, r] : Q) {
                ans[qdx] = op(dp[l], dp[r]);
            }
        };

        auto dfs = [&] (auto &&dfs, int L, int R, vector<tuple<int, int, int>> &Q) -> void {
            if (R - L <= 1) {
                for (auto &[qdx, l, r] : Q) {
                    ans[qdx] = a[l];
                }
                return;
            }
            int mid = (L + R) / 2;
            vector<tuple<int, int, int>> QL, QR, QS;
            for (auto &[qdx, l, r] : Q) {
                if (r <= mid) {
                    QL.emplace_back(qdx, l, r);
                } else if (mid <= l) {
                    QR.emplace_back(qdx, l, r);
                } else {
                    QS.emplace_back(qdx, l, r);
                }
            }
            calc(L, mid, R, QS);
            dfs(dfs, L, mid, QL);
            dfs(dfs, mid, R, QR);
        };
        dfs(dfs, 0, a.size(), query);
        return ans;
    }
};
} // namespace titan23
