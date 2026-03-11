#include <iostream>
#include <vector>
#include <cassert>
#include "titan_cpplib/others/print.cpp"
using namespace std;

namespace titan23 {

template <class T, T (*op)(T, T), T (*e)(), bool is_front>
class FoldableStack {
private:

public:
    vector<T> v, data;

    FoldableStack() {
        data.push_back(e());
    }

    void push(T x) {
        v.push_back(x);
        if (is_front) {
            data.push_back(op(x, data.back()));
        } else {
            data.push_back(op(data.back(), x));
        }
    }

    void pop() {
        assert(!v.empty());
        v.pop_back();
        data.pop_back();
    }

    T all_prod() const {
        return data.back();
    }

    bool empty() const { return v.empty(); }
    int size() const { return v.size(); }
    void clear() {
        v.clear();
        data.resize(1, e());
    }

    friend ostream& operator<<(ostream& os, const FoldableStack<T, op, e, is_front> &st) {
        return os << st.v;
    }
};
} // namespace titan23
