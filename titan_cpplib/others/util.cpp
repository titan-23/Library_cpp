/// https://github.com/titan-23/Library_cpp/blob/main/titan_cpplib/others/util.cpp
#pragma once

#include <vector>
#include <algorithm>
#include <cassert>
using namespace std;

/// @brief a で最初に現れる x を 1 つ削除し、削除したかを返す / O(n)
template <typename T>
bool discard_vec(vector<T> &a, const T &x) {
    auto it = find(a.begin(), a.end(), x);
    if (it == a.end()) return false;
    a.erase(it);
    return true;
}

/// @brief a で最初に現れる x を 1 つ削除する / x が含まれる前提 / O(n)
template <typename T>
void remove_vec(vector<T> &a, const T &x) {
    auto it = find(a.begin(), a.end(), x);
    assert(it != a.end());
    a.erase(it);
}

/// @brief a が x を含むかを返す / O(n)
template <typename T>
bool contains_vec(const vector<T> &a, const T &x) {
    return find(a.begin(), a.end(), x) != a.end();
}

/// @brief a で最初に x が現れる位置を返す / x が含まれる前提 / O(n)
template <typename T>
int index(const vector<T> &a, const T &x) {
    auto it = find(a.begin(), a.end(), x);
    assert(it != a.end());
    return it - a.begin();
}
