#include <vector>
#include <stack>
using namespace std;

// UndoableUnionFindSum
namespace titan23 {

template<typename T>
class UndoableUnionFindSum {
private:
    int _n, _group_count;
    T _e;
    vector<int> _parents;
    vector<T> _all_sum, _one_sum;
    stack<tuple<int, int, T>> _history;

public:
    UndoableUnionFindSum() {}
    UndoableUnionFindSum(int n, T e) :
        _n(n),
        _group_count(n),
        _e(e),
        _parents(n, -1),
        _all_sum(n, e),
        _one_sum(n, e) {}

    void undo() {
        auto [y, py, all_sum_y] = _history.top();
        _history.pop();
        if (y == -1) return;
        auto [x, px, all_sum_x] = _history.top();
        _history.pop();
        ++_group_count;
        _parents[y] = py;
        _parents[x] = px;
        T s = (_all_sum[x] - all_sum_y - all_sum_x) / (-py-px) * (-py);
        _all_sum[y] += s;
        _all_sum[x] -= all_sum_y + s;
        _one_sum[x] -= _one_sum[y];
    }

    int root(int x) const {
        while (_parents[x] >= 0) {
            x = _parents[x];
        }
        return x;
    }

    bool unite(int x, int y) {
        x = root(x);
        y = root(y);
        if (x == y) {
            _history.emplace(-1, -1, _e);
            return false;
        }
        if (_parents[x] > _parents[y]) swap(x, y);
        _group_count -= 1;
        _history.emplace(x, _parents[x], _all_sum[x]);
        _history.emplace(y, _parents[y], _all_sum[y]);
        _all_sum[x] += _all_sum[y];
        _one_sum[x] += _one_sum[y];
        _parents[x] += _parents[y];
        _parents[y] = x;
        return true;
    }

    int size(const int x) const {
        return -_parents[root(x)];
    }

    bool same(const int x, const int y) const {
        return root(x) == root(y);
    }

    void add_point(int x, const T v) {
        while (x >= 0) {
            _one_sum[x] += v;
            x = _parents[x];
        }
    }

    void add_group(int x, const T v) {
        x = root(x);
        _all_sum[x] += v * size(x);
    }

    int group_count() const {
        return _group_count;
    }

    T group_sum(int x) const {
        x = root(x);
        return _one_sum[x] + _all_sum[x];
    }
};
} // namespace titan23
