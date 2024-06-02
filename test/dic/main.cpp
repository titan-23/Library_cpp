#include <vector>
#include <random>
#include <iostream>

using namespace std;

namespace titan23 {
  template<typename V>
  class HashDict {
   private:
    using u64 = unsigned long long;
    static constexpr const u64 k = 0x517cc1b727220a95;
    vector<u64> exist;
    vector<u64> keys;
    vector<V> vals;
    int msk, xor_;
    int size;

    int hash(const u64 &key) const {
      return (((((key>>32)&msk) ^ (key&msk) ^ xor_)) * (k & msk)) & msk;
    }

    int get_pos(const u64 &key) const {
      int h = hash(key), s = keys.size();
      while (true) {
        if ((!(exist[h>>6]>>(h&63)&1)) || keys[h] == key) return h;
        ++h;
        if (h == s) h = 0;
      }
    }

    int bit_length(const int x) const {
      if (x == 0) return 0;
      return 32 - __builtin_clz(x);
    }

    void rebuild() {
      vector<u64> old_exist = exist;
      vector<u64> old_keys = keys;
      vector<V> old_vals = vals;
      exist.resize(2*old_exist.size());
      fill(exist.begin(), exist.end(), 0);
      keys.resize(2*old_keys.size());
      vals.resize(2*old_vals.size());
      size = 0;
      msk = (int)keys.size() - 1;
      random_device rd;
      mt19937 gen(rd());
      uniform_int_distribution<int> dis(0, msk);
      xor_ = dis(gen);
      for (int i = 0; i < old_keys.size(); ++i) {
        if (old_exist[i>>6]>>(i&63)&1) {
          set(old_keys[i], old_vals[i]);
        }
      }
    }

   public:
    HashDict() : exist(1, 0), keys(1), vals(1), msk(0), xor_(0), size(0) {}

    HashDict(const int n) {
      const int s = 1<<bit_length(n);
      exist.resize((s>>6)+1, 0);
      keys.resize(s);
      vals.resize(s);
      msk = (1<<bit_length(keys.size()-1))-1;
      random_device rd;
      mt19937 gen(rd());
      uniform_int_distribution<int> dis(0, msk);
      xor_ = dis(gen);
    }

    V get(const u64 &key) const {
      int pos = get_pos(key);
      return (exist[pos>>6]>>(pos&63)&1) ? vals[pos] : V();
    }

    V get(const u64 &key, const V missing) const {
      int pos = get_pos(key);
      return (exist[pos>>6]>>(pos&63)&1) ? vals[pos] : missing;
    }

    bool contains(const u64 key) const {
      return get_pos(key).second;
    }

    V operator[] (const u64 &key) const {
      return get(key);
    }

    void set(const u64 &key, const V val) {
      int pos = get_pos(key);
      vals[pos] = val;
      if (!(exist[pos>>6]>>(pos&63)&1)) {
        exist[pos>>6] |= 1ull<<(pos&63);
        keys[pos] = key;
        ++size;
        if (2*size > keys.size()) rebuild();
      }
    }
  };
}

#pragma GCC target("avx2")
#pragma GCC optimize("O3")
#pragma GCC optimize("unroll-loops")

#include <iomanip>
#include <unordered_map>
#include <cassert>

#define rep(i, n) for (int i = 0; i < (n); ++i)

int main() {
  ios::sync_with_stdio(false);
  cin.tie(0);
  int q;
  cin >> q;

  titan23::HashDict<unsigned long long> d;

  // unordered_map<unsigned long long, unsigned long long> ud;
  // ud.reserve(q);

  rep(_, q) {
    int com; cin >> com;
    if (com == 0) {
      unsigned long long k, v;
      cin >> k >> v;
      d.set(k, v);
    } else {
      unsigned long long k;
      cin >> k;
      cout << d.get(k) << '\n';
    }
  }
  return 0;
}
