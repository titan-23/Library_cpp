#pragma once
#include <vector>
#include <cassert>
#include "titan_cpplib/others/print.cpp"
using namespace std;

namespace titan23 {

template<typename T>
class DoubleEndedHeap {
private:
    vector<T> data;

    void heapify() {
        int n = data.size();
        for (int i = n-1; i >= 0; --i) {
            if ((i & 1) && data[i-1] < data[i]) {
                swap(data[i-1], data[i]);
            }
            int k = down(i);
            up(k, i);
        }
    }

    int parent(int k) const {
        return ((k >> 1) - 1) & (~1);
    }

    int down(int k) {
        int n = data.size();
        if (k & 1) {
            while ((k << 1) | (1 < n)) {
                int c = 2 * k + 3;
                if (n <= c || data[c - 2] < data[c]) {
                    c -= 2;
                }
                if (c < n && data[c] < data[k]) {
                    swap(data[k], data[c]);
                    k = c;
                } else {
                    break;
                }
            }
        } else {
            while (2 * k + 2 < n) {
                int c = 2 * k + 4;
                if (n <= c || data[c] < data[c - 2]) {
                    c -= 2;
                }
                if (c < n && data[k] < data[c]) {
                    swap(data[k], data[c]);
                    k = c;
                } else {
                    break;
                }
            }
        }
        return k;
    }

    int up(int k, int root = 1) {
        if ((k | 1) < (int)data.size() && data[k & (~1)] < data[k | 1]) {
            swap(data[k & (~1)], data[k | 1]);
            k ^= 1;
        }
        while (root < k) {
            int p = parent(k);
            if (data[p] >= data[k]) {
                break;
            }
            swap(data[p], data[k]);
            k = p;
        }
        while (root < k) {
            int p = parent(k) | 1;
            if (data[k] >= data[p]) {
                break;
            }
            swap(data[p], data[k]);
            k = p;
        }
        return k;
    }

public:
    DoubleEndedHeap() {}

    /// 構築する / O(N)のはず
    DoubleEndedHeap(const vector<T> &a) : data(a) {
        heapify();
    }

    /// keyを追加する / O(logN)
    void insert(T key) {
        data.push_back(key);
        up((int)data.size()-1);
    }

    /// 最小の要素を削除して返す / O(logN)
    T pop_min() {
        if ((int)data.size() < 3) {
            T res = data.back();
            data.pop_back();
            return res;
        }
        swap(data[1], data.back());
        T res = data.back();
        data.pop_back();
        int k = down(1);
        up(k);
        return res;
    }

    /// 最大の要素を削除して返す / O(logN)
    T pop_max() {
        if ((int)data.size() < 2) {
            T res = data.back();
            data.pop_back();
            return res;
        }
        swap(data[0], data.back());
        T res = data.back();
        data.pop_back();
        int k = down(0);
        up(k);
        return res;
    }

    /// 最小の要素を返す / O(1)
    T get_min() {
        return (int)data.size() < 2 ? data[0] : data[1];
    }

    /// 最大の要素を返す / O(1)
    T get_max() {
        return data[0];
    }

    void clear() {
        data.clear();
    }

    bool empty() const {
        return data.empty();
    }

    int size() const {
        return (int)data.size();
    }

    vector<T> tovector() const {
        vector<T> a = data;
        sort(a.begin(), a.end());
        return a;
    }

    friend ostream& operator<<(ostream& os, const titan23::DoubleEndedHeap<T> &hq) {
        vector<T> a = hq.tovector();
        os << a;
        return os;
    }
};

} // namespace titan23
