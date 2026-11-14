/// https://github.com/titan-23/Library_cpp/blob/main/titan_cpplib/ds/max_heap.cpp
#pragma once

#include <iostream>
#include <vector>
#include <cassert>
using namespace std;

namespace titan23 {

/// @brief 最大値を取り出す二分ヒープ
template<typename T>
class MaxHeap {
public:
    vector<T> a;

private:
    void _heapify() {
        for (int i = (int)a.size(); i >= 0; --i) {
            _down(i);
        }
    }

    void _down(int i) {
        int n = a.size();
        while ((i<<1|1) < n) {
            int u = i*2+1, v = i*2+2;
            if (v < n && a[u] < a[v]) u = v;
            if (a[i] < a[u]) {
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
            if (a[i] > a[p]) {
                swap(a[i], a[p]);
                i = p;
            } else {
                break;
            }
        }
    }

    public:
    MaxHeap() {}
    /// @brief 列 a から構築する / O(n)
    MaxHeap(vector<T> a) : a(a) { _heapify(); }

    /// @brief ヒープ条件を保たずに key を追加する / 償却 O(1)
    void __push_noheap(const T key) {
        a.emplace_back(key);
    }

    /// @brief ヒープ化する / O(n)
    void __heapify() {
        for (int i = (int)a.size(); i >= 0; --i) {
            _down(i);
        }
    }

    /// @brief 全要素を削除する
    void clear() {
        a.clear();
    }

    /// @brief 最大の要素を返す / O(1)
    T get_max() const {
        assert(!a.empty());
        return a[0];
    }

    /// @brief 最大の要素を削除して返す / O(log n)
    T pop_max() {
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

    /// @brief key を追加してから最大の要素を削除して返す / O(log n)
    T pushpop_max(const T key) {
        if (a[0] <= key) return key;
        T res = a[0];
        a[0] = key;
        _down(0);
        return res;
    }

    /// @brief 最大の要素を削除して返し、key を追加する / pop してから push するのと同じ / O(log n)
    T replace_max(const T key) {
        T res = a[0];
        a[0] = key;
        _down(0);
        return res;
    }

    /// @brief ヒープの配列をそのまま返す / O(n)
    vector<T> tolist() const {
        return a;
    }

    bool empty() const {
        return a.empty();
    }

    /// @brief 要素数を返す / O(1)
    int len() const {
        return (int)a.size();
    }

    /// @brief 要素を昇順に標準出力へ出力する / O(n log n)
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
