#include <iostream>
#include <vector>
using namespace std;

// CumulativeSum
namespace titan23 {
    template<typename T>
    class CumulativeSum {
      private:
        int n;
        vector<T> acc;

      public:
        CumulativeSum() {}
        CumulativeSum(vector<T> &a, T e) : n((int)a.size()), acc(n+1, e) {
            for (int i = 0; i < n; ++i) {
                acc[i+1] = acc[i] + a[i];
            }
        }

        T pref(const int r) const {
            assert(0 <= r && r <= n);
            return acc[r];
        }

        T all_sum() const {
            return acc.back();
        }

        T sum(const int l, const int r) const {
            assert(0 <= l && l <= r && r <= n);
            return acc[r] - acc[l];
        }

        T prod(const int l, const int r) const {
            assert(0 <= l && l <= r && r <= n);
            return acc[r] - acc[l];
        }

        T all_prod() const {
            return acc.back();
        }

        int len() const {
            return n;
        }

        void print() const {
            cout << '[';
            for (int i = 0; i < n-1; ++i) {
                cout << acc[i] << ", ";
            }
            if (n > 0) cout << acc.back();
            cout << ']' << endl;
        }
    };
}  // namespace titan23
