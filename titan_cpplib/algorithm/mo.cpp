#include <vector>
#include <cassert>
using namespace std;

// Mo
namespace titan23 {

    /**
     * @brief Mo's algorithm
     *
     * @ref https://take44444.github.io/Algorithm-Book/range/mo/main.html
     */
    class Mo {

      private:
        int n, query_count;
        long long max_n;
        vector<int> _l, _r;

        long long hilbertorder(int x, int y) const {
            long long rx, ry, d = 0;
            for (long long s = max_n>>1ll; s; s >>= 1ll) {
                rx = (x & s) > 0, ry = (y & s) > 0;
                d += s * s * ((rx * 3) ^ ry);
                if (ry) continue;
                if (rx) {
                    x = max_n - 1 - x;
                    y = max_n - 1 - y;
                }
                swap(x, y);
            }
            return d;
        }

      public:
        Mo() {}

        //! 長さ `n` の列に対する Mo's algorithm インスタンスを生成 / `O(1)`
        Mo(const int n) {
            this->max_n = 1 << 25;
            this->query_count = 0;
            this->n = n;
            assert(this->n <= this->max_n);
        }

        //! クエリ `[l, r)` を追加する / `O(1)`
        void add_query(const int l, const int r) {
            assert(0 <= l && l <= r && r <= n);
            _l.emplace_back(l);
            _r.emplace_back(r);
            ++query_count;
        }

        //! 追加されたクエリを一括処理する / `O(q√n)`
        // F1~F5: lambda関数
        template<typename F1, typename F2, typename F3, typename F4, typename F5>
        void run(F1 &&add_left, F2 &&add_right, F3 &&del_left, F4 &&del_right, F5 &&out) {
            vector<int> qi(query_count);
            iota(qi.begin(), qi.end(), 0);
            vector<long long> eval(query_count);
            for (int i = 0; i < query_count; ++i) {
                eval[i] = hilbertorder(_l[i], _r[i]);
            }
            sort(qi.begin(), qi.end(), [&] (const int &i, const int &j) {
                return eval[i] < eval[j];
            });
            int nl = 0, nr = 0;
            for (const int &i : qi) {
                const int li = _l[i], ri = _r[i];
                while (nl > li) add_left(--nl);
                while (nr < ri) add_right(nr++);
                while (nl < li) del_left(nl++);
                while (nr > ri) del_right(--nr);
                out(i);
            }
        }
    };
}  // namespace titan23
