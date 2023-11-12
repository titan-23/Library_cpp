#include <iostream>
#include <vector>
using namespace std;

namespace titan23 {

  template<typename T>
  struct BinaryTrieSet {
    vector<unsigned int> left, right, par, size;
    int _end, _root, _bit;
    T _lim, _xor_val;

    BinaryTrieSet(int bit) {
      _end = 2;
      _bit = bit;
      _root = 1;
      _lim = 1ll << bit;
      _xor_val = 0;
      left.resize(2);
      right.resize(2);
      par.resize(2);
      size.resize(2);
    }

    int _make_node() {
      if (_end >= (int)left.size()) {
        left.emplace_back(0);
        right.emplace_back(0);
        par.emplace_back(0);
        size.emplace_back(0);
      }
      ++_end;
      return _end-1;
    }

    int _find(T key) {
      key ^= _xor_val;
      int node = _root;
      for (int i = _bit-1; i >= 0; --i) {
        if ((key >> i) & 1) {
          if (!right[node]) return -1;
          node = right[node];
        } else {
          if (!left[node]) return -1;
          node = left[node];
        }
      }
      return node;
    }

    bool add(T key) {
      key ^= _xor_val;
      int node = _root;
      for (int i = _bit-1; i >= 0; --i) {
        if ((key >> i) & 1) {
          if (!right[node]) {
            int new_node = _make_node();
            right[node] = new_node;
            par[right[node]] = node;
          }
          node = right[node];
        } else {
          if (!left[node]) {
            int new_node = _make_node();
            left[node] = new_node;
            par[left[node]] = node;
          }
          node = left[node];
        }
      }
      if (size[node]) return false;
      size[node] = 1;
      for (int i = 0; i < _bit; ++i) {
        node = par[node];
        size[node] += 1;
      }
      return true;
    }

    bool contains(const T key) {
      return _find(key) != -1;
    }

    void _discard(int node) {
      for (int i = 0; i < _bit; ++i) {
        size[node] -= 1;
        if (left[par[node]] == node) {
          node = par[node];
          left[node] = 0;
          if (right[node]) break;
        } else {
          node = par[node];
          right[node] = 0;
          if (left[node]) break;
        }
      }
      while (node) {
        size[node] -= 1;
        node = par[node];
      }
    }

    bool discard(T key) {
      int node = _find(key);
      if (node == -1) return false;
      _discard(node);
      return true;
    }

    T pop(int k) {
      if (k < 0) k += len();
      int node = _root;
      T res = 0;
      for (int i = _bit-1; i >= 0; --i) {
        int b = (_xor_val >> i) & 1;
        if (b) swap(left, right);
        int t = size[left[node]];
        res <<= 1;
        if (!left[node]) {
          node = right[node];
          res |= 1;
        } else if (!right[node]) {
          node = left[node];
        } else {
          t = size[left[node]];
          if (t <= k) {
            k -= t;
            res |= 1;
            node = right[node];
          } else {
            node = left[node];
          }
        }
        if (b) swap(left, right);
      }
      _discard(node);
      return res ^ _xor_val;
    }

    T pop_min() {
      return pop(0);
    }

    T pop_max() {
      return pop(-1);
    }

    void all_xor(T x) {
      _xor_val ^= x;
    }

    T get_min() const {
      T key = _xor_val;
      T ans = 0;
      int node = _root;
      for (int i = _bit-1; i >= 0; --i) {
        ans <<= 1;
        if ((key >> i) & 1) {
          if (right[node]) {
            node = right[node];
            ans |= 1;
          } else {
            node = left[node];
          }
        } else {
          if (left[node]) {
            node = left[node];
          } else {
            node = right[node];
            ans |= 1;
          }
        }
      }
      return ans ^ _xor_val;
    }

    T get_max() const {
      T key = _xor_val;
      T ans = 0;
      int node = _root;
      for (int i = _bit-1; i >= 0; --i) {
        ans <<= 1;
        if ((key >> i) & 1) {
          if (left[node]) {
            node = left[node];
          } else {
            node = right[node];
            ans |= 1;
          }
        } else {
          if (right[node]) {
            ans |= 1;
            node = right[node];
          } else {
            node = left[node];
          }
        }
      }
      return ans ^ _xor_val;
    }

    int index(T key) const {
      int k = 0;
      int node = _root;
      key ^= _xor_val;
      for (int i = _bit-1; i >= 0; --i) {
        if ((key >> i) & 1) {
          k += size[left[node]];
          node = right[node];
        } else {
          node = left[node];
        }
        if (!node) break;
      }
      return k;
    }

    int index_right(T key) const {
      int k = 0;
      int node = _root;
      key ^= _xor_val;
      for (int i = _bit-1; i >= 0; --i) {
        if ((key >> i) & 1) {
          k += size[left[node]];
          node = right[node];
        } else {
          node = left[node];
        }
        if (!node) break;
      }
      if (node) k += 1;
      return k;
    }

    T gt(T key) const {
      int i = index_right(key);
      return (i >= size[_root]? -1 : kth_elm(i));
    }

    T lt(T key) const {
      int i = index(key) - 1;
      return (i < 0? -1 : kth_elm(i));
    }

    T ge(T key) const {
      if (key == 0) return (len()? get_min() : -1);
      int i = index_right(key - 1);
      return (i >= size[_root]? -1 : kth_elm(i));
    }

    T le(T key) const {
      int i = index(key + 1) - 1;
      return (i < 0? -1 : kth_elm(i));
    }

    vector<T> tovector() {
      vector<T> a;
      if (!len()) return a;
      a.reserve(len());
      T val = get_min();
      while (val != -1) {
        a.emplace_back(val);
        val = gt(val);
      }
      return a;
    }

    T kth_elm(int k) {
      if (k < 0) k += len();
      int node = _root;
      T res = 0;
      for (int i = _bit-1; i >= 0; --i) {
        int b = ((_xor_val >> i) & 1);
        if (b) swap(left, right);
        int t = size[left[node]];
        res <<= 1;
        if (!left[node]) {
          node = right[node];
          res |= 1;
        } else if (!right[node]) {
          node = left[node];
        } else {
          t = size[left[node]];
          if (t <= k) {
            k -= t;
            res |= 1;
            node = right[node];
          } else {
            node = left[node];
          }
        }
        if (b) swap(left, right);
      }
      return res;
    }

    bool empty() const {
      return size[_root] == 0;
    }

    void print() {
      cout << "{";
      vector<T> a = tovector();
      for (int i = 0; i < len()-1; ++i) {
        cout << a[i] << ", ";
      }
      cout << a.back();
      cout << "}" << endl;
    }

    int len() const {
      return size[_root];
    }
  };
}  // namespace titan23
