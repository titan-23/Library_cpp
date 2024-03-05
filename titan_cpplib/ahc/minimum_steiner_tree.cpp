#include <vector>
#include "titan_cpplib/graph/dijkstra_path.cpp"
using namespace std;

// MinimumSteinerTree
namespace titan23 {

  template<typename T>
  struct MinimumSteinerTree {
    int n;
    vector<vector<pair<int, T>>> G;
    T INF;
    vector<titan23::dijkstra_path<ll>> dist_path;

    MinimumSteinerTree() {}
    MinimumSteinerTree(vector<vector<pair<int, T>>> &G, T INF) : n(G.size()), G(G), INF(INF) {
      dist_path.resize(n);
      for (int s = 0; s < N; ++i) {
        titan23::dijkstra_path<ll> d(G, s, INF);
        dist_path[s] = d;
      }
    }

    vector<pair<int, int>> build_prim(vector<int> terminal) {
      vector<int> used_vertex;
      used_vertex.reserve(terminal.size());
      vector<pair<int, int>> edges;

      used_vertex.emplace_back(terminal[0]);
      terminal.erase(terminal.begin());

      while (!terminal.empty()) {
        ll min_dist = INF;
        int min_tree_indx = -1;
        int min_terminal_indx = -1;
        for (const int v: used_vertex) {
          for (int i = 0; i < terminal.size(); ++i) {
            ll c = dist_path[v].get_dist(terminal[i]);
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
        for (int i = 0; i < path.size()-1; ++i) {
          edges.emplace_back(path[i], path[i+1]);
        }
        for (int i = 0; i < path.size(); ++i) {
          used_vertex.emplace_back(path[i]);
        }
      }
      sort(edges.begin(), edges.end());
      edges.erase(unique(edges.begin(), edges.end()), edges.end());
      return edges;
    }
  };
}  // namespace titan23

