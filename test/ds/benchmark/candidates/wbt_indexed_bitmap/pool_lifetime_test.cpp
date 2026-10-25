// g++ -std=c++17 -O2 -I. test/ds/benchmark/candidates/wbt_indexed_bitmap/pool_lifetime_test.cpp -o /tmp/wbt_indexed_test
#include <cassert>
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>
#include "_wbt_indexed_pool.cpp"

template<bool Vector>
struct Node {
    using Handle = titan23::detail::WBIndexHandle<Node, Vector, 2>;
    inline static int alive = 0, copies_until_throw = -1;
    inline static bool fail = false;
    Handle left = nullptr, right = nullptr;
    std::string value;
    explicit Node(const std::string& text) : value(text) {
        if (fail) throw std::runtime_error("constructor failure");
        ++alive;
    }
    Node(const Node& other) : left(other.left), right(other.right), value(other.value) {
        if (copies_until_throw == 0) throw std::runtime_error("copy failure");
        if (copies_until_throw > 0) --copies_until_throw;
        ++alive;
    }
    Node(Node&& other) noexcept(false) : left(other.left), right(other.right), value(std::move(other.value)) { ++alive; }
    ~Node() { --alive; }
};

template<bool Vector>
void test() {
    using N = Node<Vector>;
    using H = typename N::Handle;
    static_assert(sizeof(H) == sizeof(std::uint32_t));
    auto& pool = H::storage();
    pool.clear();
    std::vector<H> nodes;
    H first = pool.emplace(std::string(90, 'a'));
    nodes.push_back(first);
    N* address = first.operator->();
    std::string* reference = &first->value;
    while (pool.size() < pool.capacity()) nodes.push_back(pool.emplace("fill"));
    // The new value aliases a live element while the underlying vector grows.
    H alias = pool.emplace(first->value);
    nodes.push_back(alias);
    assert(alias->value == std::string(90, 'a') && first->value == alias->value);
    for (int i = 0; i < 300; ++i) nodes.push_back(pool.emplace(std::to_string(i)));
    if constexpr (!Vector) {
        assert(first.operator->() == address && &first->value == reference);
        assert(*reference == std::string(90, 'a'));
    }
    pool.release(nodes.back());
    const auto free_index = nodes.back().index;
    nodes.pop_back();
    const int live = N::alive;
    N::fail = true;
    bool failed = false;
    try { pool.emplace("throw"); } catch (const std::runtime_error&) { failed = true; }
    N::fail = false;
    assert(failed && N::alive == live);
    H restored = pool.emplace("restored");
    assert(restored.index == free_index && restored->value == "restored");
    nodes.push_back(restored);
    while (pool.size() < pool.capacity()) nodes.push_back(pool.emplace("fill"));
    const auto size = pool.size();
    const int alive = N::alive;
    if constexpr (Vector) N::copies_until_throw = 1;
    else N::fail = true;
    failed = false;
    try { pool.emplace("fresh"); } catch (const std::runtime_error&) { failed = true; }
    N::fail = false;
    N::copies_until_throw = -1;
    assert(failed && pool.size() == size && N::alive == alive);
    assert(first->value == std::string(90, 'a'));
    nodes.push_back(pool.emplace("fresh"));
    pool.reserve(pool.capacity() * 2);
    assert(first->value == std::string(90, 'a'));
    if constexpr (!Vector) assert(first.operator->() == address && &first->value == reference);
    for (std::size_t i = 0; i < nodes.size(); i += 2) pool.release(nodes[i]);
    pool.clear();
    assert(N::alive == 0);
    H again = pool.emplace("again");
    assert(again.index == 1 && again->value == "again");
    pool.release(again);
    pool.clear();
    assert(N::alive == 0);
}

int main() {
    test<false>();
    std::cout << "indexed_pool: OK\n";
}
