#pragma once

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <new>
#include <type_traits>
#include <utility>
#include <vector>

namespace titan23::detail {

template<class Node, bool VectorStorage, unsigned BlockBits>
class WBIndexedPool;

template<class Node, bool VectorStorage = false, unsigned BlockBits = 12>
struct WBIndexHandle {
    using pool_type = WBIndexedPool<Node, VectorStorage, BlockBits>;
    std::uint32_t index = 0;

    /// @brief 空のハンドルを作成する / O(1)
    WBIndexHandle() noexcept = default;

    /// @brief 空のハンドルを作成する / O(1)
    WBIndexHandle(std::nullptr_t) noexcept {}

    /// @brief 指定した番号のハンドルを作成する / O(1)
    explicit WBIndexHandle(std::uint32_t value) noexcept : index(value) {}

    /// @brief ハンドルを空にする / O(1)
    WBIndexHandle& operator=(std::nullptr_t) noexcept {
        index = 0;
        return *this;
    }

    /// @brief ノードがあるか判定する / O(1)
    explicit operator bool() const noexcept { return index != 0; }

    /// @brief ノードへのポインタを返す / O(1)
    Node* operator->() const {
        assert(index && pool_);
        return &pool_->get(index);
    }

    /// @brief ノードへの参照を返す / O(1)
    Node& operator*() const { return *operator->(); }

    /// @brief ノード番号が等しいか判定する / O(1)
    friend bool operator==(WBIndexHandle a, WBIndexHandle b) noexcept { return a.index == b.index; }

    /// @brief ノード番号が異なるか判定する / O(1)
    friend bool operator!=(WBIndexHandle a, WBIndexHandle b) noexcept { return a.index != b.index; }

    /// @brief 空のハンドルか判定する / O(1)
    friend bool operator==(WBIndexHandle a, std::nullptr_t) noexcept { return !a.index; }

    /// @brief 空のハンドルか判定する / O(1)
    friend bool operator==(std::nullptr_t, WBIndexHandle a) noexcept { return !a.index; }

    /// @brief ノードがあるか判定する / O(1)
    friend bool operator!=(WBIndexHandle a, std::nullptr_t) noexcept { return a.index != 0; }

    /// @brief ノードがあるか判定する / O(1)
    friend bool operator!=(std::nullptr_t, WBIndexHandle a) noexcept { return a.index != 0; }

    /// @brief この型のノード領域を返す / O(1)
    static pool_type& storage() {
        if (!pool_) initialize();
        return *pool_;
    }

private:
    inline static pool_type* pool_ = nullptr;

    static void initialize() {
        static pool_type instance;
        pool_ = &instance;
    }
};

template<class Node, bool VectorStorage, unsigned BlockBits>
class WBIndexedPool {
    friend struct WBIndexHandle<Node, VectorStorage, BlockBits>;
    using Handle = WBIndexHandle<Node, VectorStorage, BlockBits>;
    static_assert(!VectorStorage);

    union Slot {
        Node node;
        std::uint32_t next;

        Slot() noexcept : next(0) {}

        ~Slot() noexcept {}

        Node& value() noexcept { return *std::launder(&node); }
    };
    static_assert(sizeof(Slot) == sizeof(Node));
    static constexpr std::size_t block_size = std::size_t(1) << BlockBits;
    static constexpr std::size_t block_mask = block_size - 1;
    static constexpr std::size_t alignment = alignof(Slot) > 64 ? alignof(Slot) : 64;
    static_assert(block_size <= std::size_t(-1) / sizeof(Slot));
    static constexpr std::size_t node_bytes = sizeof(Slot) * block_size;
    static constexpr std::size_t bitmap_words = block_size / 64 + (block_size % 64 != 0);
    static constexpr std::size_t bitmap_bytes = bitmap_words * sizeof(std::uint64_t);
    static_assert(node_bytes <= std::size_t(-1) - 7 - bitmap_bytes);
    static constexpr std::size_t bitmap_offset = (node_bytes + 7) & ~std::size_t(7);
    static constexpr std::size_t block_bytes = bitmap_offset + bitmap_bytes;
    std::vector<Slot*> blocks_;
    std::uint32_t used_ = 0;
    std::uint32_t free_ = 0;

    WBIndexedPool() { add_sentinel(); }

    static std::uint64_t* bitmap(Slot* block) noexcept {
        return reinterpret_cast<std::uint64_t*>(reinterpret_cast<unsigned char*>(block) + bitmap_offset);
    }

