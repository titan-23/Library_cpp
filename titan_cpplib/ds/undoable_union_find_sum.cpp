/// https://github.com/titan-23/Library_cpp/blob/main/titan_cpplib/ds/undoable_union_find_sum.cpp
#pragma once

#include <vector>
#include <stack>
using namespace std;

namespace titan23 {

/// @brief 集合ごとに頂点の値の総和を持ち、unite を新しい順に取り消せる Union-Find
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
    /// @brief n 頂点で構築する / e は和の単位元 / O(n)
    UndoableUnionFindSum(int n, T e) :
        _n(n),
        _group_count(n),
        _e(e),
        _parents(n, -1),
        _all_sum(n, e),
        _one_sum(n, e) {}

    /// @brief 直前の unite を 1 回分取り消す / 併合しなかった unite も 1 回と数える / O(1)
    void undo() {
        auto [y, py, all_sum_y] = _history.top();
        _history.pop();
        if (y == -1) return;
        auto [x, px, all_sum_x] = _history.top();
        _history.pop();
        ++_group_count;
        _parents[y] = py;
        _parents[x] = px;
        // 併合後に add_group で加えた分を、要素数の比で y 側へ戻す
        T s = (_all_sum[x] - all_sum_y - all_sum_x) / (-py-px) * (-py);
        _all_sum[y] += s;
        _all_sum[x] -= all_sum_y + s;
        _one_sum[x] -= _one_sum[y];
    }

    /// @brief x の根を返す / O(log n)
    int root(int x) const {
        while (_parents[x] >= 0) {
            x = _parents[x];
        }
        return x;
    }

    /// @brief x と y の属する集合を併合し、併合したかを返す / O(log n)
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

    /// @brief x の属する集合の要素数を返す / O(log n)
    int size(const int x) const {
        return -_parents[root(x)];
    }

    /// @brief x と y が同じ集合に属するかを返す / O(log n)
    bool same(const int x, const int y) const {
        return root(x) == root(y);
    }

    /// @brief 頂点 x の値に v を加える / O(log n)
    void add_point(int x, const T v) {
        while (x >= 0) {
            _one_sum[x] += v;
            x = _parents[x];
        }
    }

    /// @brief x の属する集合の全頂点の値に v を加える / O(log n)
    void add_group(int x, const T v) {
        x = root(x);
        _all_sum[x] += v * size(x);
    }

    /// @brief 集合の個数を返す / O(1)
    int group_count() const {
        return _group_count;
    }

    /// @brief x の属する集合の値の総和を返す / O(log n)
    T group_sum(int x) const {
        x = root(x);
        return _one_sum[x] + _all_sum[x];
    }
};
} // namespace titan23
