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
    struct Construct {};

    union Payload {
        Node node;
        std::uint32_t next;

        Payload() noexcept : next(0) {}

        ~Payload() noexcept {}
    };

    struct Slot {
        Payload payload;
        bool live = false;

        Slot() noexcept = default;

        template<class... Args>
        Slot(Construct, Args&&... args) {
            construct(std::forward<Args>(args)...);
        }

        Slot(const Slot& other) {
            if (other.live) construct(other.node());
            else payload.next = other.payload.next;
        }

        Slot(Slot&& other) noexcept(std::is_nothrow_move_constructible<Node>::value) {
            if (other.live) construct(std::move(other.node()));
            else payload.next = other.payload.next;
        }

        Slot& operator=(const Slot&) = delete;

        Slot& operator=(Slot&&) = delete;

        ~Slot() noexcept {
            if (live) node().~Node();
        }

        Node& node() noexcept { return *std::launder(&payload.node); }

        const Node& node() const noexcept { return *std::launder(&payload.node); }

        template<class... Args>
        void construct(Args&&... args) {
            ::new (static_cast<void*>(&payload.node)) Node(std::forward<Args>(args)...);
            live = true;
        }
    };

    static constexpr std::size_t block_size = std::size_t(1) << BlockBits;
    static constexpr std::size_t block_mask = block_size - 1;
    static constexpr std::size_t alignment = alignof(Slot) > 64 ? alignof(Slot) : 64;
    static_assert(block_size <= std::size_t(-1) / sizeof(Slot));
    std::vector<Slot> contiguous_;
    std::vector<Slot*> blocks_;
    std::uint32_t used_ = 0;
    std::uint32_t free_ = 0;

    WBIndexedPool() { add_sentinel(); }

    Slot& slot(std::uint32_t index) noexcept {
        if constexpr (VectorStorage) return contiguous_[index];
        else return blocks_[index >> BlockBits][index & block_mask];
    }

    const Slot& slot(std::uint32_t index) const noexcept {
        if constexpr (VectorStorage) return contiguous_[index];
        else return blocks_[index >> BlockBits][index & block_mask];
    }

    void add_block() {
        auto* memory = static_cast<Slot*>(::operator new(sizeof(Slot) * block_size,
                                                       std::align_val_t(alignment)));
        try { blocks_.push_back(memory); }
        catch (...) {
            ::operator delete(memory, std::align_val_t(alignment));
            throw;
        }
    }

    void add_sentinel() {
        if constexpr (VectorStorage) contiguous_.emplace_back();
        else {
            if (blocks_.empty()) add_block();
            ::new (static_cast<void*>(blocks_[0])) Slot();
        }
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
        assert(index && index < used_ && slot(index).live);
        return slot(index).node();
    }

    /// @brief 使用したスロット数を返す / O(1)
    std::size_t size() const noexcept { return used_; }

    /// @brief 確保済みのスロット容量を返す / O(1)
    std::size_t capacity() const noexcept {
        if constexpr (VectorStorage) return contiguous_.capacity();
        else return blocks_.size() * block_size;
    }

    /// @brief 確保済み領域のバイト数を返す / O(1)
    std::size_t capacity_bytes() const noexcept { return capacity() * sizeof(Slot); }

    /// @brief N スロット分の容量を確保する / O(N)
    void reserve(std::size_t count) {
        if constexpr (VectorStorage) contiguous_.reserve(count);
        else {
            const std::size_t required = count / block_size + (count % block_size != 0);
            blocks_.reserve(required);
            while (blocks_.size() < required) add_block();
        }
    }

    /// @brief ノードを構築してハンドルを返す / 償却 O(1)
    template<class... Args>
    Handle emplace(Args&&... args) {
        if (free_) {
            const std::uint32_t index = free_;
            auto& reusable = slot(index);
            const std::uint32_t next = reusable.payload.next;
            try { reusable.construct(std::forward<Args>(args)...); }
            catch (...) { reusable.payload.next = next; throw; }
            free_ = next;
            return Handle(index);
        }
        if (!used_) add_sentinel();
        assert(used_ != std::numeric_limits<std::uint32_t>::max());
        const std::uint32_t index = used_;
        if constexpr (VectorStorage) {
            contiguous_.emplace_back(Construct{}, std::forward<Args>(args)...);
        } else {
            if (used_ == capacity()) add_block();
            auto* place = blocks_[used_ >> BlockBits] + (used_ & block_mask);
            ::new (static_cast<void*>(place)) Slot(Construct{}, std::forward<Args>(args)...);
        }
        ++used_;
        return Handle(index);
    }

    /// @brief ノードを破棄して領域を再利用可能にする / O(1)
    void release(Handle handle) noexcept {
        assert(handle.index && handle.index < used_);
        auto& released = slot(handle.index);
        assert(released.live);
        released.node().~Node();
        released.live = false;
        released.payload.next = free_;
        free_ = handle.index;
    }

    /// @brief 全ノードと領域を破棄する / O(N)
    /// @note この領域を使うハンドルは無効になる
    void clear() noexcept {
        if constexpr (VectorStorage) {
            std::vector<Slot>().swap(contiguous_);
        } else {
            for (std::uint32_t i = 0; i < used_; ++i) slot(i).~Slot();
            for (auto* block : blocks_) ::operator delete(block, std::align_val_t(alignment));
            blocks_.clear();
        }
        used_ = 0;
        free_ = 0;
    }
};

} // namespace titan23::detail
