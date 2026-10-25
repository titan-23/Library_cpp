// g++ -std=c++17 -O2 -I. test/ds/benchmark/candidates/wbt_indexed_bitmap/pool_reentrant_test.cpp -o /tmp/wbt_indexed_test
#include <cassert>
#include <iostream>
#include <stdexcept>
#include <string>
#include "_wbt_indexed_pool.cpp"

struct Node {
    using Handle = titan23::detail::WBIndexHandle<Node, false, 2>;
    inline static int alive = 0;
    inline static bool reenter = false, fail_outer = false;
    inline static Handle nested;
    Handle left, right;
    std::string value;
    explicit Node(const std::string& value) : value(value) {
        if (reenter) {
            reenter = false;
            nested = Handle::storage().emplace("nested");
            if (fail_outer) throw std::runtime_error("outer failure");
        }
        ++alive;
    }
    ~Node() { --alive; }
};

int main() {
    using H = Node::Handle;
    auto& pool = H::storage();
    pool.clear();
    for (bool fail : {false, true}) {
        H first = pool.emplace("first"), second = pool.emplace("second");
        pool.release(second);
        pool.release(first);
        Node::reenter = true;
        Node::fail_outer = fail;
        bool failed = false;
        H outer;
        try { outer = pool.emplace("outer"); }
        catch (const std::runtime_error&) { failed = true; }
        assert(failed == fail);
        assert(Node::nested.index == second.index && Node::nested->value == "nested");
        if (!fail) assert(outer.index == first.index && outer->value == "outer");
        else {
            H recovered = pool.emplace("recovered");
            assert(recovered.index == first.index && recovered->value == "recovered");
        }
        assert(Node::alive == 2);
        pool.clear();
        assert(Node::alive == 0);
    }
    for (bool fail : {false, true}) {
        pool.emplace("first");
        pool.emplace("second");
        Node::reenter = true;
        Node::fail_outer = fail;
        bool failed = false;
        H outer;
        try { outer = pool.emplace("outer"); }
        catch (const std::runtime_error&) { failed = true; }
        assert(failed == fail);
        assert(Node::nested.index == 4 && Node::nested->value == "nested");
        if (!fail) assert(outer.index == 3 && outer->value == "outer");
        else {
            H recovered = pool.emplace("recovered");
            assert(recovered.index == 3 && recovered->value == "recovered");
        }
        assert(Node::alive == 4);
        pool.clear();
        assert(Node::alive == 0);
    }
    std::cout << "indexed_bitmap_reentrant: OK\n";
}
