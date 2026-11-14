/// https://github.com/titan-23/Library_cpp/blob/main/titan_cpplib/ds/min_heap.cpp
#pragma once

#include <iostream>
#include <vector>
#include <cassert>
using namespace std;

namespace titan23 {

/// @brief 最小値を取り出せる二分ヒープ
template<typename T>
class MinHeap {
private:
    vector<T> a;

    void _heapify() {
        for (int i = (int)a.size(); i >= 0; --i) {
            _down(i);
        }
    }

    void _down(int i) {
        int n = a.size();
        while ((i<<1|1) < n) {
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

public:
    MinHeap() {}
    /// @brief 列 a から構築する / O(n)
    MinHeap(vector<T> a) : a(a) { _heapify(); }

    /// @brief 最小の要素を返す / O(1)
    T get_min() const {
        assert(!a.empty());
        return a[0];
    }

    /// @brief 最小の要素を削除して返す / O(log n)
    T pop_min() {
        assert(!a.empty());
        T res = a[0];
        a[0] = a.back();
        a.pop_back();
        _down(0);
        return res;
    }

    /// @brief key を追加する / O(log n)
    void push(const T key) {
        a.emplace_back(key);
        _up(a.size() - 1);
    }

    /// @brief key を追加してから最小の要素を削除して返す / 空でないことが前提 / O(log n)
    T pushpop_min(const T key) {
        if (a[0] >= key) return key;
        T res = a[0];
        a[0] = key;
        _down(0);
        return res;
    }

    /// @brief 最小の要素を削除して返し、そのあと key を追加する / 空でないことが前提 / O(log n)
    T replace_min(const T key) {
        T res = a[0];
        a[0] = key;
        _down(0);
        return res;
    }

    /// @brief 要素数を返す / O(1)
    int len() const {
        return (int)a.size();
    }

    /// @brief 要素を昇順に表示する
    void print() const {
        vector<T> b = a;
        sort(b.begin(), b.end());
        cout << '[';
        for (int i = 0; i < len()-1; ++i) {
            cout << b[i] << ", ";
        }
        if (!b.empty()) {
            cout << b.back();
        }
        cout << ']' << endl;
    }
};
} // namespace titan23
