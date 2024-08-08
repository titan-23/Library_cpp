#include <cassert>
#include <unordered_set>
#include <vector>
using namespace std;

// Random
namespace titan23 {

    /**
     * @brief (疑似)乱数生成クラス
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

        //! `[begin, end)` の乱数を返す(実数)
        double randdouble(const double begin, const double end) {
            assert(begin < end);
            return begin + random() * (end-begin);
        }

        //! `vector` をインプレースにシャッフルする / `O(n)`
        template <typename T>
        void shuffle(vector<T> &a) {
            int n = (int)a.size();
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
