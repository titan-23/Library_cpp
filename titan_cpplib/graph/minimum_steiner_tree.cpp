#include <vector>
#include <algorithm>
#include "titan_cpplib/graph/dijkstra_path.cpp"
using namespace std;

// MinimumSteinerTree
namespace titan23 {

  /**
   * @brief 最小シュタイナー木など
   * @details プリム法を用いた近似解法の実装
   * ダイクストラをN回実行し、結果をすべて保存するのでメモリに注意 
   */
  template<typename T>
  class MinimumSteinerTree {
   private:
    int n;
    T INF;
    vector<titan23::dijkstra_path<T>> dist_path;

   public:
    MinimumSteinerTree() {}
    MinimumSteinerTree(vector<vector<pair<int, T>>> &G, T INF) : n((int)G.size()), INF(INF) {
      dist_path.resize(n);
      for (int s = 0; s < n; ++s) {
        titan23::dijkstra_path<T> d(G, s, INF);
        dist_path[s] = d;
      }
    }

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
            T c = dist_path[v].get_dist(terminal[i]);
            if (c < min_dist) {
              min_dist = c;
              min_tree_indx = v;
              min_terminal_indx = i;
            }
          }
        }
        int min_terminal = terminal[min_terminal_indx];
        terminal.erase(terminal.begin() + min_terminal_indx);
        vector<int> path = dist_path[min_tree_indx].get_path(min_terminal);
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
