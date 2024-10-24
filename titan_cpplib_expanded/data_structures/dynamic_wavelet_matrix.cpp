// #include "titan_cpplib/data_structures/dynamic_wavelet_matrix.cpp"
#include <iostream>
#include <vector>
#include <tuple>
#include <queue>
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

// DynamicWaveletMatrix
namespace titan23 {

    /**
     * @brief 動的ウェーブレット行列
     *
     * @note `DynamicWavletTree` の方が平均的には高速かもしれません
     *
     * @tparam T
     */
    template<typename T>
    class DynamicWaveletMatrix {
      private:
        T _sigma;
        int _log;
        vector<AVLTreeBitVector> _v;
        vector<int> _mid;
        int _size;

        int bit_length(const int n) const {
            return 32 - __builtin_clz(n);
        }

        int bit_length(const long long n) const {
            return 64 - __builtin_clz(n);
        }

        int bit_length(const unsigned long long n) const {
            return 64 - __builtin_clz(n);
        }

        void _build(vector<T> a) {
            if (a.empty()) return;
            vector<uint8_t> v(_size);
            for (int bit = _log-1; bit >= 0; --bit) {
                vector<T> zero, one;
                for (int i = 0; i < _size; ++i) {
                    if ((a[i] >> bit) & 1) {
                        v[i] = 1;
                        one.emplace_back(a[i]);
                    } else {
                        v[i] = 0;
                        zero.emplace_back(a[i]);
                    }
                }
                _mid[bit] = zero.size();
                _v[bit] = AVLTreeBitVector(v);
                a = zero;
                a.insert(a.end(), one.begin(), one.end());
            }
        }

      public:
        DynamicWaveletMatrix(const T sigma)
                : _sigma(sigma), _log(bit_length(sigma-1)), _v(_log), _mid(_log), _size(0) {
        }

        DynamicWaveletMatrix(const T sigma, vector<T> &a)
                : _sigma(sigma), _log(bit_length(sigma)), _v(_log), _mid(_log), _size(a.size()) {
            _build(a);
        }

        void reserve(const int n) {
            for (int i = 0; i < _log; ++i) {
                _v[i].reserve(n);
            }
        }

        void insert(int k, T x) {
            for (int bit = _log-1; bit >= 0; --bit) {
                if ((x >> bit) & 1) {
                    int s = _v[bit]._insert_and_rank1(k, 1);
                    k = s + _mid[bit];
                } else {
                    int s = _v[bit]._insert_and_rank1(k, 0);
                    k -= s;
                    ++_mid[bit];
                }
            }
            _size++;
        }

        T pop(int k) {
            T ans = 0;
            for (int bit = _log-1; bit >= 0; --bit) {
                int sb = _v[bit]._access_pop_and_rank1(k);
                int s = sb >> 1;
                if (sb & 1) {
                    ans |= (T)1 << bit;
                    k = s + _mid[bit];
                } else {
                    --_mid[bit];
                    k -= s;
                }
            }
            _size--;
            return ans;
        }

        void set(int k, T x) {
            assert(0 <= k && k < _size);
            pop(k);
            insert(k, x);
        }

        int rank(int r, T x) const {
            int l = 0;
            for (int bit = _log-1; bit >= 0; --bit) {
                if ((x >> bit) & 1) {
                    l = _v[bit].rank1(l) + _mid[bit];
                    r = _v[bit].rank1(r) + _mid[bit];
                } else {
                    l = _v[bit].rank0(l);
                    r = _v[bit].rank0(r);
                }
            }
            return r - l;
        }

        T access(int k) const {
            T s = 0;
            for (int bit = _log-1; bit >= 0; --bit) {
                if (_v[bit].access(k)) {
                    s |= (T)1 << bit;
                    k = _v[bit].rank1(k) + _mid[bit];
                } else {
                    k = _v[bit].rank0(k);
                }
            }
            return s;
        }

        T kth_smallest(int l, int r, int k) const {
            T s = 0;
            for (int bit = _log-1; bit >= 0; --bit) {
                int l0 = _v[bit].rank0(l);
                int r0 = _v[bit].rank0(r);
                int cnt = r0 - l0;
                if (cnt <= k) {
                    s |= (T)1 << bit;
                    k -= cnt;
                    l = l - l0 + _mid[bit];
                    r = r - r0 + _mid[bit];
                } else {
                    l = l0;
                    r = r0;
                }
            }
            return s;
        }

        T kth_largest(int l, int r, int k) const {
            return kth_smallest(l, r, r-l-k-1);
        }

        vector<pair<T, int>> topk(int l, int r, int k) {
            priority_queue<tuple<int, T, int, char>> hq;
            hq.emplace(r-l, 0, l, _log-1);
            vector<pair<T, int>> ans;
            while (!hq.empty()) {
                auto [length, x, l, bit] = hq.top();
                hq.pop();
                if (bit == -1) {
                    ans.emplace_back(x, length);
                    --k;
                    if (k == 0) break;
                } else {
                    int r = l + length;
                    int l0 = _v[bit].rank0(l);
                    int r0 = _v[bit].rank0(r);
                    if (l0 < r0) hq.emplace(r0-l0, x, l0, bit-1);
                    int l1 = _v[bit].rank1(l) + _mid[bit];
                    int r1 = _v[bit].rank1(r) + _mid[bit];
                    if (l1 < r1) hq.emplace(r1-l1, x|((T)1<<(T)bit), l1, bit-1);
                }
            }
            return ans;
        }

        int select(int k, T x) const {
            T s = 0;
            for (int bit = _log-1; bit >= 0; --bit) {
                if ((x >> bit) & 1) {
                    s = _v[bit].rank0(_size) + _v[bit].rank1(s);
                } else {
                    s = _v[bit].rank0(s);
                }
            }
            s += k;
            for (int bit = 0; bit < _log; ++bit) {
                if ((x >> bit) & 1) {
                    s = _v[bit].select1(s - _v[bit].rank0(_size));
                } else {
                    s = _v[bit].select0(s);
                }
            }
            return s;
        }

        int range_freq(int l, int r, T x) const {
            int ans = 0;
            for (int bit = _log-1; bit >= 0; --bit) {
                int l0 = _v[bit].rank0(l);
                int r0 = _v[bit].rank0(r);
                if ((x >> bit) & 1) {
                    ans += r0 - l0;
                    l += _mid[bit] - l0;
                    r += _mid[bit] - r0;
                } else {
                    l = l0;
                    r = r0;
                }
            }
            return ans;
        }

        int range_freq(int l, int r, int x, int y) const {
            return range_freq(l, r, y) - range_freq(l, r, x);
        }

        T prev_value(int l, int r, T x) const {
            return kth_smallest(l, r, _range_freq(l, r, x)-1);
        }

        T next_value(int l, int r, T x) const {
            return kth_smallest(l, r, _range_freq(l, r, x));
        }

        int range_count(int l, int r, T x) const {
            return rank(r, x) - rank(l, x);
        }

        vector<T> tovector() const {

            vector<T> a(_size);
            for (int i = 0; i < _size; ++i) {
                a[i] = access(i);
            }
            return a;
        }

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

