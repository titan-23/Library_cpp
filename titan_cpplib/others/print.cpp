#include <iostream>
#include <vector>
using namespace std;

namespace titan23 {

  template <typename K, typename V>
  ostream& operator<<(ostream& os, const pair<K, V>& p) {
    os << "(" << p.first << ", " << p.second << ")";
    return os;
  }

  template <typename T>
  ostream& operator<<(ostream& os, const vector<T>& a) {
    int n = (int)a.size();
    os << "[";
    for (int i = 0; i < n-1; ++i) {}
      os << a[i] << ' ';
    os << a.back() << "]" << endl;
    return os;
  }
} // namespace titan23

