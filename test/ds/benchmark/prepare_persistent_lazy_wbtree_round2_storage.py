#!/usr/bin/env python3
"""Generate isolated block-append fast paths without changing tree algorithms.

python3 test/ds/benchmark/prepare_persistent_lazy_wbtree_round2_storage.py \
    /tmp/wbt-round2-leaf --source titan_cpplib/ds/persistent_lazy_wbtree_leaf.cpp

append_cache keeps a pointer to the next raw slot, updating it only at block
boundaries, on pop_back, and on moves. append_cache_lazy_cap additionally
refreshes MemoryAllocator.cap only when the previous capacity is exhausted.
"""
import argparse
import re
from pathlib import Path


def replace_once(source, old, new):
    if source.count(old) != 1:
        raise ValueError(f'expected one source occurrence: {old!r}')
    return source.replace(old, new, 1)


def cache_append(source):
    source = replace_once(source, '            size_t size_ = 0;',
                          '            size_t size_ = 0;\n            U *append_ = nullptr;')
    source = replace_once(source, '                blocks_.clear();\n                size_ = 0;',
                          '                blocks_.clear();\n                size_ = 0;\n                append_ = nullptr;')
    source = replace_once(source,
                          '                : blocks_(std::move(other.blocks_)), size_(std::exchange(other.size_, 0)) {',
                          '                : blocks_(std::move(other.blocks_)), size_(std::exchange(other.size_, 0)),\n'
                          '                  append_(std::exchange(other.append_, nullptr)) {')
    source = replace_once(source, '                    size_ = std::exchange(other.size_, 0);',
                          '                    size_ = std::exchange(other.size_, 0);\n'
                          '                    append_ = std::exchange(other.append_, nullptr);')
    source = replace_once(source, '''                if (size_ == capacity()) add_block();
                U *value = blocks_[size_ >> BlockBits] + (size_ & block_mask);
                Traits::construct(allocator_, value, std::forward<Args>(args)...);
                ++size_;
                return *value;''', '''                if ((size_ & block_mask) == 0) {
                    if (size_ == capacity()) add_block();
                    append_ = blocks_[size_ >> BlockBits];
                }
                U *value = append_;
                Traits::construct(allocator_, value, std::forward<Args>(args)...);
                ++size_;
                ++append_;
                return *value;''')
    source = replace_once(source, '                Traits::destroy(allocator_, &(*this)[size_]);',
                          '                Traits::destroy(allocator_, &(*this)[size_]);\n'
                          '                append_ = blocks_[size_ >> BlockBits] + (size_ & block_mask);')
    return source


def lazy_cap(source):
    old = '                cap = pool.capacity();\n            }\n            return ptr++;'
    if source.count(old) != 2:
        raise ValueError('expected copy/new_node capacity refreshes')
    return source.replace(old,
                          '                if (ptr >= cap) cap = pool.capacity();\n            }\n            return ptr++;')


