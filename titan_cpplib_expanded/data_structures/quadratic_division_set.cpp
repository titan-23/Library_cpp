// #include "titan_cpplib/data_structures/quadratic_division_set.cpp"
exit(1);

#include<bits/stdc++.h>
using namespace std;

// QuadraticDivisionSet
namespace titan23 {

template<typename T>
class QuadraticDivisionSet {
  private:
    int n, size, bucket_cnt;
    vector<vector<T>> data;
    vector<T> bucket_data;

    QuadraticDivisionSet(int n) {
        vector<T> a(n, e);
        _build(a);
    }

    QuadraticDivisionSet(vector<T> a) {
        _build(a);
    }

    void _build(vector<T> a) {
        this->n = (int)a.size();
        this->size = sqrt(n) + 1;
        this->bucket_cnt = (n+size-1) / size;
        bucket_data.resize(bucket_cnt);
        data.resize(bucket_cnt);
        for (int k = 0; k < bucket_cnt; ++k) {
            data[k].resize(size);
            for (int i = 0; i < size; ++i) {
                data[k][i] = a[k*size+i];
                bucket_data[k] += a[k*size+i];
            }
        }
    }

    T sum(int l, int r) const {
        int k1 = l / this->size, k2 = r / this->size;
        l -= k1 * this->size;
        r -= k2 * this->size;
        T ans = 0;
        if (k1 == k2) {
            for (int i = l; i < r; ++i) {
                ans += this->data[k1][i];
            }
        } else {
            for (int i = l; i < (int)this->data[k1].size(); ++i) {
                ans += this->data[k1][i];
            }
            for (int k = k1+1; k < k2; ++k) {
                ans += this->bucket_data[k];
            }
            for (int i = 0; i < r; ++i) {
                ans += this->data[k2][i];
            }
        }
        return ans;
    }

    T get(int idx) {
        int k = idx / size;
        return data[k][idx-k*size];
    }
};
} // namespace titan23

