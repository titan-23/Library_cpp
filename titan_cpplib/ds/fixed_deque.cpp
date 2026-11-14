/// https://github.com/titan-23/Library_cpp/blob/main/titan_cpplib/ds/fixed_deque.cpp
#pragma once

#include <algorithm>
#include <bit>
#include <cassert>
#include <deque>
#include <span>
#include <utility>
#include <vector>
using namespace std;

namespace titan23 {

/// @brief 容量を固定した環状配列の両端キュー / 容量を超えて追加できない
template<class T>
class FixedDeque {
private:
    vector<T> data;
    int capacity_, mask, head, size_;

    static int get_buffer_capacity(int capacity) {
        return (int)bit_ceil((unsigned)max(capacity, 1));
    }

    int index(int i) const noexcept { return (head + i) & mask; }

public:
    /// @brief 容量 capacity の空の両端キューを作る / O(capacity)
    explicit FixedDeque(int capacity) : data(get_buffer_capacity(capacity)),
        capacity_(capacity), mask(get_buffer_capacity(capacity) - 1), head(0), size_(0) {}

    /// @brief a から構築する / 容量は a.size() になる / O(n)
    explicit FixedDeque(const deque<T>& a) : FixedDeque((int)a.size()) {
        for (const T& x : a) emplace_back(x);
    }

    bool empty() const noexcept { return size_ == 0; }
    bool full() const noexcept { return size_ == capacity_; }
    int size() const noexcept { return size_; }
    int capacity() const noexcept { return capacity_; }

    /// @brief 先頭から i 番目の要素を返す / O(1)
    T& operator[](int i) {
        assert(i < size_);
        return data[index(i)];
    }

    const T& operator[](int i) const {
        assert(i < size_);
        return data[index(i)];
    }

    /// @brief 先頭の要素を返す / O(1)
    T& front() {
        assert(!empty());
        return data[head];
    }

    const T& front() const {
        assert(!empty());
        return data[head];
    }

    /// @brief 末尾の要素を返す / O(1)
    T& back() {
        assert(!empty());
        return data[index(size_ - 1)];
    }

    const T& back() const {
        assert(!empty());
        return data[index(size_ - 1)];
    }

    /// @brief 末尾に要素を構築して追加する / O(1)
    template<class... Args>
    T& emplace_back(Args&&... args) {
        assert(!full());
        T* p = &data[index(size_)];
        *p = T(forward<Args>(args)...);
        ++size_;
        return *p;
    }

    /// @brief 先頭に要素を構築して追加する / O(1)
    template<class... Args>
    T& emplace_front(Args&&... args) {
        assert(!full());
        int new_head = (head - 1) & mask;
        T* p = &data[new_head];
        *p = T(forward<Args>(args)...);
        head = new_head;
        ++size_;
        return *p;
    }

    /// @brief 末尾に value を追加する / O(1)
    void push_back(const T& value) { emplace_back(value); }
    void push_back(T&& value) { emplace_back(move(value)); }
    /// @brief 先頭に value を追加する / O(1)
    void push_front(const T& value) { emplace_front(value); }
    void push_front(T&& value) { emplace_front(move(value)); }

    /// @brief 末尾の要素を削除する / O(1)
    void pop_back() {
        assert(!empty());
        --size_;
    }

    /// @brief 先頭の要素を削除する / O(1)
    void pop_front() {
        assert(!empty());
        head = (head + 1) & mask;
        --size_;
    }

    /// @brief 全要素を削除する / O(1)
    void clear() noexcept {
        head = 0;
        size_ = 0;
    }

    /// @brief 先頭から順に並べた要素を 2 つの連続区間に分けて返す / O(1)
    pair<span<T>, span<T>> segments() noexcept {
        if (empty()) return {span<T>(), span<T>()};
        int first_size = min(size_, mask + 1 - head);
        return {span<T>(data.data() + head, first_size), span<T>(data.data(), size_ - first_size)};
    }

    pair<span<const T>, span<const T>> segments() const noexcept {
        if (empty()) return {span<const T>(), span<const T>()};
        int first_size = min(size_, mask + 1 - head);
        return {span<const T>(data.data() + head, first_size), span<const T>(data.data(), size_ - first_size)};
    }

    /// @brief 要素を先頭から順に vector で返す / O(n)
    vector<T> tovector() const {
        vector<T> result;
        result.reserve(size_);
        auto [first, second] = segments();
        for (const T& x : first) result.emplace_back(x);
        for (const T& x : second) result.emplace_back(x);
        return result;
    }
};

} // namespace titan23
