#include <iostream>
#include <vector>
#include <cassert>
#include <algorithm>
using namespace std;

namespace titan23 {

template<typename T>
struct Deque {
    vector<T> front_vec, back_vec;

    Deque() {}

    Deque(vector<T> a) : back_vec(a) {}

    void _rebuild() {
        int n = (int)(front_vec.size() + back_vec.size()) / 2;
        int idx_front = 0, idx_back = 0;
        vector<T> new_front, new_back;
        new_front.reserve(n);
        new_back.reserve(n);
        for (int i = 0; i < n; ++i) {
            if (idx_front < front_vec.size()) {
                new_front.emplace_back(front_vec[idx_front++]);
            } else if (idx_back < back_vec.size()) {
                new_back.emplace_back(back_vec[idx_back++]);
            }
        }
        reverse(new_front.begin(), new_front.end());
        for (int i = 0; i < n; ++i) {
            if (idx_front < front_vec.size()) {
                new_front.emplace_back(front_vec[idx_front++]);
            } else if (idx_back < back_vec.size()) {
                new_back.emplace_back(back_vec[idx_back++]);
            }
        }
        this->front_vec = new_front;
        this->back_vec = new_back;
    }

    void push_back(const T v) {
        back_vec.push_back(v);
    }

    void push_front(const T v) {
        front_vec.push_back(v);
    }

    T pop_back() {
        if (back_vec.empty()) _rebuild();
        T res;
        if (back_vec.empty()) {
            res = front_vec.back();
            front_vec.pop_back();
        } else {
            res = back_vec.back();
            back_vec.pop_back();
        }
        return res;
    }

    T pop_front() {
        if (front_vec.empty()) _rebuild();
        T res;
        if (front_vec.empty()) {
            res = back_vec.back();
            back_vec.pop_back();
        } else {
            res = front_vec.back();
            front_vec.pop_back();
        }
        return res;
    }

    T front() const {
        return (*this)[0];
    }

    T back() const {
        return (*this)[size()-1];
    }

    vector<T> tovector() const {
        vector<T> a;
        a.reserve(front_vec.size() + back_vec.size());
        for (int i = front_vec.size() - 1; i >= 0; i--) {
            a.emplace_back(front_vec[i]);
        }
        for (int i = 0; i < back_vec.size(); i++) {
            a.emplace_back(back_vec[i]);
        }
        return a;
    }

    T& operator[](int k) {
        if (k < 0) k += size();
        return k < front_vec.size() ? front_vec[front_vec.size() - k - 1] : back_vec[k - front_vec.size()];
    }

    const T& operator[](int k) const {
        if (k < 0) k += size();
        return k < front_vec.size() ? front_vec[front_vec.size() - k - 1] : back_vec[k - front_vec.size()];
    }

    int size() const {
        return (int)(front_vec.size() + back_vec.size());
    }

    bool empty() const {
        return (front_vec.size() + back_vec.size()) == 0;
    }

    bool contains(const T v) const {
        for (const T &a : front_vec) {
            if (a == v) return true;
        }
        for (const T &a : back_vec) {
            if (a == v) return true;
        }
        return false;
    }

    void print() const {
        vector<T> a = tovector();
        cout << "Deque([";
        for (int i = 0; i < size()-1; ++i) {
            cout << a[i] << ", ";
        }
        if (size() > 0) {
            cout << a.back();
        }
        cout << "])\n";
    }
};
}  // namespace titan23
