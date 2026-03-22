#include <vector>
#include <algorithm>
using namespace std;

// StaticSet
namespace titan23 {

/**
 * @brief 静的な全順序集合を管理するデータ構造
 */
template<typename T>
class StaticSet {
private:
    vector<T> data;
    T missing;
    int n;

public:
    StaticSet() : missing(-1), n(0) {}

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

    //! 要素数を返す / `O(1)`
    int len() const {
        return n;
    }

    //! 空かどうか / `O(1)`
    bool empty() const {
        return n == 0;
    }

    //! 昇順 `k` 番目の要素を返す / `O(1)`
    T get(const int k) const {
        assert(0 <= k && k < len());
        return data[k];
    }

    //! `key` 以上で最小 / `O(logn)`
    T ge(const T &key) const {
        auto it = lower_bound(data.begin(), data.end(), key);
        if (it == data.end()) return missing;
        return *it;
    }

    //! `key` より大きくて最小 / `O(logn)`
    T gt(const T &key) const {
        auto it = upper_bound(data.begin(), data.end(), key);
        if (it == data.end()) return missing;
        return *it;
    }

    //! `key` 以下で最大 / `O(logn)`
    T le(const T &key) const {
        auto it = upper_bound(data.begin(), data.end(), key);
        if (it == data.begin()) return missing;
        return *(--it);
    }

    //! `key` 未満で最大 / `O(logn)`
    T lt(const T &key) const {
        auto it = lower_bound(data.begin(), data.end(), key);
        if (it == data.begin()) return missing;
        return *(--it);
    }

    //! `upper` 未満の要素数を返す / `O(logn)`
    int index(const T &upper) const {
        auto it = lower_bound(data.begin(), data.end(), upper);
        return distance(data.begin(), it);
    }

    //! `upper` 以下の要素数を返す / `O(logn)`
    int index_right(const T &upper) const {
        auto it = upper_bound(data.begin(), data.end(), upper);
        return distance(data.begin(), it);
    }

    //! `key` の要素数を返す / `O(logn)`
    int count(const T &key) const {
        return contains(key) ? 1 : 0;
    }

    //! `[lower, upper)` の要素数を返す / `O(logn)`
    int count_range(const T &lower, const T &upper) const {
        assert(lower <= upper);
        return index(upper) - index(lower);
    }

    //! `key` の存在判定 / `O(logn)`
    bool contains(const T &key) const {
        int idx = index(key);
        if (idx == (int)data.size()) {
            return false;
        }
        return data[idx] == key;
    }

    T neighbor(const T &key) const {
        T ge_key = ge(key);
        T le_key = le(key);
        if (ge_key == missing) return le_key;
        if (le_key == missing) return ge_key;
        return (ge_key-key < key-le_key) ? ge_key : le_key;
    }

    friend ostream& operator<<(ostream& os, const titan23::StaticSet<T>& s) {
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
