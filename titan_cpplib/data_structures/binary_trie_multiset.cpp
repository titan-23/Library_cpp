#include <vector>
#include <cassert>
using namespace std;

// BinaryTrieMultiset
namespace titan23 {

  template<typename T>
  class BinaryTrieMultiset {
   private:
    vector<unsigned int> left, right, par, size;
    unsigned int _end, _root, _bit;
    T _lim, _xor_val;

    unsigned int _make_node() {
      if (_end >= (int)left.size()) {
        left.emplace_back(0);
        right.emplace_back(0);
        par.emplace_back(0);
        size.emplace_back(0);
      }
      return _end++;
    }

    int _find(T key) const {
      key ^= _xor_val;
      unsigned int node = _root;
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

    void _remove(unsigned int node) {
      unsigned int cnt = size[node];
      for (int i = 0; i < _bit; ++i) {
        size[node] -= cnt;
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
        size[node] -= cnt;
        node = par[node];
      }
    }

   public:
    BinaryTrieMultiset(const unsigned int bit) {
      _end = 2;
      _bit = bit;
      _root = 1;
      _lim = (1ll) << bit;
      _xor_val = 0;
      left.resize(2);
      right.resize(2);
      par.resize(2);
      size.resize(2);
    }

    void reserve(const int n) {
      left.reserve(n);
      right.reserve(n);
      par.reserve(n);
      size.reserve(n);
    }

    void add(T key, int cnt = 1) {
      assert(0 <= key && key < _lim);
      key ^= _xor_val;
      unsigned int node = _root;
      for (int i = _bit-1; i >= 0; --i) {
        size[node] += cnt;
        if ((key >> i) & 1) {
          if (!right[node]) {
            unsigned int new_node = _make_node();
            right[node] = new_node;
            par[right[node]] = node;
          }
          node = right[node];
        } else {
          if (!left[node]) {
            unsigned int new_node = _make_node();
            left[node] = new_node;
            par[left[node]] = node;
          }
          node = left[node];
        }
      }
      size[node] += cnt;
    }

    bool contains(T key) const {
      return _find(key) != -1;
    }

    bool discard(T key, int cnt = 1) {
      assert(0 <= key && key < _lim);
      unsigned int node = _find(key);
      if (node == -1) {
        return false;
      } else if (size[node] <= cnt) {
        _remove(node);
      } else {
        while (node) {
          size[node] -= cnt;
          node = par[node];
        }
      }
      return true;
    }

    bool discard_all(T key) {
      return discard(key, count(key));
    }

    void remove(T key, int cnt = 1) {
      key ^= _xor_val;
      unsigned int node = _root;
      for (int i = _bit-1; i >= 0; --i) {
        if ((key >> i) & 1) {
          node = right[node];
        } else {
          node = left[node];
        }
        assert(node);
      }
      unsigned int c = size[node];
      if (c < cnt) {
        assert(false);
      } else if (c == cnt) {
        _remove(node);
      } else {
        while (node) {
          size[node] -= cnt;
          node = par[node];
        }
      }
    }

    int count(T key) const {
      unsigned int node = _find(key);
      return node == -1 ? 0 : size[node];
    }

    T pop(int k = -1) {
      if (k < 0) k += len();
      unsigned int node = _root;
      T key = _xor_val;
      T res = 0;
      for (int i = _bit-1; i >= 0; --i) {
        res <<= 1;
        if ((key >> i) & 1) {
          unsigned int t = size[right[node]];
          if (t <= k) {
            k -= t;
            res |= 1;
            node = left[node];
          } else {
            node = right[node];
          }
        } else {
          unsigned int t = size[left[node]];
          if (t <= k) {
            k -= t;
            res |= 1;
            node = right[node];
          } else {
            node = left[node];
          }
        }
      }
      if (size[node] == 1) {
        _remove(node);
      } else {
        while (node) {
          --size[node];
          node = par[node];
        }
      }
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
      assert(len() > 0);
      T key = _xor_val;
      T ans = 0;
      unsigned int node = _root;
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
      assert(len() > 0);
      T key = _xor_val;
      T ans = 0;
      unsigned int node = _root;
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
      assert(0 <= key && key < _lim);
      int k = 0;
      unsigned int node = _root;
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
      unsigned int node = _root;
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

    T get(int k) const {
      if (k < 0) k += len();
      unsigned int node = _root;
      T res = 0;
      for (int i = _bit-1; i >= 0; --i) {
        if ((_xor_val >> i) & 1) {
          unsigned int t = size[right[node]];
          res <<= 1;
          if (t <= k) {
            k -= t;
            res |= 1;
            node = left[node];
          } else {
            node = right[node];
          }
        } else {
          unsigned int t = size[left[node]];
          res <<= 1;
          if (t <= k) {
            k -= t;
            res |= 1;
            node = right[node];
          } else {
            node = left[node];
          }
        }
      }
      return res;
    }

    T gt(T key) const {
      int i = index_right(key);
      return (i >= size[_root]? (-1) : get(i));
    }

    T lt(T key) const {
      int i = index(key) - 1;
      return (i < 0? -1 : get(i));
    }

    T ge(T key) const {
      if (key == 0) return (len()? get_min() : -1);
      int i = index_right(key - 1);
      return (i >= size[_root]? -1 : get(i));
    }

    T le(T key) const {
      int i = index(key + 1) - 1;
      return (i < 0? -1 : get(i));
    }

    vector<T> tovector() const {
      vector<T> a;
      if (!len()) return a;
      a.reserve(len());
      for (int i = 0; i < len(); ++i) {
        T e = get(i);
        a.emplace_back(e);
      }
      return a;
    }

    bool empty() const {
      return size[_root] == 0;
    }

    void clear() {
      _root = 1;
    }

    void print() const {
      cout << "{";
      vector<T> a = tovector();
      for (int i = 0; i < len()-1; ++i) {
        cout << a[i] << ", ";
      }
      if (!a.empty()) {
        cout << a.back();
      }
      cout << "}" << endl;
    }

    int len() const {
      return size[_root];
    }
  };
}  // namespace titan23
