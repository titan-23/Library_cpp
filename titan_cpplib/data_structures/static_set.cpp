#include <vector>
#include <algorithm>
using namespace std;

// StaticSet
namespace titan23 {

  template<typename T>
  struct StaticSet {
    vector<T> data;
    int n;

    StaticSet() {}
    StaticSet(vector<T> &a) : data(a) {
      sort(data.begin(), data.end());
      data.erase(unique(data.begin(), data.end()), data.end());
      n = (int)data.size();
    }

    void print() const {
      cout << "{";
      for (int i = 0; i < n-1; ++i) {
        cout << data[i] << ", ";
      }
      if (n > 0) {
        cout << data.back();
      }
      cout << "}" << endl;
    }

    T get(const int i) const {
      return data[i];
    }

    // key以上で最小
    optional<T> ge(const T &key) const {
      if (key > data.back()) return nullopt;
      int l = -1, r = n-1;
      while (r - l > 1) {
        int mid = (l + r) >> 1;
        ((data[mid] >= key)? r : l) = mid;
      }
      return data[r];
    }

    // keyより大きくて最小
    optional<T> gt(const T &key) const {
      if (key >= data.back()) return nullopt;
      int l = -1, r = n-1;
      while (r - l > 1) {
        int mid = (l + r) >> 1;
        ((data[mid] > key)? r : l) = mid;
      }
      return data[r];
    }

    // key以下で最大
    optional<T> le(const T &key) const {
      if (key < data[0]) return nullopt;
      int l = 0, r = n;
      while (r - l > 1) {
        int mid = (l + r) >> 1;
        ((data[mid] <= key)? l : r) = mid;
      }
      return data[l];
    }

    // key未満で最大
    optional<T> lt(const T &key) const {
      if (key <= data[0]) return nullopt;
      int l = 0, r = n;
      while (r - l > 1) {
        int mid = (l + r) >> 1;
        ((data[mid] < key)? l : r) = mid;
      }
      return data[l];
    }
  };
}
