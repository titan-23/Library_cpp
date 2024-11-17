#include <iostream>
#include <algorithm>
#include <vector>
#include <cassert>
using namespace std;

namespace titan23 {

class IndexSet {
    // https://topcoder-tomerun.hatenablog.jp/entry/2021/06/12/134643
  public:
    vector<int> que, pos;

    IndexSet() {}
    IndexSet(int u) : pos(u, -1) {}

    void add(int v) {
        if (pos[v] != -1) return;
        pos[v] = que.size();
        que.push_back(v);
    }

    bool discard(int v) {
        int p = pos[v];
        if (p == -1) return false;
        int b = que.back();
        que[p] = b;
        que.pop_back();
        pos[b] = p;
        pos[v] = -1;
        return true;
    }

    void remove(int v) {
        int p = pos[v];
        assert(p != -1);
        int b = que.back();
        que[p] = b;
        que.pop_back();
        pos[b] = p;
        pos[v] = -1;
    }

    bool contains(int v) const {
        return pos[v] != -1;
    }

    void clear() {
        fill(pos.begin(), pos.end(), -1);
        que.clear();
    }

    int len() const {
        return que.size();
    }

    bool empty() const {
        return que.size() == 0;
    }

    int get(int v) const {
        assert(0 <= v && v < len());
        return que[v];
    }

    friend ostream& operator<<(ostream& os, const titan23::IndexSet &ist) {
        vector<int> a = ist.que;
        sort(a.begin(), a.end());
        int n = a.size();
        os << "{";
        for (int i = 0; i < n; ++i) {
            os << a[i];
            if (i != n-1) os << ", ";
        }
        os << "}";
        return os;
    }
};
} // namespace titan23
