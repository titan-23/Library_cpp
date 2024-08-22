#include <vector>
#include <algorithm>
using namespace std;

// StaticSet
namespace titan23 {

    /**
     * @brief 静的な全順序集合を管理するデータ構造
     */
    template<typename T>
    struct StaticSet {
        vector<T> data;
        T missing;
        int n;

        StaticSet() {}

        /**
         * @brief Construct a new Static Set object / `O(1)`
         *
         * @param missing 使用しない値
         */
        StaticSet(T missing) : missing(missing) {}

        /**
         * @brief Construct a new Static Set object / `O(nlogn)`
         *
         * @param a
         * @param missing 使用しない値
         */
        StaticSet(vector<T> &a, T missing) : data(a), missing(missing) {
            sort(data.begin(), data.end());
            data.erase(unique(data.begin(), data.end()), data.end());
            n = (int)data.size();
        }

        //! 表示する
        void print() const {
            cout << "{";
            for (int i = 0; i < n-1; ++i) {
                cout << data[i] << ", ";
            }
            if (n > 0) {
                cout << data.back();
            }
            cout << "}" << endl;
        }

        //! 要素数を返す / `O(1)`
        int len() const {
            return n;
        }

        //! 昇順 `k` 番目の要素を返す / `O(1)`
        T get(const int k) const {
            assert(0 <= k && k < len());
            return data[k];
        }

        //! `key` 以上で最小 / `O(logn)`
        T ge(const T &key) const {
            if (key > data.back()) return missing;
            int l = -1, r = n-1;
            while (r - l > 1) {
                int mid = (l + r) >> 1;
                ((data[mid] >= key)? r : l) = mid;
            }
            return data[r];
        }

        //! `key` より大きくて最小 / `O(logn)`
        T gt(const T &key) const {
            if (key >= data.back()) return missing;
            int l = -1, r = n-1;
            while (r - l > 1) {
                int mid = (l + r) >> 1;
                ((data[mid] > key)? r : l) = mid;
            }
            return data[r];
        }

        //! `key` 以下で最大 / `O(logn)`
        T le(const T &key) const {
            if (key < data[0]) return missing;
            int l = 0, r = n;
            while (r - l > 1) {
                int mid = (l + r) >> 1;
                ((data[mid] <= key)? l : r) = mid;
            }
            return data[l];
        }

        //! `key` 未満で最大 / `O(logn)`
        T lt(const T &key) const {
            if (key <= data[0]) return missing;
            int l = 0, r = n;
            while (r - l > 1) {
                int mid = (l + r) >> 1;
                ((data[mid] < key)? l : r) = mid;
            }
            return data[l];
        }

        //! `upper` 未満の要素数を返す / `O(logn)`
        int index(T upper) const {
            int l = -1, r = n;
            while (r - l > 1) {
                int mid = (l + r) >> 1;
                ((data[mid] < upper)? l : r) = mid;
            }
            return r;
        }

        //! `upper` 以下の要素数を返す / `O(logn)`
        int index_right(T upper) const {
            int l = -1, r = n;
            while (r - l > 1) {
                int mid = (l + r) >> 1;
                ((data[mid] <= upper)? l : r) = mid;
            }
            return r;
        }

        //! `key` の要素数を返す / `O(logn)`
        int count(T key) const {
            return index_right(key) - index(key);
        }

        //! `[lower, upper)` の要素数を返す / `O(logn)`
        int count_range(T lower, T upper) const {
            assert(lower <= upper);
            return index(upper) - index(lower);
        }

        //! `key` の存在判定 / `o(logn)`
        bool contains(T key) const {
            if (index_right(key) - index(key) > 0) {
                return true;
            }
            return false;
        }
    };
}
