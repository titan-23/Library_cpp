// #include "titan_cpplib/data_structures/dynamic_wavelet_tree.cpp"
#include <iostream>
#include <vector>
// #include "titan_cpplib/data_structures/avl_tree_bit_vector.cpp"
#include <iostream>
#include <vector>
#include <stack>
#include <cassert>
#include <tuple>
#include <nmmintrin.h>
#include <stdint.h>
using namespace std;

// AVLTreeBitVector
namespace titan23 {

class AVLTreeBitVector {
 private:
  using Node = int;
  // using uint64 = unsigned long long;
  // static constexpr const char _W = 63;
  using uint128 = __uint128_t;
  static constexpr const char _W = 127;
  Node _root, _end;
  vector<uint128> _key;
  vector<Node> _left, _right;
  vector<int> _size, _total;
  vector<char> _bit_len, _balance;

  void _build(const vector<uint8_t> &a) {
    auto rec = [&] (auto &&rec, Node l, Node r) -> pair<Node, char> {
      Node mid = (l + r) >> 1;
      char hl = 0, hr = 0;
      if (l != mid) {
        tie(_left[mid], hl) = rec(rec, l, mid);
        _size[mid] += _size[_left[mid]];
        _total[mid] += _total[_left[mid]];
      }
      if (mid + 1 != r) {
        tie(_right[mid], hr) = rec(rec, mid+1, r);
        _size[mid] += _size[_right[mid]];
        _total[mid] += _total[_right[mid]];
      }
      _balance[mid] = hl - hr;
      return {mid, (max(hl, hr)+1)};
    };

    const int n = a.size();
    reserve(n);
    Node pre_end = _end;
    int indx = _end;
    for (int i = 0; i < n; i += _W) {
      int j = 0;
      int pop = 0;
      uint128 v = 0;
      while (j < _W && i + j < n) {
        v <<= 1;
        if (a[i+j]) {
          v |= a[i+j];
          ++pop;
        }
        j++;
      }
      _key[indx] = v;
      _bit_len[indx] = j;
      _size[indx] = j;
      _total[indx] = pop;
      ++indx;
    }
    this->_end = indx;
    this->_root = rec(rec, pre_end, _end).first;
  }

  int _popcount(const uint128 n) const {
    return __builtin_popcountll(n >> 64) + __builtin_popcountll(n);
    // return __builtin_popcountll(n);
  }

  Node _rotate_L(Node node) {
    Node u = _left[node];
    _size[u] = _size[node];
    _total[u] = _total[node];
    _size[node] -= _size[_left[u]] + _bit_len[u];
    _total[node] -= _total[_left[u]] + _popcount(_key[u]);
    _left[node] = _right[u];
    _right[u] = node;
    if (_balance[u] == 1) {
      _balance[u] = 0;
      _balance[node] = 0;
    } else {
      _balance[u] = -1;
      _balance[node] = 1;
    }
    return u;
  }

  Node _rotate_R(Node node) {
    Node u = _right[node];
    _size[u] = _size[node];
    _total[u] = _total[node];
    _size[node] -= _size[_right[u]] + _bit_len[u];
    _total[node] -= _total[_right[u]] + _popcount(_key[u]);
    _right[node] = _left[u];
    _left[u] = node;
    if (_balance[u] == -1) {
      _balance[u] = 0;
      _balance[node] = 0;
    } else {
      _balance[u] = 1;
      _balance[node] = -1;
    }
    return u;
  }

  void _update_balance(Node node) {
    if (_balance[node] == 1) {
      _balance[_right[node]] = -1;
      _balance[_left[node]] = 0;
    } else if (_balance[node] == -1) {
      _balance[_right[node]] = 0;
      _balance[_left[node]] = 1;
    } else {
      _balance[_right[node]] = 0;
      _balance[_left[node]] = 0;
    }
    _balance[node] = 0;
  }

