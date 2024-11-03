#include <vector>
#include <stack>
using namespace std;

// RootedTree
namespace titan23 {

template<typename T>
class RootedTree {
  private:
    vector<vector<pair<int, T>>> G;

  public:
    int root;
    vector<int> toposo;
    vector<T> dist;

    RootedTree(vector<vector<pair<int, T>>> G, int root) : G(G), root(root) {}

    void calc_dist() {}
};
} // namespace titan23
