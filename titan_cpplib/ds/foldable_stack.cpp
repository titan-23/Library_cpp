/// https://github.com/titan-23/Library_cpp/blob/main/titan_cpplib/ds/foldable_stack.cpp
#pragma once

#include <iostream>
#include <vector>
#include <cassert>
#include "titan_cpplib/others/print.cpp"
using namespace std;

namespace titan23 {

/// @brief 全要素の総積を O(1) で返すスタック / is_front が true なら新しい要素を左から掛ける
template <class T, T (*op)(T, T), T (*e)(), bool is_front>
class FoldableStack {
private:

public:
    vector<T> v, data;  // v は要素、data[i] は v の先頭 i 個の総積

    FoldableStack() {
        data.push_back(e());
    }

    /// @brief x を積む / 償却 O(1)
    void push(T x) {
        v.push_back(x);
        if (is_front) {
            data.push_back(op(x, data.back()));
        } else {
            data.push_back(op(data.back(), x));
        }
    }

    /// @brief 最後に積んだ要素を削除する / O(1)
    void pop() {
        assert(!v.empty());
        v.pop_back();
        data.pop_back();
    }

    /// @brief 全要素の総積を返す / O(1)
    T all_prod() const {
        return data.back();
    }

    bool empty() const { return v.empty(); }
    int size() const { return v.size(); }
    /// @brief 全要素を削除する
    void clear() {
        v.clear();
        data.resize(1, e());
    }

    friend ostream& operator<<(ostream& os, const FoldableStack<T, op, e, is_front> &st) {
        return os << st.v;
    }
};
} // namespace titan23