  Node _rotate_LR(Node node) {
    Node B = _left[node];
    Node E = _right[B];
    _size[E] = _size[node];
    _size[node] -= _size[B] - _size[_right[E]];
    _size[B] -= _size[_right[E]] + _bit_len[E];
    _total[E] = _total[node];
    _total[node] -= _total[B] - _total[_right[E]];
    _total[B] -= _total[_right[E]] + _popcount(_key[E]);
    _right[B] = _left[E];
    _left[E] = B;
    _left[node] = _right[E];
    _right[E] = node;
    _update_balance(E);
    return E;
  }

  Node _rotate_RL(Node node) {
    Node C = _right[node];
    Node D = _left[C];
    _size[D] = _size[node];
    _size[node] -= _size[C] - _size[_left[D]];
    _size[C] -= _size[_left[D]] + _bit_len[D];
    _total[D] = _total[node];
    _total[node] -= _total[C] - _total[_left[D]];
    _total[C] -= _total[_left[D]] + _popcount(_key[D]);
    _left[C] = _right[D];
    _right[D] = C;
    _right[node] = _left[D];
    _left[D] = node;
    _update_balance(D);
    return D;
  }

  int _pref(int r) const {
    Node node = _root;
    int s = 0;
    while (r > 0) {
      int t = _size[_left[node]] + _bit_len[node];
      if (t - _bit_len[node] < r && r <= t) {
        r -= _size[_left[node]];
        s += _total[_left[node]] + _popcount(_key[node] >> (_bit_len[node] - r));
        break;
      }
      if (t > r) {
        node = _left[node];
      } else {
        s += _total[_left[node]] + _popcount(_key[node]);
        node = _right[node];
        r -= t;
      }
    }
    return s;
  }

  Node _make_node(const bool new_key, const char new_bit_len) {
    if (_end >= _key.size()) {
      _key.emplace_back(new_key);
      _bit_len.emplace_back(new_bit_len);
      _size.emplace_back(new_bit_len);
      _total.emplace_back(new_key);
      _left.emplace_back(0);
      _right.emplace_back(0);
      _balance.emplace_back(0);
    } else {
      _key[_end] = new_key;
      _bit_len[_end] = new_bit_len;
      _size[_end] = new_bit_len;
      _total[_end] = new_key;
    }
    return _end++;
  }

  uint128 _bit_insert(uint128 v, char bl, bool key) const {
    return ((((v >> bl) << 1) | key) << bl) | (v & (((uint128)1<<bl)-1));
  }

  uint128 _bit_pop(uint128 v, char bl) const {
    return ((v >> bl) << ((bl-1))) | (v & (((uint128)1<<(bl-1))-1));
  }

  void _pop_under(stack<Node> &path, int d, Node node, int res) {
    int fd = 0, lmax_total = 0;
    char lmax_bit_len = 0;
    if (_left[node] && _right[node]) {
      path.emplace(node);
      d = (d << 1) | 1;
      Node lmax = _left[node];
      while (_right[lmax]) {
        path.emplace(lmax);
        d <<= 1;
        fd = (fd << 1) | 1;
        lmax = _right[lmax];
      }
      lmax_total = _popcount(_key[lmax]);
      lmax_bit_len = _bit_len[lmax];
      _key[node] = _key[lmax];
      _bit_len[node] = lmax_bit_len;
      node = lmax;
    }
    Node cnode = _left[node] == 0 ? _right[node] : _left[node];
    if (!path.empty()) {
      ((d & 1) ? _left[path.top()] : _right[path.top()]) = cnode;
    } else {
      _root = cnode;
      return;
    }
    while (!path.empty()) {
      Node new_node = 0;
      node = path.top(); path.pop();
      _balance[node] -= (d & 1) ? 1 : -1;
      _size[node] -= (fd & 1) ? lmax_bit_len : 1;
      _total[node] -= (fd & 1) ? lmax_total : res;
      d >>= 1;
      fd >>= 1;
      if (_balance[node] == 2) {
        new_node = _balance[_left[node]] < 0 ? _rotate_LR(node) : _rotate_L(node);
      } else if (_balance[node] == -2) {
        new_node = _balance[_right[node]] > 0 ? _rotate_RL(node) : _rotate_R(node);
      } else if (_balance[node] != 0) {
        break;
      }
      if (new_node) {
        if (path.empty()) {
          _root = new_node;
          return;
        }
        ((d & 1) ? _left[path.top()] : _right[path.top()]) = new_node;
        if (_balance[new_node] != 0) break;
      }
    }
    while (!path.empty()) {
      node = path.top(); path.pop();
      _size[node] -= (fd & 1) ? lmax_bit_len : 1;
      _total[node] -= (fd & 1) ? lmax_total : res;
      fd >>= 1;
    }
  }

