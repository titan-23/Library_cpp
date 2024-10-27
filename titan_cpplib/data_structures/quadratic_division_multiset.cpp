#include<bits/stdc++.h>
using namespace std;

#include "titan_cpplib/others/print.cpp"

// QuadraticDivisionMultiset
namespace titan23 {

template<typename T>
class QuadraticDivisionMultiset {
  private:
    const int MAX_BUCKET_SIZE = 2;
    int n, size;
    vector<vector<T>> data;
    vector<T> bucket_data;

    int bisect_left(const vector<T> &a, const T key) const {
        int l = 0, r = a.size();
        while (r - l > 1) {
            int mid = (l + r) / 2;
            (a[mid] <= key ? l : r) = mid;
        }
        return l;
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
    QuadraticDivisionMultiset() {}
    QuadraticDivisionMultiset(vector<T> a) {
        sort(a.begin(), a.end());
        this->n = (int)a.size();
        const int bucket_size = sqrt(n) + 1;
        const int bucket_cnt = (n+bucket_size-1) / bucket_size;
        bucket_data.resize(bucket_cnt);
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
        data.erase(data.begin() + i);
        data.insert(data.begin() + i, left);
        data.insert(data.begin() + i+1, right);
    }

    void add(T key) {
        if (size == 0) {
            data.push_back({key});
            size = 1;
            return;
        }
        auto [i, pos] = get_pos(key);
        data[i].insert(data[i].begin() + pos, key);
        size++;
        if (data[i].size() > MAX_BUCKET_SIZE) {
            rebuild_split(i);
        }
    }

    bool discard(T key) {
        auto [i, pos] = get_pos(key);
        cerr << i << ", " << pos << endl;
        cerr << data[0] << ' ' << data[1] << endl;
        cerr << data[i][pos] << endl;
        if (pos >= data[i].size() || data[i][pos] != key) {
            cerr << "false" << endl;
            return false;
        }
        data[i].erase(data[i].begin() + pos);
        size--;
        if (data[i].empty()) {
            data.erase(data.begin() + i);
        }
        return true;
    }

    void remove(T key) {
        auto [i, pos] = get_pos(key);
        data[i].erase(data[i].begin() + pos);
        size--;
        if (data[i].empty()) {
            data.erase(data.begin() + i);
        }
    }

    T sum(int l, int r) const {}

    T get(int k) const {}

    int len() const {
        return size;
    }

    vector<T> tovector() const {
        vector<T> a;
        a.reserve(size);
        for (const vector<T> &d : data) {
            for (const T &e : d) {
                a.emplace_back(e);
            }
        }
        return a;
    }
};
} // namespace titan23

int main() {
    vector<int> a;
    titan23::QuadraticDivisionMultiset<int> qd(a);

    qd.add(0);
    qd.add(1);
    qd.add(1);
    qd.add(1);
    qd.add(2);
    qd.discard(2);
    auto v = qd.tovector();
    cout << v << endl;
}
