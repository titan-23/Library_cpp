#include <vector>
#include <algorithm>
#include "titan_cpplib/graph/warshall_floyd_path.cpp"
using namespace std;

// MinimumSteinerTree
namespace titan23 {

/**
 * @brief 最小シュタイナー木など
 * @details プリム法を用いた近似解法の実装
 * 時間 O(|V|^3), 空間 O(|V|^2)
 */
template<typename T>
class MinimumSteinerTree {
    private:
    int n;
    T INF;
    titan23::WarshallFloydPath<T> dist_path;

    public:
    MinimumSteinerTree() {}

    // 時間 O(|V|^3), 空間 O(|V|^2)
    MinimumSteinerTree(const vector<vector<pair<int, T>>> &G, const T INF) :
            n((int)G.size()), INF(INF) {
        dist_path = titan23::WarshallFloydPath<T>(G, INF);
    }

    // terminal を含むシュタイナー木の辺集合を返す
    vector<pair<int, int>> build_prim(vector<int> terminal) {
        if (terminal.empty()) return {};
        vector<int> used_vertex;
        used_vertex.reserve(terminal.size());
        vector<pair<int, int>> edges;

        used_vertex.emplace_back(terminal[0]);
        terminal.erase(terminal.begin());

        while (!terminal.empty()) {
            T min_dist = INF;
            int min_tree_indx = -1;
            int min_terminal_indx = -1;
            for (const int v: used_vertex) {
                for (int i = 0; i < terminal.size(); ++i) {
                    T c = dist_path.get_dist(v, terminal[i]);
                    if (c < min_dist) {
                        min_dist = c;
                        min_tree_indx = v;
                        min_terminal_indx = i;
                    }
                }
            }
            int min_terminal = terminal[min_terminal_indx];
            terminal.erase(terminal.begin() + min_terminal_indx);
            const vector<int> &path = dist_path.get_path(min_tree_indx, min_terminal);
            for (int i = 0; i < path.size(); ++i) {
                auto it = find(terminal.begin(), terminal.end(), path[i]);
                if (it != terminal.end()) terminal.erase(it);
                if (i+1 < path.size()) edges.emplace_back(minmax(path[i], path[i+1]));
                used_vertex.emplace_back(path[i]);
            }
        }
        sort(edges.begin(), edges.end());
        edges.erase(unique(edges.begin(), edges.end()), edges.end());
        return edges;
    }
};
}  // namespace titan23
