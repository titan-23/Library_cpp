// #pragma GCC target("avx2")
// #pragma GCC optimize("O3")
// #pragma GCC optimize("unroll-loops")

#include <bits/stdc++.h>
using namespace std;

using ll = long long;
#define rep(i, n) for (ll i = 0; i < (ll)(n); ++i)

const ll dy[] = {-1, 0, 0, 1};
const ll dx[] = {0, -1, 1, 0};

template <class T, class T1, class T2> bool isrange(T target, T1 low, T2 high) { return low <= target && target < high; }
template <class T, class U> T min(const T &t, const U &u) { return t < u ? t : u; }
template <class T, class U> T max(const T &t, const U &u) { return t < u ? u : t; }
template <class T, class U> bool chmin(T &t, const U &u) { if (t > u) { t = u; return true; } return false; }
template <class T, class U> bool chmax(T &t, const U &u) { if (t < u) { t = u; return true; } return false; }

// #include "titan_cpplib/others/print.cpp"

#include <iostream>
#include <vector>
#include <set>
#include <tuple>
#include <unordered_set>
#include <algorithm>
#include <map>
#include <unordered_map>
using namespace std;

// print

// -------------------------
// pair<K, V>
template<typename K, typename V> ostream& operator<<(ostream& os, const pair<K, V>& p);
// tuple<T1, T2, T3>
template<typename T1, typename T2, typename T3> ostream &operator<<(ostream &os, const tuple<T1, T2, T3> &t);
// tuple<T1, T2, T3, T4>
template<typename T1, typename T2, typename T3, typename T4> ostream &operator<<(ostream &os, const tuple<T1, T2, T3, T4> &t);
// vector<T>
template<typename T> ostream& operator<<(ostream& os, const vector<T>& a);
// vector<vector<T>>
template<typename T> ostream& operator<<(ostream& os, const vector<vector<T>>& a);
// array<T, 2>
template<typename T> ostream& operator<<(ostream& os, const array<T, 2>& a);
// set<T>
template<typename T> ostream& operator<<(ostream& os, const set<T>& s);
// multiset<T>
template<typename T> ostream& operator<<(ostream& os, const multiset<T>& s);
// unordered_set<T>
template<typename T> ostream& operator<<(ostream& os, const unordered_set<T>& a);
// map<K, V>
template<typename K, typename V> ostream& operator<<(ostream& os, const map<K, V>& mp);
// unordered_map<K, V>
template<typename K, typename V> ostream& operator<<(ostream& os, const unordered_map<K, V>& mp);
// __int128_t
ostream& operator<<(ostream& os, __int128_t x);
// __uint128_t
ostream& operator<<(ostream& os, __uint128_t x);

// -------------------------

// color
static const string PRINT_RED = "\033[91m"; // 赤字
static const string PRINT_GREEN = "\033[92m"; // 緑字
static const string PRINT_BLUE = "\033[94m";  // 青字
static const string PRINT_NONE = "\033[m"; // 色を元に戻す
static const string PRINT_BOLD = "\u001b[1m"; // 太字

string to_red(const string &s)   { return PRINT_RED + s + PRINT_NONE; }
string to_green(const string &s) { return PRINT_GREEN + s + PRINT_NONE; }
string to_blue(const string &s)  { return PRINT_BLUE + s + PRINT_NONE; }
string to_bold(const string &s)  { return PRINT_BOLD + s + PRINT_NONE; }

string spacefill(const string s, const int f) {
    int n = s.size();
    string t;
    for (int i = 0; i < f-n; ++i) t += " ";
    t += s;
    return t;
}

string zfill(const string s, const int f) {
    int n = s.size();
    string t;
    for (int i = 0; i < f-n; ++i) t += "0";
    t += s;
    return t;
}

string bin(long long s) {
    string t;
    while (s) {
        t += (s & 1) ? '1' : '0';
        s >>= 1;
    }
    reverse(t.begin(), t.end());
    return t;
}

void DEBUG_LINE() { cerr << "[Line] : " << __LINE__ << std::endl; }

string to_string_int128(__int128_t x) {
    if (x == 0) return "0";
    bool neg = false;
    if (x < 0) { neg = true; x = -x; }
    string s;
    while (x > 0) {
        s += char('0' + (int)(x % 10));
        x /= 10;
    }
    if (neg) s += '-';
    reverse(s.begin(), s.end());
    return s;
}

string to_string_uint128(__uint128_t x) {
    if (x == 0) return "0";
    string s;
    while (x > 0) {
        s += char('0' + (int)(x % 10));
        x /= 10;
    }
    reverse(s.begin(), s.end());
    return s;
}

// __int128_t
ostream& operator<<(ostream& os, __int128_t x) {
    os << to_string_int128(x);
    return os;
}

// __uint128_t
ostream& operator<<(ostream& os, __uint128_t x) {
    os << to_string_uint128(x);
    return os;
}

// pair<K, V>
template<typename K, typename V>
ostream& operator<<(ostream& os, const pair<K, V>& p) {
    os << "(" << p.first << ", " << p.second << ")";
    return os;
}

