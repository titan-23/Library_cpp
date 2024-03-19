// done.

#pragma GCC target("avx2")
#pragma GCC optimize("O3")
#pragma GCC optimize("unroll-loops")

#include <bits/stdc++.h>
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
  vector<char> _bit_len;
  vector<char> _balance;

  void _build(vector<uint8_t> &a) {
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
    return __builtin_popcountll(n);
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

  AVLTreeBitVector (vector<uint8_t> &a)
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

// DynamicBitVector
namespace titan23 {

  class DynamicBitVector {
   private:
    static const int BUCKET_MAX = 1000;
    vector<vector<uint8_t>> data;
    vector<int> bucket_data;
    int _size;
    int tot_one;

    void build(const vector<uint8_t> &a) {
      long long s = len();
      int bucket_size = max((int)(s+BUCKET_MAX-1)/BUCKET_MAX, (int)ceil(sqrt(s)));
      data.resize(bucket_size);
      bucket_data.resize(bucket_size);
      for (int i = 0; i < bucket_size; ++i) {
        int start = s*i/bucket_size;
        int stop = min((int)len(), (int)(s*(i+1)/bucket_size));
        vector<uint8_t> d(a.begin()+start, a.begin()+stop);
        int sum = 0;
        for (const uint8_t &x: d) sum += x;
        data[i] = d;
        tot_one += sum;
        bucket_data[i] = sum;
      }
    }

    pair<int, int> get_bucket(int k) const {
      if (k == len()) return {-1, -1};
      if (k < len()/2) {
        for (int i = 0; i < data.size(); ++i) {
          if (k < data[i].size()) return {i, k};
          k -= data[i].size();
        }
      } else {
        int tot = len();
        for (int i = data.size()-1; i >= 0; --i) {
          if (tot-data[i].size() <= k) {
            return {i, k-(tot-data[i].size())};
          }
          tot -= data[i].size();
        }
      }
      assert(false);
    }

   public:
    DynamicBitVector() : _size(0), tot_one(0) {}
    DynamicBitVector(const vector<uint8_t> &a) : _size(a.size()), tot_one(0) {
      build(a);
    }

    void insert(int k, bool key) {
      assert(0 <= k && k <= len());
      if (data.empty()) {
        ++_size;
        tot_one += key;
        bucket_data.emplace_back(key);
        data.push_back({key});
        return;
      }
      auto [bucket_pos, bit_pos] = get_bucket(k);
      if (bucket_pos == -1) {
        bucket_pos = data.size()-1;
        bucket_data.back() += key;
        data.back().emplace_back(key);
      } else {
        bucket_data[bucket_pos] += key;
        data[bucket_pos].insert(data[bucket_pos].begin() + bit_pos, key);
      }
      if (data[bucket_pos].size() > BUCKET_MAX) {
        vector<uint8_t> right(data[bucket_pos].begin() + BUCKET_MAX/2, data[bucket_pos].end());
        data[bucket_pos].erase(data[bucket_pos].begin() + BUCKET_MAX/2, data[bucket_pos].end());
        data.emplace(data.begin() + bucket_pos+1, right);
        bucket_data.insert(bucket_data.begin() + bucket_pos, 0);
        bucket_data[bucket_pos] = 0;
        bucket_data[bucket_pos+1] = 0;
        for (const uint8_t x: data[bucket_pos]) bucket_data[bucket_pos] += x;
        for (const uint8_t x: data[bucket_pos+1]) bucket_data[bucket_pos+1] += x;
      }
      ++_size;
      tot_one += key;
    }

    bool access(int k) const {
      assert(0 <= k && k < len());
      auto [bucket_pos, bit_pos] = get_bucket(k);
      return data[bucket_pos][bit_pos];
    }

    bool pop(int k) {
      assert(0 <= k && k < len());
      auto [bucket_pos, bit_pos] = get_bucket(k);
      bool res = data[bucket_pos][bit_pos];
      bucket_data[bucket_pos] -= res;
      data[bucket_pos].erase(data[bucket_pos].begin() + bit_pos);
      tot_one -= res;
      --_size;
      if (data[bucket_pos].empty()) {
        data.erase(data.begin() + bucket_pos);
        bucket_data.erase(bucket_data.begin() + bucket_pos);
      }
      return res;
    }

    void set(int k, bool v) {
      assert(0 <= k && k < len());
      auto [bucket_pos, bit_pos] = get_bucket(k);
      data[bucket_pos][bit_pos] = v;
    }

    int rank0(int r) const {
      assert(0 <= r && r <= len());
      return r - rank1(r);
    }

    int rank1(int r) const {
      assert(0 <= r && r <= len());
      int s = 0;
      for (int i = 0; i < data.size(); ++i) {
        if (r < data[i].size()) {
          const vector<uint8_t> &d = data[i];
          for (int j = 0; j < r; ++j) {
            if (d[j]) ++s;
          }
          return s;
        }
        s += bucket_data[i];
        r -= data[i].size();
      }
      return s;
      assert(false);
    }

    int rank(int r, bool key) const {
      assert(0 <= r && r <= len());
      return key ? rank1(r) : rank0(r);
    }

    int select0(int k) const {
      int s = 0;
      for (int i = 0; i < data.size(); ++i) {
        if (k < data[i].size() - bucket_data[i]) {
          for (const uint8_t &x: data[i]) {
            if (!x) --k;
            if (k < 0) return s;
            s++;
          }
          assert(false);
        }
        s += data[i].size();
        k -= data[i].size() - bucket_data[i];
      }
      assert(false);
    }

    int select1(int k) const {
      int s = 0;
      for (int i = 0; i < data.size(); ++i) {
        if (k < bucket_data[i]) {
          for (const uint8_t &x: data[i]) {
            if (x) --k;
            if (k < 0) return s;
            s++;
          }
        }
        s += data[i].size();
        k -= bucket_data[i];
      }
      assert(false);
    }

    int select(int k, bool key) const {
      return key ? select1(k) : select0(k);
    }

    int _insert_and_rank1(int k, bool key) {
      int s = 0;
      int bucket_pos = -1, bit_pos = -1;
      if (k < len()/2) {
        for (int i = 0; i < data.size(); ++i) {
          if (k < data[i].size()) {
            bucket_pos = i;
            bit_pos = k;
            const vector<uint8_t> &d = data[i];
            for (int j = 0; j < k; ++j) {
              s += d[j];
            }
            break;
          }
          s += bucket_data[i];
          k -= data[i].size();
        }
      } else {
        int tot = len();
        s = tot_one;
        for (int i = data.size()-1; i >= 0; --i) {
          if (tot-data[i].size() <= k) {
            bucket_pos = i;
            bit_pos = k-(tot-data[i].size());
            const vector<uint8_t> &d = data[i];
            for (int j = bit_pos; j < d.size(); ++j) {
              s -= d[j];
            }
            break;
          }
          tot -= data[i].size();
          s -= bucket_data[i];
        }
      }

      {
        ++_size;
        tot_one += key;
        if (data.empty()) {
          bucket_data.emplace_back(key);
          data.push_back({{key}});
          return s;
        }
        if (bucket_pos == -1) {
          bucket_pos = data.size()-1;
          bucket_data.back() += key;
          data.back().emplace_back(key);
        } else {
          bucket_data[bucket_pos] += key;
          data[bucket_pos].insert(data[bucket_pos].begin() + bit_pos, key);
        }
        if (data[bucket_pos].size() > BUCKET_MAX) {
          vector<uint8_t> right(data[bucket_pos].begin() + BUCKET_MAX/2, data[bucket_pos].end());
          data[bucket_pos].erase(data[bucket_pos].begin() + BUCKET_MAX/2, data[bucket_pos].end());
          data.emplace(data.begin() + bucket_pos+1, right);
          bucket_data.insert(bucket_data.begin() + bucket_pos, 0);
          bucket_data[bucket_pos] = 0;
          bucket_data[bucket_pos+1] = 0;
          for (const uint8_t &x: data[bucket_pos]) bucket_data[bucket_pos] += x;
          for (const uint8_t &x: data[bucket_pos+1]) bucket_data[bucket_pos+1] += x;
        }
      }
      return s;
    }

    int _access_pop_and_rank1(int k) {
      int prek = k;
      int s = 0;
      int bucket_pos, bit_pos;
      bool res;
      for (int i = 0; i < data.size(); ++i) {
        if (k < data[i].size()) {
          bucket_pos = i;
          bit_pos = k;
          res = data[bucket_pos][bit_pos];
          const vector<uint8_t> &d = data[i];
          for (int j = 0; j < k; ++j) {
            if (d[j]) ++s;
          }
          break;
        }
        s += bucket_data[i];
        k -= data[i].size();
      }
      bucket_data[bucket_pos] -= res;
      data[bucket_pos].erase(data[bucket_pos].begin() + bit_pos);
      tot_one -= res;
      --_size;
      if (data[bucket_pos].empty()) {
        data.erase(data.begin() + bucket_pos);
        bucket_data.erase(bucket_data.begin() + bucket_pos);
      }
      return s << 1 | res;
    }

    pair<bool, int> _access_ans_rank1(int k) const {
      assert(0 <= k && k < len());
      int s = 0;
      for (int i = 0; i < data.size(); ++i) {
        if (k < data[i].size()) {
          const vector<uint8_t> &d = data[i];
          for (int j = 0; j < k; ++j) {
            s += d[j];
          }
          return {data[i][k], s};
        }
        s += bucket_data[i];
        k -= data[i].size();
      }
      assert(false);
    }

    vector<uint8_t> tovector() const {
      vector<uint8_t> a(len());
      int ptr = 0;
      for (const vector<uint8_t> &d: data) for (const uint8_t &x: d) {
        a[ptr++] = x;
      }
      return a;
    }

    void print() const {
      vector<uint8_t> a = tovector();
      int n = (int)a.size();
      assert(n == len());
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

    bool empty() const { return _size == 0; }
    int len() const { return _size; }
  };
} // name space titan23

// DynamicWaveletTree
namespace titan23 {

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
      Node(vector<uint8_t> &a) : left(nullptr), right(nullptr), par(nullptr) {
        v = AVLTreeBitVector(a);
      }
    };

    int bit_length(const int n) const {
      return 32 - __builtin_clz(n);
    }

    void _build(vector<T> &a) {
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
    DynamicWaveletTree(const T sigma)
        : _sigma(sigma), _log(bit_length(sigma)), _size(0) {
      root = new Node();
    }

    DynamicWaveletTree(const T sigma, vector<T> &a)
        : _sigma(sigma), _log(bit_length(sigma)), _size(a.size()) {
      _build(a);
    }

    void insert(int k, T x) {
      assert(0 <= k && k <= len());
      Node* node = root;
      for (int bit = _log-1; node && bit >= 0; --bit) {
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

    void update(int k, T x) {
      pop(k);
      insert(k, x);
    }

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

    int range_count(int l, int r, T x) const {
      return rank(r, x) - rank(l, x);
    }

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

    T kth_largest(int l, int r, int k) const {
      return kth_smallest(l, r, r-l-k-1);
    }

    int range_freq(int l, int r, T x) const {
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

    int range_freq(int l, int r, int x, int y) const {
      return range_freq(l, r, y) - range_freq(l, r, x);
    }

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

    int len() const {
      return _size;
    }

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
        vector<uint8_t> v = node->v.tovector();
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

// Zaatsu
namespace titan23 {

  template<typename T>
  struct Zaatsu {
   private:
    vector<T> _to_origin;
    int _n;

   public:
    Zaatsu(vector<T> &used_items) {
      _to_origin = used_items;
      sort(_to_origin.begin(), _to_origin.end());
      _to_origin.erase(unique(_to_origin.begin(), _to_origin.end()), _to_origin.end());
      _n = (int)_to_origin.size();
    }

    int len() const {
      return _n;
    }

    int to_zaatsu(const T &x) const {
      return lower_bound(_to_origin.begin(), _to_origin.end(), x) - _to_origin.begin();
    }

    T to_origin(const int &x) const {
      return _to_origin[x];
    }
  };
}  // namespace titan23

void second_largest_query() {
  int n, q;
  cin >> n >> q;
  vector<int> A(n);
  for (int i = 0; i < n; ++i) cin >> A[i];

  vector<int> used_items = A;
  vector<tuple<int, int, int>> querys(q);
  for (int i = 0; i < q; ++i) {
    int com, a, b;
    cin >> com >> a >> b;
    querys[i] = make_tuple(com, a, b);
    if (com == 1) used_items.emplace_back(b);
  }

  titan23::Zaatsu<int> z(used_items);
  vector<int> B(n);
  for (int i = 0; i < n; ++i) {
    B[i] = z.to_zaatsu(A[i]);
  }

  titan23::DynamicWaveletTree<int> dwm(z.len(), B);
  // dwm.print();

  for (auto &[com, a, b]: querys) {
    if (com == 1) {
      int p = a-1, x = z.to_zaatsu(b);
      dwm.update(p, x);
    } else {
      int l = a-1, r = b;
      int wm_ans = 0;
      int m1_val = dwm.kth_largest(l, r, 0);
      int m1_cnt = dwm.range_count(l, r, m1_val);
      if (m1_cnt < r-l) {
        int m2_val = dwm.kth_largest(l, r, m1_cnt);
        int m2_cnt = dwm.range_count(l, r, m2_val);
        wm_ans = m2_cnt;
      }
      cout << wm_ans << '\n';
    }
  }
}

int main () {
  ios::sync_with_stdio(false);
  cin.tie(0);

  second_largest_query();

  return 0;
}
