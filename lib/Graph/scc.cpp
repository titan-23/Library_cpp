vector<vector<int>> get_scc(const vector<vector<int>> &G) {
  const int n = (int)G.size();
  vector<vector<int>> rG(n);
  for (int v = 0; v < n; ++v) {
    for (const int &x: G[v]) {
      rG[x].emplace_back(v);
    }
  }
  vector<int> visited(n, 0), dfsid(n, 0);
  int now = n;
  for (int s = 0; s < n; ++s) {
    if (visited[s]) continue;
    stack<int> todo;
    todo.push(~s);
    todo.push(s);
    while (!todo.empty()) {
      int v = todo.top();
      todo.pop();
      if (v >= 0) {
        if (visited[v]) continue;
        visited[v] = 2;
        for (const int &x: G[v]) {
          if (visited[x]) continue;
          todo.push(~x);
          todo.push(x);
        }
      } else {
        v = ~v;
        if (visited[v] == 1) continue;
        visited[v] = 1;
        dfsid[--now] = v;
      }
    }
  }
  vector<vector<int>> res;
  for (const int &s: dfsid) {
    if (!visited[s]) continue;
    vector<int> todo = {s};
    visited[s] = 0;
    int idx = 0;
    while (idx < todo.size()) {
      int v = todo[idx++];
      for (const int &x: rG[v]) {
        if (!visited[x]) continue;
        visited[x] = 0;
        todo.emplace_back(x);
      }
    }
    res.emplace_back(todo);
  }
  return res;
}