  void _debug_acc() {
    auto rec = [&] (auto &&rec, Node node) -> int {
      int acc = _popcount(_key[node]);
      if (_left[node]) acc += rec(rec, _left[node]);
      if (_right[node]) acc += rec(rec, _right[node]);
      if (acc != _total[node]) {
        assert(false);
      }
      return acc;
    };
    rec(rec, _root);
    cout << "debug_acc ok." << endl;
  }

 public:
  AVLTreeBitVector()
      : _root(0), _end(1),
        _key(1, 0),
        _left(1, 0), _right(1, 0),
        _size(1, 0), _total(1, 0),
        _bit_len(1, 0), _balance(1, 0) {
  }

  AVLTreeBitVector(const vector<uint8_t> &a)
      : _root(0), _end(1),
        _key(1, 0),
        _left(1, 0), _right(1, 0),
        _size(1, 0), _total(1, 0),
        _bit_len(1, 0), _balance(1, 0) {
    if (!a.empty()) _build(a);
  }

  void reserve(int n) {
    n = n / _W + 1;
    _key.insert(_key.end(), n, (uint128)0);
    _left.insert(_left.end(), n, 0);
    _right.insert(_right.end(), n, 0);
    _size.insert(_size.end(), n, 0);
    _total.insert(_total.end(), n, 0);
    _bit_len.insert(_bit_len.end(), n, (char)0);
    _balance.insert(_balance.end(), n, (char)0);
  }

  void insert(int k, bool key) {
    if (!_root) {
      Node new_node = _make_node(key, 1);
      _root = new_node;
      return;
    }
    Node node = _root;
    int d = 0;
    stack<Node> path;
    while (node) {
      int t = _size[_left[node]] + _bit_len[node];
      if (t - _bit_len[node] <= k && k <= t) break;
      d <<= 1;
      _size[node]++;
      _total[node] += key;
      path.emplace(node);
      node = (t > k) ? _left[node] : _right[node];
      if (t > k) d |= 1;
      else k -= t;
    }
    k -= _size[_left[node]];
    if (_bit_len[node] < _W) {
      uint128 v = _key[node];
      char bl = _bit_len[node] - k;
      _key[node] = _bit_insert(v, bl, key);
      _bit_len[node]++;
      _size[node]++;
      _total[node] += key;
      return;
    }
    path.emplace(node);
    _size[node]++;
    _total[node] += key;
    uint128 v = _key[node];
    char bl = _W - k;
    v = _bit_insert(v, bl, key);
    uint128 left_key = v >> _W;
    char left_key_popcount = left_key & 1;
    _key[node] = v & (((uint128)1 << _W) - 1);
    node = _left[node];
    d = (d << 1) | 1;
    if (!node) {
      if (_bit_len[path.top()] < _W) {
        _bit_len[path.top()]++;
        _key[path.top()] = (_key[path.top()] << 1) | left_key;
        return;
      } else {
        Node new_node = _make_node(left_key, 1);
        _left[path.top()] = new_node;
      }
    } else {
      path.emplace(node);
      _size[node]++;
      _total[node] += left_key_popcount;
      d <<= 1;
      while (_right[node]) {
        node = _right[node];
        path.emplace(node);
        _size[node]++;
        _total[node] += left_key_popcount;
        d <<= 1;
      }
      if (_bit_len[node] < _W) {
        _bit_len[node]++;
        _key[node] = (_key[node] << 1) | left_key;
        return;
      } else {
        Node new_node = _make_node(left_key, 1);
        _right[node] = new_node;
      }
    }
    Node new_node = 0;
    while (!path.empty()) {
      node = path.top(); path.pop();
      _balance[node] += (d & 1) ? 1 : -1;
      d >>= 1;
      if (_balance[node] == 0) break;
      if (_balance[node] == 2) {
        new_node = _balance[_left[node]] == -1 ? _rotate_LR(node) : _rotate_L(node);
        break;
      } else if (_balance[node] == -2) {
        new_node = _balance[_right[node]] == 1 ? _rotate_RL(node) : _rotate_R(node);
        break;
      }
    }
    if (new_node) {
      if (!path.empty()) {
        if (d & 1) {
          _left[path.top()] = new_node;
        } else {
          _right[path.top()] = new_node;
        }
      } else {
        _root = new_node;
      }
    }
  }

