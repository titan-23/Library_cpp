#include <iostream>
#include <vector>
using namespace std;

namespace titan23 {

template <typename T>
class PartialPersistentArray {
private:
    vector<vector<T>> a;
    vector<vector<int>> ts;
    int last;

public:
    PartialPersistentArray(const vector<T> &A) {
        int n = A.size();
        a.resize(n);
        ts.resize(n);
        for (int i = 0; i < n; ++i) {
            a[i].push_back(A[i]);
            ts[i].push_back(0);
        }
        last = 0;
    }

    void set(int k, const T &v, int t) {
        assert(t >= last);
        assert(t > ts[k].back());
        a[k].push_back(v);
        ts[k].push_back(t);
        last = t;
    }

    T get(int k, int t = -1) const {
        if (t == -1 || t >= ts[k].back()) {
            return a[k].back();
        }
        auto it = upper_bound(ts[k].begin(), ts[k].end(), t);
        int idx = (it - ts[k].begin()) - 1;
        return a[k][idx];
    }

    vector<T> tovector(int t) const {
        int n = a.size();
        vector<T> res(n);
        for (int i = 0; i < n; ++i) {
            res[i] = get(i, t);
        }
        return res;
    }

    void show(int t) const {
        cout << "Time: " << t << " ";
        auto v = tovector(t);
        for (const auto &x : v) cout << x << " ";
        cout << "\n";
    }

    int size() const { return a.size(); }

    void show_all() const {
        for (int i = 0; i <= last; ++i) {
            show(i);
        }
    }
};
} // namespace titan23
