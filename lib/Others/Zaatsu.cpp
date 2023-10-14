template<typename T>
struct Zaatsu {
  vector<T> _to_origin;
  // unordered_map<T, int> _to_zaatsu;
  // map<T, int> _to_zaatsu;

  Zaatsu(vector<T> &used_items) {
    _to_origin = used_items;
    sort(_to_origin.begin(), _to_origin.end());
    _to_origin.erase(unique(_to_origin.begin(), _to_origin.end()), _to_origin.end());
    int n = (int)_to_origin.size();
    // for (int i = 0; i < n; ++i) {
    //   _to_zaatsu[_to_origin[i]] = i;
    // }
  }

  int to_zaatsu(const T &x) const {
    // auto it = _to_zaatsu.find(x);
    // assert(it != _to_zaatsu.end());
    // return it->second;
    return lower_bound(_to_origin.begin(), _to_origin.end(), x) - _to_origin.begin();
  }

  T to_origin(const int &x) const {
    return _to_origin[x];
  }
};
