/// https://github.com/titan-23/Library_cpp/blob/main/titan_cpplib/ds/fast_stack.cpp
#pragma once

#include <vector>
using namespace std;

namespace titan23 {

/// @brief 確保した領域を再利用するスタック
template<typename T>
class FastStack {
private:
    vector<T> s;
    size_t idx;

public:
    FastStack() : idx(0) {}
    /// @brief 長さ cap の領域を先に確保する / O(cap)
    FastStack(size_t cap) : s(cap), idx(0) {}
    bool empty() const { return idx == 0; }
    /// @brief v を積む / 償却 O(1)
    void emplace(T v) {
        if (idx < s.size()) {
            s[idx] = v;
        } else {
            s.emplace_back(v);
        }
        idx++;
    }
    /// @brief 全要素を削除する / 確保した領域は残す / O(1)
    void clear() { idx = 0; }
    /// @brief 最後に積んだ要素を削除する / O(1)
    void pop() { --idx; }
    /// @brief 最後に積んだ要素を返す / O(1)
    T top() const { return s[idx-1]; }
    size_t size() const { return idx; }
};
} // namespace titan23
