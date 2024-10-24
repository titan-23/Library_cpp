// #include "titan_cpplib/graph/bfs_path.cpp"
#include <vector>
#include <algorithm>
#include <queue>
using namespace std;

// bfs_path
namespace titan23 {

    vector<int> bfs_path(const vector<vector<int>> &G, int s, int t) {
        int n = G.size();
        vector<int> prev(n, -1);
        const int inf = 1e9;
        vector<int> dist(n, inf);
        dist[s] = 0;
        queue<int> todo;
        todo.emplace(s);
        while (!todo.empty()) {
            int v = todo.front();
            todo.pop();
            for (const int x : G[v]) {
                if (dist[x] == inf) {
                    dist[x] = dist[v] + 1;
                    prev[x] = v;
                    todo.emplace(x);
                }
            }
        }
        if (dist[t] == inf) {
            return {};
        }
        vector<int> path;
        while (prev[t] != -1) {
            path.emplace_back(t);
            t = prev[t];
        }
        path.emplace_back(t);
        reverse(path.begin(), path.end());
        return path;
    }
}

