#include <iostream>
#include <vector>
#include <set>
#include <unordered_set>
#include <map>
using namespace std;

// print

// pair<K, V>
template <typename K, typename V>
ostream& operator<<(ostream& os, const pair<K, V>& p) {
    os << "(" << p.first << ", " << p.second << ")";
    return os;
}

// tuple<T1, T2, T3>
template<typename T1, typename T2, typename T3>
ostream &operator<<(ostream &os, const tuple<T1, T2, T3> &t) {
    os << "( " << get<0>(t) << ", " << get<1>(t) << ", " << get<2>(t) << " )";
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

// vector<vector<T>>
template <typename T>
ostream& operator<<(ostream& os, const vector<vector<T>>& a) {
    os << "[\n";
    int h = (int)a.size();
    for (int i = 0; i < h; ++i) {
        os << "  " << a[i] << '\n';
    }
    os << "]";
    return os;
}

// set<T>
template <typename T>
ostream& operator<<(ostream& os, const set<T>& a) {
  int n = (int)a.size();
  os << "{";
  for (const T &x: a) {
    os << x;
    if (x != *(--a.end())) {
      os << ", ";
    }
  }
  os << "}";
  return os;
}

// unordered_set<T>
template <typename T>
ostream& operator<<(ostream& os, const unordered_set<T>& a) {
  int n = (int)a.size();
  os << "{";
  for (const T &x: a) {
    os << x;
    os << ", ";
  }
  os << "}";
  return os;
}

// map<K, V>
template <typename K, typename V>
ostream& operator<<(ostream& os, const map<K, V>& mp) {
  int n = (int)mp.size();
  os << "{";
  auto it = mp.begin();
  while (it != mp.end()) {
    os << it->first << ": " << it->second;
    ++it;
    if (it != mp.end()) {
      os << ", ";
    }
  }
  os << "}";
  return os;
}
