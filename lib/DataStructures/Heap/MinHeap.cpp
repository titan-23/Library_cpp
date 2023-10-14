
template<typename T>
struct MinHeap {
  vector<T> a;

  MinHeap() {}

  MinHeap(vector<T> &a) {
    this->a = a;
    _heapify();
  }

  void _heapify() {
    for (int i = a.size(); i >= 0; --i) {
      _down(i);
    }
  }

  void _down(int i) {
    int n = a.size();
    while (i<<1|1 < n) {
      int u = i*2+1, v = i*2+2;
      if (v < n && a[u] > a[v]) u = v;
      if (a[i] > a[u]) {
        swap(a[i], a[u]);
        i = u;
      } else {
        break;
      }
    }
  }

  void _up(int i) {
    while (i > 0) {
      int p = (i - 1) >> 1;
      if (a[i] < a[p]) {
        swap(a[i], a[p]);
        i = p;
      } else {
        break;
      }
    }
  }

  T get_min() {
    return a[0];
  }

  T pop_min() {
    T res = a[0];
    a[0] = a.back();
    a.pop_back();
    _down(0);
    return res;
  }

  void push(T key) {
    a.emplace_back(key);
    _up(a.size() - 1);
  }

  T pushpoop_min(T key) {
    if (a[0] >= key) return key;
    T res = a[0];
    a[0] = key;
    _down(0);
    return res;
  }

  T replace_min(T key) {
    T res = a[0];
    a[0] = key;
    _down(0);
    return res;
  }

  int len() {
    return (int)a.size();
  }

  void print() {
    cout << '[';
    for (int i = 0; i < len()-1; ++i) {
      cout << i << ", ";
    }
    if (!a.empty()) {
      cout << a.back();
    }
    cout << ']' << endl;
  }
};
