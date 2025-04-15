#include <vector>
#include <algorithm>
using namespace std;

// StaticMultiset
namespace titan23 {

    template<typename T>
    class StaticMultiset {
      private:
        vector<T> data;
        T missing;
        int n;

      public:
        StaticMultiset() {}
        StaticMultiset(T missing) : missing(missing) {}
        StaticMultiset(vector<T> &a, T missing) : data(a), missing(missing) {
            sort(data.begin(), data.end());
            n = (int)data.size();
        }

        //! 要素数を返す / `O(1)`
        int len() const {
            return n;
        }

        //! 空かどうか / `O(1)`
        bool empty() const {
            return n == 0;
        }

        //! 昇順 `k` 番目の要素を返す / `O(1)`
        T get(const int i) const {
            return data[i];
        }

        //! `key` 以上で最小 / `O(logn)`
        T ge(const T &key) const {
            if (key > data.back() || empty()) return missing;
            int l = -1, r = n-1;
            while (r - l > 1) {
                int mid = (l + r) >> 1;
                (data[mid] >= key ? r : l) = mid;
            }
            return data[r];
        }

        //! `key` より大きくて最小 / `O(logn)`
        T gt(const T &key) const {
            if (key >= data.back() || empty()) return missing;
            int l = -1, r = n-1;
            while (r - l > 1) {
                int mid = (l + r) >> 1;
                (data[mid] > key ? r : l) = mid;
            }
            return data[r];
        }

        //! `key` 以下で最大 / `O(logn)`
        T le(const T &key) const {
            if (key < data[0] || empty()) return missing;
            int l = 0, r = n;
            while (r - l > 1) {
                int mid = (l + r) >> 1;
                (data[mid] <= key ? l : r) = mid;
            }
            return data[l];
        }

        //! `key` 未満で最大 / `O(logn)`
        T lt(const T &key) const {
            if (key <= data[0] || empty()) return missing;
            int l = 0, r = n;
            while (r - l > 1) {
                int mid = (l + r) >> 1;
                (data[mid] < key ? l : r) = mid;
            }
            return data[l];
        }

        //! `upper` 未満の要素数を返す / `O(logn)`
        int index(const T &upper) const {
            int l = -1, r = n;
            while (r - l > 1) {
                int mid = (l + r) >> 1;
                (data[mid] < upper ? l : r) = mid;
            }
            return r;
        }

        //! `upper` 以下の要素数を返す / `O(logn)`
        int index_right(const T &upper) const {
            int l = -1, r = n;
            while (r - l > 1) {
                int mid = (l + r) >> 1;
                (data[mid] <= upper ? l : r) = mid;
            }
            return r;
        }

        //! `key` の要素数を返す / `O(logn)`
        int count(const T &key) const {
            return index_right(key) - index(key);
        }

        //! `[lower, upper)` の要素数を返す / `O(logn)`
        int count_range(const T &lower, const T &upper) const {
            assert(lower <= upper);
            return index(upper) - index(lower);
        }

        //! `key` の存在判定 / `o(logn)`
        bool contains(const T &key) const {
            int idx = index(key);
            if (idx == (int)data.size()) {
                return false;
            }
            return data[idx] == key;
        }

        friend ostream& operator<<(ostream& os, const titan23::StaticMultiset<T>& s) {
            int n = s.len();
            os << "{";
            for (int i = 0; i < n; ++i) {
                os << s.data[i];
                if (i != n-1) os << ", ";
            }
            os << "}";
            return os;
        }
    };
}
