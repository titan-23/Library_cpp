#include <iostream>
using namespace std;

namespace titan23 {

  template <typename K, typename V>
  ostream& operator<<(ostream& os, const pair<K, V>& p) {
    os << "(" << p.first << ", " << p.second << ")";
    return os;
  }

} // namespace titan23

