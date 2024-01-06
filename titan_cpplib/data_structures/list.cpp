#include <iostream>
#include <vector>
#include <algorithm>
#include <cassert>
using namespace std;

namespace titan23 {

  template<typename T>
  struct List {

    vector<T> a;

    List() {}

    List(const int n) {
      assert(n >= 0);
      a.resize(n);
    }

    List(const int n, const T key) {
      assert(n >= 0);
      a.resize(n, key);
    }

    List(const vector<T> &v) {
      a.resize(v.size());
      for (int i = 0; i < v.size(); ++i) {
        a[i] = v[i];
      }
    };

    void append(const T key) {
      a.emplace_back(key);
    }

    void resize(const int n, const T e) {
      assert(n >= 0);
      a.resize(n, e);
    }

    void reserve(const int n) {
      assert(n >= 0);
      a.reserve(n);
    }

    T sum(const T e) const {
      T res = e;
      for (int i = 0; i < len(); ++i) {
        res += a[i];
      }
      return res;
    }

    void sort(const bool reverse=false) {
      if (reverse) {
        std::sort(a.rbegin(), a.rend());
      } else {
        std::sort(a.begin(), a.end());
      }
    }
    
    int index(const T key) const {
      for (int i = 0; i < len(); ++i) {
        if (a[i] == key) return i;
      }
      return -1;
    }

    int count(const T key) const {
      int res = 0;
      for (int i = 0; i < len(); ++i) {
        if (a[i] == key) ++res;
      }
      return res;
    }

    void clear() {
      a.clear();
    }

    void insert(const int i, const T key) {
      assert(0 <= i && i <= len());
      a.insert(a.begin()+i, key);
    }

    T pop(const int i=-1) {
      T key = a[i<0? i+len(): i];
      remove(key);
      return key;
    }

    void remove(const T key) {
      std::remove(a.begin(), a.end(), key);
    }

    void reverse() {
      int n = len() >> 1;
      for (int i = 0; i < n; ++i) {
        swap(a[i], a[n-i-1]);
      }
    }

    bool contains(const T key) const {
      for (int i = 0; i < len(); ++i) {
        if (a[i] == key) return true;
      }
      return false;
    }

    T& operator[] (int i) {
      assert(-len() <= i && i < len());
      return a[i<0? i+len(): i];
    }

    T operator[] (int i) const {
      assert(-len() <= i && i < len());
      return a[i<0? i+len(): i];
    }

    int len() const {
      return (int)a.size();
    }

    vector<T> tovector() const {
      vector<T> res = a;
      return res;
    }

    void print() const {
      cout << "[";
      for (int i = 0; i < len()-1; ++i) {
        cout << a[i] << ", ";
      }
      if (len() > 0) {
        cout << a.back();
      }
      cout << "]\n";
    }

    void pprint(string sep=" ", string end="\n") const {
      for (int i = 0; i < len()-1; ++i) {
        cout << a[i] << sep;
      }
      if (len() > 0) {
        cout << a.back();
      }
      cout << end;
    }
  };
}  // namespace titan23
