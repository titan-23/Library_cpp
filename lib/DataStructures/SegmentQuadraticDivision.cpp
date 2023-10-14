template<typename T>
struct SegmentQuadraticDivision {
  int n, size, bucket_cnt;
  T e;
  function<T (T, T)> op;
  vector<vector<T>> data;
  vector<T> bucket_data;

  SegmentQuadraticDivision(int n, function<T (T, T)> op, T e) {
    vector<T> a(n, e);
    _build(a, op, e);
  }

  SegmentQuadraticDivision(vector<T> a, function<T (T, T)> op, T e) {
    _build(a, op, e);
  }

  void _build(vector<T> a, function<T (T, T)> op, T e) {
    this->n = (int)a.size();
    this->op = op;
    this->e = e;
    this->size = sqrt(n) + 1;
    this->bucket_cnt = (n+size-1) / size;
    bucket_data.resize(bucket_cnt);
    data.resize(bucket_cnt);
    for (int k = 0; k < bucket_cnt; ++k) {
      data[k].resize(size);
      T v = e;
      for (int i = 0; i < size; ++i) {
        data[k][i] = a[k*size+i];
        v = op(v, a[k*size+i]);
      }
      bucket_data[k] = v;
    }
  }

  T prod(int l, int r) {
    int k1 = l / size, k2 = r / size;
    l -= k1 * size;
    r -= k2 * size;
    T ans = e;
    if (k1 == k2) {
      for (int i = l; i < r; ++i) {
        ans = op(ans, data[k1][i]);
      }
    } else {
      for (int i = l; i < (int)data[k1].size(); ++i) {
        ans = op(ans, data[k1][i]);
      }
      for (int k = k1+1; k < k2; ++k) {
        ans = op(ans, bucket_data[k]);
      }
      for (int i = 0; i < r; ++i) {
        ans = op(ans, data[k2][i]);
      }
    }
    return ans;
  }

  T get(int indx) {
    int k = indx / size;
    return data[k][indx-k*size];
  }

  void set(int indx, T key) {
    int k = indx / size;
    data[k][indx-k*size] = key;
    T v = e;
    for (const T &a: data[k]) {
      v = op(v, a);
    }
    bucket_data[k] = v;
  }
};
