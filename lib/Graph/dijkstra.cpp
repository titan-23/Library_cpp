vector<long long> dijkstra(const vector<vector<pair<int, long long>>> &G, const int s) {
  const long long INF = 1e18;
  int n = (int)G.size();
  vector<long long> dist(n, INF);
  priority_queue<pair<long long, int>> hq;
  hq.push({0, s});
  dist[s] = 0;
  while (!hq.empty()) {
    auto &[d, v] = hq.top();
    hq.pop();
    if (dist[v] > d) continue;
    for (const auto &[x, c]: G[v]) {
      if (dist[x] > d + c) {
        dist[x] = d + c;
        hq.push({dist[x], x});
      }
    }
  }
  return dist;
}
