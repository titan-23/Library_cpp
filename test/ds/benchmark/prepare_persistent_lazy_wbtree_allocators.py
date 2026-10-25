#!/usr/bin/env python3
"""Generate otherwise identical contiguous and chunked pool candidates.

The source must use vector<Node> tree and vector<Data> data. By default, the
preserved direct-operation vector implementation is used. Each candidate
retains those indexing interfaces and changes only their storage. Fixed-size
blocks allocate raw storage and construct only appended elements, making the
pool suitable for nontrivial payloads without eager block initialization.

python3 test/ds/benchmark/prepare_persistent_lazy_wbtree_allocators.py /tmp/plwbt-allocators
"""
import argparse
from pathlib import Path
import re

BLOCK_VECTOR = '''        // Fixed-size raw blocks keep node addresses stable while the pool grows.
        // Only [0, size_) contains live U objects; reserve never constructs U.
        template <class U, unsigned BlockBits>
        class BlockVector {
            static constexpr size_t block_size = size_t(1) << BlockBits;
            static constexpr size_t block_mask = block_size - 1;
            using Alloc = std::allocator<U>;
            using Traits = std::allocator_traits<Alloc>;
            Alloc allocator_;
            std::vector<U*> blocks_;
            size_t size_ = 0;

            void add_block() {
                U *block = Traits::allocate(allocator_, block_size);
                try {
                    blocks_.push_back(block);
                } catch (...) {
                    Traits::deallocate(allocator_, block, block_size);
                    throw;
                }
            }

            void clear_storage() noexcept {
                if constexpr (!std::is_trivially_destructible<U>::value) {
                    for (size_t i = 0; i < size_; ++i) {
                        Traits::destroy(allocator_, &(*this)[i]);
                    }
                }
                for (U *block : blocks_) {
                    Traits::deallocate(allocator_, block, block_size);
                }
                blocks_.clear();
                size_ = 0;
            }

        public:
            BlockVector() = default;
            BlockVector(const BlockVector&) = delete;
            BlockVector& operator=(const BlockVector&) = delete;
            BlockVector(BlockVector&& other) noexcept
                : blocks_(std::move(other.blocks_)), size_(std::exchange(other.size_, 0)) {
                other.blocks_.clear();
            }
            BlockVector& operator=(BlockVector&& other) noexcept {
                if (this != &other) {
                    clear_storage();
                    blocks_ = std::move(other.blocks_);
                    size_ = std::exchange(other.size_, 0);
                    other.blocks_.clear();
                }
                return *this;
            }
            ~BlockVector() { clear_storage(); }

            U& operator[](size_t i) noexcept {
                return blocks_[i >> BlockBits][i & block_mask];
            }
            const U& operator[](size_t i) const noexcept {
                return blocks_[i >> BlockBits][i & block_mask];
            }
            size_t size() const noexcept { return size_; }
            size_t capacity() const noexcept { return blocks_.size() * block_size; }

            void reserve(size_t n) {
                const size_t required = n / block_size + (n % block_size != 0);
                blocks_.reserve(required);
                while (blocks_.size() < required) add_block();
            }

            template <class... Args>
            U& emplace_back(Args&&... args) {
                if (size_ == capacity()) add_block();
                U *value = blocks_[size_ >> BlockBits] + (size_ & block_mask);
                Traits::construct(allocator_, value, std::forward<Args>(args)...);
                ++size_;
                return *value;
            }
            void push_back(const U& value) { emplace_back(value); }
            void pop_back() noexcept {
                assert(size_ != 0);
                --size_;
                Traits::destroy(allocator_, &(*this)[size_]);
            }
        };
'''


