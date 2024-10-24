// #include "titan_cpplib/data_structures/array.cpp"
#include <iostream>
#include <vector>
#include <algorithm>
#include <cassert>
using namespace std;

namespace titan23 {

  template<typename T>
  class Array {
   private:
    vector<T> a;

   public:
    Array() {}
    Array(const int n) : a(n) {}
    Array(const int n, const T &key) : a(n, key) {}
    Array(const vector<T> &v) : a(v) {}

    void emplace_back(const T key) {
      a.emplace_back(key);
    }

    void resize(const int n, const T &e) {
      assert(n >= 0);
      a.resize(n, e);
    }

    void reserve(const int n) {
      assert(n >= 0);
      a.reserve(n);
    }

    T sum(T e) const{
      T s = e;
      for (const T &x: a) s += x;
      return s;
    }

    bool empty() const {
      return len() == 0;
    }

    void sort(bool reverse=false) {
      if (reverse) {
        std::sort(a.rbegin(), a.rend());
      } else {
        std::sort(a.begin(), a.end());
      }
    }

    int index(T &key) const {
      for (int i = 0; i < len(); ++i) {
        if (a[i] == key) return i;
      }
      return -1;
    }

    int count(const T &key) const {
      int cnt = 0;
      for (const T &x: a) {
        if (x == key) ++cnt;
      }
      return cnt;
    }

    void clear() {
      a.clear();
    }

    void insert(const int i, const T &key) {
      assert(0 <= i && i <= len());
      a.insert(a.begin()+i, key);
    }

    T pop(int i=-1) {
      if (i < 0) i += len();
      T key = a[i];
      a.erase(a.begin() + i);
      return key;
    }

    void remove(T &key) {
      std::remove(a.begin(), a.end(), key);
    }

    void reverse() {
      int n = len() >> 1;
      for (int i = 0; i < n; ++i) {
        swap(a[i], a[n-i-1]);
      }
    }

    void reverse(int l, int r) {
      // reverse([l, r))
      int n = len();
      assert(0 <= l && l <= r && r <= n);
      --r;
      for (int i = 0; i < ((r+1-l)>>1); ++i) {
        swap(a[l+i], a[r-i]);
      }
    }

    void swap(const int i, const int j) {
      assert(0 <= i < len());
      assert(0 <= j < len());
      swap(a[i], a[j]);
    }

    void swap_range(int l0, int r0, int l1, int r1) {
      // swap([l0, r0), [l1, r1))
      // O(n)
      if (r0 > l1) {
        swap(l0, l1);
        swap(r0, r1);
      }
      assert(l0 <= r0);
      assert(r0 <= l1);
      assert(l1 <= r1);
      vector<T> b = a;
      a.erase(a.begin()+l1, a.begin()+r1);
      a.erase(a.begin()+l0, a.begin()+r0);
      a.insert(a.begin()+l0, b.begin()+l1, b.begin()+r1);
      a.insert(a.begin()+r1-(r0-l0), b.begin()+l0, b.begin()+r0);
    }

    Array<T> copy() const {
      Array<T> res(a);
      return res;
    }

    bool contain(T &key) const {
      for (const T &x: a) {
        if (x == key) return true;
      }
      return false;
    }

    T& operator[] (const int i) {
      assert(-len() <= i && i < len());
      return a[i<0? i+len(): i];
    }

    T operator[] (const int i) const {
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

    void pprint(string sep=" ", string end="\n") {
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

