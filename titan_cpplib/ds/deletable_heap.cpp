/// https://github.com/titan-23/Library_cpp/blob/main/titan_cpplib/ds/deletable_heap.cpp
#pragma once

#include <iostream>
#include <vector>
#include <cassert>
#include <queue>
using namespace std;

namespace titan23 {

/// @brief 任意の要素を削除できる最小ヒープ / 削除は遅延して行う
template<typename T>
class DeletableMinHeap {
private:
    priority_queue<T, vector<T>, greater<T>> hq, lazy;

public:
    DeletableMinHeap() {}
    /// @brief 列 a から構築する / O(n)
    DeletableMinHeap(vector<T> a) {
        hq = priority_queue<T, vector<T>, greater<T>>(a.begin(), a.end());
    }

    /// @brief key を追加する / O(log n)
    void push(T key) {
        hq.push(key);
    }

    /// @brief 最小値を削除して返す / 償却 O(log n)
    T pop_min() {
        assert(!hq.empty());
        while (!lazy.empty() && lazy.top() == hq.top()) {
            hq.pop(); lazy.pop();
        }
        assert(!hq.empty());
        T key = hq.top();
        hq.pop();
        return key;
    }

    /// @brief 最小値を返す / 償却 O(log n)
    T get_min() {
        assert(!hq.empty());
        while (!lazy.empty() && lazy.top() == hq.top()) {
            hq.pop(); lazy.pop();
        }
        assert(!hq.empty());
        return hq.top();
    }

    /// @brief key を1つ削除する / O(log n) / key はヒープに含まれていること
    void erase(T key) {
        lazy.push(key);
    }

    /// @brief 要素数を返す / O(1)
    int len() const {
        return hq.size() - lazy.size();
    }

    friend ostream& operator<<(ostream& os, const titan23::DeletableMinHeap<T> &heap) {
        auto hq = heap.hq;
        auto lazy = heap.lazy;
        vector<T> res;
        while (!hq.empty()) {
            while (!lazy.empty() && lazy.top() == hq.top()) {
                hq.pop(); lazy.pop();
            }
            if (hq.empty()) break;
            res.push_back(hq.top());
            hq.pop();
        }
        os << "[";
        for (int i = 0; i < (int)res.size(); ++i) {
            os << res[i];
            if (i + 1 < (int)res.size()) os << ", ";
        }
        os << "]";
        return os;
    }
};


/// @brief 任意の要素を削除できる最大ヒープ / 削除は遅延して行う
template<typename T>
class DeletableMaxHeap {
private:
    priority_queue<T> hq, lazy;

public:
    DeletableMaxHeap() {}
    /// @brief 列 a から構築する / O(n)
    DeletableMaxHeap(vector<T> a) {
        hq = priority_queue<T>(a.begin(), a.end());
    }

    /// @brief key を追加する / O(log n)
    void push(T key) {
        hq.push(key);
    }

    /// @brief 最大値を削除して返す / 償却 O(log n)
    T pop_max() {
        assert(!hq.empty());
        while (!lazy.empty() && lazy.top() == hq.top()) {
            hq.pop(); lazy.pop();
        }
        assert(!hq.empty());
        T key = hq.top();
        hq.pop();
        return key;
    }

    /// @brief 最大値を返す / 償却 O(log n)
    T get_max() {
        assert(!hq.empty());
        while (!lazy.empty() && lazy.top() == hq.top()) {
            hq.pop(); lazy.pop();
        }
        assert(!hq.empty());
        return hq.top();
    }

    /// @brief key を1つ削除する / O(log n) / key はヒープに含まれていること
    void erase(T key) {
        lazy.push(key);
    }

    /// @brief 要素数を返す / O(1)
    int len() const {
        return hq.size() - lazy.size();
    }

    friend ostream& operator<<(ostream& os, const titan23::DeletableMaxHeap<T> &heap) {
        auto hq = heap.hq;
        auto lazy = heap.lazy;
        vector<T> res;
        while (!hq.empty()) {
            while (!lazy.empty() && lazy.top() == hq.top()) {
                hq.pop(); lazy.pop();
            }
            if (hq.empty()) break;
            res.push_back(hq.top());
            hq.pop();
        }
        os << "[";
        for (int i = 0; i < (int)res.size(); ++i) {
            os << res[i];
            if (i + 1 < (int)res.size()) os << ", ";
        }
        os << "]";
        return os;
    }
};
} // namespace titan23
