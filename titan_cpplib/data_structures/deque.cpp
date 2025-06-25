#include <iostream>
#include <vector>
#include <cassert>
using namespace std;

namespace titan23 {

template<typename T>
struct Deque {
    vector<T> front, back;

    Deque() {}

    Deque(vector<T> a) : back(a) {}

    void _rebuild() {
        int n = (int)(front.size() + back.size()) / 2;
        int idx_front = 0, idx_back = 0;
        vector<T> new_front, new_back;
        new_front.reserve(n);
        new_back.reserve(n);
        for (int i = 0; i < n; ++i) {
            if (idx_front < front.size()) {
                new_front.emplace_back(front[idx_front++]);
            } else if (idx_back < back.size()) {
                new_back.emplace_back(back[idx_back++]);
            }
        }
        reverse(new_front.begin(), new_front.end());
        for (int i = 0; i < n; ++i) {
            if (idx_front < front.size()) {
                new_front.emplace_back(front[idx_front++]);
            } else if (idx_back < back.size()) {
                new_back.emplace_back(back[idx_back++]);
            }
        }
        this->front = new_front;
        this->back = new_back;
    }

    void push_back(const T v) {
        back.push_back(v);
    }

    void push_front(const T v) {
        front.push_back(v);
    }

    T pop_back() {
        if (back.empty()) _rebuild();
        T res;
        if (back.empty()) {
            res = front.back();
            front.pop_back();
        } else {
            res = back.back();
            back.pop_back();
        }
        return res;
    }

    T pop_front() {
        if (front.empty()) _rebuild();
        T res;
        if (front.empty()) {
            res = back.back();
            back.pop_back();
        } else {
            res = front.back();
            front.pop_back();
        }
        return res;
    }

    vector<T> tovector() const {
        vector<int> a;
        a.reserve(front.size() + back.size());
        for (int i = front.size() - 1; i >= 0; i--) {
            a.emplace_back(front[i]);
        }
        for (int i = 0; i < back.size(); i++) {
            a.emplace_back(back[i]);
        }
        return a;
    }

    T& operator[](int k) {
        if (k < 0) k += len();
        return k < front.size() ? front[front.size() - k - 1] : back[k - front.size()];
    }

    const T& operator()(int k) const {
        if (k < 0) k += len();
        return k < front.size() ? front[front.size() - k - 1] : back[k - front.size()];
    }

    int len() const {
        return (int)(front.size() + back.size());
    }

    bool empty() const {
        return (bool)(front.size() + back.size());
    }

    bool contains(const T v) const {
        for (const T &a : front) {
            if (a == v) return true;
        }
        for (const T &a : back) {
            if (a == v) return true;
        }
        return false;
    }
    
    void print() const {
        vector<T> a = tovector();
        cout << "Deque([";
        for (int i = 0; i < len()-1; ++i) {
            cout << a[i] << ", ";
        }
        if (len() > 0) {
            cout << a.back();
        }
        cout << "])\n";
    }
};
}  // namespace titan23
