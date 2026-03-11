#include <iostream>
#include <vector>
#include <cassert>
#include "titan_cpplib/ds/foldable_stack.cpp"
#include "titan_cpplib/others/print.cpp"
using namespace std;

namespace titan23 {

template<class T, T (*op)(T, T), T (*e)()>
class FoldableDeque {
private:
    FoldableStack<T, op, e, true> front;
    FoldableStack<T, op, e, false> back;

    void rebuild() {
        vector<T> a;
        a.reserve(front.size() + back.size());
        for (auto it = front.v.rbegin(); it != front.v.rend(); ++it) {
            a.push_back(*it);
        }
        for (const auto& x : back.v) {
            a.push_back(x);
        }

        front.clear();
        back.clear();

        int m = a.size() / 2;
        for (int i = m-1; i >= 0; --i) {
            front.push(a[i]);
        }
        for (int i = m; i < (int)a.size(); ++i) {
            back.push(a[i]);
        }
    }

public:
    FoldableDeque() {}
    FoldableDeque(const vector<T> &a) {
        for (const auto &v : a) {
            back.push(v);
        }
    }

    vector<T> tovector() const {
        vector<T> res;
        res.reserve(front.size() + back.size());
        for (auto it = front.v.rbegin(); it != front.v.rend(); ++it) {
            res.push_back(*it);
        }
        for (const auto &x : back.v) {
            res.push_back(x);
        }
        return res;
    }

    void append(T v) {
        back.push(v);
    }

    void appendleft(T v) {
        front.push(v);
    }

    T pop() {
        assert(!empty());
        if (back.empty()) rebuild();
        if (!back.empty()) {
            T v = back.v.back();
            back.pop();
            return v;
        } else {
            T v = front.v.back();
            front.pop();
            return v;
        }
    }

    T popleft() {
        assert(!empty());
        if (front.empty()) rebuild();
        if (!front.empty()) {
            T v = front.v.back();
            front.pop();
            return v;
        } else {
            T v = back.v.back();
            back.pop();
            return v;
        }
    }

    T all_prod() const {
        return op(front.all_prod(), back.all_prod());
    }

    int size() const {
        return front.size() + back.size();
    }

    bool empty() const {
        return front.empty() && back.empty();
    }

    friend ostream& operator<<(ostream& os, const FoldableDeque<T, op, e> &dq) {
        return os << dq.tolist();
    }
};
} // namespace titan23
