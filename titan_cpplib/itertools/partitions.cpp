#include <vector>
using namespace std;

namespace titan23 {
// 総和がSになる組合せの列挙
// 10 : 42
// 20 : 627
// 30 : 5604
// 40 : 37338
// 50 : 204226
// 60 : 966467
// 70 : 4087968
vector<vector<int>> partitions(int S) {
    vector<vector<int>> ans;
    vector<int> now;
    auto dfs = [&] (auto &&dfs, int rem) -> void {
        if (rem == 0) {
            ans.emplace_back(now);
            return;
        }
        int start = now.empty() ? 1 : now.back();
        for (int i = start; i <= rem; ++i) {
            now.emplace_back(i);
            dfs(dfs, rem-i);
            now.pop_back();
        }
    };
    dfs(dfs, S);
    return ans;
}
} // namespace titan23
