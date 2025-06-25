#include <vector>
#include <tuple>
#include <algorithm>
#include <cassert>
using namespace std;

namespace titan23 {

/// scc, 頂点を縮約した隣接リスト, もとの頂点->新たなグラフの頂点, 新たなグラフの頂点->もとの頂点
/// {groups, F, ids, ids_inv}
tuple<vector<vector<int>>, vector<vector<int>>, vector<int>, vector<vector<int>>> get_scc_graph(vector<vector<int>> G) {
    int n = G.size();
    vector<int> st(n, 0);
    int ptr = 0;
    vector<int> lowlink(n, -1), order(n, -1), ids(n, 0);
    int cur_time = 0;
    int group_cnt = 0;

    auto dfs = [&] (auto &&dfs, int v) -> void {
        order[v] = cur_time;
        lowlink[v] = cur_time;
        cur_time++;
        st[ptr] = v;
        ptr++;
        for (int x : G[v]) {
            if (order[x] == -1) {
                dfs(dfs, x);
                lowlink[v] = min(lowlink[v], lowlink[x]);
            } else {
                lowlink[v] = min(lowlink[v], order[x]);
            }
        }
        if (lowlink[v] == order[v]) {
            while (true) {
                int u = st[ptr - 1];
                ptr--;
                order[u] = n;
                ids[u] = group_cnt;
                if (u == v) {
                    break;
                }
            }
            group_cnt++;
        }
    };

    for (int v = 0; v < n; ++v) {
        if (order[v] == -1) {
            dfs(dfs, v);
        }
    }
    vector<vector<int>> groups(group_cnt);
    for (int v = 0; v < n; ++v) {
        groups[group_cnt-1-ids[v]].push_back(v);
    }
    vector<vector<int>> F(*max_element(ids.begin(), ids.end())+1);
    for (int v = 0; v < n; ++v) {
        for (int x : G[v]) {
            if (ids[v] != ids[x]) {
                F[ids[v]].push_back(ids[x]);
            }
        }
    }
    for (vector<int> &f : F) {
        sort(f.begin(), f.end());
        f.erase(unique(f.begin(), f.end()), f.end());
    }
    vector<vector<int>> ids_inv(F.size());
    for (int i = 0; i < (int)ids.size(); ++i) {
        ids_inv[ids[i]].push_back(i);
    }
    return {groups, F, ids, ids_inv};
}

} // namespace titan23
