/// https://github.com/titan-23/Library_cpp/blob/main/titan_cpplib/ds/foldable_deque.cpp
#pragma once

#include <iostream>
#include <vector>
#include <cassert>
#include "titan_cpplib/ds/foldable_stack.cpp"
#include "titan_cpplib/others/print.cpp"
using namespace std;

namespace titan23 {

/// @brief 両端の追加と削除、全要素の総積を扱う両端キュー
template<class T, T (*op)(T, T), T (*e)()>
class FoldableDeque {
private:
    FoldableStack<T, op, e, true> front;
    FoldableStack<T, op, e, false> back;

    // 全要素を前半と後半に分け、front と back に積み直す
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
    /// @brief 列 a から構築する / O(n)
    FoldableDeque(const vector<T> &a) {
        for (const auto &v : a) {
            back.push(v);
        }
    }

    /// @brief 要素を先頭から順に vector で返す / O(n)
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

    /// @brief 末尾に v を追加する / 償却 O(1)
    void append(T v) {
        back.push(v);
    }

    /// @brief 先頭に v を追加する / 償却 O(1)
    void appendleft(T v) {
        front.push(v);
    }

    /// @brief 末尾の要素を削除して返す / 償却 O(1)
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

    /// @brief 先頭の要素を削除して返す / 償却 O(1)
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

    /// @brief 全要素の総積を返す / O(1)
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
        return os << dq.tovector();
    }
};
} // namespace titan23