SPLIT_ALLOCATOR_TAIL = '''        struct Slot {
            Node tree;
            Data data;
            Slot(SizeType l, SizeType r, SizeType s, const T& value, const F& lazy, int8_t rev)
                : tree(l, r, s), data(value, lazy, rev) {}
        };
        using Pool = BlockVector<Slot, 16>;
        using LeafPool = BlockVector<T, 16>;
        using UnsignedIndex = std::make_unsigned_t<SizeType>;
        struct Frontier { SizeType branches, leaves; };
        Pool pool;
        LeafPool leaves;

        static bool is_leaf(SizeType node) { return (UnsignedIndex(node) & 1) != 0; }
        static size_t record_index(SizeType node) { return size_t(UnsignedIndex(node) >> 1); }
        static SizeType handle(size_t index, bool leaf) {
            assert(index <= (size_t(std::numeric_limits<SizeType>::max()) >> 1));
            return SizeType((UnsignedIndex(index) << 1) | UnsignedIndex(leaf));
        }
        struct TreeView {
            MemoryAllocator *owner;
            Node operator[](SizeType node) const {
                return is_leaf(node) ? Node(0, 0, 1) : owner->pool[record_index(node)].tree;
            }
            size_t size() const { return owner->ptr; }
            size_t capacity() const { return owner->cap; }
            void reserve(SizeType n) { owner->reserve(n); }
        };
        struct DataView {
            MemoryAllocator *owner;
            Data operator[](SizeType node) const {
                return is_leaf(node) ? Data(owner->leaves[record_index(node)], id(), 0)
                                     : owner->pool[record_index(node)].data;
            }
            size_t size() const { return owner->ptr; }
            size_t capacity() const { return owner->cap; }
            void reserve(SizeType n) { owner->reserve(n); }
        };
        TreeView tree;
        DataView data;
        size_t ptr, cap, branch_ptr, leaf_ptr;

        MemoryAllocator() : tree{this}, data{this}, ptr(1), cap(0), branch_ptr(1), leaf_ptr(0) {
            pool.emplace_back(0, 0, 0, T{}, F{}, 0);
            refresh_cap();
        }
        MemoryAllocator(MemoryAllocator&& other) noexcept
            : pool(std::move(other.pool)), leaves(std::move(other.leaves)),
              tree{this}, data{this}, ptr(other.ptr), cap(other.cap),
              branch_ptr(other.branch_ptr), leaf_ptr(other.leaf_ptr) {}
        MemoryAllocator& operator=(MemoryAllocator&& other) noexcept {
            if (this != &other) {
                pool = std::move(other.pool);
                leaves = std::move(other.leaves);
                ptr = other.ptr; cap = other.cap;
                branch_ptr = other.branch_ptr; leaf_ptr = other.leaf_ptr;
            }
            return *this;
        }
        void refresh_cap() { cap = pool.capacity() + leaves.capacity(); }
        Frontier frontier() const { return {SizeType(branch_ptr), SizeType(leaf_ptr)}; }
        bool owns(SizeType node, Frontier first) const {
            return node && record_index(node) >= size_t(is_leaf(node) ? first.leaves : first.branches);
        }
        SizeType node_size(SizeType node) const {
            return is_leaf(node) ? 1 : pool[record_index(node)].tree.size;
        }
        T& value(SizeType node) {
            return is_leaf(node) ? leaves[record_index(node)] : pool[record_index(node)].data.data;
        }
        const T& value(SizeType node) const {
            return is_leaf(node) ? leaves[record_index(node)] : pool[record_index(node)].data.data;
        }
        SizeType copy(SizeType node) {
            if (!node) return 0;
            const size_t index = record_index(node);
            if (is_leaf(node)) {
                const SizeType result = handle(leaf_ptr, true);
                if (leaves.size() > leaf_ptr) leaves[leaf_ptr] = leaves[index];
                else {
                    try { leaves.push_back(leaves[index]); }
                    catch (...) { refresh_cap(); throw; }
                    refresh_cap();
                }
                ++leaf_ptr; ++ptr;
                return result;
            }
            const SizeType result = handle(branch_ptr, false);
            if (pool.size() > branch_ptr) pool[branch_ptr] = pool[index];
            else {
                try { pool.push_back(pool[index]); }
                catch (...) { refresh_cap(); throw; }
                refresh_cap();
            }
            ++branch_ptr; ++ptr;
            return result;
        }
        SizeType new_leaf(const T& value) {
            const SizeType result = handle(leaf_ptr, true);
            if (leaves.size() > leaf_ptr) leaves[leaf_ptr] = value;
            else {
                try { leaves.emplace_back(value); }
                catch (...) { refresh_cap(); throw; }
                refresh_cap();
            }
            ++leaf_ptr; ++ptr;
            return result;
        }
        SizeType new_node(const T& value, const F&) { return new_leaf(value); }
        SizeType new_branch(const T& value, const F& f) {
            const SizeType result = handle(branch_ptr, false);
            if (pool.size() > branch_ptr) {
                pool[branch_ptr].tree = {0, 0, 0};
                pool[branch_ptr].data = {value, f, 0};
            } else {
                try { pool.emplace_back(0, 0, 0, value, f, 0); }
                catch (...) { refresh_cap(); throw; }
                refresh_cap();
            }
            ++branch_ptr; ++ptr;
            return result;
        }
        void reserve(SizeType n) {
            const size_t count = size_t(n);
            const size_t leaf_count = count / 2;
            try {
                pool.reserve(count - leaf_count);
                leaves.reserve(leaf_count);
            } catch (...) { refresh_cap(); throw; }
            refresh_cap();
        }
        void reset() { ptr = 1; branch_ptr = 1; leaf_ptr = 0; }
        bool almost_full() const {
            return double(branch_ptr) > max(1.0, pool.capacity() * 0.99)
                || double(leaf_ptr) > max(1.0, leaves.capacity() * 0.99);
        }
        size_t capacity_bytes() const {
            return pool.capacity() * sizeof(Slot) + leaves.capacity() * sizeof(T);
        }
    };
'''

def finish_braces(source, start):
    pos=source.index('{',start)+1
    depth=1
    while depth:
        depth+=(source[pos]=='{')-(source[pos]=='}')
        pos+=1
    return pos


def once(source,before,after):
    if source.count(before)!=1:
        raise ValueError((source.count(before), before))
    return source.replace(before,after,1)


