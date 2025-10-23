#include<bits/stdc++.h>
using namespace std;

// 2N個のものをN個のペアに分ける方法
// (2*n-1) * (2*n-3) * (2*n-5) * ... * 1
vector<vector<pair<int, int>>> grouping_pair(int n) {
    vector<vector<pair<int, int>>> result;
    vector<pair<int, int>> tmp;
    vector<bool> is_used(n, false);
    auto dfs = [&] (auto &&dfs) -> void {
        if ((int)tmp.size() == n) {
            result.push_back(tmp);
            return;
        }
        int left = -1;
        for (int i = 0; i < 2*n; ++i) {
            if (!is_used[i]) {
                left = i;
                break;
            }
        }
        assert(left != -1);
        is_used[left] = true;
        for (int right = 0; right < 2*n; ++right) {
            if (!is_used[right]) {
                tmp.emplace_back(left, right);
                is_used[right] = true;
                dfs(dfs);
                tmp.pop_back();
                is_used[right] = false;
            }
        }
        is_used[left] = false;
    };

    dfs(dfs);
    return result;
}