AOS_ALLOCATOR = '''    struct MemoryAllocator {
        struct Node {
            SizeType left, right, size;
            Node() : left(0), right(0), size(0) {}
            Node(SizeType l, SizeType r, SizeType s) : left(l), right(r), size(s) {}
        };
        struct Data {
            T key, data;
            F lazy;
            int8_t rev;
            Data() : rev(0) {}
            Data(const T& k, const T& d, const F& f, int8_t r)
                : key(k), data(d), lazy(f), rev(r) {}
        };
BLOCK_VECTOR
        struct Slot {
            Node tree;
            Data data;
            Slot(SizeType l, SizeType r, SizeType s, const T& key,
                 const T& aggregate, const F& lazy, int8_t rev)
                : tree(l, r, s), data(key, aggregate, lazy, rev) {}
        };
        using Pool = BlockVector<Slot, BLOCK_BITS>;
        Pool pool;

        template<class U, U Slot::* Member>
        struct View {
            Pool *pool;
            U& operator[](size_t i) noexcept { return (*pool)[i].*Member; }
            const U& operator[](size_t i) const noexcept { return (*pool)[i].*Member; }
            size_t size() const noexcept { return pool->size(); }
            size_t capacity() const noexcept { return pool->capacity(); }
            void reserve(size_t n) { pool->reserve(n); }
        };
        View<Node, &Slot::tree> tree;
        View<Data, &Slot::data> data;
        size_t ptr, cap;

        MemoryAllocator() : tree{&pool}, data{&pool}, ptr(1), cap(0) {
            pool.emplace_back(0, 0, 0, T{}, T{}, F{}, 0);
            cap = pool.capacity();
        }
        MemoryAllocator(MemoryAllocator&& other) noexcept
            : pool(std::move(other.pool)), tree{&pool}, data{&pool},
              ptr(other.ptr), cap(other.cap) {}
        MemoryAllocator& operator=(MemoryAllocator&& other) noexcept {
            if (this != &other) {
                pool = std::move(other.pool);
                ptr = other.ptr;
                cap = other.cap;
            }
            return *this;
        }
        SizeType copy(SizeType node) {
            if (!node) return 0;
            if (pool.size() > ptr) {
                pool[ptr] = pool[node];
            } else {
                try {
                    pool.push_back(pool[node]);
                } catch (...) {
                    cap = pool.capacity();
                    throw;
                }
                cap = pool.capacity();
            }
            return ptr++;
        }
        SizeType new_node(const T& key, const F& f) {
            if (pool.size() > ptr) {
                pool[ptr].tree = {0, 0, 1};
                pool[ptr].data = {key, key, f, 0};
            } else {
                try {
                    pool.emplace_back(0, 0, 1, key, key, f, 0);
                } catch (...) {
                    cap = pool.capacity();
                    throw;
                }
                cap = pool.capacity();
            }
            return ptr++;
        }
        void reserve(SizeType n) {
            pool.reserve(n);
            cap = pool.capacity();
        }
        void reset() { ptr = 1; }
        bool almost_full() const { return (double)ptr > max(1.0, cap * 0.99); }
        size_t capacity_bytes() const { return pool.capacity() * sizeof(Slot); }
    };
'''

def blocked_source(source, bits):
    declarations = r"(?P<indent>[ \t]*)(?:std::)?vector<Node> tree;\n[ \t]*(?:std::)?vector<Data> data;"
    match = re.search(declarations, source)
    if not match:
        raise ValueError("source does not contain contiguous Node/Data vectors")
    fields = (f"        BlockVector<Node, {bits}> tree;\n"
              f"        BlockVector<Data, {bits}> data;")
    result = source[:match.start()] + BLOCK_VECTOR + "\n" + fields + source[match.end():]
    # The sentinel allocates the first blocks, so their actual capacity is
    # already larger than the contiguous allocator's initial cap of one.
    sentinel = '            data.emplace_back(T{}, T{}, F{}, 0);'
    if sentinel not in result:
        raise ValueError('source allocator has an unexpected sentinel constructor')
    result = result.replace(sentinel, sentinel + '\n            cap = min(tree.capacity(), data.capacity());', 1)
    # Appending Data can throw after Node has been constructed. Keep both
    # pools at the same logical size so the allocator can be used again.
    for append in ('data.push_back(data[node]);', 'data.emplace_back(key, key, f, 0);'):
        if result.count(append) != 1:
            raise ValueError('source allocator has an unexpected Data append')
        guarded = ('try {\n'
                   f'                    {append}\n'
                   '                } catch (...) {\n'
                   '                    tree.pop_back();\n'
                   '                    cap = min(tree.capacity(), data.capacity());\n'
                   '                    throw;\n'
                   '                }')
        result = result.replace(append, guarded, 1)
    return result.replace('#pragma once', '#pragma once\n#include <type_traits>\n#include <utility>', 1)


def aos_source(source, bits):
    start = source.index('    struct MemoryAllocator {')
    opening = source.index('{', start)
    depth = 1
    pos = opening + 1
    while depth:
        depth += (source[pos] == '{') - (source[pos] == '}')
        pos += 1
    assert source[pos] == ';'
    allocator = AOS_ALLOCATOR.replace('BLOCK_VECTOR', BLOCK_VECTOR).replace('BLOCK_BITS', str(bits))
    result = source[:start] + allocator.rstrip() + source[pos+1:]
    return result.replace('#pragma once', '#pragma once\n#include <type_traits>\n#include <utility>', 1)


def direct_pool_access(source):
    """Rewrite indexed accesses, including indexes that themselves access ma."""
    access = re.compile(r'\bma\.(tree|data)\s*\[')
    pieces = []
    cursor = 0
    while match := access.search(source, cursor):
        pieces.append(source[cursor:match.start()])
        opening = match.end() - 1
        pos = opening + 1
        depth = 1
        while depth:
            depth += (source[pos] == '[') - (source[pos] == ']')
            pos += 1
        index = direct_pool_access(source[opening+1:pos-1])
        pieces.append(f'ma.pool[{index}].{match.group(1)}')
        cursor = pos
    pieces.append(source[cursor:])
    return ''.join(pieces)