def separate_leaves(source):
    if 'SizeType size_of(SizeType node) const' not in source:
        raise ValueError('separate pools require a leaf-oriented source')
    start=source.index('    struct MemoryAllocator {')
    end=finish_braces(source,start)+1
    slot=source.index('        struct Slot {',start)
    prefix=source[start:slot]
    tail=SPLIT_ALLOCATOR_TAIL
    allocator=prefix+tail
    rest=source[end:]
    rest=once(rest,'    static constexpr int DELTA = 3, GAMMA = 2;',
              '    using Frontier = typename MemoryAllocator::Frontier;\n    static constexpr int DELTA = 3, GAMMA = 2;')
    rest=rest.replace('size_t first','Frontier first').replace('ma.ptr','ma.frontier()')
    rest=once(rest,'return ma.pool[node].tree.size;', 'return ma.node_size(node);')
    rest=once(rest,'return node && static_cast<size_t>(node) < first ? ma.copy(node) : node;',
              'return node && !ma.owns(node, first) ? ma.copy(node) : node;')
    rest=once(rest,'assert(static_cast<size_t>(node) >= first);','assert(ma.owns(node, first));')
    rest=rest.replace('if (ma.pool[node].tree.size > 1)', 'if (!ma.is_leaf(node))')
    rest=rest.replace('if (ma.pool[node].tree.size == 1)', 'if (ma.is_leaf(node))')
    rest=rest.replace('size_of(l) > 1','!ma.is_leaf(l)').replace('size_of(r) > 1','!ma.is_leaf(r)')
    rest=rest.replace('size_of(middle) > 1','!ma.is_leaf(middle)')
    rest=rest.replace('size_of(node) == 1','ma.is_leaf(node)').replace('size_of(node) > 1','!ma.is_leaf(node)')
    # Aggregates at a child can be a T-only leaf record or an internal aggregate.
    rest=re.sub(r'ma\.pool\[([A-Za-z_]\w*)\]\.data\.data',r'ma.value(\1)',rest)
    rest=once(rest,'if (!spare) spare = ma.new_node(op(ma.value(l), ma.value(r)), id());',
              'if (!spare) spare = ma.new_branch(op(ma.value(l), ma.value(r)), id());')
    rest=rest.replace('ma.new_node(value, id())','ma.new_leaf(value)')
    rest=rest.replace('ma.new_node(a[l], id())','ma.new_leaf(a[l])')
    rest=once(rest,'                ma.pool[node].data.lazy = id(); ma.pool[node].data.rev = 0;\n','')
    old='''            const SizeType left = ma.pool[node].tree.left, right = ma.pool[node].tree.right;
            if (!left && !right) {
                assert(ma.is_leaf(node) && ma.pool[node].tree.left_size == 0);
                return 1;
            }
            assert(left && right);'''
    rest=once(rest,old,'''            if (ma.is_leaf(node)) return 1;
            const SizeType left = ma.pool[node].tree.left, right = ma.pool[node].tree.right;
            assert(left && right);''')
    # Every remaining indexed pool access is to an internal branch handle.
    def decode_indexes(text):
        chunks=[]; cursor=0
        pattern=re.compile(r'ma\.pool\[')
        while match:=pattern.search(text,cursor):
            opening=match.end()-1
            pos=opening+1;depth=1
            while depth:
                depth+=(text[pos]=='[')-(text[pos]==']');pos+=1
            index=decode_indexes(text[opening+1:pos-1])
            chunks += [text[cursor:match.start()], 'ma.pool[ma.record_index('+index+')]']
            cursor=pos
        return ''.join(chunks)+text[cursor:]
    rest=decode_indexes(rest)
    result=source[:start]+allocator+rest
    result=once(result,'#pragma once','#pragma once\n#include <limits>')
    result=result.replace('// Building n > 0 values uses 2*n-1 records. Indexes must fit all allocated records.',
                          '// Building n > 0 values uses 2*n-1 records. Encoded tagged handles must fit SizeType.')
    return result



def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('output', type=Path)
    parser.add_argument('--source', type=Path, required=True)
    parser.add_argument('--separate-leaves', action='store_true',
                        help='also generate tagged T-only leaves and separate internal records')
    args = parser.parse_args()
    source = args.source.read_text()
    cached = cache_append(source)
    args.output.mkdir(parents=True, exist_ok=True)
    variants = [('append_cache', cached), ('append_cache_lazy_cap', lazy_cap(cached))]
    if args.separate_leaves:
        variants.append(('separate_pools', separate_leaves(source)))
    for name, value in variants:
        target = args.output / (name + '.hpp')
        target.write_text(value)
        print(target)


if __name__ == '__main__':
    main()
