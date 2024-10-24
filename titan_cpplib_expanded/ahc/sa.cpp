// #include "titan_cpplib/ahc/sa.cpp"
#include <vector>
#include <cmath>
// #include "titan_cpplib/ahc/timer.cpp"
#include <iostream>
#include <chrono>
#include <thread>

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
            auto end_timepoint = chrono::high_resolution_clock::now();
            auto start = chrono::time_point_cast<chrono::microseconds>(start_timepoint).time_since_epoch().count();
            auto end = chrono::time_point_cast<chrono::microseconds>(end_timepoint).time_since_epoch().count();
            return (end - start) * 0.001;
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
        unsigned int _x, _y, _z, _w;

        unsigned int _xor128() {
            const unsigned int t = _x ^ (_x << 11);
            _x = _y;
            _y = _z;
            _z = _w;
            _w = (_w ^ (_w >> 19)) ^ (t ^ (t >> 8));
            return _w;
        }

      public:
        Random() : _x(123456789),
                   _y(362436069),
                   _z(521288629),
                   _w(88675123) {}

        //! `[0, 1]` の乱数を返す(実数)
        double random() { return (double)(_xor128()) / 0xFFFFFFFF; }

        //! `[0, end]` の乱数を返す(整数)
        int randint(const int end) {
            assert(0 <= end);
            return (((unsigned long long)_xor128() * (end+1)) >> 32);
        }

        //! `[begin, end]` の乱数を返す(整数)
        int randint(const int begin, const int end) {
            assert(begin <= end);
            return begin + (((unsigned long long)_xor128() * (end-begin+1)) >> 32);
        }

        //! `[0, end)` の乱数を返す(整数)
        int randrange(const int end) {
            assert(0 < end);
            return (((unsigned long long)_xor128() * end) >> 32);
        }

        //! `[begin, end)` の乱数を返す(整数)
        int randrange(const int begin, const int end) {
            assert(begin < end);
            return begin + (((unsigned long long)_xor128() * (end-begin)) >> 32);
        }

        //! `[0, u64_MAX)` の乱数を返す / zobrist hash等の使用を想定
        unsigned long long rand_u64() {
            return ((unsigned long long)_xor128() << 32) | _xor128();
        }

        //! `[0, end)` の異なる乱数を2つ返す
        pair<int, int> rand_pair(const int end) {
            assert(end >= 2);
            int u = randrange(end);
            int v = u + randrange(1, end);
            if (v >= end) v -= end;
            if (u > v) swap(u, v);
            return {u, v};
        }

        //! `[begin, end)` の異なる乱数を2つ返す
        pair<int, int> rand_pair(const int begin, const int end) {
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
        double randdouble(const double begin, const double end) {
            assert(begin < end);
            return begin + random() * (end-begin);
        }

        //! `vector` をインプレースにシャッフルする / `O(n)`
        template <typename T>
        void shuffle(vector<T> &a) {
            int n = a.size();
            for (int i = 0; i < n-1; ++i) {
                int j = randrange(i, n);
                swap(a[i], a[j]);
            }
        }

        template <typename T>
        vector<T> choices(const vector<T> &a, const int k) {
            assert(a.size() >= k);
            vector<T> r(k);
            unordered_set<int> seen;
            for (int i = 0; i < k; ++i) {
                int x = randrange(a.size());
                while (seen.find(x) != seen.end()) x = randrange(a.size());
                seen.insert(x);
                r[i] = a[x];
            }
            return r;
        }

        template <typename T>
        T choice(const vector<T> &a) {
            return a[randrange(a.size())];
        }

        template <typename T>
        T choice(const vector<T> &a, const vector<int> &w, bool normal) {
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
        T choice(const vector<T> &a, const vector<double> &w) {
            double i = random();
            int l = -1, r = a.size()-1;
            while (r - l > 1) {
                int mid = (l + r) / 2;
                if (w[mid] <= i) l = mid;
                else r = mid;
            }
            return a[r];
        }
    };
} // namespace titan23

