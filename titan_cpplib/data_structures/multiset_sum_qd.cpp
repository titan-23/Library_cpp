#include <vector>
#include <algorithm>
using namespace std;

// MultisetSum
namespace titan23 {

template<typename T>
class MultisetSum {
    // ref: https://github.com/tatyam-prime/SortedSet/blob/main/SortedMultiset.py

private:
    const int BUCKET_RATIO = 16;
    const int SPLIT_RATIO = 24;
    int n;
    T missing;
    vector<vector<T>> data;
    vector<T> bucket_data;

    int bisect_left(const vector<T> &a, const T &key) const {
        return lower_bound(a.begin(), a.end(), key) - a.begin();
    }

    int bisect_right(const vector<T> &a, const T &key) const {
        return upper_bound(a.begin(), a.end(), key) - a.begin();
    }

    pair<int, int> get_pos(const T key) const {
        if (data.empty()) return {0, 0};
        if (key > data.back().back()) return {data.size()-1, data.back().size()};
        int idx = lower_bound(data.begin(), data.end(), key, [&] (const vector<T> &vec, const T &key) -> bool {
            return vec.back() < key;
        }) - data.begin();
        assert(idx < data.size() && key <= data[idx].back());
        return {idx, bisect_left(data[idx], key)};
    }

    void rebuild_split(int i) {
        int m = data[i].size();
        data.insert(data.begin() + i+1, vector<T>(data[i].begin() + m/2, data[i].end()));
        data[i].erase(data[i].begin() + m/2, data[i].end());
        T right_sum = 0;
        for (const T x : data[i+1]) right_sum += x;
        bucket_data[i] -= right_sum;
        bucket_data.insert(bucket_data.begin() + i+1, right_sum);
    }

public:
    MultisetSum() : n(0) {}

    MultisetSum(T missing) : n(0), missing(missing) {}

    MultisetSum(vector<T> a, T missing) : n(a.size()), missing(missing) {
        for (int i = 0; i < n-1; ++i) {
            if (a[i] > a[i+1]) {
                sort(a.begin(), a.end());
                break;
            }
        }
        int bucket_cnt = sqrt(n / BUCKET_RATIO) + 1;
        int bucket_size = n / bucket_cnt + 1;
        data.resize(bucket_cnt);
        bucket_data.resize(bucket_cnt, 0);
        for (int k = 0; k < bucket_cnt; ++k) {
            int size = min(bucket_size, n-k*bucket_size);
            if (size <= 0) {
                for (int l = k; l < bucket_cnt; ++l) {
                    data.pop_back();
                    bucket_data.pop_back();
                }
                break;
            }
            data[k] = vector<T>(a.begin()+k*bucket_size, a.begin()+k*bucket_size+size);
            for (const T &x : data[k]) bucket_data[k] += x;
        }
    }

    void add(const T &key) {
        if (n == 0) {
            data.push_back({key});
            bucket_data.push_back(key);
            n = 1;
            return;
        }
        auto [i, pos] = get_pos(key);
        data[i].insert(data[i].begin() + pos, key);
        bucket_data[i] += key;
        n++;
        if (data[i].size() > data.size() * SPLIT_RATIO) {
            rebuild_split(i);
        }
    }

    bool discard(const T &key) {
        auto [i, pos] = get_pos(key);
        if (i >= data.size() || pos >= data[i].size() || data[i][pos] != key) {
            return false;
        }
        data[i].erase(data[i].begin() + pos);
        bucket_data[i] -= key;
        n--;
        if (data[i].empty()) {
            data.erase(data.begin() + i);
            bucket_data.erase(bucket_data.begin() + i);
        }
        return true;
    }

    void remove(const T &key) {
        assert(discard(key));
    }

    T operator[] (int k) const {
        for (const vector<T> &d : data) {
            if (k < d.size()) return d[k];
            k -= d.size();
        }
    }

    T lt(const T &key) const {
        for (auto it = this->data.rbegin(); it != this->data.rend(); ++it) {
            const vector<T> &d = *it;
            if (d[0] < key) {
                int index = bisect_left(d, key) - 1;
                if (index >= 0) return d[index];
            }
        }
        return this->missing;
    }

    T le(const T &key) const {
        for (auto it = this->data.rbegin(); it != this->data.rend(); ++it) {
            const vector<T> &d = *it;
            if (d[0] <= key) {
                int index = bisect_right(d, key) - 1;
                if (index >= 0) return d[index];
            }
        }
        return this->missing;
    }

    T gt(const T &key) const {
        for (const vector<T> &d : this->data) {
            if (d.back() > key) {
                int index = bisect_right(d, key);
                if (index < d.size()) return d[index];
            }
        }
        return this->missing;
    }

    T ge(const T &key) const {
        for (const vector<T> &d : this->data) {
            if (d.back() >= key) {
                int index = bisect_left(d, key);
                if (index < d.size()) return d[index];
            }
        }
        return this->missing;
    }

    int index(const T &x) const {
        int ans = 0;
        for (const vector<T> &d : this->data) {
            if (d.back() >= x) return ans + bisect_left(d, x);
            ans += d.size();
        }
        return ans;
    }

    int index_right(const T &x) const {
        int ans = 0;
        for (const vector<T> &d : this->data) {
            if (d.back() > x) return ans + bisect_right(d, x);
            ans += d.size();
        }
        return ans;
    }

    int count(const T &key) const {
        return index_right(key) - index(key);
    }

    bool contains(const T &key) const {
        auto [i, pos] = get_pos(key);
        return i < data.size() && pos < data[i].size() && data[i][pos] == key;
    }

    // [l, r)の総和を返す
    T sum(int l, int r) const {
        T sum = 0;
        int u = 0, v = 0;
        for (int i = 0; i < data.size(); ++i) {
            v = u + data[i].size();
            if (l <= u && v <= r) {
                sum += bucket_data[i];
            } else if (v > l && u < r) {
                int start = max(l, u);
                int end = min(r, v);
                for (int j = start; j < end; ++j) {
                    sum += data[i][j - u];
                }
            }
            u = v;
            if (u >= r) break;
        }
        return sum;
    }

    // 総和がw未満となるように先頭からとるとき、いくつとれるか？
    int count_by_sum_limit(T w) const {
        int ans = 0;
        for (int i = 0; i < data.size(); ++i) {
            if (w > bucket_data[i]) {
                w -= bucket_data[i];
                ans += data[i].size();
                continue;
            }
            for (int j = 0; j < data[i].size(); ++j) {
                if (w > data[i][j]) {
                    w -= data[i][j];
                    ans++;
                    continue;
                }
                break;
            }
            break;
        }
        return ans;
    }

    int size() const {
        return n;
    }

    int len() const {
        return n;
    }

    vector<T> tovector() const {
        vector<T> a;
        a.reserve(n);
        for (const vector<T> &d : data) {
            a.insert(a.end(), d.begin(), d.end());
        }
        return a;
    }

    friend ostream& operator<<(ostream& os, const titan23::MultisetSum<T> &ms) {
        vector<T> a = ms.tovector();
        os << "{";
        int n = ms.len();
        for (int i = 0; i < n; ++i) {
            os << a[i];
            if (i != n-1) os << ", ";
        }
        os << "}";
        return os;
    }
};
} // namespace titan23