  bool pop(int k) {
    Node node = _root;
    int d = 0;
    stack<Node> path;
    while (node) {
      int t = _size[_left[node]] + _bit_len[node];
      if (t - _bit_len[node] <= k && k < t) break;
      path.emplace(node);
      node = t > k ? _left[node] : _right[node];
      d <<= 1;
      if (t > k) d |= 1;
      else k -= t;
    }
    k -= _size[_left[node]];
    uint128 v = _key[node];
    bool res = (v >> (_bit_len[node] - k - 1)) & 1;
    if (_bit_len[node] == 1) {
      _pop_under(path, d, node, res);
      return res;
    }
    _key[node] = _bit_pop(v, _bit_len[node]-k);
    --_bit_len[node];
    --_size[node];
    _total[node] -= res;
    while (!path.empty()) {
      node = path.top(); path.pop();
      --_size[node];
      _total[node] -= res;
    }
    return res;
  }

  void set(int k, bool v) {
    Node node = _root;
    stack<Node> path;
    while (true) {
      int t = _size[_left[node]] + _bit_len[node];
      path.emplace(node);
      if (t - _bit_len[node] <= k && k < t) {
        k -= _size[_left[node]];
        if (v) {
          _key[node] |= (uint128)1 << k;
        } else {
          _key[node] &= ~((uint128)1 << k);
        }
        break;
      }
      if (t > k) {
        node = _left[node];
      } else {
        node = _right[node];
        k -= t;
      }
    }
    while (!path.empty()) {
      node = path.top(); path.pop();
      _total[node] = _popcount(_key[node]) + _total[_left[node]] + _total[_right[node]];
    }
  }

  vector<uint8_t> tovector() const {
    vector<uint8_t> a(len());
    if (!_root) return a;
    int indx = 0;
    auto rec = [&] (auto &&rec, Node node) -> void {
      if (_left[node]) rec(rec, _left[node]);
      uint128 key = _key[node];
      for (int i = _bit_len[node]-1; i >= 0; --i) {
        a[indx++] = key >> i & 1;
      }
      if (_right[node]) rec(rec, _right[node]);
    };
    rec(rec, _root);
    return a;
  }

  bool access(int k) const {
    Node node = _root;
    while (true) {
      int t = _size[_left[node]] + _bit_len[node];
      if (t - _bit_len[node] <= k && k < t) {
        k -= _size[_left[node]];
        return (_key[node] >> (_bit_len[node] - k - 1)) & 1;
      }
      if (t > k) {
        node = _left[node];
      } else {
        node = _right[node];
        k -= t;
      }
    }
  }

  int rank0(int r) const {
    return r - _pref(r);
  }

  int rank1(int r) const {
    return _pref(r);
  }

  int rank(int r, bool v) const {
    return v ? rank1(r) : rank0(r);
  }

  int select0(int k) const {
    Node node = _root;
    int s = 0;
    while (true) {
      int t = _size[_left[node]] - _total[_left[node]];
      if (k < t) {
        node = _left[node];
      } else if (k >= t + _bit_len[node] - _popcount(_key[node])) {
        s += _size[_left[node]] + _bit_len[node];
        k -= t + _bit_len[node] - _popcount(_key[node]);
        node = _right[node];
      } else {
        k -= t;
        char l = 0, r = _bit_len[node];
        while (r - l > 1) {
          char m = (l + r) >> 1;
          if (m - _popcount(_key[node]>>(_bit_len[node]-m)) > k) r = m;
          else l = m;
        }
        s += _size[_left[node]] + l;
        break;
      }
    }
    return s;
  }

