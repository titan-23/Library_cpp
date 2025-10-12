#include <vector>
using namespace std;

namespace titan23 {

template <class T, T (*op)(T, T), T (*e)()>
class SqrtSegmentTree {
private:
    int n, size;
    vector<vector<T>> a;
    vector<T> data;

public:
    SqrtSegmentTree() {}

    SqrtSegmentTree(int n) : n(n) {
        size = sqrt(n) + 1;
        int bucket_cnt = (n+size-1)/size;
        a.resize(bucket_cnt);
        data.resize(bucket_cnt);
        for (int i = 0; i < bucket_cnt; ++i) {
            int k = min(n, (i+1)*size) - i*size;
            a[i].resize(k, e());
            data[i] = e();
        }
    }

    SqrtSegmentTree(vector<T> A) : n(A.size()) {
        size = sqrt(n) + 1;
        int bucket_cnt = (n+size-1)/size;
        a.resize(bucket_cnt);
        data.resize(bucket_cnt);
        for (int i = 0; i < bucket_cnt; ++i) {
            a[i] = vector<T>(A.begin()+i*size, A.begin()+min(n, (i+1)*size));
            T s = e();
            for (int j = 0; j < a[i].size(); ++j) {
                s = op(s, a[i][j]);
            }
            data[i] = s;
        }
    }

    T prod(int l, int r) const {
        // O(√N)
        assert(0 <= l && l <= r && r <= n);
        if (l == r) return e();
        int k1 = l / size;
        int k2 = r / size;
        l -= k1 * size;
        r -= k2 * size;
        T s = e();
        if (k1 == k2) {
            for (int i = l; i < r; ++i) s = op(s, a[k1][i]);
        } else {
            for (int i = l; i < a[k1].size(); ++i) s = op(s, a[k1][i]);
            for (int i = k1+1; i < k2; ++i) s = op(s, data[i]);
            if (k2 < a.size()) {
                for (int i = 0; i < r; ++i) s = op(s, a[k2][i]);
            }
        }
        return s;
    }

    T all_prod() const {
        // O(√N)
        T s = e();
        for (const T t : data) s = s = op(s, t);
        return s;
    }

    T get(int i) const {
        // O(1)
        int k = i / size;
        return a[k][i-k*size];
    }

    void set(int i, T v) {
        int k = i / size;
        a[k][i-k*size] = v;
        data[k] = e();
        for (T t : a[k]) data[k] = op(data[k], t);
    }
};
} // namespace titan23
