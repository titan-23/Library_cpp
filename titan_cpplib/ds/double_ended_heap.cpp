#pragma once
#include <vector>
#include <utility>
#include <algorithm>
#include <cassert>
#include "titan_cpplib/others/print.cpp"
using namespace std;

namespace titan23 {

template<typename T>
class DoubleEndedHeap {
public:
    vector<T> a;

private:
    void heapify() {
        int n = a.size();
        for (int i = n - 1; i >= 0; --i) {
            if ((i & 1) && a[i - 1] < a[i]) {
                std::swap(a[i - 1], a[i]);
            }
            int k = down(i);
            up(k, i);
        }
    }

    inline int parent(int k) const {
        return ((k >> 1) - 1) & (~1);
    }

    int down(int k) {
        int n = a.size();
        T temp = std::move(a[k]);
        if (k & 1) {
            while (2 * k + 1 < n) {
                int c = 2 * k + 3;
                if (n <= c || a[c - 2] < a[c]) {
                    c -= 2;
                }
                if (c < n && a[c] < temp) {
                    a[k] = std::move(a[c]);
                    k = c;
                } else {
                    break;
                }
            }
        } else {
            while (2 * k + 2 < n) {
                int c = 2 * k + 4;
                if (n <= c || a[c] < a[c - 2]) {
                    c -= 2;
                }
                if (c < n && temp < a[c]) {
                    a[k] = std::move(a[c]);
                    k = c;
                } else {
                    break;
                }
            }
        }
        a[k] = std::move(temp);
        return k;
    }

    int up(int k, int root = 1) {
        T temp = std::move(a[k]);
        if ((k | 1) < (int)a.size()) {
            int other = k ^ 1;
            bool swap_needed = (k & 1) ? (temp > a[other]) : (temp < a[other]);
            if (swap_needed) {
                a[k] = std::move(a[other]);
                k = other;
            }
        }
        while (root < k) {
            int p_max = parent(k);
            int p_min = p_max | 1;

            if (temp > a[p_max]) {
                a[k] = std::move(a[p_max]);
                k = p_max;
            } else if (temp < a[p_min]) {
                a[k] = std::move(a[p_min]);
                k = p_min;
            } else {
                break;
            }
        }
        a[k] = std::move(temp);
        return k;
    }

public:
    DoubleEndedHeap() {}

    DoubleEndedHeap(const vector<T> &v) : a(v) {
        heapify();
    }

    void reserve(int cap) {
        a.reserve(cap);
    }

    void insert(const T& key) {
        a.push_back(key);
        up((int)a.size() - 1);
    }

    T pop_min() {
        if ((int)a.size() < 3) {
            T res = std::move(a.back());
            a.pop_back();
            return res;
        }
        T res = std::move(a[1]);
        a[1] = std::move(a.back());
        a.pop_back();
        int k = down(1);
        up(k);
        return res;
    }

    T pop_max() {
        if ((int)a.size() < 2) {
            T res = std::move(a.back());
            a.pop_back();
            return res;
        }
        T res = std::move(a[0]);
        a[0] = std::move(a.back());
        a.pop_back();
        int k = down(0);
        up(k);
        return res;
    }

    const T& get_min() const {
        return (int)a.size() < 2 ? a[0] : a[1];
    }

    /// 最大の要素を pop して返し、新たに key を挿入する / O(logN)
    void replace_max(const T& key) {
        a[0] = key;
        int k = down(0);
        up(k);
    }

    T& get_min() {
        return (int)a.size() < 2 ? a[0] : a[1];
    }

    const T& get_max() const {
        return a[0];
    }

    T& get_max() {
        return a[0];
    }

    void clear() {
        a.clear();
    }

    bool empty() const {
        return a.empty();
    }

    int size() const {
        return (int)a.size();
    }

    vector<T> tovector() const {
        vector<T> b = a;
        sort(b.begin(), b.end());
        return b;
    }

    friend ostream& operator<<(ostream& os, const titan23::DoubleEndedHeap<T> &hq) {
        vector<T> b = hq.tovector();
        os << b;
        return os;
    }
};

} // namespace titan23