// tuple<T1, T2, T3>
template<typename T1, typename T2, typename T3>
ostream &operator<<(ostream &os, const tuple<T1, T2, T3> &t) {
    os << "(" << get<0>(t) << ", " << get<1>(t) << ", " << get<2>(t) << ")";
    return os;
}

// tuple<T1, T2, T3, T4>
template<typename T1, typename T2, typename T3, typename T4>
ostream &operator<<(ostream &os, const tuple<T1, T2, T3, T4> &t) {
    os << "(" << get<0>(t) << ", " << get<1>(t) << ", " << get<2>(t) << ", " << get<3>(t) << ")";
    return os;
}

// array<T, 2>
template<typename T>
ostream& operator<<(ostream& os, const array<T, 2>& a) {
    os << "[";
    for (int i = 0; i < (int)a.size(); ++i) {
        if (i > 0) os << ", ";
        os << a[i];
    }
    os << "]";
    return os;
}

// vector<T>
template<typename T>
ostream& operator<<(ostream& os, const vector<T>& a) {
    os << "[";
    for (int i = 0; i < (int)a.size(); ++i) {
        if (i > 0) os << ", ";
        os << a[i];
    }
    os << "]";
    return os;
}

// vector<vector<T>>
template<typename T>
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
template<typename T>
ostream& operator<<(ostream& os, const set<T>& s) {
    os << "{";
    auto it = s.begin();
    while (it != s.end()) {
        os << *it;
        ++it;
        if (it != s.end()) {
            os << ", ";
        }
    }
    os << "}";
    return os;
}

// multiset<T>
template<typename T>
ostream& operator<<(ostream& os, const multiset<T>& s) {
    os << "{";
    auto it = s.begin();
    while (it != s.end()) {
        os << *it;
        ++it;
        if (it != s.end()) {
            os << ", ";
        }
    }
    os << "}";
    return os;
}

// unordered_set<T>
template<typename T>
ostream& operator<<(ostream& os, const unordered_set<T>& a) {
    set<T> s(a.begin(), a.end());
    os << s;
    return os;
}

