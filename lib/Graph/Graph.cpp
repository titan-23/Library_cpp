#include <vector>
#include <queue>
using namespace std;

namespace titan23 {

  struct Graph {
    vector<vector<int>> G;
    int n;

    Graph(int n) : n(n), G(n) {};

    void add_edge(const int u, const int v) {
      G[u].emplace_back(v);
    }

    int len() const {
      return (int)G.size();
    }
  };
} // namespace titan23