  int select1(int k) const {
    Node node = _root;
    int s = 0;
    while (true) {
      if (k < _total[_left[node]]) {
        node = _left[node];
      } else if (k >= _total[_left[node]] + _popcount(_key[node])) {
        s += _size[_left[node]] + _bit_len[node];
        k -= _total[_left[node]] + _popcount(_key[node]);
        node = _right[node];
      } else {
        k -= _total[_left[node]];
        char l = 0, r = _bit_len[node];
        while (r - l > 1) {
          char m = (l + r) >> 1;
          if (_popcount(_key[node]>>(_bit_len[node]-m)) > k) r = m;
          else l = m;
        }
        s += _size[_left[node]] + l;
        break;
      }
    }
    return s;
  }

  int select(int k, bool v) const {
    return v ? select1(k) : select0(k);
  }

  int _insert_and_rank1(int k, bool key) {
    if (_root == 0) {
      Node new_node = _make_node(key, 1);
      _root = new_node;
      return 0;
    }
    Node node = _root;
    int s = 0;
    stack<Node> path;
    int d = 0;
    while (node) {
      int t = _size[_left[node]] + _bit_len[node];
      if (t - _bit_len[node] <= k && k <= t) break;
      if (t <= k) {
        s += _total[_left[node]] + _popcount(_key[node]);
      }
      d <<= 1;
      _size[node]++;
      _total[node] += key;
      path.emplace(node);
      node = t > k ? _left[node] : _right[node];
      if (t > k) d |= 1;
      else k -= t;
    }
    k -= _size[_left[node]];
    s += _total[_left[node]] + _popcount(_key[node] >> (_bit_len[node] - k));
    if (_bit_len[node] < _W) {
      uint128 v = _key[node];
      char bl = _bit_len[node] - k;
      _key[node] = _bit_insert(v, bl, key);
      _bit_len[node]++;
      _size[node]++;
      _total[node] += key;
      return s;
    }
    path.emplace(node);
    _size[node]++;
    _total[node] += key;
    uint128 v = _key[node];
    char bl = _W - k;
    v = _bit_insert(v, bl, key);
    uint128 left_key = v >> _W;
    char left_key_popcount = left_key & 1;
    _key[node] = v & (((uint128)1 << _W) - 1);
    node = _left[node];
    d = d << 1 | 1;
    if (!node) {
      if (_bit_len[path.top()] < _W) {
        _bit_len[path.top()]++;
        _key[path.top()] = (_key[path.top()] << 1) | left_key;
        return s;
      } else {
        Node new_node = _make_node(left_key, 1);
        _left[path.top()] = new_node;
      }
    } else {
      path.emplace(node);
      _size[node]++;
      _total[node] += left_key_popcount;
      d <<= 1;
      while (_right[node]) {
        node = _right[node];
        path.emplace(node);
        _size[node]++;
        _total[node] += left_key_popcount;
        d <<= 1;
      }
      if (_bit_len[node] < _W) {
        _bit_len[node]++;
        _key[node] = (_key[node] << 1) | left_key;
        return s;
      } else {
        Node new_node = _make_node(left_key, 1);
        _right[node] = new_node;
      }
    }
    Node new_node = 0;
    while (!path.empty()) {
      node = path.top(); path.pop();
      _balance[node] += (d & 1) ? 1 : -1;
      d >>= 1;
      if (_balance[node] == 0) break;
      if (_balance[node] == 2) {
        new_node = _balance[_left[node]] == -1 ? _rotate_LR(node) : _rotate_L(node);
        break;
      } else if (_balance[node] == -2) {
        new_node = _balance[_right[node]] == 1 ? _rotate_RL(node) : _rotate_R(node);
        break;
      }
    }
    if (new_node) {
      if (!path.empty()) {
        ((d & 1) ? _left[path.top()] : _right[path.top()]) = new_node;
      } else {
        _root = new_node;
      }
    }
    return s;
  }

