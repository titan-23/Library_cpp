#include <cassert>
#include <cstdint>
#include <iostream>
#include <limits>
#include <sstream>
#include <type_traits>
#include <vector>

#include "titan_cpplib/ds/persistent_seg_wbtree.cpp"

struct Value {
    std::int64_t sum, count;
    bool operator==(const Value &other) const {
        return sum == other.sum && count == other.count;
    }
};

Value op(Value a, Value b) { return {a.sum + b.sum, a.count + b.count}; }
Value e() { return {0, 0}; }
std::ostream &operator<<(std::ostream &out, const Value &value) { return out << value.sum; }

template<class Index>
using Tree = titan23::PersistentSegWBTree<Index, Value, op, e>;

// The packed flag must preserve every nonnegative index representable by Index.
template<class Index>
void index_bits() {
    using Unsigned = typename std::make_unsigned<Index>::type;
    typename Tree<Index>::MemoryAllocator::Node node;
    const Unsigned maximum = static_cast<Unsigned>(std::numeric_limits<Index>::max());
    node.left = maximum;
    node.rev = 1;
    assert(node.left == maximum);
    node.rev = 0;
    assert(node.left == maximum);
    node.rev = 1;
    node.left = 0;
    assert(node.rev == 1);
    node.left = maximum;
    assert(node.rev == 1 && node.left == maximum);
}

template<class Index>
void small_index_operations() {
    using T = Tree<Index>;
    T::ma.reset();
    const std::vector<Value> original = {{1, 1}, {2, 1}, {3, 1}, {4, 1}};
    T base(original);
    auto reversed = base.reverse(0, 4);
    auto changed = reversed.set(1, {9, 1});
    auto inserted = changed.insert(2, {7, 1});
    auto removed = inserted.pop(2);
    assert(removed.second == (Value{7, 1}));
    auto parts = removed.first.split(2);
    auto joined = parts.first.merge(parts.second);
    const std::vector<Value> expected = {{4, 1}, {9, 1}, {2, 1}, {1, 1}};
    const auto allocated = T::ma.ptr;
    assert(joined.tovector() == expected);
    assert(joined.prod(0, 4) == (Value{16, 4}));
    for (Index i = 0; i < 4; ++i) assert(joined.get(i) == expected[i]);
    assert(base.tovector() == original);
    assert(reversed.tovector() == (std::vector<Value>{{4, 1}, {3, 1}, {2, 1}, {1, 1}}));
    joined.check();
    assert(T::ma.ptr == allocated);
    assert(T::ma.ptr <= static_cast<std::size_t>(std::numeric_limits<Index>::max()));
}

// A DAG reaches the index type's logical-size limits with only O(log N) records.
// Do not materialize or recursively check the full logical sequence here.
template<class Index, int Doublings>
void large_shared_sequence() {
    using T = Tree<Index>;
    T::ma.reset();
    T original(std::vector<Value>(3, {1, 1}));
    for (int i = 0; i < Doublings; ++i) original = original.merge(original);
    const Index n = Index(3) * (Index(1) << Doublings);
    const Index k = n / 2;
    assert(original.len() == n);
    assert(original.prod(0, n) == (Value{static_cast<std::int64_t>(n), static_cast<std::int64_t>(n)}));
    auto changed = original.set(k, {7, 1});
    auto reversed = changed.reverse(1, n - 1);
    const Index new_k = n - 1 - k;
    assert(reversed.get(new_k) == (Value{7, 1}));
    assert(reversed.get(k) == (Value{1, 1}));
    assert(reversed.prod(0, new_k).sum == new_k);
    assert(reversed.prod(new_k + 1, n).sum == n - new_k - 1);
    auto parts = reversed.split(n / 3);
    assert(parts.first.len() == n / 3 && parts.second.len() == n - n / 3);
    auto joined = parts.first.merge(parts.second);
    assert(joined.prod(0, n).sum == static_cast<std::int64_t>(n) + 6);
    auto removed = joined.pop(n - 1);
    assert(removed.second == (Value{1, 1}));
    assert(removed.first.prod(0, n - 1).sum == static_cast<std::int64_t>(n) + 5);
    assert(original.prod(0, n).sum == n && original.get(k) == (Value{1, 1}));
    assert(T::ma.ptr < 10000);
}

void output_and_construction() {
    using T = Tree<int>;
    T::ma.reset();
    T tree = std::vector<Value>{{1, 1}, {2, 1}, {3, 1}};
    const auto copied = tree.copy();
    std::ostringstream out;
    out << copied;
    assert(out.str() == "[1, 2, 3]");
    std::ostringstream printed;
    auto *saved = std::cout.rdbuf(printed.rdbuf());
    copied.print();
    std::cout.rdbuf(saved);
    assert(printed.str() == "[1, 2, 3]\n");
    const T empty;
    std::ostringstream none;
    none << empty;
    assert(none.str() == "[]");
}

int main() {
    static_assert(sizeof(Tree<int>::MemoryAllocator::Node) == 16);
    static_assert(sizeof(Tree<int>::MemoryAllocator::Data) == 16);
    static_assert(sizeof(Tree<int>::MemoryAllocator::Slot) == 32);
    static_assert(sizeof(Tree<unsigned>::MemoryAllocator::Node) == 20);
    static_assert(sizeof(Tree<unsigned>::MemoryAllocator::Slot) == 40);
    index_bits<std::int8_t>(); index_bits<std::uint8_t>();
    index_bits<std::int16_t>(); index_bits<std::uint16_t>();
    index_bits<int>(); index_bits<unsigned>();
    index_bits<long long>(); index_bits<unsigned long long>();
    small_index_operations<std::int8_t>(); small_index_operations<std::uint8_t>();
    small_index_operations<std::int16_t>(); small_index_operations<std::uint16_t>();
    small_index_operations<int>(); small_index_operations<unsigned>();
    small_index_operations<long long>(); small_index_operations<unsigned long long>();
    large_shared_sequence<int, 29>();
    large_shared_sequence<unsigned, 30>();
    output_and_construction();
    std::cout << "persistent_seg_wbtree_leaf_extra: OK\n";
}
