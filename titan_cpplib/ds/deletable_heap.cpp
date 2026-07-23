#pragma once

#include <iostream>
#include <vector>
#include <cassert>
#include <queue>
using namespace std;

namespace titan23 {

template<typename T>
class DeletableMinHeap {
private:
    priority_queue<T, vector<T>, greater<T>> hq, lazy;

public:
    DeletableMinHeap() {}
    DeletableMinHeap(vector<T> a) {
        hq = priority_queue<T, vector<T>, greater<T>>(a.begin(), a.end());
    }

    void push(T key) {
        hq.push(key);
    }

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

    T get_min() {
        assert(!hq.empty());
        while (!lazy.empty() && lazy.top() == hq.top()) {
            hq.pop(); lazy.pop();
        }
        assert(!hq.empty());
        return hq.top();
    }

    void erase(T key) {
        lazy.push(key);
    }

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


template<typename T>
class DeletableMaxHeap {
private:
    priority_queue<T> hq, lazy;

public:
    DeletableMaxHeap() {}
    DeletableMaxHeap(vector<T> a) {
        hq = priority_queue<T>(a.begin(), a.end());
    }

    void push(T key) {
        hq.push(key);
    }

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

    T get_max() {
        assert(!hq.empty());
        while (!lazy.empty() && lazy.top() == hq.top()) {
            hq.pop(); lazy.pop();
        }
        assert(!hq.empty());
        return hq.top();
    }

    void erase(T key) {
        lazy.push(key);
    }

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
