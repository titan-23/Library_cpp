/// https://github.com/titan-23/Library_cpp/blob/main/titan_cpplib/ds/_wbt_node_pool.cpp
#pragma once

#include <cassert>
#include <cstddef>
#include <new>
#include <type_traits>
#include <utility>
#include <vector>

namespace titan23::detail {

/// @brief 重み平衡木のノードを 2^BlockBits 個ずつのブロックで確保するメモリプール / 解放したスロットを再利用する
template<class Node, unsigned BlockBits = 12>
class WBNodePool {
    union Slot {
        Node node;
        Slot* next;
        unsigned char extent[sizeof(Node) + 1];

        /// @brief 空のスロットを初期化する / O(1)
        Slot() noexcept {}

        /// @brief スロット自体を破棄する / O(1)
        ~Slot() noexcept {}
    };
    static_assert(std::is_union<Slot>::value);
    static_assert(sizeof(Node) >= sizeof(Slot*));
    // Node の直後の 1 バイトで、スロットにノードが構築済みかを表す
    static bool live(const Slot* slot) noexcept {
        return reinterpret_cast<const unsigned char*>(slot)[sizeof(Node)] != 0;
    }

    static void set_live(Slot* slot, bool value) noexcept {
        // この 1 バイトは Node と next のどちらの領域よりも後ろにあり、それらを構築しても上書きされない
        reinterpret_cast<unsigned char*>(slot)[sizeof(Node)] = value;
    }
    static constexpr std::size_t block_size = std::size_t(1) << BlockBits;
    static_assert(block_size <= std::size_t(-1) / sizeof(Slot));
    static constexpr std::size_t alignment = alignof(Slot) > 64 ? alignof(Slot) : 64;
    struct Block { Slot* slots; std::size_t used; };
    std::vector<Block> blocks_;
    Slot* free_ = nullptr;

    void add_block() {
        auto* slots = static_cast<Slot*>(::operator new(sizeof(Slot) * block_size,
                                                      std::align_val_t(alignment)));
        try {
            blocks_.push_back({slots, 0});
        } catch (...) {
            ::operator delete(slots, std::align_val_t(alignment));
            throw;
        }
    }

public:
    /// @brief 空のプールを初期化する / O(1)
    WBNodePool() = default;

    /// @brief コピー構築を禁止する
    WBNodePool(const WBNodePool&) = delete;

    /// @brief コピー代入を禁止する
    WBNodePool& operator=(const WBNodePool&) = delete;

    /// @brief 使用済みの N スロットと M ブロックを破棄する / O(N + M)
    ~WBNodePool() {
        for (auto& block : blocks_) {
            for (std::size_t i = 0; i < block.used; ++i) {
                auto& slot = block.slots[i];
                if (live(&slot)) std::launder(&slot.node)->~Node();
                slot.~Slot();
            }
            ::operator delete(block.slots, std::align_val_t(alignment));
        }
    }

    /// @brief ノードを構築してポインタを返す / 償却 O(1)
    template<class... Args>
    Node* emplace(Args&&... args) {
        if (free_) {
            auto* slot = free_;
            free_ = slot->next;
            try {
                Node* node = ::new (static_cast<void*>(&slot->node)) Node(std::forward<Args>(args)...);
                set_live(slot, true);
                return node;
            } catch (...) {
                slot->next = free_;
                free_ = slot;
                throw;
            }
        }
        if (blocks_.empty() || blocks_.back().used == block_size) add_block();
        auto& block = blocks_.back();
        Slot* slot = ::new (static_cast<void*>(block.slots + block.used)) Slot();
        set_live(slot, false);
        try {
            Node* node = ::new (static_cast<void*>(&slot->node)) Node(std::forward<Args>(args)...);
            set_live(slot, true);
            ++block.used;
            return node;
        } catch (...) {
            slot->~Slot();
            throw;
        }
    }

    /// @brief ノードを破棄してスロットを再利用可能にする / O(1)
    void release(Node* node) noexcept {
        assert(node);
        // union とその直接のメンバーはポインタ相互変換可能なので、node から Slot* に戻せる
        auto* slot = reinterpret_cast<Slot*>(node);
        assert(live(slot));
        node->~Node();
        set_live(slot, false);
        slot->next = free_;
        free_ = slot;
    }
};

} // namespace titan23::detail
