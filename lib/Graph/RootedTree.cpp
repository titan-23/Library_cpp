struct RootedTree {
  vector<vector<pair<int, int>>> G;
  int root;
  vector<long long> _dist;

  RootedTree(vector<vector<pair<int, int>>> G, int root) {
    this->G = G;
    this->root = root;
  }

  vector<long long> get_dist() {

  }
};
