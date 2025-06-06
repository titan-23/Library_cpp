#pragma GCC target("avx2")
#pragma GCC optimize("O3")
#pragma GCC optimize("unroll-loops")

#include <bits/stdc++.h>
using namespace std;

#define rep(i, n) for (int i = 0; i < (n); ++i)
#define ll long long

#include "titan_cpplib/others/print.cpp"

// MultisetSum
namespace titan23 {

template<typename T>
class MultisetSum {
  public:
    const int MAX_BUCKET_SIZE = 2;
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
        for (int i = 0; i < data.size(); ++i) {
            if (key <= data[i].back()) {
                return {i, bisect_left(data[i], key)};
            }
        }
        if (data.size() == 0) {
            return {0, 0};
        }
        return {data.size()-1, data.back().size()};
    }

  public:
    MultisetSum() {}

    MultisetSum(T missing) {
        this->n = 0;
        this->missing = missing;
    }

    MultisetSum(vector<T> a, T missing) {
        sort(a.begin(), a.end());
        this->n = (int)a.size();
        this->missing = missing;
        const int bucket_size = sqrt(n) + 1;
        const int bucket_cnt = (n+bucket_size-1) / bucket_size;
        bucket_data.resize(bucket_cnt, 0);
        data.resize(bucket_cnt);
        for (int k = 0; k < bucket_cnt; ++k) {
            int size = min(bucket_size, n-k*bucket_size);
            data[k].resize(size);
            for (int i = 0; i < size; ++i) {
                data[k][i] = a[k*bucket_size+i];
                bucket_data[k] += a[k*bucket_size+i];
            }
        }
    }

    void rebuild_split(int i) {
        vector<T> left, right;
        int m = data[i].size();
        for (int j = 0; j < m/2; ++j) {
            left.emplace_back(data[i][j]);
        }
        for (int j = m/2; j < m; ++j) {
            right.emplace_back(data[i][j]);
        }
        data[i] = left;
        // data.erase(data.begin() + i);
        // data.insert(data.begin() + i, left);
        data.insert(data.begin() + i+1, right);
        T left_sum = 0;
        T right_sum = 0;
        for (const T &key : left) left_sum += key;
        for (const T &key : right) right_sum += key;
        bucket_data.erase(bucket_data.begin() + i);
        bucket_data.insert(bucket_data.begin() + i, left_sum);
        bucket_data.insert(bucket_data.begin() + i+1, right_sum);
    }

    void add(T key) {
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
        if (data[i].size() > MAX_BUCKET_SIZE) {
            rebuild_split(i);
        }
    }

    bool discard(T key) {
        auto [i, pos] = get_pos(key);
        if (i >= data.size() || pos >= data[i].size() || data[i][pos] != key) {
            return false;
        }
        data[i].erase(data[i].begin() + pos);
        bucket_data[i] -= key;
        n--;
        if (data[i].empty()) {
            data.erase(data.begin() + i);
            assert(bucket_data[i] == 0);
            bucket_data.erase(bucket_data.begin() + i);
        }
        return true;
    }

    void remove(T key) {
        auto [i, pos] = get_pos(key);
        data[i].erase(data[i].begin() + pos);
        n--;
        if (data[i].empty()) {
            data.erase(data.begin() + i);
        }
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
                if (index >= 0) return (d)[index];
            }
        }
        return this->missing;
    }

    T gt(const T &key) const {
        for (const auto &d : this->data) {
            if (d.back() > key) {
                int index = bisect_right(d, key);
                if (index < d.size()) return d[index];
            }
        }
        return this->missing;
    }

    T ge(const T &key) const {
        for (const auto &d : this->data) {
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
};
} // namespace titan23

bool vec_contains(const vector<int> &a, int x) {
    for (int e : a) {
        if (e == x) return true;
    }
    return false;
}

void solve() {
    int q;
    cin >> q;

    vector<int> naive;
    titan23::MultisetSum<int> s(-1);

    rep(query, q) {
        cout << "query : " << query << endl;
        int com;
        cin >> com;
        if (com == 0) { // add
            int v; cin >> v;
            cout << "  add(" << v << ")" << endl;
            if (!s.contains(v)) {
                s.add(v);
                naive.push_back(v);
            } else {
                cout << s.tovector() << endl;
                cout << naive << endl;
                assert(s.contains(v));
                assert(vec_contains(naive, v));
            }
        } else if (com == 1) { // discard
            int v; cin >> v;
            cout << "  discard(" << v << ")" << endl;
            if (s.contains(v)) {
                s.discard(v);
                auto it = std::find(naive.begin(), naive.end(), v);
                if (it != naive.end()) naive.erase(it);
            }
        } else if (com == 2) { // sum
            int l, r; cin >> l >> r;
            cout << "  sum(" << l << ", " << r << ")" << endl;
            int sum = s.sum(l, r);

            int sum_naive = 0;
            for (int i = l; i < r; ++i) sum_naive += naive[i];

            cout << s.tovector() << endl;
            cout << naive << endl;

            cout << "set : " << sum << " | " << "naive : " << sum_naive << endl;
            assert(sum == sum_naive);
        } else if (com == 3) {
            int x; cin >> x;
            cout << "  ge(" << x << ")" << endl;
            int ge = s.ge(x);

            int ge_naive = -1;
            for (int e : naive) {
                if (e >= x) {
                    ge_naive = e;
                    break;
                }
            }
            // cout << s.data << endl;
            // cout << naive << endl;
            cout << "set : " << ge << " | " << "naive : " << ge_naive << endl;
            assert(ge == ge_naive);
        }

        sort(naive.begin(), naive.end());
        cerr << "contains" << endl;
        for (int x : naive) {
            assert(s.contains(x));
        }
        assert(naive == s.tovector());
    }

    cout << "OK" << endl;
}

int main() {
    ios::sync_with_stdio(false);
    cin.tie(0);
    cout << fixed << setprecision(15);
    solve();
    return 0;
}
