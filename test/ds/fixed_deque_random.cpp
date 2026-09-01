#include <algorithm>
#include <cassert>
#include <deque>
#include <random>
#include <vector>

#include "titan_cpplib/ds/fixed_deque.cpp"

using namespace std;

template<class T>
void verify(const titan23::FixedDeque<T>& actual, const deque<T>& expected) {
    assert(actual.size() == (int)expected.size());
    assert(actual.empty() == expected.empty());
    assert(actual.full() == ((int)expected.size() == actual.capacity()));
    for (int i = 0; i < (int)expected.size(); ++i) assert(actual[i] == expected[i]);
    assert(actual.tovector() == vector<T>(expected.begin(), expected.end()));

    vector<T> from_segments;
    auto [first, second] = actual.segments();
    from_segments.insert(from_segments.end(), first.begin(), first.end());
    from_segments.insert(from_segments.end(), second.begin(), second.end());
    assert(from_segments == vector<T>(expected.begin(), expected.end()));
}

void random_test(int capacity, uint32_t seed) {
    mt19937 rng(seed);
    titan23::FixedDeque<int> actual(capacity);
    deque<int> expected;
    for (int q = 0; q < 100000; ++q) {
        int op = rng() % 6;
        if ((op == 0 || op == 1) && (int)expected.size() == capacity) op += 2;
        if ((op >= 2 && op <= 3) && expected.empty()) op -= 2;
        int value = static_cast<int>(rng());
        if (op == 0) {
            actual.push_front(value);
            expected.push_front(value);
        } else if (op == 1) {
            actual.push_back(value);
            expected.push_back(value);
        } else if (op == 2) {
            assert(actual.front() == expected.front());
            actual.pop_front();
            expected.pop_front();
        } else if (op == 3) {
            assert(actual.back() == expected.back());
            actual.pop_back();
            expected.pop_back();
        } else if (op == 4) {
            titan23::FixedDeque<int> copy(actual);
            verify(copy, expected);
            actual = copy;
        } else {
            verify(actual, expected);
        }
        if ((q & 255) == 0) verify(actual, expected);
    }
    verify(actual, expected);
}

void zero_capacity_test() {
    titan23::FixedDeque<int> deque(0);
    assert(deque.empty());
    assert(deque.full());
    assert(deque.capacity() == 0);
    assert(deque.segments().first.empty());
    assert(deque.segments().second.empty());
}

void deque_constructor_test() {
    deque<int> expected = {3, 1, 4, 1, 5};
    titan23::FixedDeque<int> actual(expected);
    verify(actual, expected);
    assert(actual.full());
}

int main() {
    for (int capacity : {1, 2, 3, 7, 8, 31, 100}) {
        for (uint32_t seed = 0; seed < 10; ++seed) {
            random_test(capacity, seed);
        }
    }
    zero_capacity_test();
    deque_constructor_test();
}
