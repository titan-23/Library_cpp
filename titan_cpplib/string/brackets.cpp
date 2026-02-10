#pragma once
#include <iostream>
#include <vector>
#include <string>
#include <algorithm>
#include <cassert>
#include "titan_cpplib/ds/segment_tree.cpp"
using namespace std;

namespace titan23 {

class Brackets {
private:
    struct S { int sum, min; };
    static S op(S s, S t) { return {s.sum+t.sum, min(s.min, s.sum+t.min)}; }
    static S e() { return {0, (int)1e9}; }

    static S conv(char c) { return c == '(' ? S{1, 1} : S{-1, -1}; }

    int n;
    string s;
    titan23::SegmentTree<S, op, e> seg;

public:
    Brackets() : n(0) {}
    Brackets(const string &s) : n(s.size()), s(s) {
        vector<S> a(n);
        for (int i = 0; i < n; ++i) {
            assert(s[i] == '(' || s[i] == ')');
            a[i] = conv(s[i]);
        }
        seg = titan23::SegmentTree<S, op, e>(a);
    }

    /// @brief 括弧列s[l, r)が正しい括弧列かどうか判定する / O(logN)
    bool is_valid(int l, int r) const {
        assert(0 <= l && l <= r && r <= n);
        S res = seg.prod(l, r);
        return res.sum == 0 && res.min >= 0;
    }

    /// @brief s[k]を返す
    char get(int k) const {
        assert(0 <= k && k < n);
        return s[k];
    }

    /// @brief s[k]をcに更新する / O(logN)
    void set(int k, char c) {
        assert(0 <= k && k < n);
        assert(c == '(' || c == ')');
        s[k] = c;
        seg.set(k, conv(c));
    }

    /// @brief 現在の文字列を返す
    string get_string() const { return s; }

    friend ostream& operator<<(ostream& os, const titan23::Brackets &S) {
        os << S.s;
        return os;
    }
};
} // namespace titan23