def aligned_blocks(source):
    """Align block bases to at least 64 bytes without padding each element."""
    source = source.replace(
        '            using Alloc = std::allocator<U>;',
        '            static constexpr size_t block_alignment = alignof(U) > 64 ? alignof(U) : 64;\n'
        '            static_assert(block_size <= size_t(-1) / sizeof(U), "block too large");\n'
        '            using Alloc = std::allocator<U>;', 1)
    source = source.replace(
        'Traits::allocate(allocator_, block_size)',
        'static_cast<U*>(::operator new(sizeof(U) * block_size, std::align_val_t(block_alignment)))')
    source = source.replace(
        'Traits::deallocate(allocator_, block, block_size)',
        '::operator delete(block, std::align_val_t(block_alignment))')
    return source.replace('#pragma once', '#pragma once\n#include <new>', 1)


def fixed_table_blocks(source):
    table = '''            // A 32-bit node index needs only a modest pointer table.
            // Keep wider index types on the dynamically sized table.
            template<class P, size_t N>
            class FixedTable {
                P values_[N];
                size_t size_ = 0;
            public:
                FixedTable() = default;
                FixedTable(FixedTable&& other) noexcept : size_(other.size_) {
                    std::copy_n(other.values_, size_, values_);
                    other.size_ = 0;
                }
                FixedTable& operator=(FixedTable&& other) noexcept {
                    if (this != &other) {
                        size_ = other.size_;
                        std::copy_n(other.values_, size_, values_);
                        other.size_ = 0;
                    }
                    return *this;
                }
                P& operator[](size_t i) noexcept { return values_[i]; }
                const P& operator[](size_t i) const noexcept { return values_[i]; }
                P* begin() noexcept { return values_; }
                P* end() noexcept { return values_ + size_; }
                size_t size() const noexcept { return size_; }
                void clear() noexcept { size_ = 0; }
                void reserve(size_t n) {
                    if (n > N) throw std::bad_alloc();
                }
                void push_back(P value) {
                    if (size_ == N) throw std::bad_alloc();
                    values_[size_++] = value;
                }
            };
            static constexpr bool fixed_table = std::numeric_limits<SizeType>::digits <= 32;
            static constexpr size_t table_size = fixed_table
                ? (size_t(std::numeric_limits<SizeType>::max()) >> BlockBits) + 1 : 1;
            using BlockTable = std::conditional_t<fixed_table,
                FixedTable<U*, table_size>, std::vector<U*>>;
            BlockTable blocks_;'''
    if source.count('            std::vector<U*> blocks_;') != 1:
        raise ValueError('source does not have the expected dynamic block table')
    source = source.replace('            std::vector<U*> blocks_;', table, 1)
    return source.replace('#pragma once', '#pragma once\n#include <limits>', 1)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('output', type=Path)
    parser.add_argument('--source', type=Path)
    parser.add_argument('--aos', action='store_true', help='also generate combined Node/Data pools')
    parser.add_argument('--aos-direct', action='store_true',
                        help='also generate AoS with direct pool access inside tree methods')
    parser.add_argument('--align64', action='store_true',
                        help='also generate pools with block bases aligned to at least 64 bytes')
    parser.add_argument('--fixed-table', action='store_true',
                        help='also use inline block tables for node indexes of at most 32 bits')
    parser.add_argument('--bits', type=int, nargs='+', default=[12, 14, 16],
                        help='log2 of elements per block (default: 12 14 16)')
    args = parser.parse_args()
    if any(bits < 1 or bits > 24 for bits in args.bits):
        parser.error('--bits must be between 1 and 24')
    root = Path(__file__).resolve().parents[3]
    source_path = args.source or root / 'test/ds/benchmark/baseline/persistent_lazy_wbtree_fast_vector.cpp'
    source = source_path.read_text()
    args.output.mkdir(parents=True, exist_ok=True)
    variants = [('vector', source)]
    variants += [(f'blocked_{1 << bits}', blocked_source(source, bits)) for bits in args.bits]
    if args.aos:
        variants += [(f'blocked_aos_{1 << bits}', aos_source(source, bits)) for bits in args.bits]
    if args.aos_direct:
        variants += [(f'blocked_aos_direct_{1 << bits}', direct_pool_access(aos_source(source, bits)))
                     for bits in args.bits]
    if args.align64:
        variants += [(name + '_align64', aligned_blocks(result))
                     for name, result in variants if name != 'vector']
    if args.fixed_table:
        variants += [(name + '_fixed_table', fixed_table_blocks(result))
                     for name, result in variants if name != 'vector']
    for name, result in variants:
        target = args.output / (name + '.hpp')
        target.write_text(result)
        print(target)


if __name__ == '__main__':
    main()
