#include <iostream>
#include <vector>
using namespace std;

// print
namespace titan23 {

  // pair<K, V>
  template <typename K, typename V>
  ostream& operator<<(ostream& os, const pair<K, V>& p) {
    os << "(" << p.first << ", " << p.second << ")";
    return os;
  }

  // vector<T>
  template <typename T>
  ostream& operator<<(ostream& os, const vector<T>& a) {
    int n = (int)a.size();
    os << "[";
    for (int i = 0; i < n-1; ++i) {
      os << a[i] << ", ";
    }
    if (n > 0) {
      os << a.back();
    }
    os << "]";
    return os;
  }
} // namespace titan23

