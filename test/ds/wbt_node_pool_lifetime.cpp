#include <cassert>
#include <cstdint>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <vector>
#include "titan_cpplib/ds/_wbt_node_pool.cpp"

struct Resource {
    inline static int alive = 0;
    int value;
    explicit Resource(int value) : value(value) { ++alive; }
    ~Resource() { --alive; }
};
struct Base {
    inline static int alive = 0;
    std::string text;
    explicit Base(int value) : text(80, char('a' + value % 26)) { ++alive; }
    virtual ~Base() { --alive; }
    virtual int get() const = 0;
};
template<std::size_t Alignment>
struct alignas(Alignment) Node : Base {
    inline static int alive = 0;
    inline static bool fail = false;
    std::unique_ptr<Resource> resource;
    Node *left = nullptr, *right = nullptr;
    int value;
    explicit Node(int value) : Base(value), resource(std::make_unique<Resource>(value)), value(value) {
        if (fail) throw std::runtime_error("requested constructor failure");
        ++alive;
    }
    ~Node() override { --alive; }
    int get() const override { return value + resource->value; }
};

template<std::size_t Alignment>
void exercise_pool() {
    using N = Node<Alignment>;
    static_assert(!std::is_standard_layout<N>::value);
    struct PreviousSlotLayout {
        alignas(N) unsigned char storage[sizeof(N)];
        bool live;
    };
    {
        titan23::detail::WBNodePool<N, 2> pool;
        std::vector<N*> nodes;
        for (int i = 0; i < 11; ++i) {
            N* node = pool.emplace(i);
            assert(node->get() == 2 * i);
            assert(reinterpret_cast<std::uintptr_t>(node) % alignof(N) == 0);
            nodes.push_back(node);
        }
        assert(reinterpret_cast<std::uintptr_t>(nodes[1]) - reinterpret_cast<std::uintptr_t>(nodes[0])
               == sizeof(PreviousSlotLayout));
        for (int index : {2, 7, 10}) pool.release(nodes[index]);
        assert(N::alive == 8 && Base::alive == 8 && Resource::alive == 8);
        N::fail = true;
        bool failed = false;
        try { pool.emplace(70); } catch (const std::runtime_error&) { failed = true; }
        N::fail = false;
        assert(failed && N::alive == 8 && Base::alive == 8 && Resource::alive == 8);
        N* reused = pool.emplace(71);
        assert(reused == nodes[10] && reused->get() == 142);
        nodes[10] = reused;
        nodes[7] = pool.emplace(72);
        nodes[2] = pool.emplace(73);
        assert(N::alive == 11 && Base::alive == 11 && Resource::alive == 11);

        // The free list is empty; a failing fresh slot must not enter used/live.
        N::fail = true;
        failed = false;
        try { pool.emplace(74); } catch (const std::runtime_error&) { failed = true; }
        N::fail = false;
        assert(failed && N::alive == 11 && Base::alive == 11 && Resource::alive == 11);
        nodes.push_back(pool.emplace(75));
        assert(nodes.back()->get() == 150);
        pool.release(nodes.back());
        nodes.pop_back();
        for (int i = 0; i < 1000; ++i) {
            N* node = pool.emplace(i);
            assert(node->get() == 2 * i);
            pool.release(node);
        }
        assert(N::alive == 11 && Base::alive == 11 && Resource::alive == 11);
        for (std::size_t i = 0; i < nodes.size(); i += 2) pool.release(nodes[i]);
        assert(N::alive == 5 && Base::alive == 5 && Resource::alive == 5);
        // Destruction visits the remaining live union members and skips links.
    }
    assert(N::alive == 0 && Base::alive == 0 && Resource::alive == 0);
}

int main() {
    exercise_pool<alignof(void*)>();
    exercise_pool<64>();
    std::cout << "wbt_node_pool_lifetime: OK\n";
}
