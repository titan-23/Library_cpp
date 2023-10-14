vector<int> topological_sort(const vector<vector<int>> &G) {
  int n = G.size();
  vector<int> d(n);
  for (int i = 0; i < n; ++i) d[i] = 0;
  for (int i = 0; i < n; ++i) {
    for (const int &x: G[i]) {
      ++d[x];
    }
  }
  vector<int> res, todo;
  for (int i = 0; i < n; ++i) {
    if (d[i] == 0) todo.emplace_back(i);
  }
  while (!todo.empty()) {
    int v = todo.back();
    todo.pop_back();
    res.emplace_back(v);
    for (const int &x: G[v]) {
      --d[x];
      if (d[x] == 0) {
        todo.emplace_back(x);
      }
    }
  }
  return res;
}
