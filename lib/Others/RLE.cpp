template<typename T>
struct RLE {
  static vector<pair<T, int>> calc(vector<T> a) {
    int n = (int)a.size();
    vector<pair<T, int>> res;
    T now = a[0];
    res.emplace_back(now, 1);
    for (int i = 1; i < n; ++i) {
      if (a[i] == now) {
        res.back().second++;
      } else {
        res.emplace_back(a[i], 1);
        now = a[i];
      }
    }
    return res;
  }
};
