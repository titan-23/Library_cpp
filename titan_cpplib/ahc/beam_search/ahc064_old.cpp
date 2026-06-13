#pragma GCC target("avx2")
#pragma GCC optimize("O3")
#pragma GCC optimize("unroll-loops")
#include <bits/stdc++.h>
using namespace std;
using ll = long long;
#define rep(i, n) for (int i = 0; i < (int)(n); ++i)
template <class T, class U> T min(const T &t, const U &u) { return t < u ? t : u; }
template <class T, class U> T max(const T &t, const U &u) { return t < u ? u : t; }

// #include "titan_cpplib/ahc/timer.cpp"

#include <iostream>
#include <chrono>

using namespace std;

// Timer
namespace titan23 {

/**
 * @brief 時間計測クラス
 */
class Timer {
private:
    chrono::time_point<chrono::high_resolution_clock> start_timepoint;

public:
    Timer() : start_timepoint(chrono::high_resolution_clock::now()) {}

    //! リセットする
    void reset() {
        start_timepoint = chrono::high_resolution_clock::now();
    }

    //! 経過時間[ms](double)を返す
    double elapsed() const {
        // auto end_timepoint = chrono::high_resolution_clock::now();
        // auto start = chrono::time_point_cast<chrono::microseconds>(start_timepoint).time_since_epoch().count();
        // auto end = chrono::time_point_cast<chrono::microseconds>(end_timepoint).time_since_epoch().count();
        // return (end - start) * 0.001;
        auto end_timepoint = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double, std::milli> duration = end_timepoint - start_timepoint;
        return duration.count();
    }
};
}  // namespace titan23
// #include "titan_cpplib/algorithm/random.cpp"

#include <cassert>
#include <unordered_set>
#include <vector>
using namespace std;

// Random
namespace titan23 {

/**
 * @brief (疑似)乱数生成クラス(XOR shift)
 */
class Random {
private:
    uint32_t _x, _y, _z, _w;

    constexpr uint32_t _xor128() {
        const uint32_t t = _x ^ (_x << 11);
        _x = _y;
        _y = _z;
        _z = _w;
        _w = (_w ^ (_w >> 19)) ^ (t ^ (t >> 8));
        return _w;
    }

public:
    Random() : _x(123456789), _y(362436069), _z(521288629), _w(88675123) {}
    Random(uint32_t seed) { set_seed(seed); }

    void set_seed(uint32_t seed) {
        _x = seed;
        _y = _x * 1812433253U + 1;
        _z = _y * 1812433253U + 1;
        _w = _z * 1812433253U + 1;
    }

    //! `[0, 1]` の乱数を返す(実数)
    constexpr double random() { return (double)(_xor128()) / 0xFFFFFFFF; }

    //! `[0, end]` の乱数を返す(整数)
    constexpr int randint(const int end) {
        assert(0 <= end);
        return (((uint64_t)_xor128() * (end+1)) >> 32);
    }

    //! `[begin, end]` の乱数を返す(整数)
    constexpr int randint(const int begin, const int end) {
        assert(begin <= end);
        return begin + (((uint64_t)_xor128() * (end-begin+1)) >> 32);
    }

    //! `[0, end)` の乱数を返す(整数)
    constexpr int randrange(const int end) {
        assert(0 < end);
        return (((uint64_t)_xor128() * end) >> 32);
    }

    //! `[begin, end)` の乱数を返す(整数)
    constexpr int randrange(const int begin, const int end) {
        assert(begin < end);
        return begin + (((uint64_t)_xor128() * (end-begin)) >> 32);
    }

    //! `[0, u64_MAX)` の乱数を返す / zobrist hash等の使用を想定
    constexpr uint64_t rand_u64() {
        return ((uint64_t)_xor128() << 32) | _xor128();
    }

    //! `[0, end)` の異なる乱数を2つ返す
    constexpr pair<int, int> rand_pair(const int end) {
        assert(end >= 2);
        int u = randrange(end);
        int v = u + randrange(1, end);
        if (v >= end) v -= end;
        if (u > v) swap(u, v);
        return {u, v};
    }

    //! `[begin, end)` の異なる乱数を2つ返す
    constexpr pair<int, int> rand_pair(const int begin, const int end) {
        assert(end - begin >= 2);
        int u = randrange(begin, end);
        int v = (u + randrange(1, end-begin));
        if (v >= end) v -= (end-begin);
        if (u > v) swap(u, v);
        return {u, v};
    }

    //! Note `a`は非const
    vector<int> rand_vec(const int cnt, vector<int> &a) {
        int n = a.size();
        for (int i = 0; i < cnt; ++i) {
            int j = randrange(i, n);
            swap(a[i], a[j]);
        }
        vector<int> res(a.begin(), a.begin()+cnt);
        return res;
    }

    //! `[begin, end)` の乱数を返す(実数)
    constexpr double randdouble(const double begin, const double end) {
        assert(begin <= end);
        return begin + random() * (end-begin);
    }

    template <typename RandomIt>
    void shuffle(RandomIt first, RandomIt last) {
        int n = distance(first, last);
        for (int i = 0; i < n-1; ++i) {
            int j = randrange(i, n);
            iter_swap(first+i, first+j);
        }
    }

    //! `vector` をインプレースにシャッフルする / `O(n)`
    template <typename T>
    void shuffle(vector<T> &a) {
        shuffle(a.begin(), a.end());
    }

    template <typename T>
    vector<T> choices(const vector<T> &a, const int k) {
        int n = a.size();
        assert(n >= k);
        vector<int> idx(n);
        iota(idx.begin(), idx.end(), 0);
        for (int i = 0; i < k; ++i) {
            int j = randrange(i, n);
            swap(idx[i], idx[j]);
        }
        vector<T> r(k);
        for (int i = 0; i < k; ++i) {
            r[i] = a[idx[i]];
        }
        return r;
    }

    template <typename T>
    constexpr T choice(const vector<T> &a) {
        assert(!a.empty());
        return a[randrange(a.size())];
    }

    template <typename T>
    constexpr T choice(const vector<T> &a, const vector<int> &w, bool normal) {
        assert(normal == false);
        assert(a.size() == w.size());
        double sum = 0.0;
        for (const int &x: w) sum += x;
        assert(sum > 0);
        vector<double> x(w.size());
        for (int i = 0; i < x.size(); ++i) {
            x[i] = (double)w[i] / sum;
            if (i-1 >= 0) x[i] += x[i-1];
        }
        return choice(a, x);
    }

    template <typename T>
    constexpr T choice(const vector<T> &a, const vector<double> &w) {
        double i = random();
        int l = -1, r = a.size()-1;
        while (r - l > 1) {
            int mid = (l + r) / 2;
            if (w[mid] <= i) l = mid;
            else r = mid;
        }
        return a[r];
    }

    template <typename T>
    constexpr T rand_pop(vector<T> &a) {
        assert(!a.empty());
        int idx = randrange(a.size());
        T res = a[idx];
        a.erase(a.begin() + idx);
        return res;
    }
};
} // namespace titan23
// #include "titan_cpplib/others/print.cpp"

#include <bits/stdc++.h>
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
// array<T, N>
template <typename T, size_t N> ostream& operator<<(ostream &os, const array<T, N> &a);
// stack<T>
template<typename T> ostream& operator<<(ostream& os, const stack<T>& s);
// queue<T>
template<typename T> ostream& operator<<(ostream& os, const queue<T>& q);
// deque<T>
template<typename T> ostream& operator<<(ostream& os, const deque<T>& dq);
// priority_queue<T, Container, Compare>
template<typename T, typename Container, typename Compare> ostream& operator<<(ostream& os, const priority_queue<T, Container, Compare>& pq);
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
static const string PRINT_RED    = "\033[91m"; // 赤字
static const string PRINT_GREEN  = "\033[92m"; // 緑字
static const string PRINT_YELLOW = "\033[93m"; // 黄字
static const string PRINT_BLUE   = "\033[94m"; // 青字
static const string PRINT_DIM    = "\033[2m";  // 薄字
static const string PRINT_NONE   = "\033[m";   // 色を元に戻す
static const string PRINT_BOLD = "\u001b[1m"; // 太字

string to_red   (const string &s) { return PRINT_RED    + s + PRINT_NONE; }
string to_green (const string &s) { return PRINT_GREEN  + s + PRINT_NONE; }
string to_yellow(const string &s) { return PRINT_YELLOW + s + PRINT_NONE; }
string to_blue  (const string &s) { return PRINT_BLUE   + s + PRINT_NONE; }
string to_dim   (const string &s) { return PRINT_DIM    + s + PRINT_NONE; }
string to_bold  (const string &s) { return PRINT_BOLD   + s + PRINT_NONE; }

string spacefill(const string s, const int f) {
    int n = s.size();
    string t;
    for (int i = 0; i < f-n; ++i) t += " ";
    t += s;
    return t;
}

string spacefill(const int x, const int f) {
    string s = to_string(x);
    return spacefill(s, f);
}

string zfill(const string s, const int f) {
    int n = s.size();
    string t;
    for (int i = 0; i < f-n; ++i) t += "0";
    t += s;
    return t;
}