// #include "titan_cpplib/others/print.cpp"
#include <iostream>
#include <vector>
#include <set>
#include <unordered_set>
#include <map>
using namespace std;

// print

// color
static const string PRINT_RED = "\033[31m";
static const string PRINT_GREEN = "\033[32m";
static const string PRINT_NONE = "\033[m";

// pair<K, V>
template <typename K, typename V>
ostream& operator<<(ostream& os, const pair<K, V>& p) {
    os << "(" << p.first << ", " << p.second << ")";
    return os;
}

// tuple<T1, T2, T3>
template<typename T1, typename T2, typename T3>
ostream &operator<<(ostream &os, const tuple<T1, T2, T3> &t) {
    os << "( " << get<0>(t) << ", " << get<1>(t) << ", " << get<2>(t) << " )";
    return os;
}

// vector<T>
template <typename T>
ostream& operator<<(ostream& os, const vector<T>& a) {
    int n = (int)a.size();
    os << "[";
    for (int i = 0; i < n-1; ++i) {
        os << a[i] << ", ";
    }
    if (n > 0) {
        os << a.back();
    }
    os << "]";
    return os;
}

// vector<vector<T>>
template <typename T>
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
template <typename T>
ostream& operator<<(ostream& os, const set<T>& a) {
    os << "{";
    for (const T &x: a) {
        os << x;
        if (x != *(--a.end())) {
            os << ", ";
        }
    }
    os << "}";
    return os;
}

// unordered_set<T>
template <typename T>
ostream& operator<<(ostream& os, const unordered_set<T>& a) {
    set<T> s;
    for (const T &x : a) {
        s.insert(x);
    }
    os << s;
    return os;
}

// map<K, V>
template <typename K, typename V>
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


using namespace std;

// sa 最小化
namespace titan23 {

namespace sa {

    struct Param {
        double start_temp, end_temp;
    };

    Param param;
    titan23::Random sa_random;
    using ScoreType = double;

    struct Changed {
        int type;
        ScoreType pre_score;
        Changed() {}
    };

    Changed changed;

    class State {
      public:
        ScoreType score;
        State() {}

        void init() {}

        ScoreType get_score() const { return score; }
        ScoreType get_true_score() const { return score; }

        // thresholdを超えたら、だめ
        void modify(const ScoreType threshold) {}

        void rollback() {}

        void advance() {}

        void print() const {}
    };

    // TIME_LIMIT: ms
    State sa_run(const double TIME_LIMIT, const bool verbose = false) {
        titan23::Timer sa_timer;

        // const double START_TEMP = param.start_temp;
        // const double END_TEMP   = param.end_temp;
        const double START_TEMP = 1000;
        const double END_TEMP   = 1;
        const double TEMP_VAL = (START_TEMP - END_TEMP) / TIME_LIMIT;

        State ans;
        ans.init();
        State best_ans = ans;
        ScoreType score = ans.get_score();
        ScoreType best_score = score;
        double now_time;

        int cnt = 0;
        int upd_cnt = 0;
        while (true) {
            // if ((cnt & 31) == 0) now_time = sa_timer.elapsed();
            now_time = sa_timer.elapsed();
            if (now_time > TIME_LIMIT) break;
            ++cnt;
            ScoreType threshold = score - (START_TEMP-TEMP_VAL*now_time) * log(sa_random.random());
            changed.pre_score = ans.score;
            ans.modify(threshold);
            ScoreType new_score = ans.get_score();
            if (new_score <= threshold) {
                ++upd_cnt;
                ans.advance();
                score = new_score;
                if (score < best_score) {
                    best_score = score;
                    best_ans = ans;
                    if (verbose) {
                        cerr << "Info: score=" << best_score << endl;
                    }
                }
            } else {
                ans.score = changed.pre_score;
                ans.rollback();
            }
        }
        if (verbose) {
            cerr << "Info: upd=" << upd_cnt << endl;
            cerr << "Info: cnt=" << cnt << endl;
        }
        return best_ans;
    }
}
}  // namespace titan23

