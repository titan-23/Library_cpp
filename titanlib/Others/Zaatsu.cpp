#include <vector>
using namespace std;

namespace titan23 {
    
  template<typename T>
  struct Zaatsu {
    vector<T> _to_origin;

    Zaatsu(vector<T> &used_items) {
      _to_origin = used_items;
      sort(_to_origin.begin(), _to_origin.end());
      _to_origin.erase(unique(_to_origin.begin(), _to_origin.end()), _to_origin.end());
      int n = (int)_to_origin.size();
    }

    int to_zaatsu(const T &x) const {
      return lower_bound(_to_origin.begin(), _to_origin.end(), x) - _to_origin.begin();
    }

    T to_origin(const int &x) const {
      return _to_origin[x];
    }
  };
}  // namespace titan23

