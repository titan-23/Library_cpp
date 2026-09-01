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
    explicit FixedDeque(int capacity) : data(get_buffer_capacity(capacity)),
        capacity_(capacity), mask(get_buffer_capacity(capacity) - 1), head(0), size_(0) {}

    explicit FixedDeque(const deque<T>& a) : FixedDeque((int)a.size()) {
        for (const T& x : a) emplace_back(x);
    }

    bool empty() const noexcept { return size_ == 0; }
    bool full() const noexcept { return size_ == capacity_; }
    int size() const noexcept { return size_; }
    int capacity() const noexcept { return capacity_; }

    T& operator[](int i) {
        assert(i < size_);
        return data[index(i)];
    }

    const T& operator[](int i) const {
        assert(i < size_);
        return data[index(i)];
    }

    T& front() {
        assert(!empty());
        return data[head];
    }

    const T& front() const {
        assert(!empty());
        return data[head];
    }

    T& back() {
        assert(!empty());
        return data[index(size_ - 1)];
    }

    const T& back() const {
        assert(!empty());
        return data[index(size_ - 1)];
    }

    template<class... Args>
    T& emplace_back(Args&&... args) {
        assert(!full());
        T* p = &data[index(size_)];
        *p = T(forward<Args>(args)...);
        ++size_;
        return *p;
    }

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

    void push_back(const T& value) { emplace_back(value); }
    void push_back(T&& value) { emplace_back(move(value)); }
    void push_front(const T& value) { emplace_front(value); }
    void push_front(T&& value) { emplace_front(move(value)); }

    void pop_back() {
        assert(!empty());
        --size_;
    }

    void pop_front() {
        assert(!empty());
        head = (head + 1) & mask;
        --size_;
    }

    void clear() noexcept {
        head = 0;
        size_ = 0;
    }

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