    Slot& slot(std::uint32_t index) noexcept {
        return blocks_[index >> BlockBits][index & block_mask];
    }

    bool live(std::uint32_t index) const noexcept {
        const std::size_t offset = index & block_mask;
        return (bitmap(blocks_[index >> BlockBits])[offset >> 6] >> (offset & 63)) & 1;
    }

    void set_live(std::uint32_t index, bool value) noexcept {
        const std::size_t offset = index & block_mask;
        auto& word = bitmap(blocks_[index >> BlockBits])[offset >> 6];
        const std::uint64_t bit = std::uint64_t(1) << (offset & 63);
        if (value) word |= bit;
        else word &= ~bit;
    }

    void add_block() {
        auto* memory = static_cast<Slot*>(::operator new(block_bytes, std::align_val_t(alignment)));
        auto* bits = bitmap(memory);
        for (std::size_t i = 0; i < bitmap_words; ++i) {
            ::new (static_cast<void*>(bits + i)) std::uint64_t(0);
        }
        try { blocks_.push_back(memory); }
        catch (...) {
            ::operator delete(memory, std::align_val_t(alignment));
            throw;
        }
    }

    void add_sentinel() {
        if (blocks_.empty()) add_block();
        ::new (static_cast<void*>(blocks_[0])) Slot();
        used_ = 1;
    }

public:
    /// @brief 領域の複製を禁止する
    WBIndexedPool(const WBIndexedPool&) = delete;

    /// @brief 領域の複製を禁止する
    WBIndexedPool& operator=(const WBIndexedPool&) = delete;

    /// @brief 全ノードと領域を破棄する / O(N)
    ~WBIndexedPool() { clear(); }

    /// @brief 指定した番号のノードへの参照を返す / O(1)
    Node& get(std::uint32_t index) {
        assert(index && index < used_ && live(index));
        return slot(index).value();
    }

    /// @brief 使用したスロット数を返す / O(1)
    std::size_t size() const noexcept { return used_; }

    /// @brief 確保済みのスロット容量を返す / O(1)
    std::size_t capacity() const noexcept { return blocks_.size() * block_size; }

    /// @brief 確保済み領域のバイト数を返す / O(1)
    std::size_t capacity_bytes() const noexcept { return blocks_.size() * block_bytes; }

    /// @brief N スロット分の容量を確保する / O(N)
    void reserve(std::size_t count) {
        const std::size_t required = count / block_size + (count % block_size != 0);
        blocks_.reserve(required);
        while (blocks_.size() < required) add_block();
    }

    /// @brief ノードを構築してハンドルを返す / 償却 O(1)
    template<class... Args>
    Handle emplace(Args&&... args) {
        if (free_) {
            const std::uint32_t index = free_;
            Slot& reusable = slot(index);
            free_ = reusable.next;
            try { ::new (static_cast<void*>(&reusable.node)) Node(std::forward<Args>(args)...); }
            catch (...) {
                reusable.next = free_;
                free_ = index;
                throw;
            }
            set_live(index, true);
            return Handle(index);
        }
        if (!used_) add_sentinel();
        assert(used_ != std::numeric_limits<std::uint32_t>::max());
        const std::uint32_t index = used_;
        if (used_ == capacity()) add_block();
        Slot* place = ::new (static_cast<void*>(&slot(index))) Slot();
        ++used_;
        try { ::new (static_cast<void*>(&place->node)) Node(std::forward<Args>(args)...); }
        catch (...) {
            if (used_ == index + 1) {
                --used_;
                place->~Slot();
            } else {
                place->next = free_;
                free_ = index;
            }
            throw;
        }
        set_live(index, true);
        return Handle(index);
    }

    /// @brief ノードを破棄して領域を再利用可能にする / O(1)
    void release(Handle handle) noexcept {
        assert(handle.index && handle.index < used_ && live(handle.index));
        auto& released = slot(handle.index);
        released.value().~Node();
        set_live(handle.index, false);
        released.next = free_;
        free_ = handle.index;
    }

    /// @brief 全ノードと領域を破棄する / O(N)
    /// @note この領域を使うハンドルは無効になる
    void clear() noexcept {
        for (std::uint32_t i = 0; i < used_; ++i) {
            auto& current = slot(i);
            if (live(i)) current.value().~Node();
            current.~Slot();
        }
        for (auto* block : blocks_) ::operator delete(block, std::align_val_t(alignment));
        blocks_.clear();
        used_ = 0;
        free_ = 0;
    }
};

} // namespace titan23::detail