  int _access_pop_and_rank1(int k) {
    int s = 0, d = 0;
    Node node = _root;
    stack<Node> path;
    while (node) {
      int t = _size[_left[node]] + _bit_len[node];
      if (t - _bit_len[node] <= k && k < t) break;
      if (t <= k) {
        s += _total[_left[node]] + _popcount(_key[node]);
      }
      path.emplace(node);
      node = t > k ? _left[node] : _right[node];
      d <<= 1;
      if (t > k) d |= 1;
      else k -= t;
    }
    k -= _size[_left[node]];
    s += _total[_left[node]] + _popcount(_key[node] >> (_bit_len[node] - k));
    uint128 v = _key[node];
    bool res = v >> (_bit_len[node] - k - 1) & 1;
    if (_bit_len[node] == 1) {
      _pop_under(path, d, node, res);
      return (s << 1) | res;
    }
    _key[node] = _bit_pop(v, _bit_len[node]-k);
    --_bit_len[node];
    --_size[node];
    _total[node] -= res;
    while (!path.empty()) {
      node = path.top(); path.pop();
      --_size[node];
      _total[node] -= res;
    }
    return (s << 1) | res;
  }

  pair<bool, int> _access_ans_rank1(int k) const {
    Node node = _root;
    int s = 0;
    bool res;
    while (true) {
      int t = _size[_left[node]] + _bit_len[node];
      if (t - _bit_len[node] <= k && k < t) {
        k -= _size[_left[node]];
        s += _total[_left[node]] + _popcount(_key[node] >> (_bit_len[node] - k));
        res = (_key[node] >> (_bit_len[node] - k - 1)) & 1;
        break;
      }
      if (t > k) {
        node = _left[node];
      } else {
        s += _total[_left[node]] + _popcount(_key[node]);
        node = _right[node];
        k -= t;
      }
    }
    return make_pair(res, s);
  }

  void print() const {
    vector<uint8_t> a = tovector();
    int n = (int)a.size();
    cout << "[";
    for (int i = 0; i < n-1; ++i) {
      cout << a[i] << ", ";
    }
    if (n > 0) {
      cout << a.back();
    }
    cout << "]";
    cout << endl;
  }

  bool empty() const {
    return len() == 0;
  }

  int len() const {
    return _size[_root];
  }
};
} // namespace titan23

using namespace std;

// DynamicWaveletTree
namespace titan23 {

    /**
     * @brief 動的ウェーブレット木
     * 
     * @tparam T 値の型
     */
    template<typename T>
    class DynamicWaveletTree {
      private:
        struct Node;
        Node* root;
        T _sigma;
        int _log;
        int _size;

        struct Node {
            Node* left;
            Node* right;
            Node* par;
            AVLTreeBitVector v;
            Node() : left(nullptr), right(nullptr), par(nullptr) {}
            Node(const vector<uint8_t> &a) : left(nullptr), right(nullptr), par(nullptr) {
                v = AVLTreeBitVector(a);
            }
        };

        int bit_length(const int n) const {
            return 32 - __builtin_clz(n);
        }

        void _build(const vector<T> &a) {
            vector<int> buff0(a.size()), buff1;
            auto build = [&] (auto &&build,
                              int bit,
                              bool flag01,
                              int s0, int g0,
                              int s1, int g1
                              ) -> Node* {
                int s = flag01 ? s1 : s0;
                int g = flag01 ? g1 : g0;
                if (s == g || bit < 0) return nullptr;
                vector<int> &vec = flag01 ? buff1 : buff0;
                vector<uint8_t> v(g-s, 0);
                int start_0 = buff0.size(), start_1 = buff1.size();
                for (int i = s; i < g; ++i) {
                    if (a[vec[i]] >> bit & 1) {
                        v[i-s] = 1;
                        buff1.emplace_back(vec[i]);
                    } else {
                        buff0.emplace_back(vec[i]);
                    }
                }
                int end_0 = buff0.size(), end_1 = buff1.size();
                Node* node = new Node(v);
                node->left  = build(build, bit-1, 0, start_0, end_0, start_1, end_1);
                if (node->left) node->left->par = node;
                node->right = build(build, bit-1, 1, start_0, end_0, start_1, end_1);
                if (node->right) node->right->par = node;
                return node;
            };
            for (int i = 0; i < a.size(); ++i) {
                buff0[i] = i;
            }
            this->root = build(build, _log-1, 0, 0, a.size(), 0, 0);
            if (this->root == nullptr) {
                this->root = new Node();
            }
        }