string bin(long long s) {
    if (s == 0) return "0";
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

// array
template <typename T, size_t N>
ostream& operator<<(ostream &os, const array<T, N> &a) {
    os << "[";
    for (int i = 0; i < (int)N; ++i) {
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

// stack<T>
template<typename T> ostream& operator<<(ostream& os, const stack<T>& s) {
    stack<T> tmp = s;
    vector<T> v;
    while (!tmp.empty()) {
        v.push_back(tmp.top());
        tmp.pop();
    }
    os << "[";
    for (int i = (int)v.size()-1; i >= 0; --i) {
        if (i != (int)v.size()-1) os << ", ";
        os << v[i];
    }
    os << "]";
    return os;
}

// queue<T>
template<typename T> ostream& operator<<(ostream& os, const queue<T>& q) {
    queue<T> tmp = q;
    os << "[";
    bool first = true;
    while (!tmp.empty()) {
        if (!first) os << ", ";
        first = false;
        os << tmp.front();
        tmp.pop();
    }
    os << "]";
    return os;
}

// deque<T>
template<typename T>
ostream& operator<<(ostream& os, const deque<T>& dq) {
    vector<T> a(dq.begin(), dq.end());
    os << a;
    return os;
}

// priority_queue<T, Container, Compare>
template<typename T, typename Container, typename Compare> ostream& operator<<(ostream& os, const priority_queue<T, Container, Compare>& pq) {
    priority_queue<T, Container, Compare> tmp = pq;
    os << "[";
    bool first = true;
    while (!tmp.empty()) {
        if (!first) os << ", ";
        first = false;
        os << tmp.top();
        tmp.pop();
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

// #include "titan_cpplib/ahc/beam_search/beam_search_turn.cpp"
#include <bits/stdc++.h>
// #include "titan_cpplib/ds/hash_dict.cpp"
#include <vector>
#include <random>
#include <cassert>
#include <algorithm>
#include <emmintrin.h>

using namespace std;

namespace titan23 {

template<typename V>
class HashDict {
private:
    using u64 = uint64_t;
    static constexpr const uint8_t EMPTY = 0x80;

    vector<uint8_t> meta;
    vector<u64> keys;
    vector<V> vals;
    int cap;
    int msk;
    u64 xor_;
    int size;

    constexpr u64 hash(u64 key) const {
        key ^= xor_;
        key = (key ^ (key >> 30)) * 0xbf58476d1ce4e5b9;
        key = (key ^ (key >> 27)) * 0x94d049bb133111eb;
        key = key ^ (key >> 31);
        return key;
    }

    void init_seed() {
        random_device rd;
        mt19937 gen(rd());
        uniform_int_distribution<u64> dis(0, UINT64_MAX);
        xor_ = dis(gen);
    }

    void rebuild() {
        vector<uint8_t> old_meta = move(meta);
        vector<u64> old_keys = move(keys);
        vector<V> old_vals = move(vals);
        int old_cap = cap;

        cap *= 2;
        msk = cap - 1;
        meta.assign(cap + 16, EMPTY);
        keys.resize(cap);
        vals.resize(cap);
        size = 0;

        for (int i = 0; i < old_cap; ++i) {
            if (old_meta[i] != EMPTY) {
                set(old_keys[i], old_vals[i]);
            }
        }
    }

public:
    HashDict() {
        cap = 16;
        meta.assign(cap + 16, EMPTY);
        keys.resize(cap);
        vals.resize(cap);
        msk = cap - 1;
        init_seed();
        size = 0;
    }

    HashDict(const int n) {
        cap = 16;
        while (cap < n * 2) {
            cap *= 2;
        }
        meta.assign(cap + 16, EMPTY);
        keys.resize(cap);
        vals.resize(cap);
        msk = cap - 1;
        init_seed();
        size = 0;
    }

    pair<int, bool> get_pos(const u64 &key) const {
        u64 h = hash(key);
        uint8_t h2 = h & 0x7F;
        int idx = (h >> 7) & msk;

        __m128i match = _mm_set1_epi8(h2);
        __m128i empty_match = _mm_set1_epi8(EMPTY);

        while (true) {
            __m128i meta_data = _mm_loadu_si128(reinterpret_cast<const __m128i*>(&meta[idx]));

            int mask = _mm_movemask_epi8(_mm_cmpeq_epi8(meta_data, match));
            while (mask != 0) {
                int bit_pos = __builtin_ctz(mask);
                int target = (idx + bit_pos) & msk;
                if (keys[target] == key) {
                    return {target, true};
                }
                mask &= (mask - 1);
            }

            int empty_mask = _mm_movemask_epi8(_mm_cmpeq_epi8(meta_data, empty_match));
            if (empty_mask != 0) {
                int bit_pos = __builtin_ctz(empty_mask);
                int target = (idx + bit_pos) & msk;
                return {target, false};
            }

            idx = (idx + 16) & msk;
        }
    }

    V get(const u64 key) const {
        const auto [pos, exist_res] = get_pos(key);
        if (!exist_res) return V();
        else return vals[pos];
    }

    V get(const u64 key, const V missing) const {
        const auto [pos, exist_res] = get_pos(key);
        if (!exist_res) return missing;
        else return vals[pos];
    }

    bool contains(const u64 key) const {
        return get_pos(key).second;
    }

    pair<int, bool> pos(const u64 key) const {
        return get_pos(key);
    }

    V operator[] (const u64 key) {
        const auto [pos, exist_res] = get_pos(key);
        if (!exist_res) {
            V res = V{};
            inner_set({pos, false}, key, res);
            return res;
        } else {
            return vals[pos];
        }
    }

    V inner_get(const pair<int, bool> &dat, const V missing) const {
        const auto [pos, is_exist] = dat;
        if (!is_exist) return missing;
        return vals[pos];
    }

    V inner_get(const pair<int, bool> &dat) {
        const auto [pos, is_exist] = dat;
        if (!is_exist) return V();
        return vals[pos];
    }

    void inner_set(const pair<int, bool> &dat, const u64 key, const V val) {
        const auto [pos, is_exist] = dat;
        vals[pos] = val;
        if (!is_exist) {
            uint8_t h2 = hash(key) & 0x7F;
            meta[pos] = h2;
            if (pos < 16) meta[cap + pos] = h2;
            keys[pos] = key;
            ++size;
            if (size * 2 > cap) {
                rebuild();
            }
        }
    }

    void set(const u64 key, const V val) {
        const auto [pos, is_exist] = get_pos(key);
        inner_set({pos, is_exist}, key, val);
    }

    void add(const u64 key, const V val) {
        const auto [pos, is_exist] = get_pos(key);
        if (!is_exist) {
            inner_set({pos, false}, key, val);
        } else {
            vals[pos] += val;
        }
    }

    bool contains_set(const u64 key, const V val) {
        const auto [pos, is_exist] = get_pos(key);
        if (is_exist) {
            if (val < vals[pos]) {
                vals[pos] = val;
            } else {
                return false;
            }
            return true;
        } else {
            inner_set({pos, false}, key, val);
            return false;
        }
    }

    vector<V> values() const {
        vector<V> res;
        res.reserve(size);
        for (int i = 0; i < cap; ++i) {
            if (meta[i] != EMPTY) {
                res.emplace_back(vals[i]);
            }
        }
        return res;
    }

    vector<pair<u64, V>> items() const {
        vector<pair<u64, V>> res;
        res.reserve(size);
        for (int i = 0; i < cap; ++i) {
            if (meta[i] != EMPTY) {
                res.emplace_back(keys[i], vals[i]);
            }
        }
        return res;
    }

    void clear() {
        if (empty()) return;
        size = 0;
        fill(meta.begin(), meta.end(), EMPTY);
    }

    int len() const {
        return size;
    }

    int inner_len() const {
        return cap;
    }

    bool empty() const {
        return size == 0;
    }
};

} // namespace titan23
// #include "titan_cpplib/ahc/beam_search/beam_param.cpp"


namespace flying_squirrel { // flying squirrel

struct BeamParam {
    int max_turn, beam_width;
    // time_limit は ms 単位 (Timer::elapsed() と単位を揃える)。1 秒なら 1000。
    double time_limit;
    bool is_adjusting;
    // 毎ターン Candidates 内の hash dict を clear するか。
    // true : 各ターンの beam 内重複排除のみ行う安全な既定動作。
    // false: clear のオーバーヘッドを省くが、State 側の hash がターン情報を含まないと
    //        ターン跨ぎの stale entry により候補が黙って drop される。設計を理解した上で使うこと。
    bool clear_hash_every_turn;

    // clear_hash_every_turn=false のときのみ意味を持つ。
    // > 0 のとき、K = hash_window_turns ターンに 1 回 hash dict を全 clear して
    // 古い entry を捨てる。これにより dict size を高々 O(K * W) に抑える。
    // = 0 (デフォルト) では従来通り無制限蓄積。
    // 厳密に「過去 K ターン」ではなく「最後の clear から 1〜K ターン分」の窓になる点に注意。
    int hash_window_turns;

    // 内部で使用する変数
    int pool_size_sum, beam_width_sum, turn_sum;
    double time_sum;
    int prev_beam_width;

    // 動的ビーム幅の推移 report 用。timestamp / timestamp_meta が
    // 毎ターン (メタターン) 末尾に実効幅を push する。探索挙動には不使用。
    vector<int> width_hist;

    // target_turn 進行速度の実測 (beam_search_turn.cpp のマルチターン用)
    // 1 メタターンで進む target_turn の期待値 = target_step_sum / target_step_count
    long long target_step_sum;
    long long target_step_count;

    // beam_search_turn.cpp の global seen_hash の初期キャパシティのヒント (0 で自動)
    int seen_hash_capacity_hint;

    // ---- 累積コスト計測 (beam_search_turn.cpp 用) ----
    // 全時間は ms 単位 (param.time_limit と一致)。
    //
    // dt は active メタターン (applied_w > 0) と empty メタターン
    // (applied_w = 0) で構造的に違う:
    //   - empty : dt = tree 走査のみ。W に依存しない
    //   - active: dt = W に比例する成分が支配
    // 単一の EMA / 平均で混ぜると bimodal な分布を平均してしまい、
    // empty が多いほど平均が下振れ、W を絞るタイミングが遅れる。
    // したがって active / empty を別々に累積で記録し、線形スケーリングは
    // active 成分にだけ適用する。詳細は recommend_width 参照。
    //
    //   time_active_sum / count_active : active メタターン累積
    //   time_empty (= time_sum - time_active_sum) は副次的に求まる
    //   count_empty (= turn_sum - count_active) も同様
    //   beam_width_sum は applied_w の累積 (= active 時のみ加算される)
    //
    // 補助 EMA (現状 recommend_width 内では未使用、観測用):
    //   ema_active_rate : EMA( applied_w > 0 ? 1 : 0 )
    //   ema_step        : EMA( delta_target )    残メタターン推定用
    double time_active_sum;
    long long count_active;
    double ema_active_rate;
    double ema_step;
    int    meta_sample_count;

    // EMA 平滑化係数 (0 < alpha <= 1, 大きいほど直近重視)
    double ema_alpha_rate;
    double ema_alpha_step;

    // 動的調整の安全率。予測 1 メタターン時間が予算を超過しないように、
    // 推奨幅をこの倍率だけ下振れさせる。
    double width_safety_factor;

    // 計測安定化のため、最初の数メタターンは固定幅で動かす。
    // EMA に十分なサンプルが入ってから動的調整を始める。
    int calibration_meta_count;

    BeamParam() { init(); }

    BeamParam(
        int max_turn,
        int beam_width,
        double time_limit,
        bool is_adjusting=false,
        bool clear_hash_every_turn=true,
        int hash_window_turns=0
    ) {
        init();
        this->max_turn = max_turn;
        this->beam_width = beam_width;
        this->time_limit = time_limit;
        this->is_adjusting = is_adjusting;
        this->clear_hash_every_turn = clear_hash_every_turn;
        this->hash_window_turns = hash_window_turns;
    }

    void init() {
        max_turn = 0;
        beam_width = 0;
        time_limit = 0;
        is_adjusting = false;
        clear_hash_every_turn = true;
        hash_window_turns = 0;
        pool_size_sum = 0;
        beam_width_sum = 0;
        turn_sum = 0;
        time_sum = 0;
        prev_beam_width = -1;
        width_hist.clear();
        target_step_sum = 0;
        target_step_count = 0;
        seen_hash_capacity_hint = 0;

        time_active_sum = 0.0;
        count_active    = 0;
        ema_active_rate = -1.0;
        ema_step        = -1.0;
        meta_sample_count = 0;
        ema_alpha_rate = 0.20;
        ema_alpha_step = 0.30;
        width_safety_factor    = 0.90;
        calibration_meta_count = 3;
    }

    static double ema_update(double cur, double x, double alpha) {
        return (cur < 0.0) ? x : (alpha * x + (1.0 - alpha) * cur);
    }

    void timestamp(int pool_size, int beam_width, double time) {
        pool_size_sum += pool_size;
        beam_width_sum += beam_width;
        time_sum += time;
        turn_sum++;
        width_hist.push_back(beam_width);
    }

    // beam_search_turn.cpp 用: 1 メタターン分の計測サンプルを EMA に流し、
    // 互換のため累積指標 (turn_sum / time_sum 等) も同時に更新する。
    //   dt_expand_ms : get_next_beam の所要時間 [ms]
    //   dt_update_ms : sort + update_tree の所要時間 [ms]
    //   tree_size    : update_tree 後の tree.size()
    //   exp_count    : current_new_candidates.size() (採用された展開数)
    //   applied_w    : このメタターン中に展開した leaf 数
    //                  (= target_turn == 現メタターン だった tree 内 leaf 数)
    //   delta_target : 1 メタターンで進んだ target_turn の量 (>=0)
    //
    // dt_expand_ms / dt_update_ms は分けて取っているが、現状の累積モデルでは
    // 合算 dt のみを使う。分割は将来の精度向上のため signature だけ残す。
    void timestamp_meta(double dt_expand_ms, double dt_update_ms,
                        int tree_size, int exp_count,
                        int applied_w, int delta_target) {
        double dt_ms = dt_expand_ms + dt_update_ms;

        // active / empty を分けて累積する。
        // empty は dt が W に依存しないので別計上したい。
        if (applied_w > 0) {
            time_active_sum += dt_ms;
            count_active++;
        }
        // 補助 EMA (観測用)
        ema_active_rate = ema_update(ema_active_rate,
                                     (applied_w > 0 ? 1.0 : 0.0),
                                     ema_alpha_rate);
        // ema_step は zero meta-turn も含めた平均にする
        // (0 を入れずに更新すると進捗ゼロの実態を反映できず remain_meta を過小評価する)
        ema_step = ema_update(ema_step, (double)max(0, delta_target), ema_alpha_step);
        meta_sample_count++;

        // 互換のため既存の累積も更新する。
        (void)exp_count;
        pool_size_sum  += tree_size;
        beam_width_sum += applied_w;  // applied_w==0 のとき加算されないので、結果として active 時の W 累積
        time_sum       += dt_ms;
        turn_sum++;
        width_hist.push_back(applied_w);
    }

    // beam_search_turn.cpp 用: target_turn の進行量を記録する (累積のみ)。
    // EMA 更新は timestamp_meta 側で行う。
    void note_target_step(int step) {
        target_step_sum += step;
        target_step_count++;
    }

    // モデルが予測に使える状態か。active を 1 回以上観測している必要がある。
    bool cost_model_ready() const {
        return count_active > 0 && turn_sum > 0;
    }

    // 残り remain_meta メタターンを remain_time_ms 以内に収める W を返す。
    // モデル未初期化なら -1。
    //
    // active メタターン (dt_active = a + b*W に近い) と empty メタターン
    // (dt_empty = const) を分けて扱う:
    //   rate            = count_active / turn_sum
    //   dt_empty        = (time_sum - time_active_sum) / (turn_sum - count_active)
    //   dt_active_obs   = time_active_sum / count_active        (W=w_obs での実測)
    //   w_obs           = beam_width_sum / count_active         (active 時の平均 W)
    //
    //   target_dt        = remain_time / remain_meta
    //   target_dt_active = (target_dt - (1-rate) * dt_empty) / rate
    //   W*               = w_obs * target_dt_active / dt_active_obs
    //
    // dt_active が W に厳密に比例する仮定 (overhead 無視) なので多少
    // バイアスはあるが、recommend_width は毎メタターン何度も呼ばれるので
    // 反復で自己補正される (W を下げれば dt_active 累積が下がり、次の
    // 推奨も整合する)。
    int recommend_width(double remain_time_ms, int remain_meta) const {
        if (!cost_model_ready())       return -1;
        if (remain_time_ms  <= 0.0)    return 1;
        if (remain_meta < 1) remain_meta = 1;

        double dt_active_obs = time_active_sum / (double)count_active;
        if (dt_active_obs <= 0.0) return beam_width;
        double w_obs = (double)beam_width_sum / (double)count_active;
        if (w_obs <= 0.0) return beam_width;

        double rate = (double)count_active / (double)turn_sum;
        if (rate <= 0.0) return beam_width;

        double dt_empty = 0.0;
        if ((long long)turn_sum > count_active) {
            dt_empty = (time_sum - time_active_sum)
                     / (double)((long long)turn_sum - count_active);
            if (dt_empty < 0.0) dt_empty = 0.0;
        }

        double target_dt = remain_time_ms / (double)remain_meta;
        double target_dt_active = (target_dt - (1.0 - rate) * dt_empty) / rate;
        if (target_dt_active <= 0.0) return 1;

        double W_d = w_obs * target_dt_active / dt_active_obs * width_safety_factor;
        int W = (int)W_d;
        if (W < 1) W = 1;
        if (W > beam_width) W = beam_width;
        return W;
    }

    int get_beam_width(int remain_turn, int now_pool_size, double remain_time) {
        // return beam_width;
        if (!is_adjusting || turn_sum <= 10) {
            return beam_width;
        }
        // 10回ごとに更新してみる
        if (turn_sum % 10 != 0 && prev_beam_width != -1) {
            return prev_beam_width;
        }
        if (remain_turn <= 0) return beam_width;
        int ave_beam_width = (double)beam_width_sum / turn_sum;
        double can_use_time = (double)remain_time / remain_turn;
        double pred_one_time = (double)time_sum / beam_width_sum;
        int pred_width = max(1.0, can_use_time / pred_one_time);
        int beam_width = (pred_width*2 + ave_beam_width) / 3;
        prev_beam_width = beam_width;
        return beam_width;
    }

    // @deprecated `beam_log::end_banner` で同等の情報が出力される。
    // 個別に呼ぶことも引き続き可能。
    void report() const {
        cerr << to_bold("BeamParam-report----------------") << endl;
        if (turn_sum == 0) {
            cerr << "turn_sum = 0" << endl;
        } else {
            cerr << "ave_beam_width=" << (double)beam_width_sum / turn_sum << endl;
        }
        cerr << "--------------------------------" << endl;
    }

    // 平均ビーム幅 (turn_sum == 0 の時は 0)
    double ave_width() const {
        return turn_sum > 0 ? (double)beam_width_sum / turn_sum : 0.0;
    }
};

BeamParam gen_param(int max_turn, int beam_width) {
    return {max_turn, beam_width, -1};
}

BeamParam gen_param(int max_turn, int beam_width, double time_limit, bool is_adjusting, bool clear_hash_every_turn=true) {
    return {max_turn, beam_width, time_limit, is_adjusting, clear_hash_every_turn};
}
} // namespace flying_squirrel
// #include "titan_cpplib/ahc/beam_search/beam_log.cpp"

#include <bits/stdc++.h>
using namespace std;

namespace flying_squirrel {
namespace beam_log {

inline const string& tag_bs() {
    static const string s = PRINT_BOLD + string("[BS]") + PRINT_NONE + " ";
    return s;
}
inline const string& tag_info()  { return tag_bs(); }
inline const string& tag_ok()    { return tag_bs(); }
inline const string& tag_warn()  { return tag_bs(); }
inline const string& tag_error() { return tag_bs(); }
inline const string& tag_debug() { return tag_bs(); }
inline const string& tag_plain() { return tag_bs(); }
inline const string& tag_turn()  { return tag_bs(); }

inline string col_ok   (const string& m) { return PRINT_GREEN  + m + PRINT_NONE; }
inline string col_warn (const string& m) { return PRINT_YELLOW + m + PRINT_NONE; }
inline string col_error(const string& m) { return PRINT_RED    + m + PRINT_NONE; }
inline string col_debug(const string& m) { return PRINT_DIM    + m + PRINT_NONE; }

inline void info (ostream& os, const string& msg) { os << tag_bs() << msg << "\n"; }
inline void ok   (ostream& os, const string& msg) { os << tag_bs() << col_ok(msg)    << "\n"; }
inline void warn (ostream& os, const string& msg) { os << tag_bs() << col_warn(msg)  << "\n"; }
inline void error(ostream& os, const string& msg) { os << tag_bs() << col_error(msg) << "\n"; }
inline void debug(ostream& os, const string& msg) { os << tag_bs() << col_debug(msg) << "\n"; }

inline void start_banner(ostream& os, const char* impl_name, const BeamParam& param) {
    os << tag_plain() << "================================================\n";
    os << tag_info()  << "start " << to_bold(impl_name) << "\n";
    os << tag_info()  << "  max_turn   = " << param.max_turn << "\n";
    os << tag_info()  << "  beam_width = " << param.beam_width << "\n";
    os << tag_info()  << "  time_limit = " << fixed << setprecision(1) << param.time_limit << " [ms]\n";
    os << tag_info()  << "  adjusting  = " << (param.is_adjusting ? "true" : "false") << "\n";
    os << tag_info()  << "  clear_hash = " << (param.clear_hash_every_turn ? "true" : "false") << "\n";
    os << tag_plain() << "------------------------------------------------\n";
}

template<class ScoreType>
inline void turn_line(ostream& os,
                      int turn, int max_turn,
                      double elapsed_ms,
                      int width, int pool, int cand, int explored,
                      ScoreType best_score, bool has_best = true) {
    (void)pool; // 木サイズ。コンパクト表示のため非表示（必要なら一行戻す）
    // expl = そのターン実際に探索した頂点数 (= try_op の呼び出し回数)
    // cand = expl のうちビームに残った件数 / w = ビーム幅
    ostringstream ss;
    ss << setw(4) << setfill(' ') << turn << "/"
       << setw(4) << setfill(' ') << max_turn << setfill(' ')
       << " | t=" << fixed << setprecision(1) << setw(8) << elapsed_ms << "ms";
    if (explored >= 0) {
        ss << " | expl= " << setw(7) << explored;
    }
    ss << " | cand/w= " << setw(4) << cand << "/" << setw(4) << width;
    if (has_best) {
        ss << " | best= " << best_score;
    }
    os << tag_turn() << ss.str() << "\n";
}

inline void turn_line_extra(ostream& os, const string& extra) {
    os << tag_plain() << "  " << to_dim(extra) << "\n";
}

template<class ScoreType>
inline void on_solution_found(ostream& os, int turn, ScoreType score) {
    ostringstream ss;
    ss << "valid solution found at turn " << turn << " (score=" << score << ")";
    ok(os, ss.str());
}
inline void on_no_candidates(ostream& os, int turn) {
    ostringstream ss;
    ss << "no candidates at turn " << turn;
    error(os, ss.str());
}
inline void on_max_turn(ostream& os) {
    ok(os, "reached max_turn");
}

template<class ScoreType>
inline void end_banner(ostream& os,
                       const char* reason,
                       int turns_done, int max_turn,
                       double total_ms,
                       double ave_width,
                       ScoreType best_score, bool has_best,
                       int actions_count) {
    os << tag_plain() << "------------------------------------------------\n";
    os << tag_bs()    << col_ok(string("finished: ") + reason) << "\n";
    os << tag_info()  << "  turns done    = " << turns_done << " / " << max_turn << "\n";
    os << tag_info()  << "  total time    = " << fixed << setprecision(1) << total_ms << " [ms]\n";
    os << tag_info()  << "  ave width     = " << fixed << setprecision(2) << ave_width << "\n";
    if (has_best) {
        os << tag_info() << "  best score    = " << best_score << "\n";
    }
    os << tag_info()  << "  actions count = " << actions_count << "\n";
    os << tag_plain() << "================================================\n";
}

inline void end_banner_extra(ostream& os, const string& key, long long value) {
    os << tag_info() << "  " << left << setw(14) << key << "= " << value << "\n";
}

// 動的ビーム幅の推移を 1 行 sparkline + 統計で出す。
// hist は timestamp / timestamp_meta が貯めた毎ターンの実効幅。
// 点数が cols を超える場合はバケット平均でダウンサンプルする
// (推移の形は保たれる)。is_adjusting=false でも幅一定の確認に使える。
inline void width_trace(ostream& os, const vector<int>& hist, int cols = 40) {
    if (hist.empty()) {
        os << tag_info() << "  wtrace = (no samples)\n";
        return;
    }
    static const char* blocks[8] = {
        "▁","▂","▃","▄","▅","▆","▇","█"
    };
    const int n = (int)hist.size();

    // ダウンサンプル: cols 個のバケットに平均化
    vector<double> bucket;
    if (n <= cols) {
        bucket.assign(hist.begin(), hist.end());
    } else {
        bucket.resize(cols, 0.0);
        for (int b = 0; b < cols; ++b) {
            int lo = (int)((long long)b * n / cols);
            int hi = (int)((long long)(b + 1) * n / cols);
            if (hi <= lo) hi = lo + 1;
            double s = 0.0;
            for (int i = lo; i < hi && i < n; ++i) s += hist[i];
            bucket[b] = s / (hi - lo);
        }
    }

    int vmin = hist[0], vmax = hist[0];
    long long sum = 0;
    for (int v : hist) { vmin = min(vmin, v); vmax = max(vmax, v); sum += v; }
    double mean = (double)sum / n;

    vector<int> sorted = hist;
    sort(sorted.begin(), sorted.end());
    int p50 = sorted[n / 2];

    string spark;
    double span = (vmax > vmin) ? (double)(vmax - vmin) : 1.0;
    for (double v : bucket) {
        int lv = (int)((v - vmin) / span * 7.0 + 0.5);
        if (lv < 0) lv = 0;
        if (lv > 7) lv = 7;
        spark += blocks[lv];
    }

    os << tag_info() << "  wtrace = " << spark << " (n=" << n << ")\n";
    os << tag_info() << "  wstats = min=" << vmin
       << " p50=" << p50
       << " mean=" << fixed << setprecision(1) << mean
       << " max=" << vmax
       << " last=" << hist.back() << "\n";
}

} // namespace beam_log
} // namespace flying_squirrel
using namespace std;

namespace flying_squirrel {

template<typename ScoreType, typename HashType, class Action, class State, ScoreType INF, bool record_history=false>
class BeamSearchWithTree {
private:
    static constexpr const int PRE_ORDER = -1;
    static constexpr const int POST_ORDER = -2;
    titan23::Timer beam_timer;
    using ActionId = int;
    static constexpr ActionId BAD_ID = -1;
    vector<Action> result;
    Action DUMMY_ACTION;

    bool found_finished;
    ScoreType best_finished_score;
    ActionId best_finished_par_aid;
    Action best_finished_action;

    struct TreeNode {
        // leaf: 0 以上 (世代ごとに振り直す leaf_id) / PRE_ORDER / POST_ORDER
        int dir_or_leaf_id;
        // PRE_ORDER 専用: 対応する POST_ORDER の次の index (部分木の skip 用)
        int subtree_end;
        // 同じ action の PRE/POST/leaf ノードは同じ aid を持つ
        ActionId aid;
        // leaf: action の target_turn / PRE_ORDER: 部分木 leaf の target_turn の最小値
        int target_turn;
        TreeNode(int d, ActionId a, int target) : dir_or_leaf_id(d), subtree_end(0), aid(a), target_turn(target) {}
    };

    struct BeamCandidate {
        ScoreType score;
        HashType hash;
        int par; // 親 leaf の leaf_id
        ActionId aid;
        int target_turn;
        BeamCandidate() : score(0), hash(0), par(0), aid(BAD_ID), target_turn(0) {}
        BeamCandidate(int p, ScoreType s, HashType h, ActionId a, int target) : score(s), hash(h), par(p), aid(a), target_turn(target) {}
    };

    vector<TreeNode> tree, nxt_tree;

    // ---- Action pool --------------------------------------------------------
    vector<Action> action_pool;
    vector<int> free_slots;     // 解放済み slot の LIFO
    vector<uint32_t> alive_gen; // mark_and_sweep 用の slot ごとの世代タグ
    uint32_t now_gen;           // 現在の世代番号

    Action& act(ActionId id) {
        return action_pool[id];
    }
    ActionId arena_put_reserve() {
        ActionId slot;
        if (!free_slots.empty()) {
            slot = free_slots.back();
            free_slots.pop_back();
        } else {
            slot = action_pool.size();
            action_pool.emplace_back();
            alive_gen.push_back(0);
            is_survived_node.push_back(0);
        }
        return slot;
    }
    void arena_put_fill(ActionId slot, const Action& a) {
        action_pool[slot] = a;
    }
    void arena_release(ActionId slot) {
        free_slots.push_back(slot);
    }
    // tree に残らなかった aid を free_slots に戻す (update_tree の swap 後に呼ぶ)
    //   step1: 新 tree (= 現 tree) の aid のタグを now_gen にする
    //   step2: 旧 tree (= swap 後の nxt_tree) と new_candidates のうち
    //          タグが now_gen でない aid を解放する (解放時にタグも更新して二重解放を防ぐ)
    void mark_and_sweep() {
        ++now_gen;
        for (const auto& nd : tree) {
            alive_gen[nd.aid] = now_gen;
        }
        for (const auto& nd : nxt_tree) {
            if (alive_gen[nd.aid] != now_gen) {
                free_slots.push_back(nd.aid);
                alive_gen[nd.aid] = now_gen;
            }
        }
        for (const auto& nc : new_candidates) {
            if (alive_gen[nc.aid] != now_gen) {
                free_slots.push_back(nc.aid);
                alive_gen[nc.aid] = now_gen;
            }
        }
    }

    struct HistoryNode {
        int node_id;
        int parent_id;
        int turn;
        ScoreType score;
        HashType hash;
        string action_str;
        string state_info;
        int status;
    };
    struct TurnSnapshot {
        int turn;
        vector<int> active_node_ids;
    };
    vector<TurnSnapshot> snapshots;
    vector<HistoryNode> history;
    int node_id_counter;
    int max_turn_global;
    vector<uint8_t> is_survived_node;  // aid 添字の生存フラグ (arena_put_reserve で action_pool と同時に伸ばす)
    vector<int> aid_to_node_id;  // record_history=true のときだけ使う aid → node_id (history JSON 用)

    void dump_history_json(const string& filename) const {
        ofstream ofs(filename);
        if(!ofs) return;
        ofs << "{\n  \"INF\": " << INF << ",\n  \"nodes\": [\n";
        for (int i = 0; i < history.size(); ++i) {
            const auto& node = history[i];
            ofs << "    {\n"
                << "      \"node_id\": " << node.node_id << ",\n"
                << "      \"parent_id\": " << node.parent_id << ",\n"
                << "      \"turn\": " << node.turn << ",\n"
                << "      \"score\": " << node.score << ",\n"
                << "      \"hash\": " << node.hash << ",\n"
                << "      \"action\": \"" << node.action_str << "\",\n"
                << "      \"state_info\": " << (node.state_info.empty() ? "{}" : node.state_info) << ",\n"
                << "      \"status\": " << node.status << "\n"
                << "    }";
            if (i + 1 < history.size()) ofs << ",";
            ofs << "\n";
        }
        ofs << "  ],\n  \"snapshots\": [\n";
        for (int i = 0; i < snapshots.size(); ++i) {
            ofs << "    {\n"
                << "      \"turn\": " << snapshots[i].turn << ",\n"
                << "      \"active_node_ids\": [";
            for (int j = 0; j < snapshots[i].active_node_ids.size(); ++j) {
                ofs << snapshots[i].active_node_ids[j];
                if (j + 1 < snapshots[i].active_node_ids.size()) ofs << ", ";
            }
            ofs << "]\n    }";
            if (i + 1 < snapshots.size()) ofs << ",";
            ofs << "\n";
        }
        ofs << "  ]\n}\n";
    }

    class Candidates {
    private:
        using T = pair<ScoreType, int>;
        vector<HashType> hashidx; // slot → hash の逆引き
        titan23::HashDict<int> hash_to_idx;
        int beam_width, entry;
        int s = 1;
        vector<T> seg;

        // entry < beam_width の間は segtree を作らず、entry == beam_width に達した時点で一括構築する
        // is_built==false の間は seg[1] / seg[idx+s] を参照しないこと
        bool is_built = false;

        void set(int k, T v) {
            k += s;
            seg[k] = v;
            while (k > 1) {
                k >>= 1;
                T nv = seg[k<<1].first > seg[k<<1|1].first ? seg[k<<1] : seg[k<<1|1];
                if (nv == seg[k]) break;
                seg[k] = nv;
            }
        }

        // entry 個の next_beam[] から segtree を O(W) で一括構築する
        // seg の残り ([entry+s, 2s)) は reset で {-INF, -1} になっている
        void build_segtree() {
            for (int i = 0; i < entry; ++i) {
                seg[i + s] = {next_beam[i].score, i};
            }
            for (int k = s - 1; k > 0; --k) {
                seg[k] = seg[k<<1].first > seg[k<<1|1].first ? seg[k<<1] : seg[k<<1|1];
            }
        }

    public:
        vector<BeamCandidate> next_beam;

        Candidates() {}

        int size() const { return entry; }
        int get_width() const { return beam_width; }

        ScoreType threshold() const { return entry < beam_width ? INF : seg[1].first; }

        // 受理した場合 next_beam のインデックスを返し、棄却なら -1
        // aid は呼び出し側が arena_put_reserve で取って渡す (棄却時は arena_release)
        // is_survived は採用した aid に 1、追い出した aid に 0 をセットする
        int push(ScoreType score, HashType hash, int par, ActionId aid, vector<uint8_t>& is_survived, int target_turn) {
            // segtree 構築済みなら worst 以上のスコアを早期に棄却できる
            if (is_built && score >= seg[1].first) return -1;
            auto pos = hash_to_idx.get_pos(hash);
            int idx = hash_to_idx.inner_get(pos, -1);
            if (idx != -1) {
                // 既に同じ hash がある場合、score が改善するときだけ置き換える
                if (score < next_beam[idx].score) {
                    is_survived[next_beam[idx].aid] = 0;
                    next_beam[idx] = {par, score, hash, aid, target_turn};
                    is_survived[aid] = 1;
                    if (is_built) {
                        set(idx, {score, idx});
                    }
                    return idx;
                }
                return -1;
            }
            if (entry < beam_width) {
                // entry < beam_width の間は segtree を更新せず、末尾に追加するだけ
                int slot = entry;
                hash_to_idx.inner_set(pos, hash, slot);
                next_beam[slot] = {par, score, hash, aid, target_turn};
                is_survived[aid] = 1;
                hashidx[slot] = hash;
                entry++;
                if (entry == beam_width) {
                    build_segtree();
                    is_built = true;
                }
                return slot;
            }
            // entry == beam_width なので worst と置き換える
            auto [_, i] = seg[1];
            is_survived[next_beam[i].aid] = 0;
            next_beam[i] = {par, score, hash, aid, target_turn};
            is_survived[aid] = 1;
            hash_to_idx.set(hashidx[i], -1);
            hash_to_idx.inner_set(pos, hash, i);
            hashidx[i] = hash;
            set(i, {score, i});
            return i;
        }

        void reset(int w) {
            beam_width = w;
            while (s < w) {
                s <<= 1;
            }
            if (seg.size() < 2*s) {
                seg.resize(2*s);
            }
            fill(seg.begin(), seg.begin()+(2*s), make_pair(-INF, -1));
            if (hashidx.size() < w) {
                hashidx.resize(w);
                next_beam.resize(w);
            }
            // pool 内の dedup はここで毎回 clear する
            // 全ターンを通した dedup は BeamSearchWithTree::seen_hash が担う
            hash_to_idx.clear();
            if (hash_to_idx.inner_len() == 1) {
                hash_to_idx = titan23::HashDict<int>(beam_width*8);
            }
            entry = 0;
            is_built = false;
        }
    };

    vector<Candidates> cands_pool;
    vector<int> turn_to_pool_idx; // target_turn → cands_pool の index (-1 は未確保)
    vector<int> free_pool_indices;
    vector<ScoreType> thresholds; // thresholds[t] = pool t の現在の worst score
    // update_tree 用の (PRE_ORDER の index, 要再計算フラグ) スタック
    vector<pair<int, bool>> pre_stack;

    // clear_hash_every_turn=false のとき、全ターンを通して一度見た hash を dedup する
    //   value = (best_score_at_smallest_t, smallest_t)
    // - 未登録                            → 通す
    // - 既登録 (s0, t0) で t < t0         → 通す (より小さい target_turn 側で再挑戦)
    // - 既登録 (s0, t0) で t==t0 かつ score < s0 → 通す (同じ最小 t で score 改善)
    // - その他                            → 捨てる
    // 既に大きい target_turn の pool に入っている同 hash の entry はそのまま残す
    titan23::HashDict<pair<ScoreType, int>> seen_hash;
    bool use_global_seen;

    // tree 内 leaf の target_turn の最小値
    // update_tree 末尾で更新し、compute_req_w の残り世代数の推定に使う
    int min_target_in_tree;

    // この世代で展開した leaf 数 (BeamParam::timestamp_meta の applied_w)
    int expanded_leaf_count;

    // is_adjusting のときは残時間と 1 世代の所要時間の実測から
    // BeamParam::recommend_width で見積もる
    int compute_req_w(BeamParam& param) {
        if (!param.is_adjusting) return param.beam_width;
        if (param.meta_sample_count < param.calibration_meta_count || !param.cost_model_ready()) {
            return param.beam_width;
        }
        // 1 世代で進む target_turn 量
        // ema_step を優先し、未初期化なら累積平均、それも無ければ 1
        double ave_step;
        if (param.ema_step > 0.0) {
            ave_step = max(0.5, param.ema_step);
        } else if (param.target_step_count > 0) {
            ave_step = max(0.5, (double)param.target_step_sum / param.target_step_count);
        } else {
            ave_step = 1.0;
        }
        int base = max_turn_global - min_target_in_tree;
        if (base < 0) base = 0;
        int remain_meta = max(1, (int)ceil(base / ave_step));
        double remain_time_ms = param.time_limit - beam_timer.elapsed();
        if (remain_time_ms <= 0.0) return 1;
        int rec = param.recommend_width(remain_time_ms, remain_meta);
        if (rec < 0) return param.beam_width;
        return rec;
    }

    // target_turn の pool を返す (未確保なら遅延確保する)
    Candidates& get_cands(BeamParam& param, int target_turn) {
        int idx = turn_to_pool_idx[target_turn];
        if (idx == -1) {
            if (!free_pool_indices.empty()) {
                idx = free_pool_indices.back();
                free_pool_indices.pop_back();
            } else {
                idx = cands_pool.size();
                cands_pool.emplace_back();
            }
            turn_to_pool_idx[target_turn] = idx;
            cands_pool[idx].reset(compute_req_w(param));
            thresholds[target_turn] = INF;
        }
        return cands_pool[idx];
    }

    vector<BeamCandidate> new_candidates;
    vector<Action> actions;

    // 候補 1 件を処理する
    //   - state.try_op で (score, hash, finished) を計算
    //   - score >= INF / target_turn 超過 / seen_hash 重複は捨てる
    //   - finished なら best_finished を更新
    //   - 通常候補は target_turn の pool へ push し、採否で fill / release
    // always_inline は user の try_op をインライン化させるため
    // (関数境界越しだと try_op が inline されず数 % 遅くなる)
    // parent_leaf: 親 leaf の leaf_id (turn==0 では 0)
    // parent_aid : 親 leaf の aid (turn==0 では BAD_ID)
    [[gnu::always_inline]] inline void process_candidate(BeamParam& param, State& state, Action& action, int parent_leaf, ActionId parent_aid) {
        auto [score, hash, finished] = state.try_op(action, thresholds);
        if (score >= INF) return;
        int target_turn = action.target_turn;
        if (target_turn > max_turn_global) return;

        pair<int, bool> seen_pos;
        if (use_global_seen) {
            seen_pos = seen_hash.get_pos(hash);
            if (seen_pos.second) {
                auto sv = seen_hash.inner_get(seen_pos);
                ScoreType seen_s0 = sv.first;
                int seen_t0 = sv.second;
                bool pass = (target_turn < seen_t0) || (target_turn == seen_t0 && score < seen_s0);
                if (!pass) return;
            }
        }

        int status = 0;
        int node_id = -1;
        if (finished) {
            if (!found_finished || score < best_finished_score) {
                found_finished = true;
                best_finished_score = score;
                best_finished_par_aid = parent_aid;
                best_finished_action = action;
            }
            if constexpr (record_history) node_id = node_id_counter++;
        } else {
            ActionId aid = arena_put_reserve();
            Candidates& cands = get_cands(param, target_turn);
            int slot = cands.push(score, hash, parent_leaf, aid, is_survived_node, target_turn);
            if (slot >= 0) {
                arena_put_fill(aid, action);
                thresholds[target_turn] = cands.threshold();
                new_candidates.push_back({parent_leaf, score, hash, aid, target_turn});
                if (use_global_seen) {
                    seen_hash.inner_set(seen_pos, hash, {score, target_turn});
                }
                if constexpr (record_history) {
                    if (aid >= aid_to_node_id.size()) aid_to_node_id.resize(aid + 1, -1);
                    node_id = node_id_counter++;
                    aid_to_node_id[aid] = node_id;
                }
            } else {
                arena_release(aid);
                status = 1;
                if constexpr (record_history) node_id = node_id_counter++;
            }
        }
        if constexpr (record_history) {
            int parent_node_id = (parent_aid == BAD_ID || parent_aid >= aid_to_node_id.size())
                                 ? -1 : aid_to_node_id[parent_aid];
            history.push_back({node_id, parent_node_id, target_turn, score, hash,
                               action.to_string(), state.get_state_info(), status});
        }
    }

    // get_actions に渡す emit オブジェクト
    // emit(action) で候補を登録し、emit.threshold(target_turn) で枝刈り閾値を取得する
    struct Emitter {
        BeamSearchWithTree& bs;
        BeamParam& param;
        State& st;
        int parent_leaf;
        ActionId parent_aid;

        inline ScoreType threshold(int target_turn) const { return bs.thresholds[target_turn]; }
        inline void operator()(Action& a) { bs.process_candidate(param, st, a, parent_leaf, parent_aid); }
    };

    void get_next_beam(State& state, BeamParam& param, const int turn) {
        new_candidates.clear();
        expanded_leaf_count = 0;

        if (turn == 0) {
            // 初期状態を 1 つの leaf として展開する (parent_leaf=0, parent_aid=BAD_ID)
            expanded_leaf_count = 1;
            const Action& last_action = (result.empty() ? DUMMY_ACTION : result.back());
            if constexpr (requires(Emitter& e) { state.get_actions(last_action, e); }) {
                Emitter emit{*this, param, state, 0, BAD_ID};
                state.get_actions(last_action, emit);
            } else {
                actions.clear();
                state.get_actions(actions, last_action, thresholds);
                for (Action &action : actions) {
                    process_candidate(param, state, action, 0, BAD_ID);
                }
            }
            return;
        }

        int leaf_id = 0;
        const int tree_size = tree.size();
        for (int i = 0; i < tree_size; ) {
            TreeNode& node = tree[i];
            const int dir_or_leaf_id = node.dir_or_leaf_id;
            if (dir_or_leaf_id >= 0) {
                if (node.target_turn == turn) {
                    ++expanded_leaf_count;
                    // ループ内の arena_put_reserve が action_pool を reallocate しうるので、
                    // 参照では保持できず、ローカルに copy する
                    Action action = act(node.aid);
                    state.apply_op(action);
                    ActionId parent_aid = node.aid;

                    if constexpr (requires(Emitter& e) { state.get_actions(action, e); }) {
                        Emitter emit{*this, param, state, leaf_id, parent_aid};
                        state.get_actions(action, emit);
                    } else {
                        actions.clear();
                        state.get_actions(actions, action, thresholds);
                        for (Action &child_action : actions) {
                            process_candidate(param, state, child_action, leaf_id, parent_aid);
                        }
                    }
                    node.dir_or_leaf_id = leaf_id;
                    ++leaf_id;
                    state.rollback(action);
                } else {
                    node.dir_or_leaf_id = leaf_id;
                    ++leaf_id;
                }
                ++i;
            } else if (dir_or_leaf_id == PRE_ORDER) {
                // この世代で展開する leaf が無い部分木は、apply せずに POST_ORDER の次へジャンプ
                if (node.target_turn > turn) {
                    i = node.subtree_end;
                    continue;
                }
                state.apply_op(act(node.aid));
                ++i;
            } else {
                state.rollback(act(node.aid));
                ++i;
            }
        }
    }

    // tree の root レベルを走査して leaf の target_turn の最小値を求める
    // 部分木は PRE_ORDER の subtree_end で skip するので root レベルのノード数に比例
    void update_min_target_in_tree() {
        int min_t = INT_MAX;
        const int n = tree.size();
        int k = 0;
        while (k < n) {
            const TreeNode& nd = tree[k];
            int d = nd.dir_or_leaf_id;
            if (d >= 0) {
                if (nd.target_turn < min_t) min_t = nd.target_turn;
                ++k;
            } else if (d == PRE_ORDER) {
                if (nd.target_turn < min_t) min_t = nd.target_turn;
                k = nd.subtree_end;
            } else {
                ++k;
            }
        }
        min_target_in_tree = (min_t == INT_MAX) ? max_turn_global : min_t;
    }

    void update_tree(State& state, BeamParam& param, const int turn) {
        // 世代開始時の min target_turn (終了時との差を 1 世代の進行量として記録する)
        const int prev_min_target = min_target_in_tree;
        nxt_tree.clear();
        if (turn == 0) {
            for (int i = 0; i < new_candidates.size(); ++i) {
                const auto &[score, hash, par, aid, t_turn] = new_candidates[i];
                if (is_survived_node[aid]) {
                    nxt_tree.emplace_back(0, aid, t_turn);
                }
            }
            swap(tree, nxt_tree);
            update_min_target_in_tree();
            mark_and_sweep();
            int delta = min_target_in_tree - prev_min_target;
            if (delta > 0) param.note_target_step(delta);
            return;
        }

        // root 直下が一本道の間、その action を result に確定して state に apply する
        // (ここで apply した分は以後 rollback しない)
        int i = 0;
        while (i < tree.size()) {
            int dir_or_leaf_id = tree[i].dir_or_leaf_id;
            if (dir_or_leaf_id == PRE_ORDER && i + 1 < tree.size() && tree[i].aid == tree.back().aid) {
                result.emplace_back(act(tree[i].aid));
                state.apply_op(act(tree[i].aid));
                tree.pop_back();
                ++i;
            } else {
                break;
            }
        }

        // subtree_min (PRE_ORDER の target_turn) は遅延更新する:
        //   PRE_ORDER をそのままコピーするときは旧 subtree_min を入れ、フラグを立てずに push する
        //   再計算フラグを立てるのは、親の subtree_min に影響しうる次の変更があったときだけ:
        //     - subtree_min に寄与していた leaf の追い出し / 展開 / 空になった部分木の除去
        //     - 新しい subtree_min が親の値を下回る
        //   POST_ORDER を処理するとき、フラグの立った部分木だけ直接の子を線形 scan して再計算する
        //   再計算結果が旧値と同じなら親への伝播もスキップする
        pre_stack.clear();
        nxt_tree.reserve(tree.size() + new_candidates.size());

        int next_beam_idx = 0;
        const int num_candidates = new_candidates.size();
        const int tree_size = tree.size();
        for (; i < tree_size; ++i) {
            TreeNode& src = tree[i];
            const int dir_or_leaf_id = src.dir_or_leaf_id;
            if (dir_or_leaf_id >= 0) {
                if (src.target_turn == turn) {
                    // 展開済み leaf: 先に PRE_ORDER を emit しておき、生存子を直接 emit する
                    // (生存子が 0 件なら取り消す)
                    int pre_idx = nxt_tree.size();
                    nxt_tree.emplace_back(PRE_ORDER, src.aid, INT_MAX);
                    int subtree_min = INT_MAX;
                    int emit_cnt = 0;
                    while (next_beam_idx < num_candidates
                           && new_candidates[next_beam_idx].par == dir_or_leaf_id) {
                        const auto& nc = new_candidates[next_beam_idx];
                        if (is_survived_node[nc.aid]) {
                            nxt_tree.emplace_back(dir_or_leaf_id, nc.aid, nc.target_turn);
                            if (nc.target_turn < subtree_min) subtree_min = nc.target_turn;
                            ++emit_cnt;
                        }
                        ++next_beam_idx;
                    }
                    if (emit_cnt > 0) {
                        nxt_tree.emplace_back(POST_ORDER, src.aid, 0);
                        nxt_tree[pre_idx].target_turn = subtree_min;
                        nxt_tree[pre_idx].subtree_end = nxt_tree.size();
                    } else {
                        nxt_tree.pop_back();
                    }
                    // 旧 leaf が親の subtree_min に寄与していた、
                    // または新 subtree_min が親の値を下回るときだけ親の再計算フラグを立てる
                    if (!pre_stack.empty()) {
                        auto& top = pre_stack.back();
                        int gp_min = nxt_tree[top.first].target_turn;
                        if (src.target_turn == gp_min || (emit_cnt > 0 && subtree_min < gp_min)) {
                            top.second = true;
                        }
                    }
                } else {
                    if (is_survived_node[src.aid]) {
                        nxt_tree.emplace_back(dir_or_leaf_id, src.aid, src.target_turn);
                    } else {
                        // ビームから追い出された leaf は捨て、親の subtree_min に寄与していたときだけ再計算フラグを立てる
                        if (!pre_stack.empty()) {
                            auto& top = pre_stack.back();
                            if (src.target_turn == nxt_tree[top.first].target_turn) {
                                top.second = true;
                            }
                        }
                    }
                }
            } else if (dir_or_leaf_id == PRE_ORDER) {
                // src.target_turn は前世代の subtree_min で、変化が無ければそのまま使える
                int pre_idx = nxt_tree.size();
                nxt_tree.emplace_back(PRE_ORDER, src.aid, src.target_turn);
                pre_stack.push_back({pre_idx, false});
            } else {
                if (!nxt_tree.empty()
                    && nxt_tree.back().dir_or_leaf_id == PRE_ORDER
                    && nxt_tree.back().aid == src.aid) {
                    // 空になった部分木: PRE_ORDER を取り消し、POST_ORDER も emit しない
                    int popped_min = nxt_tree.back().target_turn;
                    nxt_tree.pop_back();
                    pre_stack.pop_back();
                    // 消えた部分木が親の subtree_min に寄与していたときだけ再計算フラグを立てる
                    if (!pre_stack.empty()) {
                        auto& top = pre_stack.back();
                        if (popped_min == nxt_tree[top.first].target_turn) {
                            top.second = true;
                        }
                    }
                } else {
                    int pre_idx = pre_stack.back().first;
                    bool need_recalc = pre_stack.back().second;
                    int old_min = nxt_tree[pre_idx].target_turn;
                    nxt_tree.emplace_back(POST_ORDER, src.aid, 0);
                    nxt_tree[pre_idx].subtree_end = nxt_tree.size();
                    pre_stack.pop_back();
                    if (need_recalc) {
                        // 直接の子だけ線形 scan して subtree_min を再計算する
                        // (孫以下は subtree_end で skip するので O(直接の子数))
                        int min_t = INT_MAX;
                        int k = pre_idx + 1;
                        const int end_excl = nxt_tree.size() - 1; // POST_ORDER の直前まで
                        while (k < end_excl) {
                            const TreeNode& child = nxt_tree[k];
                            int t = child.target_turn;
                            if (t < min_t) min_t = t;
                            if (child.dir_or_leaf_id == PRE_ORDER) {
                                k = child.subtree_end;
                            } else {
                                ++k;
                            }
                        }
                        nxt_tree[pre_idx].target_turn = min_t;
                        // 旧値から変わり、かつ親の subtree_min に影響しうるときだけ伝播する
                        if (min_t != old_min && !pre_stack.empty()) {
                            auto& top = pre_stack.back();
                            int gp_min = nxt_tree[top.first].target_turn;
                            if (old_min == gp_min || min_t < gp_min) {
                                top.second = true;
                            }
                        }
                    }
                }
            }
        }

        swap(tree, nxt_tree);
        update_min_target_in_tree();
        mark_and_sweep();
        int delta = min_target_in_tree - prev_min_target;
        if (delta > 0) param.note_target_step(delta);
    }

    void get_result() {
        ActionId target_aid = BAD_ID;
        Action best_action;

        if (found_finished) {
            target_aid = best_finished_par_aid;
            best_action = best_finished_action;
        } else {
            // target_turn の小さい pool から順に、生存中の最良候補を探す
            ScoreType best_score = INF;
            for (int t = 0; t <= max_turn_global; ++t) {
                if (turn_to_pool_idx[t] == -1) continue;
                Candidates& cands = cands_pool[turn_to_pool_idx[t]];
                for (int i = 0; i < cands.size(); ++i) {
                    const auto &[score, hash, par, aid, t_turn] = cands.next_beam[i];
                    if (is_survived_node[aid]) {
                        if (target_aid == BAD_ID || score < best_score) {
                            best_score = score;
                            target_aid = aid;
                            best_action = act(aid);
                        }
                    }
                }
                if (target_aid != BAD_ID) break;
            }
        }

        if (target_aid == BAD_ID) {
             if (found_finished) {
                 result.emplace_back(best_action);
             }
             return;
        }

        // Euler tour を歩き、PRE で push / POST で pop しながら target までの経路を復元する
        for (const auto& node : tree) {
            int dir_or_leaf_id = node.dir_or_leaf_id;
            const Action& action = act(node.aid);
            if (dir_or_leaf_id >= 0) {
                if (node.aid == target_aid) {
                    result.emplace_back(action);
                    if (found_finished) {
                        result.emplace_back(best_action);
                    }
                    return;
                }
            } else if (dir_or_leaf_id == PRE_ORDER) {
                result.emplace_back(action);
            } else if (dir_or_leaf_id == POST_ORDER) {
                result.pop_back();
            }
        }
    }

    void init_bs(BeamParam &param) {
        beam_timer.reset();
        node_id_counter = 0;
        history.clear();
        snapshots.clear();
        result.clear();
        tree.clear();
        nxt_tree.clear();
        new_candidates.clear();
        found_finished = false;
        best_finished_score = INF;
        best_finished_par_aid = BAD_ID;
        max_turn_global = param.max_turn;
        DUMMY_ACTION.target_turn = -1;

        action_pool.clear();
        free_slots.clear();
        alive_gen.clear();
        now_gen = 0;
        if constexpr (record_history) aid_to_node_id.clear();

        turn_to_pool_idx.assign(max_turn_global + 1, -1);

        free_pool_indices.clear();
        for (int i = 0; i < cands_pool.size(); ++i) {
            free_pool_indices.push_back(i);
        }

        thresholds.assign(max_turn_global + 1, INF);
        is_survived_node.clear();

        // target_step 統計は毎 search でリセット
        param.target_step_sum = 0;
        param.target_step_count = 0;

        min_target_in_tree = 0;
        expanded_leaf_count = 0;

        // global seen_hash は clear_hash_every_turn==false のときだけ有効化
        use_global_seen = !param.clear_hash_every_turn;
        if (use_global_seen) {
            int cap = param.seen_hash_capacity_hint;
            if (cap <= 0) {
                cap = max(1 << 14, param.beam_width * max(1, max_turn_global) * 2);
            }
            seen_hash = titan23::HashDict<pair<ScoreType, int>>(cap);
        }
    }

public:
    vector<Action> search(BeamParam &param, const bool verbose=false, const string& history_file = "") {
        init_bs(param);
        if (verbose) {
            beam_log::start_banner(cerr, "BeamSearchWithTree (multi-turn)", param);
            if (param.is_adjusting) beam_log::warn(cerr, "dynamic beam width is experimental");
        }
        State state;
        state.init();

        int turns_done = 0;
        for (int turn = 0; turn < param.max_turn; ++turn) {
            double now_time = beam_timer.elapsed();

            get_next_beam(state, param, turn);
            // dt_expand: get_next_beam の所要時間 (W に比例する成分)
            // verbose / record_history のオーバーヘッドを含めないようここで時刻を取る
            double dt_expand_ms = beam_timer.elapsed() - now_time;

            if (found_finished) {
                turns_done = turn + 1;
                get_result();
                if constexpr (record_history) dump_history_json(history_file);
                if (verbose) {
                    beam_log::on_solution_found(cerr, turns_done, best_finished_score);
                    beam_log::end_banner(cerr, "solution found", turns_done, param.max_turn, beam_timer.elapsed(), param.ave_width(), best_finished_score, true, result.size());
                }
                return result;
            }

            if (verbose) {
                // best と w は、最初に候補が見つかった pool (target_turn が最小の非空 pool) のものを出す
                ScoreType best_for_log = 0;
                bool has_best = false;
                int w = param.beam_width;
                for (int t = turn + 1; t <= max_turn_global; ++t) {
                    if (turn_to_pool_idx[t] == -1) continue;
                    Candidates& cands = cands_pool[turn_to_pool_idx[t]];
                    if (!has_best) w = cands.get_width();
                    for (int i = 0; i < cands.size(); ++i) {
                        ScoreType s = cands.next_beam[i].score;
                        if (!has_best || s < best_for_log) { best_for_log = s; has_best = true; }
                    }
                    if (has_best) break;
                }
                beam_log::turn_line(cerr, turn + 1, param.max_turn, now_time,
                                    w, tree.size(), new_candidates.size(),
                                    -1, best_for_log);
                if (!has_best) {
                    beam_log::turn_line_extra(cerr, "(no candidates at this turn yet)");
                }
            }

            if constexpr (record_history) {
                vector<int> active_ids;
                for (int t = turn; t <= max_turn_global; ++t) {
                    if (turn_to_pool_idx[t] == -1) continue;
                    Candidates& cands = cands_pool[turn_to_pool_idx[t]];
                    for (int i = 0; i < cands.size(); ++i) {
                        ActionId aid = cands.next_beam[i].aid;
                        if (is_survived_node[aid]) {
                            int node_id = (aid < aid_to_node_id.size()) ? aid_to_node_id[aid] : -1;
                            if (node_id >= 0) active_ids.push_back(node_id);
                        }
                    }
                }
                snapshots.push_back({turn + 1, active_ids});
            }

            // dt_update: sort + update_tree の所要時間 (tree サイズに比例する成分)
            // verbose / record_history のオーバーヘッドを除くためここから時刻を取り直す
            double t_update_start = beam_timer.elapsed();
            if (turn != 0) {
                // update_tree が leaf_id 順に消費するので par を第一キーにする
                sort(new_candidates.begin(), new_candidates.end(), [] (const auto& a, const auto& b) { if (a.par != b.par) return a.par < b.par; return a.score < b.score; });
            } else {
                sort(new_candidates.begin(), new_candidates.end(), [] (const auto& a, const auto& b) { return a.score < b.score; });
            }
            int prev_min_target = min_target_in_tree;
            update_tree(state, param, turn);
            int delta_target = min_target_in_tree - prev_min_target;
            if (delta_target < 0) delta_target = 0;
            double dt_update_ms = beam_timer.elapsed() - t_update_start;

            // このターンの pool は以降使わないので解放する
            if (turn_to_pool_idx[turn] != -1) {
                free_pool_indices.push_back(turn_to_pool_idx[turn]);
                turn_to_pool_idx[turn] = -1;
                thresholds[turn] = INF;
            }

            param.timestamp_meta(dt_expand_ms, dt_update_ms, tree.size(), new_candidates.size(), expanded_leaf_count, delta_target);
            turns_done = turn + 1;
        }

        get_result();
        if constexpr (record_history) dump_history_json(history_file);
        if (verbose) {
            beam_log::on_max_turn(cerr);
            beam_log::end_banner(cerr, "max_turn reached", turns_done, param.max_turn, beam_timer.elapsed(), param.ave_width(), (ScoreType)0, false, result.size());
        }
        return result;
    }
};
} // namespace flying_squirrel

constexpr const int N = 10;
constexpr const int MAX_S = 15;
constexpr const int MAX_T = 20;
int R;
vector<vector<int>> Y;

void input() {
    cin >> R;
    Y.resize(R, vector<int>(N));
    rep(i, R) rep(j, N) cin >> Y[i][j];
}

namespace beam_search {

using ScoreType = long long;
using HashType = unsigned long long;
const ScoreType INF = 1e18;
titan23::Random brnd;

// zhs_s[v][i][j]:=値vが位置(i,j)にあるハッシュ
HashType zhs_s[N*N][N][MAX_S];
HashType zhs_t[N*N][N][MAX_T];
HashType hash_AC;

void beam_init() {
    rep(i, N*N) rep(j, N) rep(k, MAX_S) zhs_s[i][j][k] = brnd.rand_u64();
    rep(i, N*N) rep(j, N) rep(k, MAX_T) zhs_t[i][j][k] = brnd.rand_u64();
    hash_AC = 0;
    rep(i, N*N) {
        hash_AC ^= zhs_s[i][i/10][i%10];
    }
}

struct Action {
    ScoreType pre_score, nxt_score;
    HashType pre_hash, nxt_hash;
    int target_turn;
    int pre_turn, pre_s, pre_t;
    int nxt_turn, nxt_s, nxt_t;
    int si, ti;
    int scnt, tcnt;

    Action() : pre_score(INF), nxt_score(INF), pre_hash(0), nxt_hash(0), pre_turn(0), target_turn(-1) {}
    Action(int si, int ti, int scnt, int tcnt) : si(si), ti(ti), scnt(scnt), tcnt(tcnt) {}
    friend ostream& operator<<(ostream& os, const Action &action) {
        return os;
    }

    string to_string() const {
        return "";
    }
};

struct SArray {
    int sz;
    int data[MAX_S];

    SArray() : sz(0) {}

    SArray(const SArray& other) : sz(other.sz) {
        for (int i = 0; i < sz; ++i) data[i] = other.data[i];
    }
    SArray& operator=(const SArray& other) {
        if (this != &other) {
            sz = other.sz;
            for (int i = 0; i < sz; ++i) data[i] = other.data[i];
        }
        return *this;
    }

    void clear() { sz = 0; }
    void push_back(int v) { data[sz++] = v; }
    void pop_back() { --sz; }
    int back() const { return data[sz - 1]; }
    int size() const { return sz; }
    bool empty() const { return sz == 0; }
    int operator[](int i) const { return data[i]; }
};

struct TArray {
    int head;
    int data[MAX_T];

    TArray() : head(MAX_T) {}

    TArray(const TArray& other) : head(other.head) {
        for (int i = head; i < MAX_T; ++i) data[i] = other.data[i];
    }
    TArray& operator=(const TArray& other) {
        if (this != &other) {
            head = other.head;
            for (int i = head; i < MAX_T; ++i) data[i] = other.data[i];
        }
        return *this;
    }

    void clear() { head = MAX_T; }
    void push_front(int v) { data[--head] = v; }
    void pop_front() { ++head; }
    int front() const { return data[head]; }
    int size() const { return MAX_T - head; }
    bool empty() const { return head == MAX_T; }
    int operator[](int i) const { return data[head + i]; }
};

class State {
private:
    ScoreType score;
    HashType hash;
    int turn;
    int s, t;
    array<SArray, N> S;
    array<TArray, N> T;

public:
    void init() {
        turn = 0;
        s = 0;
        t = 0;
        rep(i, N) {
            S[i].clear();
            T[i].clear();
        }
        rep(i, N) rep(j, MAX_S) {
            if (j < N) S[i].push_back(Y[i][j]);
        }
        score = 0;
        rep(i, N) score += calc_score_S(i, S[i]);
        hash = 0;
        rep(i, N) hash ^= calc_hash_S(i, S[i]);
        rep(i, N) hash ^= calc_hash_T(i, T[i]);
    }

    bool is_consecutive(int x, int y) const {
        if (x % 10 == 9) return false;
        if (y != x+1) return false;
        return true;
    }

    HashType calc_hash_S(int c, const SArray &q) const {
        HashType h = 0;
        rep(k, q.size()) h ^= zhs_s[q[k]][c][k];
        return h;
    }

    HashType calc_hash_T(int c, const TArray &q) const {
        HashType h = 0;
        rep(k, q.size()) h ^= zhs_t[q[k]][c][k];
        return h;
    }

    ScoreType calc_score_S(int c, const SArray &q) const {
        ScoreType s = 0;
        rep(i, q.size()) {
            if (i == 0) {
                if (q[i] == c*10) s -= 100;
            } else {
                if (is_consecutive(q[i-1], q[i])) s -= 100;
            }
            s += abs(c - q[i]/10);
        }
        return s;
    }

    ScoreType calc_score_T(int c, const TArray &q) const {
        ScoreType s = 0;
        rep(i, q.size()) {
            if (i && is_consecutive(q[i-1], q[i])) s -= 100;
            s += abs(c - q[i]/10);
        }
        return s;
    }

    ScoreType pair_score(int i, const SArray &sq, int j, const TArray &tq) const {
        if (sq.empty() || tq.empty()) return 0;
        if (is_consecutive(sq.back(), tq.front())) return -50 + abs(i-j)*abs(i-j);
        return 0;
    }

    tuple<ScoreType, HashType, bool> try_op(Action &action, const vector<ScoreType>& thresholds) const {
        action.pre_score = score;
        action.pre_hash = hash;
        action.pre_turn = turn;
        ScoreType nxt_score = score;
        HashType nxt_hash = hash;

        action.pre_turn = turn;
        action.pre_s = s;
        action.pre_t = t;
        int i = action.si;
        int j = action.ti;
        if (i == -1 || j == -1) {
            action.nxt_turn = turn + 1;
            action.nxt_s = 0;
            action.nxt_t = 0;
            action.target_turn = (turn+1)*N*N;
            action.nxt_score = nxt_score;
            action.nxt_hash = nxt_hash;
            return {nxt_score, nxt_hash, hash == hash_AC};
        }

        action.nxt_turn = turn;
        action.nxt_s = i + 1;
        action.nxt_t = j + 1;
        if (action.nxt_s == N || action.nxt_t == N) {
            action.nxt_turn = turn + 1;
            action.nxt_s = 0;
            action.nxt_t = 0;
        }
        action.target_turn = (action.nxt_turn)*N*N + (action.nxt_s)*N + (action.nxt_t);
        if (nxt_score - 100*2 >= thresholds[action.target_turn]) return {INF, 0, 0};

        SArray ns = S[i];
        TArray nt = T[j];
        if (action.scnt != -1) {
            rep(_, action.scnt) {
                int v = ns.back();
                ns.pop_back();
                nt.push_front(v);
            }
        }
        if (action.tcnt != -1) {
            rep(_, action.tcnt) {
                int v = nt.front();
                nt.pop_front();
                ns.push_back(v);
            }
        }

        nxt_score -= calc_score_S(i, S[i]) + calc_score_T(j, T[j]);
        nxt_score += calc_score_S(i, ns) + calc_score_T(j, nt);
        rep(tc, N) {
            if (tc == j) continue;
            nxt_score -= pair_score(i, S[i], tc, T[tc]);
            nxt_score += pair_score(i, ns, tc, T[tc]);
        }
        rep(sc, N) {
            if (sc == i) continue;
            nxt_score -= pair_score(sc, S[sc], j, T[j]);
            nxt_score += pair_score(sc, S[sc], j, nt);
        }
        nxt_score -= pair_score(i, S[i], j, T[j]);
        nxt_score += pair_score(i, ns, j, nt);
        if (nxt_score >= thresholds[action.target_turn]) return {INF, 0, 0};

        nxt_hash ^= calc_hash_S(i, S[i]) ^ calc_hash_T(j, T[j]);
        nxt_hash ^= calc_hash_S(i, ns) ^ calc_hash_T(j, nt);

        action.nxt_score = nxt_score;
        action.nxt_hash = nxt_hash;
        return {nxt_score, nxt_hash, nxt_hash == hash_AC};
    }

    void apply_op(const Action &action) {
        int i = action.si;
        int j = action.ti;
        if (action.si != -1 && action.ti != -1) {
            if (action.scnt != -1) {
                rep(_, action.scnt) {
                    int v = S[i].back();
                    S[i].pop_back();
                    T[j].push_front(v);
                }
            }
            if (action.tcnt != -1) {
                rep(_, action.tcnt) {
                    int v = T[j].front();
                    T[j].pop_front();
                    S[i].push_back(v);
                }
            }
        }

        score = action.nxt_score;
        hash = action.nxt_hash;
        turn = action.nxt_turn;
        s = action.nxt_s;
        t = action.nxt_t;
    }

    void rollback(const Action &action) {
        int i = action.si;
        int j = action.ti;
        if (action.si != -1 && action.ti != -1) {
            if (action.scnt != -1) {
                rep(_, action.scnt) {
                    int v = T[j].front();
                    T[j].pop_front();
                    S[i].push_back(v);
                }
            }
            if (action.tcnt != -1) {
                rep(_, action.tcnt) {
                    int v = S[i].back();
                    S[i].pop_back();
                    T[j].push_front(v);
                }
            }
        }
        score = action.pre_score;
        hash = action.pre_hash;
        turn = action.pre_turn;
        s = action.pre_s;
        t = action.pre_t;
    }

    template<typename Emit>
    void get_actions(const Action &last_action, Emit &&emit) const {
        // legacy 版の actions.size() による確率的 thinning を、emit 数カウンタ
        // emitted_cnt で再現する。emit(a) は try_op + 各種判定を内部で行うため
        // 棄却の有無に関わらずカウンタは進める (legacy も push_back 後に size 増)。
        int emitted_cnt = 0;
        Action a;
        for (int i = s; i < N; ++i) {
            bool done = true;
            if (S[i].size() != N) done = false;
            int done_cnt = 0;
            if (done) {
                rep(j, N) {
                    if (S[i][j] != i*N+j) {
                        done = false;
                        break;
                    }
                    done_cnt++;
                }
            }
            if (done) continue;

            for (int j = t; j < N; ++j) {
                const int n = S[i].size();
                const int m = T[j].size();
                for (int p = 1; p <= n; ++p) {
                    if (m+p > MAX_T) break;
                    if (p < n && is_consecutive(S[i][n-p-1], S[i][n-p])) continue;
                    if (n-p < done_cnt) break;
                    if (n > 0 && m > 0 && is_consecutive(S[i].back(), T[j].front())) {
                        a = Action(i, j, p, -1); emit(a); ++emitted_cnt;
                        continue;
                    }
                    if (emitted_cnt > 10 && brnd.randint(100) < 90) continue;
                    a = Action(i, j, p, -1); emit(a); ++emitted_cnt;
                }

                for (int q = 1; q <= m; ++q) {
                    if (n+q > MAX_S) break;
                    if (q < m && is_consecutive(T[j][q-1], T[j][q])) continue;
                    if (n > 0 && m > 0 && is_consecutive(S[i].back(), T[j].front())) {
                        a = Action(i, j, -1, q); emit(a); ++emitted_cnt;
                        continue;
                    }
                    if (emitted_cnt > 10 && brnd.randint(100) < 80) continue;
                    a = Action(i, j, -1, q); emit(a); ++emitted_cnt;
                }
            }
        }
        a = Action(-1, -1, -1, -1); emit(a);
    }

    void print() const {}

    string get_state_info() const {
        return "{}";
    }
};

/// @brief BeamParamを返す
/// @param max_turn 最大探索ターン
/// @param beam_width ビーム幅
/// @return BeamParam
flying_squirrel::BeamParam gen_param(int max_turn, int beam_width) {
    return {max_turn, beam_width, -1};
}

/// @brief BeamParamを返す
/// @param max_turn 最大探索ターン
/// @param beam_width ビーム幅
/// @param time_limit 制限時間
/// @param is_adjusting 制限時間に合わせるかどうか
/// @param clear_hash_every_turn ハッシュ辞書を毎ターンclearするかどうか
/// @return BeamParam
flying_squirrel::BeamParam gen_param(int max_turn, int beam_width, double time_limit, bool is_adjusting=false, bool clear_hash_every_turn=true) {
    return {max_turn, beam_width, time_limit, is_adjusting, clear_hash_every_turn};
}

vector<Action> search(flying_squirrel::BeamParam &param, const bool verbose=false) {
    flying_squirrel::BeamSearchWithTree<ScoreType, HashType, Action, State, INF, false> bs;
    return bs.search(param, verbose, "history.json");
}
} // namespace beam_search

struct S {
    int type, i, j, k;
};

void solve() {
    beam_search::beam_init();
    auto param = beam_search::gen_param(40*100, 100, 1900, false, true);
    auto result = beam_search::search(param, true);
    cerr << "resulit.size()=" << result.size() << endl;
    vector<vector<S>> ans;
    int pre_turn = -1;
    for (auto &res : result) {
        if (res.pre_turn != pre_turn) {
            ans.push_back({});
        }
        pre_turn = res.pre_turn;
        if (res.si != -1 && res.ti != -1) {
            int type = res.scnt == -1 ? 1 : 0;
            int k = res.scnt == -1 ? res.tcnt : res.scnt;
            ans.back().push_back({type, res.si, res.ti, k});
        }
    }
    vector<vector<S>> final_ans;
    for (auto &turn_actions : ans) {
        if (!turn_actions.empty()) {
            final_ans.push_back(turn_actions);
        }
    }
    cout << final_ans.size() << "\n";
    for (auto &res : final_ans) {
        cout << res.size() << "\n";
        for (auto &s : res) {
            cout << s.type << " " << s.i << " " << s.j << " " << s.k << "\n";
        }
    }

    cerr << "Score = " << final_ans.size() << endl;
}

int main(int argc, char* argv[]) {
    ios::sync_with_stdio(false);
    cin.tie(0);
    cout << fixed << setprecision(3);
    cerr << fixed << setprecision(3);

    input();
    solve();

    return 0;
}
