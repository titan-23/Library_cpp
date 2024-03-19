#include <iostream>
#include <vector>
#include <set>
#include <map>
using namespace std;

// print
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