      public:
        //! 各要素が `[0, sigma)` の `DynamicWaveletTree` を作成する / `O(1)`
        DynamicWaveletTree(const T sigma)
                : _sigma(sigma), _log(bit_length(sigma)), _size(0) {
            root = new Node();
        }

        //! 各要素が `[0, sigma)` の `DynamicWaveletTree` を作成する / `O(nlog(σ))`
        DynamicWaveletTree(const T sigma, vector<T> &a)
                : _sigma(sigma), _log(bit_length(sigma)), _size(a.size()) {
            _build(a);
        }

        //! 位置 `k` に `x` を挿入する / `O(log(n)log(σ))`
        void insert(int k, T x) {
            assert(0 <= k && k <= len());
            assert(0 <= x && x < sigma);
            Node* node = root;
            for (int bit = _log-1; bit >= 0; --bit) {
                if ((x >> bit) & 1) {
                    k = node->v._insert_and_rank1(k, 1);
                    if (!node->right) {
                        node->right = new Node();
                        node->right->par = node;
                    }
                    node = node->right;
                } else {
                    k -= node->v._insert_and_rank1(k, 0);
                    if (!node->left) {
                        node->left = new Node();
                        node->left->par = node;
                    }
                    node = node->left;
                }
            }
            _size++;
        }

        //! 位置 `k` の値を削除して返す / `O(log(n)log(σ))`
        T pop(int k) {
            Node* node = root;
            T ans = 0;
            for (int bit = _log-1; node && bit >= 0; --bit) {
                int sb = node->v._access_pop_and_rank1(k);
                if (sb & 1) {
                    ans |= (T)1 << bit;
                    k = sb >> 1;
                    node = node->right;
                } else {
                    k -= sb >> 1;
                    node = node->left;
                }
            }
            _size--;
            return ans;
        }

        //! 位置 `k` の値を `x` に更新する / `O(log(n)log(σ))`
        void set(int k, T x) {
            pop(k);
            insert(k, x);
        }

        //! 区間 `[0, r)` の `x` の個数を返す / `O(log(n)log(σ))`
        int rank(int r, T x) const {
            Node* node = root;
            int l = 0;
            for (int bit = _log-1; node && bit >= 0; --bit) {
                if ((x >> bit) & 1) {
                    l = node->v.rank1(l);
                    r = node->v.rank1(r);
                    node = node->right;
                } else {
                    l = node->v.rank0(l);
                    r = node->v.rank0(r);
                    node = node->left;
                }
            }
            return r - l;
        }

        //! 区間 `[l, r)` の `x` の個数を返す / `O(log(n)log(σ))`
        int range_count(int l, int r, T x) const {
            return rank(r, x) - rank(l, x);
        }

        //! `k` 番目の要素を返す / `O(log(n)log(σ))`
        T access(int k) const {
            assert(0 <= k && k < len());
            Node* node = root;
            T s = 0;
            for (int bit = _log-1; bit >= 0; --bit) {
                auto [b, r] = node->v._access_ans_rank1(k);
                if (b) {
                    s |= (T)1 << bit;
                    k = r;
                    node = node->right;
                } else {
                    k -= r;
                    node = node->left;
                }
            }
            return s;
        }

        //! 区間 `[l, r)` で昇順 `k` 番目の値を返す / `O(log(n)log(σ))`
        T kth_smallest(int l, int r, int k) const {
            Node* node = root;
            T s = 0;
            for (int bit = _log-1; node && bit >= 0; --bit) {
                int l0 = node->v.rank0(l);
                int r0 = node->v.rank0(r);
                int cnt = r0 - l0;
                if (cnt <= k) {
                    s |= (T)1 << bit;
                    k -= cnt;
                    l -= l0;
                    r -= r0;
                    node = node->right;
                } else {
                    l = l0;
                    r = r0;
                    node = node->left;
                }
            }
            return s;
        }