// map<K, V>
template<typename K, typename V>
ostream& operator<<(ostream& os, const map<K, V>& mp) {
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

// unordered_map<K, V>
template<typename K, typename V>
ostream& operator<<(ostream& os, const unordered_map<K, V>& mp) {
    map<K, V> m(mp.begin(), mp.end());
    os << m;
    return os;
}


namespace persistent_lazy_tree {

using U = ll;
using T = char;

struct Node {
  Node const *l, *r;
  U size; T v; int dep;
};
using NP = Node const*;
const int CHUNK_SIZE = 1 << 20;
struct Pool {
  static vector<Node*> blocks;
  static Node* ptr;
  static Node* lim;
  static int idx;
  static void expand() {
    if (idx == (int)blocks.size()) {
      Node* new_block = new Node[CHUNK_SIZE];
      blocks.push_back(new_block);
    }
    ptr = blocks[idx];
    lim = ptr + CHUNK_SIZE;
    idx++;
  }
  static NP newnode(NP l, NP r, T v, U size, int dep) {
    if (ptr == lim) expand();
    *ptr = Node{l, r, size, v, dep};
    return ptr++;
  }
  static void reset() {
    idx = 0;
    ptr = nullptr;
    lim = nullptr;
  }
};
Node* Pool::ptr = nullptr;
Node* Pool::lim = nullptr;
int Pool::idx = 0;
vector<Node*> Pool::blocks;
U size(NP x) { return x ? x->size : 0; }
int dep(NP x) { return x ? x->dep : 0; }
T v(NP x) { return x ? x->v : 'a'; }
NP newpar(NP l, NP r) {
  return Pool::newnode(l, r, 'a', size(l) + size(r), max(dep(l), dep(r)) + 1);
}
NP add(NP x) {
  if (!x) return nullptr;
  return Pool::newnode(x->l, x->r, x->v, x->size, x->dep);
}
void push_rev(NP t, NP &l, NP &r) {
  l = t->l; r = t->r;
}
NP merge(NP a, NP b);
pair<NP, NP> merge2(NP a, NP b);
pair<NP, NP> merge1(NP a, NP b, NP c) {
  if (abs(max(dep(a), dep(b)) + 1 - dep(c)) <= 1) return {newpar(a, b), c};
  if (abs(max(dep(b), dep(c)) + 1 - dep(a)) <= 1) return {a, newpar(b, c)};
  NP bl, br; push_rev(b, bl, br);
  return {newpar(a, add(bl)), newpar(add(br), c)};
}
pair<NP, NP> merge2(NP a, NP b) {
  if (dep(a) < dep(b)) {
    NP bl, br; push_rev(b, bl, br);
    pair<NP, NP> x = merge2(a, bl);
    return merge1(x.first, x.second, add(br));
  } else if (dep(a) > dep(b)) {
    NP al, ar; push_rev(a, al, ar);
    pair<NP, NP> x = merge2(ar, b);
    return merge1(add(al), x.first, x.second);
  } else { return {add(a), add(b)}; }
}
NP merge(NP a, NP b) {
  if (!a) return b;
  if (!b) return a;
  pair<NP, NP> x = merge2(a, b);
  return newpar(x.first, x.second);
}
pair<NP, NP> split(NP x, U s) {
  if (size(x) == s) return { add(x), nullptr };
  if (s == 0) return { nullptr, add(x) };
  NP xl, xr; push_rev(x, xl, xr);
  if (s <= size(xl)) {
    auto [l, r] = split(xl, s);
    return {l, merge(r, add(xr))};
  } else {
    auto [l, r] = split(xr, s-size(xl));
    return {merge(add(xl), l), r};
  }
}
NP split_left(NP x, U s) {
  if (size(x) == s) return add(x);
  if (s == 0) return nullptr;
  NP xl, xr; push_rev(x, xl, xr);
  if (s <= size(xl)) {
    auto l = split_left(xl, s);
    return l;
  } else {
    auto l = split_left(xr, s-size(xl));
    return merge(add(xl), l);
  }
}
NP split_right(NP x, U s) {
  if (size(x) == s) return nullptr;
  if (s == 0) return add(x);
  NP xl, xr; push_rev(x, xl, xr);
  if (s <= size(xl)) {
    auto r = split_right(xl, s);
    return merge(r, add(xr));
  } else {
    auto r = split_right(xr, s-size(xl));
    return r;
  }
}
vector<T> tovector(NP root) {
  vector<T> res;
  if (!root) return res;
  vector<NP> st;
  st.reserve(root->dep + 1);
  st.emplace_back(root);
  while (!st.empty()) {
    auto t = st.back(); st.pop_back();
    if (t->size == 1) {
      res.push_back(t->v);
    } else {
      if (t->r) st.emplace_back(t->r);
      if (t->l) st.emplace_back(t->l);
    }
  }
  return res;
}
struct Tree {
  NP root;
  Tree() : root(nullptr) {}
  Tree(NP r) : root(r) {}
  Tree(vector<T> &v) {
    if (v.empty()) { root = nullptr; return; }
    auto dfs = [&](auto &&dfs, int l, int r) -> NP {
      if (l >= r) return nullptr;
      if (r - l == 1) return Pool::newnode(nullptr, nullptr, v[l], 1, 0);
      int m = (l + r) >> 1;
      NP L = dfs(dfs, l, m);
      NP R = dfs(dfs, m, r);
      return newpar(L, R);
    };
    root = dfs(dfs, 0, (int)v.size());
  }
  U size() { return persistent_lazy_tree::size(root); }
//   T prod(U l, U r) {
//     if (l >= r) return 'a';
//     auto abc = split_left(root, r);
//     auto b = split_right(abc, l);
//     return v(b);
//   }
//   T get(U k) { return prod(k, k + 1); }
  T get(U k) {
    NP now = root;
    while (1) {
        if (now->size == 1) return now->v;
        U ls = now->l ? now->l->size : 0;
        if (k < ls) {
            now = now->l;
        } else {
            k -= ls;
            now = now->r;
        }
    }
  }
  vector<T> to_vector() { return tovector(root); }
  void rebuild() {
    if (size() == 0) {
      Pool::reset();
      root = nullptr;
      return;
    }
    vector<T> data = to_vector();
    Pool::reset();
    *this = Tree(data);
  }
};
void reserve(int n) {
  while (Pool::blocks.size()*CHUNK_SIZE < n) Pool::expand();
}
// pair<Tree, Tree> split(const Tree& t, U k) {
//   auto [l, r] = split(t.root, k);
//   return { Tree(l), Tree(r) };
// }
Tree split_left(const Tree& t, U k) {
  auto l = split_left(t.root, k);
  return Tree(l);
}
Tree merge(const Tree& a, const Tree& b) { return Tree(merge(a.root, b.root)); }
}

void solve() {
    int q; cin >> q;
    using namespace persistent_lazy_tree;
    // reserve(1e7);
    vector<Tree> T(q+2);
    vector<char> s0 = {'0'};
    vector<char> s1 = {'1'};
    T[0] = Tree(s0);
    T[1] = Tree(s1);
    rep(i, q) {
        if (i%(10000) == 0) cerr << i << endl;
        ll l, r, x; cin >> l >> r >> x;
        --x;
        T[i+2] = merge(T[l], T[r]);
        if (T[i+2].size() >= 1e18) {
            T[i+2] = split_left(T[i+2], (ll)1e18);
        }
        cout << T[i+2].get(x) << "\n";
    }
}

int main() {
    ios::sync_with_stdio(false);
    cin.tie(0);
    cout << fixed << setprecision(15);
    cerr << fixed << setprecision(15);

    int t = 1;
    // cin >> t;
    for (int i = 0; i < t; ++i) {
        solve();
    }

    return 0;
}

// 1588ms
// AC
// 730368 KiB