        //! 区間 `[l, r)` で降順 `k` 番目の値を返す / `O(log(n)log(σ))`
        T kth_largest(int l, int r, int k) const {
            return kth_smallest(l, r, r-l-k-1);
        }

        //! 区間 `[l, r)` で `x` 未満の要素の個数を返す / `O(log(n)log(σ))`
        int range_freq(int l, int r,  const T &x) const {
            Node* node = root;
            int ans = 0;
            for (int bit = _log-1; node && bit >= 0; --bit) {
                int l0 = node->v.rank0(l);
                int r0 = node->v.rank0(r);
                if ((x >> bit) & 1) {
                    ans += r0 - l0;
                    l -= l0;
                    r -= r0;
                    node = node->right;
                } else {
                    l = l0;
                    r = r0;
                    node = node->left;
                }
            }
            return ans;
        }

        //! 区間 `[l, r)` で `x` 以上 `y` 未満の要素の個数を返す / `O(log(n)log(σ))`
        int range_freq(int l, int r, int x, int y) const {
            return range_freq(l, r, y) - range_freq(l, r, x);
        }

        //! `k` 番目の `x` の位置を返す / `O(log(n)log(σ))`
        int select(int k, T x) const {
            Node* node = root;
            for (int bit = _log-1; bit > 0; --bit) {
                if ((x >> bit) & 1) {
                    node = node->right;
                } else {
                    node = node->left;
                }
            }
            for (int bit = 0; bit < _log; ++bit) {
                if ((x >> bit) & 1) {
                    k = node->v.select1(k);
                } else {
                    k = node->v.select0(k);
                }
                node = node->par;
            }
            return k;
        }

        //! `k` 番目の `x` の位置を返して削除する / `O(log(n)log(σ))`
        int select_remove(int k, T x) {
            Node* node = root;
            for (int bit = _log-1; bit > 0; --bit) {
                if ((x >> bit) & 1) {
                    node = node->right;
                } else {
                    node = node->left;
                }
            }
            for (int bit = 0; bit < _log; ++bit) {
                if ((x >> bit) & 1) {
                    k = node->v.select1(k);
                } else {
                    k = node->v.select0(k);
                }
                node->v.pop(k);
                node = node->par;
            }
            _size--;
            return k;
        }

        //! 要素数を返す / `O(1)`
        int len() const {
            return _size;
        }

        //! `vector` にして返す / `O(nlog(σ))`
        //! (n 回 access するよりも高速)
        vector<T> tovector() const {
            vector<T> a(len(), 0);
            vector<int> buff0(a.size()), buff1;
            auto dfs = [&] (auto &&dfs,
                            Node* node,
                            int bit,
                            bool flag01,
                            int s0, int g0,
                            int s1, int g1
                            ) -> void {
                int s = flag01 ? s1 : s0;
                int g = flag01 ? g1 : g0;
                if (s == g || bit < 0) return;
                vector<int> &vec = flag01 ? buff1 : buff0;
                const vector<uint8_t> &v = node->v.tovector();
                int start_0 = buff0.size(), start_1 = buff1.size();
                for (int i = s; i < g; ++i) {
                    if (v[i-s]) {
                        a[vec[i]] |= (T)1 << bit;
                        buff1.emplace_back(vec[i]);
                    } else {
                        buff0.emplace_back(vec[i]);
                    }
                }
                int end_0 = buff0.size(), end_1 = buff1.size();
                dfs(dfs, node->left,  bit-1, 0, start_0, end_0, start_1, end_1);
                dfs(dfs, node->right, bit-1, 1, start_0, end_0, start_1, end_1);
            };
            for (int i = 0; i < a.size(); ++i) {
                buff0[i] = i;
            }
            dfs(dfs, this->root, _log-1, 0, 0, a.size(), 0, 0);
            return a;
        }

        //! 表示する / `O(nlog(σ))`
        void print() const {
            vector<T> a = tovector();
            int n = (int)a.size();
            cout << "[";
            for (int i = 0; i < n-1; ++i) {
                cout << a[i] << ", ";
            }
            if (n > 0) {
                cout << a.back();
            }
            cout << "]";
            cout << endl;
        }
    };
} // namespace titan23

